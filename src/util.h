#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))

#define OK  0
#define ERR -1

struct event_t
{
    int state;
    int manual_reset;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};


#ifdef _WIN32
typedef HANDLE MUTEX_ID;
typedef HANDLE TASK_ID;
typedef LPTHREAD_START_ROUTINE TASK_ENTRY_FUNC;
typedef SOCKET SOCKET_ID;
typedef HANDLE SEM_ID;
typedef HANDLE EVENT_ID;
#else

#ifndef UINT
typedef unsigned int UINT;
#endif

#ifndef DWORD
typedef unsigned int DWORD;
#endif

//typedef bool BOOL;
#ifndef BYTE
typedef unsigned char BYTE;
#endif

typedef pthread_mutex_t* MUTEX_ID;
typedef pthread_t * TASK_ID;
typedef int SOCKET_ID;
typedef void *(*TASK_ENTRY_FUNC)(void *pArg);
typedef sem_t* SEM_ID;
typedef struct event_t*  EVENT_ID;

#define THREAD_TYPE_DETACH 1
#define THREAD_TYPE_JOIN 2
#endif

MUTEX_ID OSCreateMutex();
int OSEnterMutex(MUTEX_ID nMutex);
int OSLeaveMutex(MUTEX_ID nMutex);
int OSCloseMutex(MUTEX_ID nMutex);
SEM_ID OSCreateSemaphore(int initCount, int maxCount);
int OSCloseSemaphore(SEM_ID nSemId);
int OSReleaseSemaphore(SEM_ID nSemId);
int OSWaitSemaphore(SEM_ID nSemId);
int OSTryWaitSemaphore(SEM_ID nSemId);
TASK_ID OSCreateThread(TASK_ENTRY_FUNC lpThreadFunc, void* param, int type = THREAD_TYPE_DETACH);
TASK_ID OSCreateThread(TASK_ENTRY_FUNC lpThreadFunc, void* param, int type, int pri);
int OSWaitThread(TASK_ID hThread);
int OSTerminateThread(TASK_ID hThread, int waitMS);
int OSCloseThread(TASK_ID hThread);
int OSSleep(UINT nSleepTime);
SOCKET_ID OSCreateSocket(int af, int type, int protocol);
int OSCloseSocket(SOCKET_ID nSockFd);
int OSSetSocketBlock(SOCKET_ID sockFd, int bblock);
int OSGetSocketOpt(int sockFd, int level, int optName, char* optValue, int* optLen);
int OSSetSocketOpt(int sockFd, int level, int optName, const char* optValue, int optLen);
UINT OSGetSocketError();
UINT OSGetTimeNow();
EVENT_ID OSCreatEvent(int bManualReset, int bInitState);
int OSWaitEvent(EVENT_ID hEvent);
int OSTimedWaitEvent(EVENT_ID hEvent, long MilliSeconds);
int OSSetEvent(EVENT_ID hEvent);
int OSResetEvent(EVENT_ID hEvent);
int OSCloseEvent(EVENT_ID hEvent);

#ifndef _WIN32
struct condsem_s
{
    unsigned int nCount;
    pthread_mutex_t tMutex;
    pthread_cond_t tCond;
};
typedef struct condsem_s* CONDSEM_ID;

CONDSEM_ID OSCreateSemCond();
int OSReleaseSemCond(CONDSEM_ID CondSemId);
int OSWaitSemCond(CONDSEM_ID CondSemId, int iTimeOut);
int OSCloseSemCond(CONDSEM_ID CondSemId);
int OSLockSet(int fd, int type);

#endif
#endif
