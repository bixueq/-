//
// Created by Night on 2020/8/26.
//

#include <Qosdef.h>
#include "QosFrameSender.h"

QosFrameSender::QosFrameSender()
{

}

QosFrameSender::~QosFrameSender()
{

}



int QosFrameSender::setLocalAddr(const char *ip, int VOPort, int AOPort)
{
	m_mutex.lock();
	do {
		if(m_bInit||m_bStart)
			break;
		if (VOPort < 0 || AOPort<0 || ip == nullptr)
			break;

		m_ip = ip;
		m_videoOutPort = VOPort;
		m_audioOutPort = AOPort;
		fprintf(stderr, "set ip:%s port:%d\r\n", m_ip.c_str(), m_port);
	} while (false);
	m_mutex.unlock();
	return 0;
}

int QosFrameSender::setRemoteAddr(const char *remoteIp, int VIPort, int AIPort)
{
	m_mutex.lock();
	do {
		if(m_bStart ||m_bInit)
			break;
		if (remoteIp == nullptr || VIPort < 0||AIPort <0)
			break;

		m_remoteIp = remoteIp;
		m_remoteVIPort = VIPort;
		m_remoteAIPort = AIPort;
		fprintf(stderr, "setRemoteAddr:%s VOut Port:%d  AO Port:%d\r\n",
				m_remoteIp.c_str(), m_remoteVIPort,m_remoteAIPort);
	} while (false);
	m_mutex.unlock();
	return 0;
}


int QosFrameSender::init()
{
	m_mutex.lock();
	do {
		if(m_bInit)
			break;
		if (m_ip.empty() || m_remoteIp.empty())
			break;

		if (m_audioOutPort < 0 || m_videoOutPort < 0) {
			fprintf(stderr, "本地音视频发送端口错误\r\n");
			break;
		}

		if (m_remoteVIPort < 0 || m_remoteAIPort < 0) {
			fprintf(stderr, "远端音视频接收端口错误\r\n");
			break;
		}

		fprintf(stderr, "0001---init vo port:%d ao port:%d\r\n", m_videoOutPort,m_audioOutPort);


		m_pData = (uint8_t*)malloc(1920*1080);

		m_qosVideoSender = new CQosTerminalProcessInterface();
		m_qosVideoSender->SetupInputDataStyle(false, m_videoSendFps);
		m_qosVideoSender->SetupProcessModule(nullptr, this, false, m_method, m_videoRedundanceValue, 12, 0,true);
		m_qosVideoSender->SetupLocalNetworkModule(m_ip.c_str(), m_videoOutPort);
		m_qosVideoSender->SetupRemoteNeworkModule(m_remoteIp.c_str(), m_remoteVIPort);

		m_qosAudioSender = new CQosTerminalProcessInterface();
		m_qosAudioSender->SetupProcessModule(nullptr, this, true, m_method, m_audioRedundanceValue, m_audioFecGropSize, 0,true);
		m_qosAudioSender->SetupLocalNetworkModule(m_ip.c_str(), m_audioOutPort);
		m_qosAudioSender->SetupRemoteNeworkModule(m_remoteIp.c_str(), m_remoteAIPort);
		fprintf(stderr, "0002---init remote:%s: vi port:%d ai port:%d\r\n", m_remoteIp.c_str(), m_remoteVIPort, m_remoteAIPort);
		m_bInit = true;
	} while (false);
	m_mutex.unlock();
	return 0;
}


int QosFrameSender::start()
{
	m_mutex.lock();
	do {
		if(!m_bInit || m_bStart)
			break;

		m_bStart = true;
		if(!m_sendTask->isRunning())
			m_sendTask->createThread(sendMediaFrameThread, this, osthread::eThreadType_Joinable);

	} while (false);
	m_mutex.unlock();
	return 0;
}

int QosFrameSender::stop()
{
	m_mutex.lock();
	do {
		fprintf(stderr, "发送器停止\r\n");
		if(!m_bInit || !m_bRunning)
			break;
		m_bStart = false;
		m_bRunning = false;
		m_audioIndex = 0;
		m_videoIndex = 0;
		m_sendTask->waitThread();
		m_sendTask->closeThread();

		if(m_qosVideoSender != nullptr){
			m_qosVideoSender->Clear();
			delete m_qosVideoSender;
			m_qosVideoSender = nullptr;
		}

		if(m_qosAudioSender != nullptr){
			m_qosAudioSender->Clear();
			delete m_qosAudioSender;
			m_qosAudioSender = nullptr;
		}
		m_bInit = false;
	} while (false);
	m_mutex.unlock();
	return 0;
}

