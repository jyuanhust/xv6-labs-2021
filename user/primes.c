#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{

  int p[2];
  pipe(p);

  // 第一次的全部发送不需要，下面的这个11需要修改为35
  for (int i = 2; i <= 35; i++) {
    write(p[1], &i, 4);
  }
  close(p[1]);

  if (fork() == 0) {

    // 循环体
    while (1) {
      // 子进程接受，
      int nums[35];
      int count = 0, num;

      while (read(p[0], &num, 4)) {  // 这里发生阻塞，说明有p[1]没有close
        // printf("num:%d\n", num);
        nums[count++] = num;
        // printf("while\n");
      }

      close(p[0]);

      if (count) {
        printf("prime %d\n", nums[0]);
      }

      if (count == 1)
        break;
      else {
        pipe(p); //重用
        if (fork()) {
          // 作为父进程，发送
          for (int i = 1; i < count; i++) {
            if (nums[i] % nums[0]) {
              // 取模不为0，说明不是倍数，则发送
              write(p[1], &nums[i], 4);
            }
          }
          close(p[1]);
          wait(0);
          
          break; //发送完数据，父进程退出while(1)循环
        }else{
          close(p[1]);
        }
      }
    }
  }else{
    close(p[0]);
  }
  wait(0);

  exit(0);
}
