#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX 36

int init1(){
    int p[2];
    pipe(p);
    if(fork()==0){
        for(int i=2;i<MAX;i++){
            write(p[1],&i,sizeof(int));
        }
        close(p[1]);
        exit(0);
    }
    close(p[1]);
    return p[0];

}
int prime_filter(int in,int prime){
    int num;
    int p[2];
    pipe(p);
    if(fork()==0){
        while(read(in,&num,sizeof(int))){
            if(num%prime){
                write(p[1],&num,sizeof(int));
            }
        }
        close(p[1]);
        close(in);
        exit(0);
    }
    close(in);
    close(p[1]);
    return p[0];

    
}
int main(int argc,char *argv[]){
    int in=init1();
    int prime;
    while(read(in,&prime,sizeof(int))){
        fprintf(1,"prime %d\n",prime);
        in =prime_filter(in,prime);
    }
    exit(0);
}