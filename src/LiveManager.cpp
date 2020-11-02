//
// Created by Night on 2020/9/3.
//

#include "LiveManager.h"
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <netdb.h>  
#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>  
#include <ifaddrs.h>  

#include <stdio.h>  

#include <stdlib.h>  
 


LiveManager::LiveManager()
{

}

LiveManager::~LiveManager()
{

}

//设置服务参数
int LiveManager::setSeverInfo(ServerInfo info)
{
	m_mutex.lock();
	do {

		if(m_bInit)
			break;

		m_serverInfo.deviceType = info.deviceType;
		m_serverInfo.listenPort = info.listenPort;
		m_serverInfo.localIp = info.localIp,
		m_serverInfo.version = info.version;

	} while (false);
	m_mutex.unlock();
	return 0;
}

//#初始化
int LiveManager::init()
{
	m_mutex.lock();
	do {

		m_qosServer.setSeverInfo(m_serverInfo);
		m_bInit = true;

	} while (false);
	m_mutex.unlock();
	return 0;
}

//#开始
int LiveManager::start()
{
	m_mutex.lock();
	do {

		fprintf(stderr, "#######启动监听服务#######\r\n");
		m_qosServer.startServer();
		m_bStart = true;

	} while (false);
	m_mutex.unlock();
	return 0;
}

//#停止
int LiveManager::stop()
{
	m_mutex.lock();
	do {

		if(!m_bStart || !m_bInit)
			break;

		m_qosServer.stopServer();
		m_bStart = false;

	} while (false);
	m_mutex.unlock();
	return 0;
}


//#放入一帧数据 通过index区别码流类型 带缓存
int LiveManager::inputMediaFrame(int index, MediaPacket *packet)
{
	m_mutex.lock();
	do {
		if(!m_bStart)
			break;

		m_qosServer.inputMediaFrame(index, packet);

	} while (false);
	m_mutex.unlock();
	return 0;
}

//#使用例子
void LiveManager::thisTest()
{
	ServerInfo info;
	info.localIp = "192.168.5.198";
	info.listenPort = SENDER_COMM_LISTEN_PORT;

	LiveManager::getInst().setSeverInfo(info);
	LiveManager::getInst().init();
	LiveManager::getInst().start();
	int i = 0;
	MediaPacket pack;
	char buff[ ] = "hello this test!";
	pack.bAudio = false;
	pack.iLen = strlen(buff);
	pack.pData = (uint8_t *)buff;

	while (1){
		pack.iStreamIdx = i++;
		if(i%2 == 0){
			pack.bAudio = true;
		}
		else
			pack.bAudio = false;

		LiveManager::getInst().inputMediaFrame(StremType_CloseShot, &pack);
		LiveManager::getInst().inputMediaFrame(StremType_LongShot, &pack);
		sleep(1);
	}

}



void LiveManager::Init_Sender()
{
	int sockfd_broadcast; // 套接字文件描述符 
	int inet_sock;
	struct ifreq ifr;
	struct sockaddr_in dest_addr; // 目标ip  
	fprintf(stderr, "启动发生器！！！！！！！！！！！\r\n");
	ServerInfo info;

	//sockfd_broadcast = socket(AF_INET, SOCK_DGRAM, 0); // 建立套接字  
	//if (sockfd_broadcast == -1)
	//{
	//	printf("sockfd_broadcast socket()");
	//	return -1;
	//}
	////Get IP
	//if (ioctl(sockfd_broadcast, SIOCGIFADDR, &ifr) < 0)
	//{
	//	printf("ioctl SIOCGIFADDR error\n");
	//}
	//else
	//{
	//	strncpy(info.localIp, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),32);
	//	printf("IP:%s \n", info.localIp);
	//}


	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;
	void * tmpAddrPtr = NULL;

	getifaddrs(&ifAddrStruct);

	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			char addressBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
			info.localIp=std::string(addressBuffer);
			//strcpy(info.localIp, addressBuffer);

		}
	}
	if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);


	info.listenPort = SENDER_COMM_LISTEN_PORT;

	LiveManager::getInst().setSeverInfo(info);
	LiveManager::getInst().init();
	LiveManager::getInst().start();

	close(sockfd_broadcast);
}

void LiveManager::Sender_Data(int Ch, int index, int len, unsigned char *data)
{

	int i = 0;
	MediaPacket pack;
	pack.bAudio = false;
	pack.type = index;
	pack.iLen = len;
	pack.pData = (uint8_t *)data;
	

	if (Ch == 0)
	{
		LiveManager::getInst().inputMediaFrame(StremType_CloseShot, &pack);
	}
	
	if (Ch == 1)
	{
		LiveManager::getInst().inputMediaFrame(StremType_LongShot, &pack);
	}
	

	//inputMediaFrame(&pack);
}



