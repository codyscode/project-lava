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
/*
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
            printf("Started %d\n", *num);
            for(int i = 0; i < 100; i++){
                Counter = *num;
                if(Counter == *num){
                    printf("%d\n", Counter);
                }
                else{
                    printf("Cant");
                }
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
*/
int x[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

void *function2(void *args){
    int * t = (int *)args;
    printf("%d\n", *t);
}

typedef struct test{
    int array[3];
    int other;
}test_t;

int main(void){


    pthread_t tids[10];
    for(int i = 0; i < 5; i++){
        pthread_create(&tids[i], NULL, &function2, &x[i]);
    }
    for(int i = 5; i < 10; i++){
        pthread_create(&tids[i], NULL, &function2, &x[i]);
    }
    for(int i = 0; i < 10; i++){
        pthread_join(tids[i], NULL);
    }
    
    /*
    test_t testArray[3];
    testArray[2].array[0] = 1;
    testArray[2].array[1] = 2;
    testArray[2].array[2] = 3;
    test_t sub = testArray[2];
    printf("%d, %d, %d", sub.array[0], sub.array[1], sub.array[2]);


    /* --Passing a function to another function--
    void *funcPtr = &test2;
    int otherArgs[] = {10, 9, 8, 7};
    inputArgs cmdInputs;
    cmdInputs.function = funcPtr;
    cmdInputs.speed = otherArgs;
    testMethod(&cmdInputs);
    */
}