#include "kernel/types.h"
#include "user.h"
#include "kernel/stat.h"
int main(int argc,char* argv[])
{
    int pid;
    int status;
    int pipe1[2];
    int pipe2[2];

    
    pipe(pipe1);
    pipe(pipe2);
    pid = fork();
    

    if (pid == 0) //子进程
    {   
       
        int childpid;
        close (pipe2[1]);
        close (pipe1[0]);
        char childchar[10];

        read(pipe2[0],childchar,20);
        childpid = getpid();
        printf("%d: received %s\n",childpid,childchar);
         //关闭读端
        close(pipe2[0]);
        
        write(pipe1[1],"pong",100);
        close(pipe1[1]);
        
    }

    else if (pid>0)//父进程
    {   
        
        int fatherpid;
        close (pipe2[0]);
        close (pipe1[1]);
        char fatherchar[10];
        
        write(pipe2[1],"ping",20);
        close(pipe2[1]);
        wait(&status);
        fatherpid = getpid();
        
        
        read(pipe1[0],fatherchar,100);
        printf("%d: received %s\n",fatherpid,fatherchar);
        close (pipe1[0]);
        
        
        
    }
    exit(0);

}