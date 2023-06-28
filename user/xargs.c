#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXTOKLEN 32

char* xargv[MAXARG];
int xargc = 0;
char buf[MAXARG * MAXTOKLEN];

int 
main(int argc, char* argv[]) {
    if(argc <= 2){
        fprintf(2, "usage: xargs cmd [args ...]\n");
        exit(1);
    }
    
    for(int i = 1; i < argc; ++ i) {
        xargv[xargc ++] = argv[i];
    }
    char* p = buf;
    int k = 0;
    while (1) {
        while ((k = read(0, p, 1)) &&  *p != '\n') {
            ++ p;
        }
        if (k == 0) 
            break;
        *p = 0;
        xargv[xargc ++] = buf;
        if (fork() == 0) {
            exec(xargv[0], xargv);
            fprintf(2, "xargs: exec %s fail\n", xargv[0]);
        } 
        wait(0);
        p = buf;
        xargc = argc - 1;
        
    }   
    exit(0);
}