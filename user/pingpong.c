#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p2c[2] = {-1, -1};
  int c2p[2] = {-1, -1};

  int ret = pipe(p2c);
  if (ret < 0) exit(1);
  ret = pipe(c2p);
  if (ret < 0) exit(1);
  
  int pid = fork();
  if (pid < 0) exit(1);
  char ch;
  if (pid == 0) {
    // child process
    int self = getpid();
    read(p2c[0], &ch, 1);
    printf("%d: received ping\n", self);

    write(c2p[1], &ch, 1);
  } else {
    // parent process
    int self = getpid();
    write(p2c[1], &ch, 1);

    read(c2p[0], &ch, 1);
    printf("%d: received pong\n", self);
  }
  exit(0);
}
