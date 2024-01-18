#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


char*
fmtname(char* path)
{
  static char buf[DIRSIZ + 1];
  char* p;

  // Find first character after last slash. slash（斜杠），即找最后一个斜杠后的第一个字母
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;  // 如果传入的path没有斜杠呢？那么这里的++怎么搞。根据下面的buf可知，这里必定有一个斜杠

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p)); // 将p拷贝到buf中
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}



// filename为需匹配的文件名称，curPath为当前文件夹路径
void find(char* curPath, char* filename) {
  char buf[512], * p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(curPath, 0)) < 0) {
    fprintf(2, "ls: cannot open %s\n", curPath);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "ls: cannot stat %s\n", curPath);
    close(fd);
    return;
  }

  switch (st.type) {
    // case T_FILE:
    //   printf("%s %d %d %l\n", fmtname(curPath), st.type, st.ino, st.size);
    //   break;

  case T_DIR:
    if (strlen(curPath) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, curPath);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (stat(buf, &st) < 0) {
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      // printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);

      // buf 是当前路径+斜杠+文件名称

      char* p;
      for (p = buf + strlen(buf); p >= buf && *p != '/'; p--)
        ;
      p++;

      if (st.type == T_FILE) {
        // 是文件，进行比对
        // printf("%s %s\n", buf, filename);

        if (!strcmp(p, filename)) {
          printf("%s/%s\n", curPath, filename);
        }
      }
      else if (st.type == T_DIR && strcmp(".", p) && strcmp("..", p)) {
        // 是文件夹，进行递归
        // printf("dir: %s\n", buf);
        find(buf, filename);
      }
    }
    break;
  }
  close(fd);

}


int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(2, "need 3 arguments...\n");
  }

  find(argv[1], argv[2]);
  exit(0);
}
