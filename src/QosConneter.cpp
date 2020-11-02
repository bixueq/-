//
// Created by Night on 2020/9/4.
//

#include "QosConneter.h"


QosConneter::QosConneter()
{

}


QosConneter::~QosConneter()
{

}

QosConneter::QosConneter(int type)
{
	m_type = type;
}

int QosConneter::setServerInfo(ServerInfo info)
{

	m_mutex.lock();
	do {
		fprintf(stderr, "QosConneter  :localIP:%s  prot:%d\r\n", info.localIp.c_str(), info.listenPort);
		if (m_bInit)
			break;
		if(info.localIp.empty())
			break;
		m_serverPort = info.listenPort;
		m_deviceType = info.deviceType;
		m_serverIp = info.localIp;
		if(info.version.empty())
			m_version= info.version;

	} while (false);
	m_mutex.unlock();
	return 0;
}

int QosConneter::init(int commFd)
{
	m_mutex.lock();
	do {
		if(m_bInit)
			break;
		fprintf(stderr,"连接器初始化成功\r\n");
		m_connFd = commFd;
		m_bInit = true;
	} while (false);
	m_mutex.unlock();

	return 0;
}


int QosConneter::start()
{
	m_mutex.lock();
	do {
		if(!m_bInit)
			break;
		m_bStart = true;
		m_commTask.createThread(connectCommTask, this, osthread::eThreadType_Joinable);
	} while (false);
	m_mutex.unlock();
	return 0;
}

int QosConneter::stop()
{
	m_mutex.lock();
	do {
		if(!m_bInit || m_bStart)
			break;
		m_bInit = false;
		m_bStart = false;
		if (m_connFd > 0)
			close(m_connFd);
		m_bRunnig = false;
		if(m_commTask.isRunning()){
			m_commTask.waitThread();
			m_commTask.closeThread();
		}
		m_frameSender.stop();
		m_type = -1;
	} while (false);
	m_mutex.unlock();

	return 0;
}


void *QosConneter::connectCommTask(void *args) {
	if (args == nullptr)
		true;
	QosConneter &conneter = *(QosConneter *) args;
	fprintf(stderr, "启动连接通信线程\r\n");
	do {
		MsgHead recvBuf;
		memset(&recvBuf, 0, sizeof(MsgHead));
		int len = read(conneter.m_connFd, &recvBuf, sizeof(MsgHead));
		if (!MsgHeadCheck(&recvBuf))
			break;

		if (recvBuf.msgType == MsgType_Request) {
			fprintf(stderr, "连接器类型：%d 请求码流类型：%d\r\n", conneter.m_type, recvBuf.streamTpye);
			if (conneter.m_type < 0) {
				conneter.m_type = recvBuf.streamTpye;
			}else if(conneter.m_type != recvBuf.streamTpye){
				break;
			}
			fprintf(stderr, "请求ip:%s VI Port:%d  AI Port:%d\r\n", recvBuf.requestIp, recvBuf.videoInPort, recvBuf.audioInPort);
			if (recvBuf.videoInPort <= 0 || recvBuf.requestIp == nullptr || recvBuf.audioInPort <= 0)
				break;


			if(conneter.m_type == StremType_CloseShot){
				conneter.m_videoOutPort = SENDER_CLOSE_SHOT_VIDEO_OUT_PORT;
				conneter.m_audioOutPort = SENDER_CLOSE_SHOT_AUDIO_OUT_PORT;
			}
			else if(conneter.m_type == StremType_LongShot){
				conneter.m_videoOutPort = SENDER_LONG_SHOT_VIDEO_OUT_PORT;
				conneter.m_audioOutPort = SENDER_LONG_SHOT_AUDIO_OUT_PORT;
			}

			conneter.m_frameSender.setLocalAddr(conneter.m_serverIp.c_str(), conneter.m_videoOutPort,conneter.m_audioOutPort);
			conneter.m_frameSender.setRemoteAddr(recvBuf.requestIp, recvBuf.videoInPort,recvBuf.audioInPort);
			conneter.m_frameSender.init();
			conneter.m_frameSender.start();
			conneter.sendReadyMsg();
			conneter.m_bRunnig = true;
		}
		//开始心跳监听
		while (conneter.m_bRunnig) {
			MsgHead recvBuf;
			memset(&recvBuf, 0, sizeof(MsgHead));
			int len = read(conneter.m_connFd, &recvBuf, sizeof(MsgHead));
			if (len > 0) {
				if (!MsgHeadCheck(&recvBuf))
					continue;
				if (recvBuf.msgType == MsgType_HeartBeat) {
					//fprintf(stderr, "#######接收到心跳消息#######\r\n");
					conneter.m_timeOutNum = CONNECT_TIMEOUT_NUM;
					continue;
				}
				else if(recvBuf.msgType == MsgType_Disconnect){
					break;
				}
			} else {
				conneter.m_timeOutNum--;
				usleep(100000);
			}

			if (conneter.m_timeOutNum < 0) {
				break;
			}
		}

	} while (false);
	fprintf(stderr, "##########连接断开###########\r\n");
	conneter.m_bInit = false;
	conneter.m_bStart = false;
	conneter.m_type = -1;
	if (conneter.m_connFd > 0)
		close(conneter.m_connFd);

	conneter.m_bRunnig = false;
	conneter.m_frameSender.stop();

}



int QosConneter::inputMediaFrame(int index, MediaPacket *packet)
{
	m_mutex.lock();
	do {
		if(!m_bRunnig)
			break;
		if(index != m_type)
			break;
		m_frameSender.inputMediaFrame(packet);
	} while (false);
	m_mutex.unlock();
	return 0;
}


int QosConneter::sendReadyMsg()
{
	int ret = -1;
	m_mutex.lock();
	do {
		if(m_connFd < 0)
			break;
		MsgHead msg;
		memset(&msg, 0, sizeof(MsgHead));
		for(int i = 0; i < 4; i++){
			msg.check_start[i] = '#';
			msg.check_end[i] = '*';
		}
		msg.deviceType = m_deviceType;
		memcpy(msg.version, m_version.c_str(), m_version.size());
		msg.msgType = MsgType_Ready;
		ret = write(m_connFd,(const uint8_t *)&msg, sizeof(msg));
		fprintf(stderr, "######发送就绪消息#######\r\n");
	} while (false);
	m_mutex.unlock();

	return ret;
}
int QosConneter::setStreamType(int type)
{
	m_mutex.lock();
	do {
		if(m_bInit)
			break;
		m_type = type;
		fprintf(stderr, "设置连接器类型：%d\r\n", m_type);
	} while (false);
	m_mutex.unlock();
	return 0;
}


