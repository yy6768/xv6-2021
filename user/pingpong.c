// lab1 utils pingpong
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[1024];

int 
main(int argc, char*argv[]) {
    int fp[2], cp[2]; // 0作为输入端，1进程作为输出端
    if (pipe(fp) < 0 || pipe(cp) < 0) {
        fprintf(2, "panic: pipe\n");
        exit(1);
    }

    if (fork() == 0) {
        close(fp[1]);
        close(cp[0]);
        if(read(fp[0],buf, 1) == 1) 
            printf("%d: received ping\n", getpid());
        write(cp[1], "b", 1);
        close(fp[0]);
        close(cp[1]);
    } else {
        close(fp[0]);
        close(cp[1]);
        write(fp[1], "a", 1);
        if(read(cp[0], buf, 1) == 1) 
            printf("%d: received pong\n", getpid());
        close(cp[0]);
        close(fp[1]);
    }
    exit(0);
}
