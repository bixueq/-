//
// Created by Night on 2020/8/26.
//

#include "QosFrameReceiver.h"

QosFrameReceiver::QosFrameReceiver()
{

}

QosFrameReceiver::~QosFrameReceiver()
{

}

int QosFrameReceiver::init(int index)
{
	m_mutex.lock();
	do {
		if(m_bInit)
			break;
		m_index = index;
		m_mediaDate = new uint8_t[1920*1080];
		if(m_streamType == StremType_CloseShot){
			m_videoInPort = RECEIVER_CLOSE_SHOT_VIDEO_IN_PORT + (100 * index);
			m_audioInPort = RECEIVER_CLOSE_SHOT_AUDIO_IN_PORT + (100 * index);
			m_remoteVOPort = SENDER_CLOSE_SHOT_VIDEO_OUT_PORT ;
			m_remoteAOPort = SENDER_CLOSE_SHOT_AUDIO_OUT_PORT;
		}else if(m_streamType == StremType_LongShot){
			m_videoInPort = RECEIVER_LONG_SHOT_VIDEO_IN_PORT + (100 * index);
			m_audioInPort = RECEIVER_LONG_SHOT_AUDIO_IN_PORT + (100 * index);
			m_remoteVOPort = SENDER_LONG_SHOT_VIDEO_OUT_PORT ;
			m_remoteAOPort = SENDER_LONG_SHOT_AUDIO_OUT_PORT;
		}
		printf( "视频接收器初始化：local ip:%s port:%d\r\n", m_ip.c_str(), m_videoInPort);
		printf("视频发送器初始化：remote ip:%s port:%d\r\n", m_remoteIp.c_str(), m_remoteVOPort);
		printf( "this :%p\r\n", this);

		m_videoReceiver = new CQosTerminalProcessInterface();
		m_videoReceiver
			->SetupProcessModule(RecvVideoDataListener, this, false, m_method, m_videoRedundanceValue, m_videoFecGropSize, 0);
		m_videoReceiver->SetupLocalNetworkModule(m_ip.c_str(), m_videoInPort);
		m_videoReceiver->SetupRemoteNeworkModule(m_remoteIp.c_str(), m_remoteVOPort);

		printf( "音频接收器初始化：local ip:%s port:%d\r\n", m_ip.c_str(), m_audioInPort);
		printf("音频发送器初始化：remote ip:%s port:%d\r\n", m_remoteIp.c_str(), m_remoteAOPort);

		m_audioReceiver = new CQosTerminalProcessInterface();
		m_audioReceiver
			->SetupProcessModule(RecvAudioDataListener, this, true, m_method, m_audioRedundanceValue, m_audioFecGropSize, 0);
		m_audioReceiver->SetupLocalNetworkModule(m_ip.c_str(), m_audioInPort);
		m_audioReceiver->SetupRemoteNeworkModule(m_remoteIp.c_str(), m_remoteAOPort);





		m_bInit = true;

	} while (false);
	m_mutex.unlock();
	return 0;
}


//#设置本地地址
int QosFrameReceiver::setLocalAddr(int8_t *ip, int32_t port, int type)
{
	m_mutex.lock();
	do {
		if(m_bConnect)
			break;

		if(ip == nullptr || port <0 )
			break;
		m_ip = std::string((const char *)ip);
		m_port = port;
		m_streamType = type;
	} while (false);
	m_mutex.unlock();
	return 0;
}

//#设置发送端地址
int QosFrameReceiver::setRemoteAddr(int8_t *ip, int32_t port)
{
	m_mutex.lock();
	do {
		if(m_bConnect)
			break;

		if(ip == nullptr || port <0 )
			break;
		m_remoteIp = std::string((const char *)ip);
		m_remotePort = port;

	} while (false);
	m_mutex.unlock();
	return 0;
}

