//
// Created by Night on 2020/9/3.
//
#pragma once

#include <cstdint>
#include <mutex>
#include "osthread.h"
#include "QosConneter.h"
class QosServer
{
#define MAX_QOS_RECEIVER_CONNECT_NUM 2
  //#暂时只支持 一路近景 一路远景
public:

	QosServer();

	~QosServer();

	//#设置服务端信息
	int setSeverInfo(ServerInfo info);

  	//#启动服务
	int32_t startServer();

  	//#停止服务
	int32_t stopServer();

	//#放入数据
	int inputMediaFrame(int index, MediaPacket *packet);

private:

	//# 消息监听线程
	static void *listenMsgThread(void *args);

  	int32_t createSocket(const char *ip, int32_t port);

private:

	std::mutex  m_mutex;

  	//#服务信息
  	ServerInfo m_serverInfo;

	//# 监听IP
	std::string m_severIp;

	//# 监听端口
	int32_t m_severPort = 0;

  	bool m_bIsRebind;

  	std::string m_castIp;

	bool m_bInit = false;

	bool m_bStart = false;

	//socket listen句柄
  	int m_commFd = -1;

  	//#码流类型
  	int m_streamType[MAX_QOS_RECEIVER_CONNECT_NUM];

  	osthread *m_listenTask = new osthread("listenTask");

  	//#连接器
  	QosConneter m_conneter[MAX_QOS_RECEIVER_CONNECT_NUM];

};


