#include "util.h"
#ifdef _WIN32
#include "YCatLog.h"
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
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#endif

MUTEX_ID OSCreateMutex()
{
#ifdef _WIN32
    CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(pSection);
    return (HANDLE)pSection;
#else
    MUTEX_ID nMutexId;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    nMutexId = (MUTEX_ID)malloc(sizeof(pthread_mutex_t));
    if (!nMutexId)
    {
        return NULL;
    }

    if (pthread_mutex_init((pthread_mutex_t*)nMutexId, &attr) != 0)
    {
        free((void *)nMutexId);
        return (MUTEX_ID)NULL;
    }
    pthread_mutexattr_destroy(&attr);
    return nMutexId;
#endif
}

int OSEnterMutex(MUTEX_ID nMutexId)
{
#ifdef _WIN32
    assert(nMutexId);
    EnterCriticalSection((CRITICAL_SECTION*)nMutexId);
    return OK;
#else
    if (nMutexId == NULL)
    {
        return ERR;
    }

    if (pthread_mutex_lock((pthread_mutex_t *)nMutexId) != 0)
    {
        return ERR;
    }
    return OK;
#endif
}

int OSTryEnterMutex(MUTEX_ID nMutexId)
{
#ifdef _WIN32
    assert(nMutexId);
    if (TryEnterCriticalSection((CRITICAL_SECTION*)nMutexId))
        return OK;
    return ERR;
#else
    if (nMutexId == (MUTEX_ID)NULL)
    {
        return ERR;
    }

    if (pthread_mutex_trylock((pthread_mutex_t *)nMutexId) != 0)
    {
        return ERR;
    }
    return OK;
#endif
}

int OSLeaveMutex(MUTEX_ID nMutexId)
{
#ifdef _WIN32
    assert(nMutexId);
    LeaveCriticalSection((CRITICAL_SECTION*)nMutexId);
    return OK;
#else
    if (nMutexId == (MUTEX_ID)NULL)
    {
        return ERR;
    }

    if (pthread_mutex_unlock((pthread_mutex_t *)nMutexId) != 0)
    {
        return ERR;
    }
    return OK;
#endif
}

int OSCloseMutex(MUTEX_ID nMutexId)
{
#ifdef WIN32
    assert(nMutexId);
    DeleteCriticalSection((CRITICAL_SECTION*)nMutexId);
    free(nMutexId);
    return OK;
#else
    if (nMutexId == (MUTEX_ID)NULL)
    {
        return ERR;
    }

    if (pthread_mutex_destroy((pthread_mutex_t *)nMutexId) != 0)
    {
        return ERR;
    }
    free((void *)nMutexId);
    return OK;
#endif
}

SEM_ID OSCreateSemaphore(int initCount, int maxCount)
{
#ifdef _WIN32
    return CreateSemaphore(NULL, initCount, maxCount, NULL);
#else
    SEM_ID nSemId = (SEM_ID)malloc(sizeof(sem_t));
    if (nSemId == NULL)
        return NULL;
    int ret = sem_init(nSemId, initCount, initCount);
    if (ret == -1)
    {
        free((void *)nSemId);
        return NULL;
    }
    return nSemId;
#endif
}

int OSCloseSemaphore(SEM_ID nSemId)
{
#ifdef _WIN32
    if (CloseHandle(nSemId))
        return OK;
    return ERR;
#else
    if (sem_destroy((sem_t *)nSemId) != 0)
        return ERR;
    free((void*)nSemId);
    return OK;
#endif
}

int OSReleaseSemaphore(SEM_ID nSemId)
{
#ifdef _WIN32
    if (ReleaseSemaphore(nSemId, 1, NULL))
        return OK;
    return ERR;
#else
    if (sem_post((sem_t *)nSemId) != 0)
    {
        return ERR;
    }
    return OK;
#endif
}

int OSWaitSemaphore(SEM_ID nSemId)
{
#ifdef _WIN32
    return WaitForSingleObject(nSemId, INFINITE);
#else
    return sem_wait(nSemId);
#endif
}

int OSTryWaitSemaphore(SEM_ID nSemId)
{
#ifdef _WIN32
    return WaitForSingleObject(nSemId, 0);
#else
    return sem_trywait(nSemId);
#endif
}

