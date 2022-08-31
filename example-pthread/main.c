#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
 
// Let us create a global variable to change it in threads
int g = 0;
 
pthread_mutex_t lock;

// The function to be executed by all threads
void *myThreadFun()
{  
    pthread_mutex_lock(&lock);
    int a = g;

    a++;
    g = a;
    pthread_mutex_unlock(&lock);
}
 
int main()
{
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }
    int i;
    pthread_t tid;

    for (i = 0; i < 100; i++) {
        pthread_create(&tid, NULL, myThreadFun, NULL);
        pthread_detach(tid);
    }

    for (size_t i = 0; i < 100000000; i++);
    
    printf("g = %d\n",g);
    pthread_mutex_destroy(&lock);
}