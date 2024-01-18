#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
  char c;
  char buf[22];
  memset(buf, '\0', 22);
  int count = 0;
  char* argvChild[MAXARG];
  int argvChildIndex = 0;
  for (int i = 1; i < argc; i++) {
    argvChild[argvChildIndex++] = argv[i];
  }

  while (read(0, &c, 1)) {
    if (c == '\n') {
      argvChild[argvChildIndex] = buf;

      // 调用fork，子进程直接break
      if (fork()) {
        // 子进程
        exec(argvChild[0], argvChild);
        // 在调用exec之后，下面的代码还会继续执行么？可以printf试试，不会了
        // printf("hhhhhh\n");
        break;
      }
      else {
        // 父进程
        wait(0);
        memset(buf, '\0', 22);
        count = 0;
      }
    }else{
      buf[count++] = c;
    }
    // printf("%c\n", c);
    // 这里面的最后是默认包含着一个换行符号的
  }

  exit(0);
}
