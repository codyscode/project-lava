/*
Used for testing out verious code segments
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef unsigned long long tsc_t;

static __inline__ tsc_t rdtsc(void)
{
    register unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (tsc_t)lo)|( ((tsc_t)hi)<<32 );
}

/* --Passing Functions to functions--
typedef struct Input{
    void (*function)();
    int *speed;
}inputArgs;

void testMethod(inputArgs *arg){
    inputArgs *arguments = (inputArgs *)arg;
    void (*function)(int *) = arguments->function;  
    int *speedArg = arguments->speed;
    (*function)(speedArg);
    printf("Test");
}

void test2(int *array){
    printf("Hello %d %d %d %d", array[0], array[1], array[2], array[3]);
}
*/

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int Counter = 0;

void *function1(void *args){
    int *num = (int *)args;
    int count = 0;
    while(1){
        if(pthread_mutex_trylock(&lock)){
            ;
        }
        else{
            //printf("Started %d\n", *num);
            for(int i = 0; i < 100; i++){
                Counter = *num;
                /*
                if(Counter == *num){
                    printf("%d\n", Counter);
                }
                else{
                    printf("Cant");
                }
                */
            }
            //printf("Finished %d\n", *num);
            pthread_mutex_unlock(&lock);
            if(*num == 2){
                printf("Reached 2 after %d\n", count);
                exit(1);
            }
        }
        count++;
    }

}

int main(void){
    int x = 1;
    int y = 2;

    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, &function1, &x);
    pthread_create(&thread2, NULL, &function1, &y);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);


    /* --Passing a function to another function--
    void *funcPtr = &test2;
    int otherArgs[] = {10, 9, 8, 7};
    inputArgs cmdInputs;
    cmdInputs.function = funcPtr;
    cmdInputs.speed = otherArgs;
    testMethod(&cmdInputs);
    */
}