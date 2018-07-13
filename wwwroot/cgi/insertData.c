#include <stdio.h>
#include <mysql.h>

int main()
{
    printf("mysql client Version: %s\n", mysql_get_client_info());
    return 0;
}
