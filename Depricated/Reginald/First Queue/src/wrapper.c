//	This file contains wrapper functions used for error handling.
//	The naming convention capitalized the first letter of the original function name.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int Socket(int domain, int type, int protocol)
{
	int sockfd;

	if((sockfd = socket(domain, type, protocol)) != -1)
	{
		return sockfd;
	}

	perror("ERROR: socket() error");
	exit(EXIT_SUCCESS);
}

int Inet_pton(int af, const char *src, void *dst)
{
	int returnVal;
	
	if((returnVal = inet_pton(af, src, dst)) != 1)
	{
		errno = EFAULT;
		perror("Error: Invalid network address");
		exit(EXIT_SUCCESS);
	}
	
	return returnVal;
}

int Connect(int sockfd, const struct sockaddr *addr, socklen_t addlen)
{
	int returnVal;
	
	if((returnVal = connect(sockfd, addr, addlen)) < 0)
	{
		perror("ERROR: Connection failed");
		exit(EXIT_SUCCESS);
	}
	
	return returnVal;
}

int Write(int fd, const void *buf, size_t count)
{
	int bytesSent;
	
	if((bytesSent = write(fd, buf, count)) < 0)
	{
		perror("write() error");
		exit(EXIT_SUCCESS);
	}

	return bytesSent;
}

int Read(int fd, void *buf, size_t count)
{
	int bytesRead = 0;
	
	if((bytesRead = read(fd, buf, count)) < 0)
	{
		perror("read() error");
		exit(EXIT_SUCCESS);
	}
	
	return bytesRead;
}

int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int returnVal;
	
	if((returnVal = bind(sockfd, addr, addrlen)) < 0)
	{
		perror("bind() error");
		exit(1);
	}
	
	return returnVal;
}

int Listen(int sockfd, int backLog)
{
	int returnVal;
	
	if((returnVal = listen(sockfd, backLog)) < 0)
	{
		perror("listen() error");
		exit(1);
	}
	
	return returnVal;
}

// returns filed descriptor of accepted socket
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int listenfd;
	
	if((listenfd = accept(sockfd, addr, addrlen)) < 0)
	{
		perror("accept() error");
		exit(1);
	}
	
	return listenfd;
}

int Pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
	int returnVal;

	if((returnVal = pthread_create(thread, attr, start_routine, arg)) != 0)
	{
		perror("pthread_create() error");
		exit(1);
	}
	
	return returnVal;
}

int Pthread_detach(pthread_t thread)
{
	int returnVal;

	if((returnVal = pthread_detach(thread)) != 0)
	{
		perror("pthread_detach() error");
		exit(1);
	}

	return returnVal;
}

void *Malloc(size_t size)
{
	void *returnPtr;

	if((returnPtr = malloc(size)) == NULL)
	{
		perror("malloc() error");
		exit(1);
	}

	return returnPtr;
}

int Pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int returnVal;

	if((returnVal = pthread_mutex_lock(mutex)) != 0)
	{
		perror("pthread_mutex_lock() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	int returnVal;

	if((returnVal = pthread_mutex_unlock(mutex)) != 0)
	{
		perror("pthread_mutex_unlock() error");
		exit(1);
	}

	return returnVal;
}

int Pthread_join(pthread_t thread, void **retval)
{
	int returnVal;

	if((returnVal = pthread_join(thread, retval)) != 0)
	{
		perror("pthread_join() error");
		exit(1);
	}

	return returnVal;
}

