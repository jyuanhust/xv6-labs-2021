#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{ 
  int i;

  for (i = 1; i < argc; i++) {
    write(1, argv[i], strlen(argv[i])); 
    // 从上面的调用可以看出，write的最后一个参数是字符串的长度，不包括最后的\0
    // 判断是否是数组的最后一个元素
    if (i + 1 < argc) {
      write(1, " ", 1);
    }
    else {
      write(1, "\n", 1);
    }
  }
  exit(0);
  // return 0;
}
