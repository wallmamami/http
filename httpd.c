#include "threadpool.h"
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <strings.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX 1024
#define HOME_PAGE "index.html"

static int startup(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket");
        exit(2);
    }

    //为了解决因为服务器主动断开连接造成time_wait而不能立即绑定的问题
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0)
    {
        perror("bind");
        exit(3);
    }

    if(listen(sock, 5) < 0)
    {
        perror("listen");
        exit(4);
    }

    return sock;
}

static void usage(char* ptr)
{
    printf("Usage: %s [port]\n", ptr);
}

//因为客户端有很有很多种，所以换行有可能是以下几种\n \r\n \r,因为我们是c语言，所以全都换成\n,
//这个函数的作用就是读取一行放在line中，并且将换行统一换作\n
int get_line(int sock, char line[], int size)
{
    int c = 'a';
    int i = 0;
    ssize_t s = 0;

    //为了给最后一个放'\0'，所以是size-1
    while(i < size-1 && c != '\n')
    {
        s = recv(sock, &c, 1, 0);
        if(s > 0)
        {
            if(c == '\r')
            {
                //有两种情况，一种\r 一种\r\n
                recv(sock, &c, 1, MSG_PEEK);//只“看看”接收缓冲区中的东西，不“拿”出来
                if(c != '\n')
                {
                    c = '\n';
                }
                else
                {
                    //读出来
                    recv(sock, &c, 1, 0);//c == '\n'
                }
            }

            //此时，c如果是换行符，一定是'\n'
            line[i++] = c;
        }
        else
            break;
    }

    line[i] = '\0';
    return i;
}


void clear_header(int sock)
{
    char line[MAX];

    do
    {
        get_line(sock, line, sizeof(line));

    }while(strcmp(line, "\n") != 0);//读到空行表示读完HTTP请求，因为GET方法一般是没有正文的
}

