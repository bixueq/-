//
// Created by Night on 2020/9/3.
//

#include "QosServer.h"
#include "Qosdef.h"
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>


QosServer::QosServer()
{

}

QosServer::~QosServer()
{

}

int QosServer::setSeverInfo(ServerInfo info)
{
	m_mutex.lock();
	do {
		fprintf(stderr, "0001---localIP:%s  prot:%d\r\n", info.localIp.c_str(), info.listenPort);
		if(m_bInit)
			break;
		if(info.localIp.empty())
			break;
		fprintf(stderr, "localIP:%s  prot:%d\r\n", info.localIp.c_str(), info.listenPort);
		m_serverInfo.deviceType = info.deviceType;
		m_serverInfo.listenPort = info.listenPort;
		m_serverInfo.localIp = info.localIp;
		if(!info.version.empty())
			m_serverInfo.version = info.version;
		for(int i=0;i<2;i++){
			m_conneter[i].setServerInfo(m_serverInfo);
		}
		createSocket(nullptr,m_serverInfo.listenPort);
		m_bInit = true;
	} while (false);
	m_mutex.unlock();
	return 0;
}

int32_t QosServer::createSocket(const char  *ip, int32_t port)
{
	int ret = -1;
	do {
		if(port < 0)
			break;
		sockaddr_in clientAddr;
		memset(&clientAddr, 0, sizeof(sockaddr_in));
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_port = htons(port);
		if(ip == nullptr)
			clientAddr.sin_addr.s_addr = INADDR_ANY;
		else
			clientAddr.sin_addr.s_addr = inet_addr(ip);
		m_commFd = socket(PF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
		if( -1 == m_commFd ) {
			fprintf(stderr, "UDPServer create socket failed %s\n", strerror(errno));
			break;
		}

		ret = bind(m_commFd, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
		if (-1 == ret){
			fprintf(stderr, "UDPServer create bind failed %s \n", strerror(errno));
			ret = -2;
			break;
		}
		ret = listen(m_commFd, 1);
		if (ret< 0) {
			fprintf(stderr, "listen is error\r\n");
			ret = -3;
			break;
		}

	}while (false);

	return ret;
}



int32_t QosServer::startServer()
{
	m_mutex.lock();
	do {
		if(!m_bInit || m_bStart)
			break;
		fprintf(stderr, "##########开始监听端口：%d#########\r\n", m_serverInfo.listenPort);
		m_bStart = true;
		if(!m_listenTask->isRunning())
			m_listenTask->createThread(listenMsgThread, this, osthread::eThreadType_Joinable);
	} while (false);
	m_mutex.unlock();
	return 0;
}

int32_t QosServer::stopServer()
{
	m_mutex.lock();
	do {
		if(!m_bStart)
			break;
		m_bStart = false;
		if(m_listenTask->isRunning()){
			m_listenTask->waitThread();
			m_listenTask->closeThread();
		}
		if (m_commFd > 0) {
			close(m_commFd);
			m_commFd = -1;
		}
	} while (false);
	m_mutex.unlock();
	return 0;
}



void *QosServer::listenMsgThread(void *args)
{
	if(args == nullptr)
		true;
	QosServer &Server = *(QosServer *) args;
	fprintf(stderr, "消息监听线程启动\r\n");
	while(Server.m_bStart){
		fd_set readfd;
		struct timeval timeout;
		timeout.tv_sec = 3;
		timeout.tv_usec = 100;

		FD_ZERO(&readfd);
		FD_SET(Server.m_commFd, &readfd);

		int32_t ret = select(Server.m_commFd + 1, &readfd, nullptr, nullptr, &timeout);
		if(ret == 0) {
			usleep(2000);
			continue;
		}
		else if(ret < 0) {
			fprintf(stderr, "error \r\n");
			usleep(2000);
			continue;
		}
		//检查是否有连接
		if(FD_ISSET(Server.m_commFd, &readfd)){
			struct sockaddr_in clientAddr;
			socklen_t addrLen = sizeof(clientAddr);
			bzero(&clientAddr, sizeof(struct sockaddr_in));
			int connFd = accept(Server.m_commFd, (struct sockaddr *) &clientAddr, &addrLen);
			fprintf(stderr, "有连接：ip:%s \r\n", inet_ntoa(clientAddr.sin_addr));
			int nextType = -1;
			//# m_streamType数组 记录连接器类型 在每次连接前做更新操作
			Server.m_streamType[0] = Server.m_conneter[0].getStreamType();
			Server.m_streamType[1] = Server.m_conneter[1].getStreamType();
			fprintf(stderr, "####Type[0] = %d Type[1] = %d\r\n#####",Server.m_streamType[0],Server.m_streamType[1]);
			for (int i = 0;i < 2;i++) {
				//只连接一个近景 和一个远景 自动判断
				fprintf(stderr, "i:%d\r\n", i);
				if (!Server.m_conneter[i].isRunnig()) {
					if(i == 0){
						if(Server.m_streamType[1]>=0){
							if(Server.m_streamType[1] == StremType_CloseShot)
								nextType = StremType_LongShot;
							else if(Server.m_streamType[1] == StremType_LongShot)
								nextType = StremType_CloseShot;
							Server.m_conneter[0].setStreamType(nextType);
						}
					}
					if(i == 1){
						if(Server.m_streamType[0]>=0){
							if(Server.m_streamType[0] == StremType_CloseShot)
								nextType = StremType_LongShot;
							else if(Server.m_streamType[0] == StremType_LongShot)
								nextType = StremType_CloseShot;
							Server.m_conneter[i].setStreamType(nextType);
						}
					}

					Server.m_conneter[i].init(connFd);
					Server.m_conneter[i].start();
					Server.m_streamType[i] = -1;
					break;
				}
			}



		}
	}

	return nullptr;
}


int QosServer::inputMediaFrame(int index, MediaPacket *packet)
{
	m_mutex.lock();
	do {
		if (!m_bInit || !m_bStart)
			break;

		m_conneter[0].inputMediaFrame(index,packet);
		m_conneter[1].inputMediaFrame(index,packet);

	} while (false);
	m_mutex.unlock();
	return 0;
}



