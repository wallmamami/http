#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mysql.h>
#include <strings.h>
#include <string.h>

#define MAX 1024

void selecdata(char* major, char* grade, char* subject)
{
    //初始化数据库
    MYSQL* mysql_fd = mysql_init(NULL);
    if(mysql_real_connect(mysql_fd, "127.0.0.1", "root", "", "testpaper", 3306, NULL, 0) == NULL)
    {
        printf("connect failed!\n");
        return;
    }
    //printf("connect mysql success!\n");

    char sql[MAX];//用来保存sql的命令
    sprintf(sql, "select * from college1 where major='%s' and grade='%s' and subject='%s'", major, grade, subject);
    mysql_query(mysql_fd, sql);

    //如果是选择这种命令就要获取结果
    MYSQL_RES* res = mysql_store_result(mysql_fd);
    int row = mysql_num_rows(res);
    int col = mysql_num_fields(res);
    MYSQL_FIELD* field = mysql_fetch_fields(res);
    int i = 0;
    for(; i < col; i++)
    {
        printf("%s\t", field[i].name);//列名
    }
    printf("\n");
    
    printf("<table border=\"1\">");
    for(i=0; i<row; i++)
    {
        MYSQL_ROW rowData = mysql_fetch_row(res);
        int j = 0;
        printf("<tr>");
        for(; j < col; j++)
        {
            if(j == col-1)//只将最后一列转换为超链接
            {
                printf("<td><a href=\"http:\/\/172.20.10.9:9090%s\">Browse</a></td>", rowData[j]);
            }
            else
                printf("<td>%s</td>", rowData[j]);
        }
        printf("</tr>");
    }
    printf("</table>");

    mysql_close(mysql_fd);
}

int main()
{

    //首先，导出环境变量
    char buf[MAX];
    if(getenv("METHOD"))
    {
        if(strcasecmp(getenv("METHOD"), "GET") == 0)
        {
            strcpy(buf, getenv("QUERY_STRING"));
        }
        else
        {
            int content_length = atoi(getenv("CONTENT_LENGTH"));
            int i = 0; 
            char c;
            for(; i < content_length; i++)
            {
                read(0, &c, 1);
                buf[i] = c;
            }
            buf[i] = '\0';
        }
    }

    //printf("%s\n", buf);

    char* major = NULL;
    char* grade = NULL;
    char* subject = NULL;
    //buf 中的字符串格式为
    //major=network&grade=2015&subject=math;
    strtok(buf, "=&");//在分隔符处放'\0'
    major = strtok(NULL, "=&");
    strtok(NULL, "=&");
    grade = strtok(NULL, "=&");
    strtok(NULL, "=&");
    subject = strtok(NULL, "=&");
    
    //printf("major = %s\n grade = %s\n subject = %s\n", major, grade, subject);
    selecdata(major, grade, subject);
    return 0;
}