//#发送数据线程
void *QosFrameSender::sendMediaFrameThread(void *args)
{
	if(args == nullptr)
		true;
	QosFrameSender &FrameSender = *(QosFrameSender *) args;
	fprintf(stderr, "######启动发送线程！！！！！！！####\r\n");
	FrameSender.m_bRunning = true;
	while (FrameSender.m_bRunning){
		auto frame = FrameSender.m_mediaList.getFrame(50);
		if(frame == nullptr){
			usleep(1000);
			continue;
		}

		FrameSender.sendMediaFrame(frame);
		delete frame;

	}
	return nullptr;
}

//#放入一帧数据
int QosFrameSender::inputMediaFrame(MediaPacket *packet)
{
	int ret = -1;
	m_mutex.lock();
	do {
		if(!m_bRunning)
			break;
		if(packet == nullptr)
			break;
		if(m_mediaList.getFrameNum() > MAX_FRAME_LIST_NUM)
			break;

		auto mediaPakcet = new MediaPacket();
		mediaPakcet->bAudio = packet->bAudio;
		mediaPakcet->iStreamIdx = packet->iStreamIdx;
		mediaPakcet->pData = packet->pData;
		mediaPakcet->bAudio = packet->bAudio;
		mediaPakcet->bKeyFrame = packet->bKeyFrame;
		mediaPakcet->iLen = packet->iLen;
		mediaPakcet->pts =  packet->pts;
		mediaPakcet->dts = packet->dts;
		ret = m_mediaList.putFrame(mediaPakcet);
	} while (false);
	m_mutex.unlock();
	return ret;
}

//#发送音视频数据带数据MsgHead头
int QosFrameSender::sendMediaFrame(MediaPacket *packet)
{
	int ret = -1;
	m_mutex.lock();
	do {
		if(packet == nullptr)
			break;
		MsgHead msgHead;
		memset(&msgHead, 0, sizeof(MsgHead));
		for(int i = 0; i < 4; i++){
			msgHead.check_start[i] = '#';
			msgHead.check_end[i] = '*';
		}
		msgHead.msgType = MsgType_Stream;
		msgHead.timeTick = packet->pts;
		msgHead.iLen = packet->iLen;
		if(packet->bAudio){
			msgHead.bAudio = packet->bAudio;
			msgHead.channel = packet->channel;
			msgHead.samplerate = packet->samplerate;
			msgHead.iStreamIdx = m_audioIndex++;
			const size_t headLen = sizeof(MsgHead);
			memcpy(m_pData, &msgHead, headLen);
			memcpy(m_pData+headLen, packet->pData,  packet->iLen);
			ret = m_qosAudioSender->SendStreamData((packet->iLen+headLen), (unsigned char *) m_pData, msgHead.timeTick);
		}
		else{
			msgHead.width = packet->width;
			msgHead.height = packet->height;
			msgHead.bKeyFrame = packet->bKeyFrame;
			msgHead.iStreamIdx = m_videoIndex++;
			const size_t headLen = sizeof(MsgHead);
			memcpy(m_pData, &msgHead, headLen);
			memcpy(m_pData+headLen, packet->pData,  packet->iLen);
			ret = m_qosVideoSender->SendStreamData((packet->iLen+headLen), (unsigned char *) m_pData, msgHead.timeTick);
		}

	} while (false);
	m_mutex.unlock();
	return ret;
}


void QosFrameSender::thisTest()
{
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "启动发生器！！！！！！！！！！！\r\n");
	auto FrameSender = new QosFrameSender();

	//FrameSender->setLocalAddr("192.168.5.16", SENDER_COMM_LISTEN_PORT);
	FrameSender->init();
	FrameSender->start();
	int i = 0;
	MediaPacket pack;
	char buff[ ] = "hello this test";
	pack.iLen = strlen(buff);
	pack.pData = (uint8_t *)buff;

	while (1){
		pack.iStreamIdx = i++;
		FrameSender->inputMediaFrame(&pack);
		sleep(1);
	}
}





