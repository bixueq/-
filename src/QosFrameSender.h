//
// Created by Night on 2020/8/26.
//

#pragma once
#include <string>
#include <mutex>
#include <osthread.h>
#include <QosTerminalProcessInterface.h>
#include "Qosdef.h"
#include "StreamChan.h"

class QosFrameSender
{
#define MAX_FRAME_LIST_NUM 20


public:

	QosFrameSender();

	~QosFrameSender();

  	//# 设置本地地址
  	int setLocalAddr(const char *ip, int VOPort, int AOPort);

  	//# 设置远端地址以及音视频数据接收端口
  	int setRemoteAddr(const char *remoteIp, int VIPort, int AIPort);

 	//# 初始化
  	int init();

  	//# 启动
	int start();

	//# 停止
	int stop();

  	//# 放入一帧数据 带发送缓存
  	int inputMediaFrame(MediaPacket *packet);

  	bool isRunning(){return m_bRunning;}


  	static void thisTest();

private:


  	//# 发送数据线程
  	static void *sendMediaFrameThread(void *args);


  	//# 发送音视频数据
  	int sendMediaFrame(MediaPacket *packet);


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
  	int32_t m_port = -1;

  	//# 远端IP
	std::string m_remoteIp;

	int64_t m_videoIndex = 0;

	int64_t m_audioIndex = 0;


	//# 远端端口
  	int m_remotePort = -1;

  	//# 视频发送端口
  	int m_videoOutPort = -1;

	//# 音频发送端口
	int m_audioOutPort = -1;

	//# 远端视频接收端口
	int m_remoteVIPort = -1;

	//# 远端音频接收端口
	int m_remoteAIPort = -1;


  	//#	冗余模式 1：固定 0：自动
  	int m_method = 1;

  	int m_videoSendFps = 30;

  	//# 视频冗余值
  	int m_videoRedundanceValue = 30;

  	//# 音频冗余值
  	int m_audioRedundanceValue = 50;

  	//#视频FEC分组数量
  	int m_videoFecGropSize = 18;

  	//#音频FEC分组数量
  	int m_audioFecGropSize = 4;


  	std::mutex  m_mutex;

  	//# 发送线程句柄
  	osthread *m_sendTask = new osthread("sendTask");

  	CQosTerminalProcessInterface *m_qosVideoSender;

  	CQosTerminalProcessInterface *m_qosAudioSender;

  	//# 数据缓存队列
  	StreamChan<MediaPacket> m_mediaList;

  	uint8_t *m_pData = nullptr;

};

