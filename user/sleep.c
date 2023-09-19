#include "kernel/types.h"
#include "user.h"
//eg：在执行命令“cp file_a file_b”时，程序cp的main函数接收到的参数为：argc=3，argv=["cp", "file_a", "file_b"，null]
//不难理解执行“sleep x”时，argc = 2，argv=["sleep","x",null]
int main(int argc,char *argv[])
{
    if (argc!=2){
        printf("Sleep needs one argument!\n");
        exit(-1);
    }
    int ticks = atoi(argv[1]);
    sleep(ticks);
    printf("(nothing happens for a little while)\n");
    exit(0);
    
}