TASK_ID OSCreateThread(TASK_ENTRY_FUNC lpThreadFunc, void* param, int type, int pri)
{
#ifdef _WIN32
    HANDLE hThread;
    DWORD dummy;

    hThread = CreateThread(NULL, 0, (TASK_ENTRY_FUNC)lpThreadFunc, param, 0, &dummy);
    if (!hThread)
        fprintf(stderr,"CreateThread 0x%p failed: %d", (void*)lpThreadFunc, GetLastError());

	fprintf(stderr,"CreateThread ThreadId:%d", hThread);
    return hThread;
#else
    pthread_attr_t  attr;
    pthread_t *ptask = (pthread_t *)malloc(sizeof(pthread_t));
    if (!ptask)
    {
        fprintf(stderr,"pthread alloc failed!\n");
        return NULL;
    }

    if (pthread_attr_init(&attr) != 0)
    {
        fprintf(stderr,"pthread init attr failed!\n");
        return NULL;
    }

    int pthreadType;
    if(type == THREAD_TYPE_DETACH)
    {
        pthreadType = PTHREAD_CREATE_DETACHED;
    }
    else
    {
        pthreadType = PTHREAD_CREATE_JOINABLE;
    }

    struct sched_param schedParam;
    if(pri != -1) {
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        schedParam.sched_priority = pri;
        pthread_attr_setschedparam(&attr, &schedParam);
    }

    if (pthread_attr_setdetachstate(&attr, pthreadType) != 0)
    {
        fprintf(stderr,"pthread_attr_setdetachstate failed!\n");
        return NULL;
    }

    if (pthread_create(ptask, &attr, lpThreadFunc, param) != 0)
    {
        fprintf(stderr,"pthread init attr failed,%s!\n", strerror(errno));
        return NULL;
    }

    if (pthread_attr_destroy(&attr) != 0)
    {
        fprintf(stderr,"pthread destroy attr failed!\n");
        return NULL;
    }

    //fprintf(stderr,"CreateThread ThreadId:%u", (unsigned int)(*ptask));
    return ptask;
#endif
}

TASK_ID OSCreateThread(TASK_ENTRY_FUNC lpThreadFunc, void* param, int type)
{
    return OSCreateThread(lpThreadFunc, param, type, -1);
}

int OSWaitThread(TASK_ID hThread)
{
#ifdef _WIN32
	return WaitForMsgSingleThread(hThread);
#else
	if(NULL == hThread)
		return -1;
	
	int ret = pthread_join(*hThread, NULL);
	if(ret != 0)
		fprintf(stderr,"pthread_join failure, ret = %s\n", strerror(ret));
	return ret;
#endif
}

int OSCloseThread(TASK_ID hThread)
{
#ifdef _WIN32
    assert(hThread);

    if (CloseHandle(hThread))
        return OK;
    return ERR;
#else
    if (0 == hThread)
    {
        fprintf(stderr,"NULL task id!\n");
        return ERR;
    }

    int ret = pthread_cancel(*hThread);
    if (ret == 3)
    {
        fprintf(stderr,"pthread_cancel ret = 3");
    }
    else if (ret != 0 && ESRCH != errno)
    {
        fprintf(stderr,"pthread_cancel failed, ret = %d\n", ret);
        return ERR;
    }
    free((pthread_t*)hThread);
    return OK;
#endif
}

#ifdef _WIN32
int OSTerminateThread(TASK_ID hThread, DWORD waitMS)
{
    assert(hThread);

    if (WaitForSingleObjectEx(hThread, waitMS, 0) == WAIT_TIMEOUT)
    {
        fprintf(stderr,"WARNING: Forcibly terminating a thread after %d ms timeout!", waitMS);

        //force a crash report. if we're terminating threads, something terribly wrong is happening, best
        //to get a report of that rather than let corruption from thread termination continue to propagate
        //and cause harder to debug errors.
        DebugBreak();
        TerminateThread(hThread, 0);
    }

    if (CloseHandle(hThread))
        return OK;
    return ERR;
}
#endif

int OSSleep(UINT nSleepTime)
{
#ifdef _WIN32
    Sleep(nSleepTime);
    return OK;
#else
    struct timespec ts, rem;

    ts.tv_sec = nSleepTime / 1000;
    ts.tv_nsec = (nSleepTime % 1000) * 1000000;
    while (nanosleep(&ts, &rem) != 0)
    {
        if (EINTR == errno)
        {
            ts = rem;
            errno = 0;
            continue;
        }

        fprintf(stderr,"[taskSleep]sleep failed!\n");
        return ERR;
    }

    return OK;
#endif
}

SOCKET_ID OSCreateSocket(int af, int type, int protocol)
{
#ifdef _WIN32
    SOCKET sockfd;
#else
    int sockfd;
#endif
    sockfd = socket(af, type, protocol);
    return sockfd;
}

int OSCloseSocket(SOCKET_ID sockFd)
{
#ifdef _WIN32
    return closesocket(sockFd);
#else
    return close(sockFd);
#endif
}

UINT OSGetSocketError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

