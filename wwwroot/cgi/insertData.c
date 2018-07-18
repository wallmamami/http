#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mysql.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>


#define MAX 1024

void recvData(int fd, int content_length)
{
    int i = 0;
    char c;
    for(; i < content_length; i++)
    {
        read(0, &c, 1);
        write(fd, &c, 1);
    }
}

//将上传到本地的照片路径插到数据库当中
void insertData(char* major, char* subject, char* grade, char* path)
{
    MYSQL* mysql_fd = mysql_init(NULL);
    if(mysql_real_connect(mysql_fd, "127.0.0.1", "root", "", "testpaper", 3306, NULL, 0) == NULL)
    {
        printf("connect failed!\n");
        return;
    }

    
    //insert into peer values('computer', 'math', 2015, '////');
    char sql[MAX];
    sprintf(sql, "insert into peer values('%s', '%s', %d, '%s')", major, subject, atoi(grade), path);
    printf("sql = %s\n", sql);

    mysql_query(mysql_fd, sql);
}

//根据照片格式格式找到上传文件的存放路径
void get_path(char path[])
{

    //"computer_math_2015.jpeg"
    char buf[MAX];
    char* major = NULL;
    char* subject = NULL;
    char* grade = NULL;
    char* Grade = NULL;
    char* type = NULL;

    strcpy(buf, getenv("FILENAME"));
    major = strtok(buf, "_");
    major++;
    subject = strtok(NULL, "_");
    Grade = strtok(NULL, "_");
    grade = strtok(Grade, ".");
    type = strtok(NULL, ".");
    int len = strlen(type);
    type[len-2] = '\0';//去掉"
    //printf("len=%d type = %s\n", len, type);
    //printf("%s\n", buf);
    
    char name[MAX/32] = {};
    strcpy(path, "./wwwroot/picture/peer/");
    strcat(path, major);
    strcat(path, "/");
    strcat(path, subject);
    strcat(path, "/");
    strcat(path,grade);
    strcat(path, "/");
    srand((unsigned int) time(NULL));
    int x = rand()%100 + 1;
    sprintf(name, "%d.%s", x, type);
    strcat(path, name);
    printf("major = %s subject = %s grade = %s name = %s\n", major, subject, grade, name);

    insertData(major, subject, grade, path);
}
int main()
{

    //printf("mysql client Version: %s\n", mysql_get_client_info());
    int content_length = atoi(getenv("CONTENT_LENGTH"));
    //printf("content_length = %d\n", content_length);

    char path[MAX] = {};
    get_path(path);
    printf("path = %s\n", path);
    
    int fd = open(path, O_WRONLY|O_CREAT, 0666);
    if(fd < 0)
    {
        printf("push failed\n");
        return 0;
    }
    recvData(fd, content_length);
    return 0;
}
