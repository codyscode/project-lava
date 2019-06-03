//Wrapper functions for Built in library functions for error checking

#include<global.h>
#include<pthread.h>

int Pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg){
	int returnVal;

	if((returnVal = pthread_create(thread, attr, start_routine, arg)) != 0)
	{
		perror("pthread_create() error");
		exit(1);
	}
	
	return returnVal;
}

int Pthread_detach(pthread_t thread){
	int returnVal;

	if((returnVal = pthread_detach(thread)) != 0){
		perror("pthread_detach() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_join(pthread_t thread, void **retval){
	int returnVal;

	if((returnVal = pthread_join(thread, retval)) != 0){
		perror("pthread_join() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr){
	int returnVal;

	if((returnVal = pthread_mutex_init(mutex, mutexattr)) != 0){
		perror("pthread_mutex_init() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_mutex_lock(pthread_mutex_t *mutex){
	int returnVal;

	if((returnVal = pthread_mutex_lock(mutex)) != 0){
		perror("pthread_mutex_lock() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_mutex_unlock(pthread_mutex_t *mutex){
	int returnVal;

	if((returnVal = pthread_mutex_unlock(mutex)) != 0){
		perror("pthread_mutex_unlock() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched){
	int returnVal;

	if((returnVal = pthread_attr_setinheritsched(attr, inheritsched)) != 0){
		perror("pthread_attr_setinheritsched() error");
		exit(1);
	}

	return returnVal;
}

void *Malloc(size_t size){
	void *returnPtr;

	if((returnPtr = malloc(size)) == NULL){
		perror("malloc() error");
		exit(1);
	}

	return returnPtr;
}

FILE *Fopen(const char *filename, const char *mode){
	FILE *fptr;
	
	if((fptr = fopen(filename, mode)) == NULL){
		fprintf(stderr, "ERROR: Unable to open \"%s\" for \"%s\"", filename, mode);
		perror("");
		exit(1);
	}
}






