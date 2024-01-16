#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]){
    int p[2];

    pipe(p);

    if(fork() == 0){
        // 子进程
        char buf[2];
        read(p[0], buf, 1);
        printf("%d: received ping\n", getpid());
        write(p[0], "a", 1);
        exit(0);
    }else{
        // 父进程
        char buf[2];
        write(p[1], "a", 1);
        wait(0); // 注意这里要加入wait，否则父子进程的打印交叉
        read(p[1], buf, 1);
        printf("%d: received pong\n", getpid());
        exit(0);
    }

}
