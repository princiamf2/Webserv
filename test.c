// test.cgi.c
#include <stdio.h>

int main()
{
    printf("Content-Type: text/plain\r\n\r\n");
    printf("Hello from CGI binary\n");
    return 0;
}