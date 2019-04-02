#ifndef WRAPPER_H
#define WRAPPER_H

#include<pthread.h>

int Pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int Pthread_detach(pthread_t thread);
int Pthread_join(pthread_t thread, void **retval);

int Pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched);

void *Malloc(size_t size);

int Pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int Pthread_mutex_lock(pthread_mutex_t *mutex);
int Pthread_mutex_unlock(pthread_mutex_t *mutex);

FILE *Fopen(const char *filename, const char *mode);

#endif