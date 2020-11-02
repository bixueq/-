//
// Created by Night on 2020/9/3.
//
#pragma once

#include <cstdint>
#include "CSingletonStatic.h"
#include "osthread.h"
#include "Qosdef.h"
#include "QosFrameSender.h"
#include "QosServer.h"

class LiveManager: public CSingletonStatic<LiveManager>
{
	friend CSingletonStatic<LiveManager>;


private:

	LiveManager();

	~LiveManager();

public:

  	//#设置服务端信息
  	int setSeverInfo(ServerInfo info);

	//# 初始化
	int init() ;

	//# 启动
	int start();

	//# 停止
	int stop();

	//#放入数据
	int inputMediaFrame(int index, MediaPacket *packet);

	static void thisTest();

	void Sender_Data(int Ch, int index, int len, unsigned char *data);

	void Init_Sender();

private:

	//#初始化标记
	bool m_bInit = false;

	//#开始标准
	bool m_bStart = false;

	//#发送数据中
	bool m_bRunning = false;

	//# 监听IP
	std::string m_ip;

	//# 监听端口
	int32_t m_port = 0;

	//# 远端IP
	std::string m_remoteIp;

	//#超时次数
	int32_t m_timeOutNum = 0;

	//# 远端端口
	int m_remotePort = 0;

	//# 设备类型
	int m_deviceType = 0;

  	//#TCPSever监听器
	QosServer  m_qosServer;

  	ServerInfo m_serverInfo;
	//# 版本号
	std::string m_version;

	std::mutex  m_mutex;


};

