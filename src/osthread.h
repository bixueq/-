//
// Created by golist on 2019/5/27.
//

#ifndef ABOX_OSTHREAD_H
#define ABOX_OSTHREAD_H

#include <string>
#include <mutex>
#include <pthread.h>


class osthread {

public:

	typedef void *(*OS_TASK_FUN)(void *args);

	enum eThreadType {
		//! 分离
		eThreadType_Detach = 0,

		//! 需要等待
		eThreadType_Joinable
	};

public:

	//! 构造函数
	osthread();
	osthread(std::string &name);
	osthread(const char *name);

	//! 析构函数
	~osthread();

	//! 创建线程
	int createThread(OS_TASK_FUN fun, void *param, enum eThreadType type = eThreadType_Detach);
	int createThread(OS_TASK_FUN fun, void *param, enum eThreadType type, const int pri);

	//! 等待线程退出
	int waitThread();

	//! 关闭线程
	int closeThread();

	//! 线程是否在运行中
	bool isRunning();

private:

	//! 任务线程
	static void *funtask(void *args);

	//! 自定义的线程标识
	std::string m_name = "osthread";

	//# 任务锁
	std::mutex m_mutex;

	//! 线程是否还处于运行标识
	bool m_bRunning = false;

	//! 线程是否已标志为已退出
	bool m_bExited = true;

	//! 线程句柄
	pthread_t *m_pThid = nullptr;

	int m_nTID = 0;

	//! 线程主体
	OS_TASK_FUN m_task = nullptr;

	//! 线程参数
	void *m_args = nullptr;

	//! 线程退出类型
	enum eThreadType m_eThreadType = eThreadType_Detach;

	//! 线程优先级
	int pri = 50;

};


#endif //ABOX_OSTHREAD_H
