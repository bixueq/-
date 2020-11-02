//
// Created by Night on 2020/9/4.
//


#pragma once
#include <cstdint>
#include "QosFrameSender.h"
#include <mutex>
class QosConneter
{
#define CONNECT_TIMEOUT_NUM  3
public:

  	QosConneter();

  	//#设置连接器类型
	QosConneter(int type);

	~QosConneter();

	int setServerInfo(ServerInfo info);

	int init(int commFd);

	int start();

	int stop();

	//#放入数据
	int inputMediaFrame(int index, MediaPacket *packet);

  	bool isRunnig(){return m_bRunnig;}

  	int setStreamType(int type);

  	int getStreamType(){return m_type;}

private:

  	//#通信连接线程
	static void *connectCommTask(void *args);

  	//#发送就绪消息
	int sendReadyMsg();

private:

  	//类型 区别近景 远景连接发送什么码流
  	int m_type  = -1;

  	//#版本号
  	std::string m_version;

  	//#设备类型
  	int m_deviceType;

  	//#超时
  	int m_timeOutNum = CONNECT_TIMEOUT_NUM;

  	//#初始化标志
  	bool m_bInit = false;

  	//#开始标志
  	bool m_bStart = false;

  	//#数据发送开始标志
  	bool m_bRunnig = false;

  	//#服务端IP 即本地IP
  	std::string m_serverIp;

  	int m_serverPort = -1;

  	std::string m_clientIp;

  	int m_clientPort;

  	//#視頻数据发送端口
  	int m_videoOutPort = -1;

	//#音頻数据发送端口
	int m_audioOutPort = -1;

	int m_connFd = -1;

  	std::mutex m_mutex;

  	//#通信sock线程句柄
	osthread m_commTask;

	//#码流发送器
	QosFrameSender m_frameSender;



};