int QosFrameReceiver::creatMsgSocket(const char *ip, int port)
{
	int ret = -1;
	do {
		sockaddr_in clientAddr;
		memset(&clientAddr, 0, sizeof(sockaddr_in));
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_port = htons(port);
		if(ip == nullptr)
			clientAddr.sin_addr.s_addr = INADDR_ANY;
		else
			clientAddr.sin_addr.s_addr = inet_addr(ip);
		m_msgFd = socket(PF_INET, SOCK_STREAM, 0);
		if( -1 == m_msgFd ) {
			printf( "UDPServer create socket failed %s\n", strerror(errno));
			break;
		}

		int ret = connect(m_msgFd, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
		if (-1 == ret){
			printf( "TCPClient create bind failed %s \n", strerror(errno));
			break;
		}
		printf("TCPClient::SocketConnect ret:%d ",ret);
		return 0;

	} while (false);

	if(m_msgFd >= 0)
		close(m_msgFd);

	return -1;

}
void *QosFrameReceiver::listenMsgThread(void *args)
{
	if(args == nullptr)
		true;
	QosFrameReceiver &receiver = *(QosFrameReceiver *) args;
	printf( "消息监听线程启动\r\n");
	while(receiver.m_bStart) {
		fd_set readfd;
		struct timeval timeout;
		timeout.tv_sec = 3;
		timeout.tv_usec = 100;

		FD_ZERO(&readfd);
		FD_SET(receiver.m_msgFd, &readfd);

		int32_t ret = select(receiver.m_msgFd + 1, &readfd, nullptr, nullptr, &timeout);
		if (ret == 0) {
			usleep(2000);
			receiver.m_timeOutNum--;
			if(receiver.m_timeOutNum == 0){
				printf( "连接启动超时\r\n");
				if(receiver.m_connectTimeOutListener){
					receiver.m_connectTimeOutListener(receiver);
				}
			}
			continue;
		} else if (ret < 0) {
			usleep(2000);
			continue;
		}

		if (FD_ISSET(receiver.m_msgFd, &readfd)) {
			MsgHead recBuf;
			int len = read(receiver.m_msgFd, (void *)&recBuf, sizeof(MsgHead));
			if (!MsgHeadCheck(&recBuf) || len < sizeof(MsgHead)) {
				break;
			}

			if (recBuf.msgType == MsgType_Ready) {
				if(!receiver.m_bConnect){
					receiver.m_deviceType = recBuf.deviceType;
					receiver.m_version = std::string((char *)recBuf.version);
					receiver.m_bConnect = true;
					if(!receiver.m_heartBeatTask->isRunning())
						receiver.m_heartBeatTask->createThread(sendHeartBeatThread, &receiver, osthread::eThreadType_Joinable);
					printf( "############已连接#############\r\n");
				}
			}
		}
	}
	printf( "连接断开\r\n");
	return nullptr;
}

void *QosFrameReceiver::sendHeartBeatThread(void *args)
{
	if(args == nullptr)
		true;
	QosFrameReceiver &receiver = *(QosFrameReceiver *) args;
	printf( "启动心跳发送线程\r\n");
	while (receiver.m_bConnect){
		if(receiver.m_msgFd<0)
			break;

		if(receiver.sendCommMsg(MsgType_HeartBeat)<0){
			receiver.m_bConnect = false;
			break;
		}
		receiver.sendQosHeatBeat();
		sleep(1);
	}
	printf( "心跳发送线程退出\r\n");
	return nullptr;
}

//#开始
int QosFrameReceiver::start()
{
	m_mutex.lock();
	do {
		if(!m_bInit)
			break;
		creatMsgSocket(m_remoteIp.c_str(), m_remotePort);
		m_bStart = true;
		if(!m_listenTask->isRunning()) {
			m_listenTask->createThread(listenMsgThread, this, osthread::eThreadType_Joinable);
		}

	} while (false);
	m_mutex.unlock();
	return 0;
}

//#设置数据接收回调
void QosFrameReceiver::setRecvVideoListener(QosFrameReceiver::OnRecvFrameListener listener)
{
	m_mutex.lock();
	do {

		if (m_bInit)
			break;
		if (listener)
			m_recvVideoListener = listener;

	} while (false);
	m_mutex.unlock();

}

//#设置音频数据接收回调
void QosFrameReceiver::setRecvAudioListener(QosFrameReceiver::OnRecvFrameListener listener)
{
	m_mutex.lock();
	do {

		if (m_bInit)
			break;
		if (listener)
			m_recvAudioListener = listener;

	} while (false);
	m_mutex.unlock();
}

//#设置数据超时回调
void QosFrameReceiver::setConnectTimeOutListener(QosFrameReceiver::OnConnectTimeOutListener listener)
{
	m_mutex.lock();
	do {

		if (m_bInit)
			break;
		if (listener)
			m_connectTimeOutListener = listener;
	} while (false);
	m_mutex.unlock();
}

//#设置数据错误回调
void QosFrameReceiver::setOnRecvErrorListener(QosFrameReceiver::OnRecvErrorListener listener)
{
	m_mutex.lock();
	do {

		if (m_bInit)
			break;
		if (listener)
			m_recvErrorListener = listener;
	} while (false);
	m_mutex.unlock();
}

int QosFrameReceiver::stop()
{
	m_mutex.lock();
	do{
		if(!m_bInit|| !m_bStart)
			break;

		m_bStart = false;
		sendCommMsg(MsgType_Disconnect);
		if(m_msgFd>0){
			close(m_msgFd);
			m_msgFd = -1;
		}
		if(m_heartBeatTask->isRunning()){
			m_heartBeatTask->waitThread();
			m_heartBeatTask->closeThread();
		}

		if(m_listenTask->isRunning()){
			m_listenTask->waitThread();
			m_listenTask->closeThread();
		}
		m_videoReceiver->Clear();
		delete m_videoReceiver;
		m_videoReceiver = nullptr;
	}while (false);
	m_mutex.unlock();
	return 0;
}

//#发送请求数据消息
int QosFrameReceiver::requestConnect()
{
	int ret = -1;
	m_mutex.lock();
	do {
		if(!m_bInit ||m_msgFd < 0)
			break;

		MsgHead msgBuff;
		memset(&msgBuff, 0, sizeof(MsgHead));
		for(int i = 0; i < 4; i++){
			msgBuff.check_start[i] = '#';
			msgBuff.check_end[i] = '*';
		}
		msgBuff.msgType = MsgType_Request;
		msgBuff.videoInPort = m_videoInPort;
		msgBuff.audioInPort = m_audioInPort;
		msgBuff.streamTpye = m_streamType;
		memcpy(msgBuff.requestIp, m_ip.c_str(), m_ip.size());

		int len = write(m_msgFd, &msgBuff, sizeof(MsgHead));
		printf("发送请求连接消息 ip:%s  port:%d type:%d send:%d\r\n",m_remoteIp.c_str(),m_remotePort,msgBuff.streamTpye,len);

	} while (false);
	m_mutex.unlock();
	return ret;
}

//#发送通信消息
int QosFrameReceiver::sendCommMsg(int type)
{
	int ret = -1;
	do {
		if (!m_bConnect || m_msgFd <= 0) {
			break;
		}
		MsgHead msgBuff;
		memset(&msgBuff, 0, sizeof(MsgHead));
		for(int i = 0; i < 4; i++){
			msgBuff.check_start[i] = '#';
			msgBuff.check_end[i] = '*';
		}
		msgBuff.msgType = type;
		ret = write(m_msgFd, &msgBuff, sizeof(MsgHead));
	} while (false);
	return ret;
}

//视频数据回调
void QosFrameReceiver::RecvVideoDataListener(void *thiz, unsigned char *data, unsigned int nLen, unsigned int unPTS, int dataType)
{
	do {
		QosFrameReceiver *p = (QosFrameReceiver *) thiz;
		if (!p)
			break;

		QosFrameReceiver &freamReceiver = *p;

		if(!freamReceiver.m_bStart||!freamReceiver.m_bConnect){
			break;
		}

		if(freamReceiver.m_bConnect){
			freamReceiver.m_mediaDate = data;
			MsgHead *pFrameHead = (MsgHead *)freamReceiver.m_mediaDate;
			if (MsgHeadCheck(pFrameHead)||pFrameHead->msgType == MsgType_Stream){
				if(freamReceiver.m_recvVideoListener)
					freamReceiver.m_recvVideoListener(freamReceiver, *pFrameHead, (freamReceiver.m_mediaDate+sizeof(MsgHead)));
			}
		}

	} while (false);

}

//音频数据回调
void QosFrameReceiver::RecvAudioDataListener(void *thiz, unsigned char *data ,unsigned int nLen, unsigned int unPTS, int dataType)
{
	do {
		QosFrameReceiver *p = (QosFrameReceiver *) thiz;
		if (!p)
			break;

		QosFrameReceiver &freamReceiver = *p;

		if(!freamReceiver.m_bStart||!freamReceiver.m_bConnect){
			break;
		}

		if(freamReceiver.m_bConnect){
			freamReceiver.m_mediaDate = data;
			MsgHead *pFrameHead = (MsgHead *)freamReceiver.m_mediaDate;
			if (MsgHeadCheck(pFrameHead)||pFrameHead->msgType == MsgType_Stream){
				if(freamReceiver.m_recvAudioListener)
					freamReceiver.m_recvAudioListener(freamReceiver, *pFrameHead, (freamReceiver.m_mediaDate+sizeof(MsgHead)));
			}
		}

	} while (false);

}

//# 发送心跳用Qos 接口内部需要收数据时发个空包
int QosFrameReceiver::sendQosHeatBeat()
{
	unsigned char buf[32] = "1";
	m_videoReceiver->SendStreamData(32, buf);
	m_audioReceiver->SendStreamData(32, buf);
	return 0;
}


//#例子
static int32_t frameListen(QosFrameReceiver &sudp, MsgHead &frame, uint8_t *data)
{
	printf( "接收到：audio:%d len :%d [%d] %s\r\n",frame.bAudio,frame.iLen,frame.iStreamIdx, data);
	return 0;
}

void QosFrameReceiver::thisTest()
{
	signal(SIGPIPE, SIG_IGN);
	printf( "启动接收器！！！！！！！！！\r\n");
	auto receiver = new QosFrameReceiver();

	receiver->setRecvVideoListener(frameListen);
	receiver->setLocalAddr((int8_t *) "192.168.5.169", 18001,1);
	receiver->setRemoteAddr((int8_t *) "192.168.5.198", 18002);
	receiver->init(0);
	receiver->start();
	//发起数据连接请求
	receiver->requestConnect();
	while (1){
		//receiver->requestConnect();
		sleep(1);
	}
}
void QosFrameReceiver::setUserData(void *data)
{
	m_mutex.lock();
	do {
		if(data == nullptr)
			break;

		m_userData = data;

	} while (false);

	m_mutex.unlock();
}


void *QosFrameReceiver::getUserData()
{
	return m_userData;
}







