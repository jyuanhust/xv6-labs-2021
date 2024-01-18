#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(){
  if(fork() == 0){
    //子进程
    write(1, "hello ", 6);
    exit(0);
  }else{
    //父进程
    wait(0);
    write(1, "world\n", 6);
    exit(0);
  }
}