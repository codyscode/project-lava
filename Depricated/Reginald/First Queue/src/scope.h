#ifndef SCOPE_H
#define SCOPE_H

#define MAXLINE 1000
#define MAXCLIENTS 10

// wrapper.c

int Pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int Pthread_detach(pthread_t thread);
void *Malloc(size_t size);

int Pthread_mutex_lock(pthread_mutex_t *mutex);
int Pthread_mutex_unlock(pthread_mutex_t *mutex);

int Pthread_join(pthread_t thread, void **retval);

#endif
