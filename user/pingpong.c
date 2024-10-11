#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]){
    int pipe1[2]; // 用于父进程到子进程的管道
    int pipe2[2]; // 用于子进程到父进程的管道
    char buffer[5];
    char buffer2[4] = "ping";
    char buffer3[4] = "pong";
    int pid;
    // 创建两个管道
    pipe(pipe1);
    pipe(pipe2);
    int ppid = getpid();
    // 创建子进程
    pid = fork();
    if (pid < 0) {
        // fork失败
        printf("fork failed\n");
        exit(0);
    }
    if (pid == 0) {
        // 子进程
        close(pipe1[1]); // 关闭写端
        close(pipe2[0]); // 关闭读端

        // while (read(pipe1[0], buffer, 4)==0){

        // }
        if (read(pipe1[0], buffer, 4)){
            // printf("%d: received",getpid());
            // sprintf(buffer, "from ");
            // printf("pid %d",getppid());
            printf("%d: received %s from pid %d\n",getpid(),buffer,ppid);
        }

        write(pipe2[1], buffer3, 4); // 向父进程写入

        close(pipe1[0]);
        close(pipe2[1]);
        exit(0);
    } else {
        // 父进程
        close(pipe1[0]); // 关闭读端
        close(pipe2[1]); // 关闭写端

        
        
        write(pipe1[1], buffer2, 4); // 向子进程写入,写入传向子进程的通道里

        if (read(pipe2[0], buffer, 4)>0){
            // printf("%d: received ",getpid());
           // sprintf(buffer, "from ");
            // printf("pid %d",pid);
            printf("%d: received %s from pid %d\n",getpid(),buffer,pid);
            // sprintf(buffer, "from ");
            // printf("pid %d",pid);
            // sprintf("%d: received ", buffer," from pid %d",getPid(),pid);
        }
         

        close(pipe1[1]);
        close(pipe2[0]);
        // wait(); // 等待子进程结束
    }

    exit(0);
}
