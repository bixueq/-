#pragma once

#include <queue>
#include "util.h"


	template<class T>
	class StreamChan
	{

//#define SC_lock()		std::unique_lock<std::mutex> lk(m_mutex)

	public:
		StreamChan();
		~StreamChan();

		/**
		 * \brief 插入一个元素到流通道中
		 * \param frame
		 * \return
		 */
		bool putFrame(T *frame);

		/**
		 * \brief 从流通道中获取一个元素
		 * \return 如果有元素，则直接返回元素指针，如果无元素或元素数量未超过缓冲数量，则返回空
		 */
		T* getFrame();

		/**
		* \brief 从流通道中获取一个元素，可指定超时时间
		* \param timeout_millisecond 超时时间，单位为毫秒
		* \return 如果有元素，则直接返回元素指针，如果超时了，则返回空
		*/
		T* getFrame(int32_t timeout_millisecond);

		/**
		 * \brief 设置缓冲的数量，当数量未达到缓冲值时，将取不到数据
		 * \param num
		 * \return
		 */
		int32_t setSettingBufferingNum(uint32_t num);

		/**
		 * \brief 获取所缓冲的数量值
		 * \return
		 */
		uint32_t getSettingBufferingNum();

		uint32_t getFrameNum();


		/**
		* \brief 设置此通道ID值
		* \param id
		*/
		void setChId(uint32_t id);

		/**
		 * \brief 获取此通道ID值
		 * \return
		 */
		uint32_t getChId();

		/**
		 * \brief 设置队列ID值
		 * \param id
		 */
		void setQueId(uint32_t id);

		/**
		 * \brief 获取队列ID值
		 * \return
		 */
		uint32_t getQueId();

		/**
		 * \brief 获取此通道元素数量
		 * \return
		 */
		int32_t getSize();

	private:
		std::queue<T*> m_list;			// 一个 StreamChan 包含多个 RawVideoFrame		
//		std::mutex m_mutex;

		pthread_mutex_t m_tMutex;
		pthread_cond_t m_tCon;

		/**
		 * \brief 标识此通道所在队列中的通道ID值，在队列中应该是唯一的
		 */
		uint32_t m_chid;

		/**
		 * \brief 标识此通道所在队列的队列ID值，同一个link的不同输入或不同输出队列，应该有不同的队列ID值
		 */
		uint32_t m_queid;

		/**
		 * \brief 缓冲数量，默认不缓冲，即为0
		 */
//		std::atomic<uint32_t> m_bufferingNum;
		uint32_t m_bufferingNum;

		/**
		 * \brief 元素数量，在这里不使用 queue.size()，因不知道其性能怎么样
		 */
//		std::atomic<uint32_t> m_frameNum;
		uint32_t m_frameNum;
	};

	template <class T>
	StreamChan<T>::StreamChan()
	{
		m_chid = 0;
		m_queid = 0;
		m_bufferingNum = 0;
		m_frameNum = 0;
		pthread_mutex_init(&m_tMutex, NULL);
		pthread_cond_init(&m_tCon, NULL);
	}

	template<class T>
	StreamChan<T>::~StreamChan()
	{
		pthread_mutex_destroy(&m_tMutex) || pthread_cond_destroy(&m_tCon);
	}

	template<class T>
	bool StreamChan<T>::putFrame(T* frame)
	{
		pthread_mutex_lock(&m_tMutex);

		m_list.push(frame);
		m_frameNum += 1;

//		pthread_cond_broadcast(&m_tCon);
		pthread_cond_signal(&m_tCon);
		pthread_mutex_unlock(&m_tMutex);
		
		return true;
	}

	template<class T>
	T* StreamChan<T>::getFrame()
	{
		pthread_mutex_lock(&m_tMutex);

		T* frame = NULL;

		do
		{
			if (m_frameNum < m_bufferingNum)
				break;

			if (m_list.empty())
				break;

			frame = m_list.front();
			m_list.pop();
			m_frameNum -= 1;

		} while (false);

		pthread_mutex_unlock(&m_tMutex);

		return frame;
	}

	template<class T>
	T* StreamChan<T>::getFrame(int32_t timeout_millisecond)
	{
		struct timeval tNow;
		struct timespec tAbsTime;

		pthread_mutex_lock(&m_tMutex);

		if (-1 != timeout_millisecond) {
			gettimeofday(&tNow, NULL);

			int sec_time = timeout_millisecond / 1000;
			long int nsec_time = (timeout_millisecond % 1000) * 1000000;
			nsec_time += tNow.tv_usec * 1000;

			if (nsec_time >= 1000000000) {
				sec_time += 1;
				nsec_time -= 1000000000;
			}

			tAbsTime.tv_sec = tNow.tv_sec + sec_time;
			tAbsTime.tv_nsec = nsec_time;
		}

//		cerr << "m_frameNum " << m_frameNum << " m_bufferingNum " << m_bufferingNum << endl;
		while (m_frameNum == 0 || m_frameNum < m_bufferingNum) {
			if (-1 == timeout_millisecond) {
				pthread_cond_wait(&m_tCon, &m_tMutex);
			}
			else {
				if (ETIMEDOUT == pthread_cond_timedwait(&m_tCon, &m_tMutex, &tAbsTime))
					break;
			}
		}

		T* frame = NULL;

		do
		{
			if (m_frameNum == 0 || m_frameNum < m_bufferingNum)
				break;

			if (m_list.empty())
				break;

			frame = m_list.front();
			m_list.pop();

			m_frameNum -= 1;

		} while (false);

		pthread_mutex_unlock(&m_tMutex);

		return frame;
	}

	template <class T>
	int32_t StreamChan<T>::setSettingBufferingNum(uint32_t num)
	{
		m_bufferingNum = num;
		return 0;
	}

	template <class T>
	uint32_t StreamChan<T>::getSettingBufferingNum()
	{
		return m_bufferingNum;
	}

	template<class T>
	uint32_t StreamChan<T>::getFrameNum()
	{
		return m_frameNum;
	}

	template <class T>
	void StreamChan<T>::setChId(uint32_t id)
	{
		m_chid = id;
	}

	template <class T>
	uint32_t StreamChan<T>::getChId()
	{
		return m_chid;
	}

	template <class T>
	void StreamChan<T>::setQueId(uint32_t id)
	{
		m_queid = id;
	}

	template <class T>
	uint32_t StreamChan<T>::getQueId()
	{
		return m_queid;
	}

	template<class T>
	int32_t StreamChan<T>::getSize()
	{
		int32_t size = 0;

		pthread_mutex_lock(&m_tMutex);
		size = m_list.size();
		pthread_mutex_unlock(&m_tMutex);

		return size;
	}