int OSGetSocketOpt(int sockFd, int level, int optName, char* optValue, int* optLen)
{
#ifdef _WIN32
    return getsockopt(sockFd, level, optName, optValue, optLen);
#else
    return getsockopt(sockFd, level, optName, optValue, (socklen_t*)optLen);
#endif
}

int OSSetSocketOpt(int sockFd, int level, int optName, const char* optValue, int optLen)
{
#ifdef _WIN32
    return setsockopt(sockFd, level, optName, optValue, optLen);
#else
    return setsockopt(sockFd, level, optName, optValue, (socklen_t)optLen);
#endif
}

int OSSetSocketBlock(SOCKET_ID sockFd, int bblock)
{
    unsigned long mode;

#ifdef _WIN32
    mode = bblock?0:1;
    return ioctlsocket(sockFd,FIONBIO,&mode);
#else
    mode = fcntl(sockFd, F_GETFL, 0);
	return bblock?fcntl(sockFd,F_SETFL, mode&~O_NONBLOCK): fcntl(sockFd, F_SETFL, mode | O_NONBLOCK);
#endif
}

UINT OSGetTimeNow()
{
#ifdef _WIN32
    return((UINT)timeGetTime());
    //return((UINT)GetTickCount());
#else
    struct timeval nowTime;
    UINT nMicroSecs;
    gettimeofday(&nowTime, NULL);
    nMicroSecs = nowTime.tv_sec * 1000 + nowTime.tv_usec / 1000;
    return nMicroSecs;
#endif
}

#ifndef _WIN32
CONDSEM_ID OSCreateSemCond()
{
    CONDSEM_ID CondSemId = (CONDSEM_ID)malloc(sizeof(struct condsem_s));
    if (!CondSemId)
        return OK;

    CondSemId->nCount = 0;
    pthread_mutex_init(&CondSemId->tMutex, 0);
    pthread_cond_init(&CondSemId->tCond, 0);

    return CondSemId;
}

int OSReleaseSemCond(CONDSEM_ID CondSemId)
{
	if(CondSemId == NULL) {
		fprintf(stderr,"\n\n\n---- OSReleaseSemCond CondSemId is NULL ----\n\n\n");
		return OK;
	}
	
    pthread_mutex_lock(&CondSemId->tMutex);
    ++CondSemId->nCount;
    pthread_cond_broadcast(&CondSemId->tCond);
    pthread_mutex_unlock(&CondSemId->tMutex);

    return OK;
}

// ???:iTimeOut
int OSWaitSemCond(CONDSEM_ID CondSemId, int iTimeOut)
{
    struct timeval tNow;
    struct timespec tAbsTime;

    pthread_mutex_lock(&CondSemId->tMutex);

    if (-1 != iTimeOut)
    {
        gettimeofday(&tNow, 0);
        //tAbsTime.tv_sec = tNow.tv_sec + iTimeOut;
        //tAbsTime.tv_nsec = tNow.tv_usec * 1000;
        if ((tNow.tv_usec+iTimeOut) * 1000 >= 1000000000)
        {
        	tAbsTime.tv_sec = tNow.tv_sec + 1;
			tAbsTime.tv_nsec = (tNow.tv_usec+iTimeOut) * 1000 - 1000000000;
        }
		else
		{
	        tAbsTime.tv_sec = tNow.tv_sec;
	        tAbsTime.tv_nsec = (tNow.tv_usec+iTimeOut) * 1000;
		}
    }

    while (!CondSemId->nCount)
    {
        if (-1 == iTimeOut)
        {
            pthread_cond_wait(&CondSemId->tCond, &CondSemId->tMutex);
        }
        else
        {
            if (ETIMEDOUT == pthread_cond_timedwait(&CondSemId->tCond, &CondSemId->tMutex, &tAbsTime))
                break;
        }
    }

    if (CondSemId->nCount > 0)
    {
        --CondSemId->nCount;
        pthread_mutex_unlock(&CondSemId->tMutex);
        return OK;
    }
    else
    {
        pthread_mutex_unlock(&CondSemId->tMutex);
        return ERR;
    }

}

int OSCloseSemCond(CONDSEM_ID CondSemId)
{
    if (pthread_mutex_destroy(&CondSemId->tMutex) ||
        pthread_cond_destroy(&CondSemId->tCond))
    {
        return ERR;
    }
    free(CondSemId);
    return OK;
}
#endif