static void show_404(int sock)
{
    clear_header(sock);

    char line[MAX];
    //响应也需要响应标准格式(响应行, 响应报头)
    sprintf(line, "HTTP/1.0 200 OK\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "Content-Type: text/html\r\n");
    send(sock, line, strlen(line), 0);

    sprintf(line, "\r\n");//一定注意，还有空行
    send(sock, line, strlen(line), 0);

    const char* err = "wwwroot/error/404.html";
    int fd = open(err, O_RDONLY);
    struct stat st;
    stat(err, &st);

    sendfile(sock, fd, NULL, st.st_size);
    close(fd);

}
static void echo_error(int errCode, int sock)
{
    switch(errCode)
    {
    case 403:
        break;
    case 404:
        show_404(sock);
        break;
    case 501:
        break;
    case 503:
        break;
    default:
        break;
    }
}

void echo_www(int sock, char path[], int size, int* err)
{
    //进入这里，说明一定是GET方法，之前只是读了HTTP请求的请求行
    //在响应之前一定要保证读完HTTP请求
    clear_header(sock);

    int fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        *err = 404;
        return;
    }

    char line[MAX];

    //响应也需要响应标准格式(响应行, 响应报头)
    sprintf(line, "HTTP/1.0 200 OK\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "Content-Type: text/html\r\n");
    send(sock, line, strlen(line), 0);

    sprintf(line, "\r\n");//一定注意，还有空行
    send(sock, line, strlen(line), 0);

    sendfile(sock, fd, NULL, size);
    close(fd);
}

//提供cgi机制
int exe_cgi(int sock, char path[], char method[], char* query_string)
{

    char line[MAX];
    int content_length = -1;//用来保存POST方法中正文的长度(字节)
    char method_env[MAX/32];
    char query_string_env[MAX];
    char content_length_env[MAX/16];

    //如果是GET方法,清空请求行和请求头部
    if(strcasecmp(method, "GET") == 0)
    {
        clear_header(sock);
    }
    else//POST
    {
        //不仅要读完请求行\请求报头,还要读出Content-Length字段
        do
        {
            get_line(sock, line, sizeof(line));
            if(strncmp(line, "Content-Length: ", 16) == 0)
            {
                content_length = atoi(line+16);
            }
        }while(strcmp(line, "\n") != 0);

        if(content_length == -1)
        {
            return 404;
        }
    }
    sprintf(line, "HTTP/1.0 200 OK\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "Content-Type: text/html\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "\r\n");
    send(sock, line, strlen(line), 0);

    //如果是POST方法,需要父进程读取请求正文的内容交给子进程
    //这里就需要管道让父子进程可以通信，并且需要两个，因为子进程也需要
    //将执行结果放回给父进程

    int input[2];//相对于子进程
    int output[2];

    pipe(input);
    pipe(output);


    //因为cgi模式就是需要去执行文件,将执行结果返回给客户端
    //所以这里需要程序替换，但是服务器是多线程，所以不能直接
    //程序替换，需要创建子进程

    pid_t id = fork();
    if(id < 0)
    {
        return 404;
    }
    else if(id == 0)//child
    {
        close(input[1]);
        close(output[0]);

        //子进程需要程序替换，但是替换的程序并不知道管道文件描述符到底是
        //什么，所以需要将子进程的文件描述符重定向
        dup2(input[0], 0);//将读管道重定向为标准输入,新的拷贝旧的
        dup2(output[1], 1);

        //因为替换的程序也需要知道客户端传来的参数信息，所以需要想办法将参数交给它
        //又因为程序替换不会替换环境变量，所以只需要将参数信息导出位环境变量
        sprintf(method_env, "METHOD=%s", method);
        putenv(method_env);

        if(strcasecmp(method, "GET") == 0)
        {
            sprintf(query_string_env, "QUERY_STRING=%s", query_string);
            putenv(query_string_env);
        }
        else
        {
            sprintf(content_length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(content_length_env);
        }
        execl(path, path, NULL);//可以两个都是绝对路径
        exit(1);//能回来说明绝对错了

    }
    else
    {
        close(input[0]);
        close(output[1]);

        char c;
        //如果是POST方法,因为参数在正文中,所以
        //就需要用管道将信息传给子进程
        if(strcasecmp(method, "POST") == 0)
        {
            int i = 0;
            for(; i < content_length; i++)
            {
                read(sock, &c, 1);
                write(input[1], &c, 1);
            }
        }

        while(read(output[0], &c, 1) > 0)
        {
            send(sock, &c, 1, 0);//从管道读取数取，发给客户端
        }

        waitpid(id, NULL, 0);
        close(input[1]);
        close(output[0]);
    }

    return 200;
}

static void* handler_request(void* arg)
{
    int sock = (int)arg;//因为指针和sock都是4字节，所以可以直接强转
    char line[MAX];
    int errCode = 200;
    char method[MAX/32];//保存请求行中的方法GET/POST
    char url[MAX];//路径,如果是GET方法,可能会有浏览器提交的参数
    int cgi = 0;//标记时是否按照cgi方式运行
    char* query_string = NULL;//用来指向GET方法中url的参数信息
    char path[MAX];//用来保存路径

#ifdef Debug
    do
    {
        get_line(sock, line, sizeof(line));
        printf("%s", line);

    }while(strcmp(line, "\n") != 0);//读到空行退出,只拿到请求行和请求报头
#else

    //分析请求行信息--只读了一行
    if(get_line(sock, line, sizeof(line)) < 0)
    {
        errCode = 404;
        goto end;
    }

    //line[] = GET /index.php?a=100&&b=200 HTTP/1.1
    //将line中的信息分离，分别保存到method\url

    int i = 0;
    int j = 0;
    while(i < sizeof(method)-1 && j < sizeof(line) && !isspace(line[j]))
    {
        method[i] = line[j];
        i++;
        j++;
    }
    method[i] = '\0';

    if(strcasecmp(method, "GET") == 0)//比较不区分大小写
    {}
    else if(strcasecmp(method, "POST") == 0)
    {
        //按照cgi方式运行i,因为POST方法,浏览器一定向服务器提交参数了
        cgi = 1;
    }
    else
    {
        errCode = 404;
        goto end;
    }

    //因为在请求行中是以空格分隔的,所以这里要跳过空格
    while(j < sizeof(line) && isspace(line[j]))
    {
        j++;
    }

    i = 0;
    while(i < sizeof(url)-1 && j < sizeof(line) && !isspace(line[j]))
    {
        url[i] = line[j];
        i++;
        j++;
    }
    url[i] = '\0';

    //分析url中的信息,如果是GET方法,就一定会将参数添加到url中
    if(strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while(*query_string)
        {
            if(*query_string == '?')//如果有参数,会以?分隔,前面的为路径,后面的为参数,并且要以cgi方式运行
            {
                *query_string = '\0';//将路径和参数分隔开
                query_string++;
                cgi = 1;
                break;
            }

            query_string++;
        }
    }

    //因为http请求中的路径的根目录就是服务器的根目录就是这里的wwwroot,所以将其添加进去
    sprintf(path, "wwwroot%s", url);

    //如果请求中url为某个目录,默认响应"首页"
    if(path[strlen(path)-1] == '/')
    {
        strcat(path, HOME_PAGE);
    }

    //判断文件是否存在以及属性信息
    struct stat st;
    if(stat(path, &st) < 0)
    {
        errCode = 404;
        goto end;
    }

    else
    {
        if(S_ISDIR(st.st_mode))
        {
            strcat(path, HOME_PAGE);
        }
        else
        {
            //如果任何一个人有执行权限都要以cgi模式运行
            if(st.st_mode & S_IXUSR || st.st_mode & S_IXGRP || st.st_mode & S_IXOTH)
            {
                cgi = 1;
            }
        }
        if(cgi)
        {
            exe_cgi(sock, path, method, query_string);
        }

        else
        {
            printf("path = %s\n", path);
            echo_www(sock, path, st.st_size, &errCode);
        }
    }


#endif
end:
    if(errCode != 200)
    {
        echo_error(errCode, sock);
    }
    close(sock);
}

//./http 8080
int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        usage(argv[0]);
        exit(1);
    }

    //创建线程池并初始化
    threadpool_t pool;
    threadpool_init(&pool, 2, 5);//创建两个空闲线程，线程池最多5个线程
    int listen_sock = startup(atoi(argv[1]));

    //多线程版
    while(1)
    {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int new_sock = -1;

        if((new_sock = accept(listen_sock, (struct sockaddr*)&client, &len))< 0)
        {
            perror("accept");
            continue;
        }

        //向线程池中添加任务
        threadpool_add_task(&pool, handler_request, (void*)new_sock);
        ////创建线程
        //pthread_t tid;
        //pthread_create(&tid, NULL, handler_request, (void*)new_sock);

        ////为了不等待，将其设置为分离状态
        //pthread_detach(tid);
    }

    threadpool_destroy(&pool);
    return 0;
}
