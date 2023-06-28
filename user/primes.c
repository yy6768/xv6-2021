// lab1 utils primes
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[]) {
    int start = 2, end = 35;
    int p[2];
    int input[34];
    int len = 0;
    for(int i = start; i <= end; ++i){
        input[len ++] = i;
    }
    int pid = 0;
    do {
        if(pipe(p) < 0){
            fprintf(2, "panic: pipe");
            exit(1);
        } 
        pid = fork();
        if(pid == 0) {
            close(p[1]);
            len = 0;
            while(read(p[0], input + len, 4))
                ++ len;
            close(p[0]);
            if(len == 0) 
                exit(0);
        } else {
            close(p[0]);
            printf("prime %d\n",input[0]);
            for(int i = 1; i < len; ++ i) {
                if(input[i] % input[0]) {
                     write(p[1], input + i, sizeof(int));
                }
            }
            close(p[1]);
        }
    } while(pid == 0);
    wait(0);
    exit(0);
}