EVENT_ID OSCreatEvent(int bManualReset, int bInitState)
{
#ifdef _WIN32
	HANDLE hevent = CreateEvent(NULL, bManualReset, bInitState, NULL);
#else
	EVENT_ID hevent = (EVENT_ID)malloc(sizeof(struct event_t));
	if (hevent == NULL)
	{
		return NULL;
	}

	hevent->state = bInitState;
	hevent->manual_reset = bManualReset;
	if (pthread_mutex_init(&hevent->mutex, NULL))
	{
		free(hevent);
		return NULL;
	}

	if (pthread_cond_init(&hevent->cond, NULL))
	{
		pthread_mutex_destroy(&hevent->mutex);
		free(hevent);
		return NULL;
	}
#endif

	return hevent;
}

int OSWaitEvent(EVENT_ID hEvent)
{
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(hEvent, INFINITE);
    if (ret == WAIT_OBJECT_0)
    {
        return 0;
    }
    return -1;
#else
    if (pthread_mutex_lock(&hEvent->mutex))
    {
        return -1;
    }
    while (!hEvent->state)
    {
        if (pthread_cond_wait(&hEvent->cond, &hEvent->mutex))
        {
            pthread_mutex_unlock(&hEvent->mutex);
            return -1;
        }
    }
    if (!hEvent->manual_reset)
    {
        hEvent->state = false;
    }
    if (pthread_mutex_unlock(&hEvent->mutex))
    {
        return -1;
    }
    return 0;
#endif
}

int OSTimedWaitEvent(EVENT_ID hEvent, long MilliSeconds)
{
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(hEvent, MilliSeconds);
    if (ret == WAIT_OBJECT_0)
    {
        return 0;
    }
    if (ret == WAIT_TIMEOUT)
    {
        return 1;
    }
    return -1;
#else

    int rc = 0;
    struct timespec abstime;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    abstime.tv_sec  = tv.tv_sec + MilliSeconds / 1000;
    abstime.tv_nsec = tv.tv_usec*1000 + (MilliSeconds % 1000)*1000000;
    if (abstime.tv_nsec >= 1000000000)
    {
        abstime.tv_nsec -= 1000000000;
        abstime.tv_sec++;
    }

    if (pthread_mutex_lock(&hEvent->mutex) != 0)
    {
        return -1;
    }
    while (!hEvent->state)
    {
        if ((rc = pthread_cond_timedwait(&hEvent->cond, &hEvent->mutex, &abstime)))
        {
            if (rc == ETIMEDOUT) break;
            pthread_mutex_unlock(&hEvent->mutex);
            return -1;
        }
    }
    if (rc == 0 && !hEvent->manual_reset)
    {
        hEvent->state = false;
    }
    if (pthread_mutex_unlock(&hEvent->mutex) != 0)
    {
        return -1;
    }
    if (rc == ETIMEDOUT)
    {
        //timeout return 1
        return 1;
    }
    //wait event success return 0
    return 0;
#endif
}

int OSSetEvent(EVENT_ID hEvent)
{
#ifdef _WIN32
    return !SetEvent(hEvent);
#else
    if (pthread_mutex_lock(&hEvent->mutex) != 0)
    {
        return -1;
    }

    hEvent->state = true;

    if (hEvent->manual_reset)
    {
        if(pthread_cond_broadcast(&hEvent->cond))
        {
            return -1;
        }
    }
    else
    {
        if(pthread_cond_signal(&hEvent->cond))
        {
            return -1;
        }
    }

    if (pthread_mutex_unlock(&hEvent->mutex) != 0)
    {
        return -1;
    }

    return 0;
#endif
}

int OSResetEvent(EVENT_ID hEvent)
{
#ifdef _WIN32
    //ResetEvent 返回非零表示成功
    if (ResetEvent(hEvent))
    {
        return 0;
    }
    return -1;
#else
    if (pthread_mutex_lock(&hEvent->mutex) != 0)
    {
        return -1;
    }

    hEvent->state = false;

    if (pthread_mutex_unlock(&hEvent->mutex) != 0)
    {
        return -1;
    }
    return 0;
#endif
}

int OSCloseEvent(EVENT_ID hEvent)
{
#ifdef _WIN32
	CloseHandle(hEvent);
#else
	pthread_cond_destroy(&hEvent->cond);
	pthread_mutex_destroy(&hEvent->mutex);

	if(hEvent)
		free(hEvent);
#endif

	return 0;
}

#ifndef _WIN32
int OSLockSet(int fd, int type)
{
#if 0
    struct flock lock;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
	lock.l_type = type;
	
	if(fcntl(fd, F_SETLK, &lock) != 0) {
		// ���벻���������̷���
		fprintf(stderr,"OSLockSet failed, return!");
		return -1;
	}

#endif

	if(flock(fd, type) != 0) {
		// ���벻���������̷���
		fprintf(stderr,"OSLockSet failed, return!");
		return -1;
	}

//	fprintf(stderr,"OSLockSet success, return!");
	
	return 0;
}
#endif


