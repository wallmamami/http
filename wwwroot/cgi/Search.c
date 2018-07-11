#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#define MAX 1024

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

    char Student_Number[16];//学号,整形不够存
    char Major[32] = {0};//专业
    printf("%s\n", buf);

    //buf 中的字符串格式为
    //Student+Number=201506020925&Major=network
    sscanf(buf, "Student+Number=%s", Student_Number);
    printf("SN = %s\n", Student_Number);
    //printf("Major = %s\n", Major);
    return 0;
}
