struct stat;
struct rtcdate;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);   // 传入fd，关闭该fd
int kill(int);  // 传入进程号pid，杀死该进行
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);  // 获取当前进程的pid
char* sbrk(int);  // 栈增长？
int sleep(int);  // 
int uptime(void); // 是啥

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*); // 后者拷贝到前者，从const也可以看出来
void *memmove(void*, const void*, int); // 拷贝n个字节
char* strchr(const char*, char c);  // 
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);  // 将字符串转为数字
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
