//
// Created by Night on 2020/8/26.
//

#pragma once
#include <cstdint>
#include <osthread.h>
#include <StreamChan.h>
#include <QosTerminalProcessInterface.h>
#include "Qosdef.h"
class QosFrameReceiver
{
#define CONNECT_SERVER_TIMEOUT_NUM 10

  typedef int32_t (*OnRecvFrameListener)(QosFrameReceiver &sudp, MsgHead &frame, uint8_t *data);
  typedef int32_t(*OnRecvFrameFailedListener)(QosFrameReceiver &sudp, int32_t ret);
  typedef void(*OnConnectTimeOutListener)(QosFrameReceiver &sudp);
  typedef int32_t(*OnRecvErrorListener)(QosFrameReceiver &sudp);

public:

	QosFrameReceiver();

	~QosFrameReceiver();

	int init(int index);

  	//#设置本地地址
  	int setLocalAddr(int8_t *ip, int32_t port,int type);

  	//#设置发送端地址
	int setRemoteAddr(int8_t *ip, int32_t port);

  	//#设置视频数据接收回调
	void setRecvVideoListener(OnRecvFrameListener listener);

	//#设置音频数据接收回调
	void setRecvAudioListener(OnRecvFrameListener listener);

  	//#设置数据超时回调
	void setConnectTimeOutListener(OnConnectTimeOutListener listener);

  	//#设置数据错误回调
	void setOnRecvErrorListener(OnRecvErrorListener listener);

	//#发送请求数据消息
	int requestConnect();

  	//#开始
	int start();

  	//#停止
	int stop();

  	//#设置用户私有数据
	void setUserData(void *data);

  	//#获取用户私有数据
	void *getUserData();

  	//# 获取连接状态
  	bool getConnectStatus(){ return m_bConnect;}

  	//使用实例
  	static void thisTest();

private:

	//#创建数据监听socket
	int creatMsgSocket(const char *ip, int port);

	//视频数据回调
  	static void RecvVideoDataListener(void *thiz, unsigned char *data, unsigned int nLen, unsigned int unPTS, int dataType);

  	//音频数据回调
  	static void RecvAudioDataListener(void *thiz, unsigned char *data, unsigned int nLen, unsigned int unPTS, int dataType);

	//#发送通信消息
	int sendCommMsg(int type);

  	//# 发送心跳用Qos 接口内部需要收数据时发个空包
	int sendQosHeatBeat();

	//# 消息监听线程
	static void *listenMsgThread(void *args);

  	//#心跳发送线程
  	static void *sendHeartBeatThread(void *args);

private:
 	 //#标识
  	int m_index;

 	 //#初始化标记
	bool m_bInit = false;

	//#开始标志
	bool m_bStart = false;

	//#连接标志
	bool m_bConnect = false;

	//# 超时时间
	int m_timeOutNum = CONNECT_SERVER_TIMEOUT_NUM;

	//# 监听IP
	std::string m_ip;

	//# 监听端口
	int32_t m_port;

	//# 远端IP
	std::string m_remoteIp;

  	//# 消息通信句柄
  	int m_msgFd = -1;

	//# 远端端口
	int m_remotePort = 0;

  	//#视频接收端口
	int m_videoInPort = -1;

  	//#音频接收端口
  	int m_audioInPort = -1;

  	//远端视频数据发送端口
	int m_remoteVOPort = -1;

	//远端视频数据发送端口
	int m_remoteAOPort = -1;

  	//# 发送端设备类型
	int m_deviceType = 0;

	//# 码流类型
	int m_streamType = 0;

	//# 发送端版本号
	std::string m_version;

	std::mutex  m_mutex;

	//#	冗余模式 1：固定 0：自动
	int m_method = 1;

	int m_videoSendFps = 30;

	//# 视频冗余值
	int m_videoRedundanceValue = 30;

	//# 音频冗余值
	int m_audioRedundanceValue = 50;

	//#视频FEC分组数量
	int m_videoFecGropSize = 26;

	//#音频FEC分组数量
	int m_audioFecGropSize = 1;


  	//#码流接收器
  	CQosTerminalProcessInterface *m_videoReceiver;

	//#码流接收器
	CQosTerminalProcessInterface *m_audioReceiver;


	//#消息监听线程句柄
	osthread *m_listenTask = new osthread("listenTask");

	//#心跳发送线程句柄
	osthread *m_heartBeatTask = new osthread("heartBeatTask");

  	OnRecvFrameListener 		m_recvVideoListener = nullptr;

  	OnRecvFrameListener 		m_recvAudioListener = nullptr;

  	OnConnectTimeOutListener 	m_connectTimeOutListener = nullptr;

  	OnRecvErrorListener m_recvErrorListener = nullptr;

	//# 转发数据载体
	uint8_t * m_mediaDate;
  git clone https://github.com/rockchip-linux/repogit clone https://github.com/rockchip-linux/repogit clone https://github.com/rockchip-linux/repogit clone https://github.com/rockchip-linux/repo
  	//#用户私有数据
  	void *m_userData = nullptr;

};


