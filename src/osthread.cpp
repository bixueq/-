//
// Created by golist on 2019/5/27.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <syscall.h>
#include <cerrno>
#include <mutex>
#include <unistd.h>
#include "osthread.h"


//! 构造函数
osthread::osthread() {

}

//! 构造函数
osthread::osthread(std::string &name)
{
	m_name = name;
}

//! 构造函数
osthread::osthread(const char *name)
{
	m_name = std::string(name);
}

//! 析构函数
osthread::~osthread() {
	if(m_pThid) {
		delete m_pThid;
		m_pThid = nullptr;
	}
}

//! 创建线程
int osthread::createThread(osthread::OS_TASK_FUN fun, void *param, osthread::eThreadType type)
{
	return createThread(fun, param, type, -1);
}

//! 创建线程
int osthread::createThread(osthread::OS_TASK_FUN fun, void *param, osthread::eThreadType type, const int pri)
{
	pthread_attr_t attr;
	int ret = -1;

	if(!fun) {
		fprintf(stderr, "[%s] %s failed, no function input.\n", m_name.c_str(), __FUNCTION__);
		return -1;
	}

	m_mutex.lock();

	if(m_bRunning) {
		fprintf(stderr, "[%s] %s failed, thread is already running.\n", m_name.c_str(), __FUNCTION__);
		m_mutex.unlock();
		return -1;
	}

	ret = pthread_attr_init(&attr);
	if(ret != 0) {
		fprintf(stderr, "[%s] %s failed, pthread_attr_init error.\n", m_name.c_str(), __FUNCTION__);
		m_mutex.unlock();
		return -1;
	}

	do {

		int pthreadType = (type == eThreadType_Detach ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);

		struct sched_param schedParam;
		if(pri != -1) {
			pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
			schedParam.sched_priority = pri;
			pthread_attr_setschedparam(&attr, &schedParam);
		}

		ret = pthread_attr_setdetachstate(&attr, pthreadType);
		if(ret != 0) {
			fprintf(stderr, "[%s] %s failed, pthread_attr_setdetachstate error.\n", m_name.c_str(), __FUNCTION__);
			break;
		}

		m_task = fun;
		m_args = param;
		if(m_pThid != NULL){
			delete(m_pThid);
			m_pThid = NULL;
		}
		m_pThid = new pthread_t();

		ret = pthread_create(m_pThid, &attr, funtask, this);
		if(ret != 0) {
			fprintf(stderr, "[%s] %s failed, pthread_create error, %s.\n", m_name.c_str(), __FUNCTION__, strerror(errno));
			delete m_pThid;
			m_pThid = nullptr;
			break;
		}

		ret = 0;

	} while(false);

	pthread_attr_destroy(&attr);

	m_mutex.unlock();

	return ret;
}

//! 等待线程退出
int osthread::waitThread()
{

	m_mutex.lock();
	do {

		if(m_bExited){
//			fprintf(stderr, "[%s] %s failed, thread was not running.\n", m_name.c_str(), __FUNCTION__);
//			break;
		}

		if(!m_pThid) {
			fprintf(stderr, "[%s] %s failed, task is not exist.\n", m_name.c_str(), __FUNCTION__);
			break;
		}
		//! 有可能在线程内部改变了线程性质
//	if(m_eThreadType != eThreadType_Joinable)
//		fprintf(stderr, "[%s] %s failed, thread was not joinable.\n", m_name.c_str(), __FUNCTION__);

//		fprintf(stderr, "thid = %p\n", m_pThid);
		void *tret = nullptr;
		int ret = pthread_join(*m_pThid, &tret);
		if(ret != 0)
			fprintf(stderr, "[%s] %s failed, pthread_join error, %s\n", m_name.c_str(), __FUNCTION__, strerror(ret));

		delete m_pThid;
		m_pThid = nullptr;

	} while(false);

	m_mutex.unlock();

	return 0;
}

//! 关闭线程
int osthread::closeThread()
{
	return 0;
	int ret = -1;
	m_mutex.lock();

	do {

		if(!m_pThid) {
//			fprintf(stderr, "[%s] %s failed, task is invalid.\n", m_name.c_str(), __FUNCTION__);
			break;
		}

		ret = pthread_cancel(*m_pThid);
		if (ret == 3) {
			fprintf(stderr, "[%s] %s failed, pthread_cancel error, ret = 3\n", m_name.c_str(), __FUNCTION__);
			break;
		}
		else if (ret != 0 && ESRCH != errno) {
			fprintf(stderr, "[%s] %s failed, pthread_cancel error, ret = %d\n", m_name.c_str(), __FUNCTION__, ret);
			break;
		}

		delete m_pThid;
		m_pThid = nullptr;

		ret = 0;

	} while (false);

	m_mutex.unlock();

	return ret;
}

void *osthread::funtask(void *args)
{
	if(!args)
		return nullptr;

	osthread &thread = *(osthread*)args;

	thread.m_bExited = false;
	thread.m_bRunning = true;
	thread.m_nTID = syscall(SYS_gettid);

	//! 设置线程名称
	prctl(PR_SET_NAME, thread.m_name.c_str());

	void *retvalue = nullptr;

	//! 执行用户线程主体
	if(thread.m_task)
		retvalue = thread.m_task(thread.m_args);

	thread.m_bRunning = false;
	thread.m_bExited = true;

	return retvalue;
}

bool osthread::isRunning()
{
	return m_bRunning;
}


