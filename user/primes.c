#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void work(int fd) {
  int num;
  int ret = read(fd, &num, sizeof(num));
  int nxt[2] = {};
  int n;
  int first = 1; // true if nothing needs to be passed to the right

  if (ret == 0) goto END;
  printf("prime %d\n", num);
  do {
    int x;
    n = read(fd, &x, sizeof(x));
    if (x%num) {
      if (first) {
        ret = pipe(nxt);
        if (ret < 0) {
          printf("cannot open pipe\n");
          exit(1);
        }
      }
      write(nxt[1], &x, sizeof(x));
      //printf("write %d\n", x);
      if (first) {
        first = 0;
        ret = fork();
        if (ret < 0) {
          printf("cannot fork\n");
          exit(1);
        } else if (ret == 0) {
          // child
          close(fd);
          close(nxt[1]);
          work(nxt[0]);
          return;
        } else {
          // parent
          // do nothing but continuing the loop
        }
      }
    }
  } while(n);

  END:
  if (!first) {
    close(nxt[0]);
    close(nxt[1]);
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int fd[2];
  if (pipe(fd) < 0) {
    printf("cannot open pipe\n");
    exit(1);
  }
  for (int i = 2; i <= 35; i++)
    write(fd[1], &i, sizeof(i));
  close(fd[1]);

  work(fd[0]);

  wait(0);

  exit(0);
}
