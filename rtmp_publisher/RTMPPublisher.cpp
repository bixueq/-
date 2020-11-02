#include "RTMPPublisher.h"
#ifdef _WIN32
#include "YCatLog.h"
#endif
#include "util.h"
#include "nalu.h"
#include <sys/syscall.h>

#define INVALID     0xFFFFFFFF
#define ENABLE_SENDWIN_OPTION 1

//#define DIRECT_SEND

RTMPPublisher* RTMPPublisher::g_pInst = NULL;
char RTMPPublisher::g_strRTMPErrors[1024];
char RTMPPublisher::g_strURL[256];
UINT initVideoTimestamp = 0;
char g_strEvent[] = {"event"};
bool g_sendLogSwitch = false;
long int acount = 0;
long int bcount = 0;


RTMPPublisher::RTMPPublisher()
{
    m_bStopping = false;
    m_bStreamStarted = false;
    m_bConnecting = false;
    m_bConnected = false;

    m_bRun = true;
//    m_hSendThread = NULL;
//    m_hSocketThread = NULL;
//    m_hConnectionThread = NULL;
    m_hSendSempahore = NULL;
#ifdef _WIN32
    m_hWriteEvent = NULL;
    m_hBufferEvent = NULL;
    m_hSendBacklogEvent = NULL;
#endif
    m_hBufferSpaceAvailableEvent = NULL;
    m_hSendLoopExit = NULL;
    m_hSocketLoopExit = NULL;
    m_hDataMutex = NULL;
    m_hDataBufferMutex = NULL;
    m_hRTMPMutex = NULL;

    m_listDataPacket.clear();
    m_nCurrentBufferSize = 0;
    m_pDataBuffer = NULL;
    m_nDataBufferSize = 131072;  // adjust?
    m_nCurDataBufferLen = 0;
    m_nLowLatencyMode = LL_MODE_NONE;

    m_nTotalVideoFrames = 0;
    m_nNumBFramesDumped = 0;
    m_nNumPFramesDumped = 0;
    m_nTotalTimesWaited = 0;
    m_nTotalBytesWaited = 0;
    m_nTotalSendBytes = 0;
    m_nBytesSent = 0;
    m_nTotalSendPeriod = 0;
    m_nTotalSendCount = 0;

    m_pRtmp = NULL;
    m_bEncVideoSeqHeader = false;
    m_nVideoSeqHeaderLen = 0;
    m_bFirstKeyframe = true;

    m_nBFrameDropThreshold = 400;
    m_nDropThreshold = 600;
    m_nMinFramedropTimestsamp = 0;
    m_nLastBFrameDropTime = 0;

	m_nPrevConnectTime = 0;

    memset(&m_metaData, 0, sizeof(m_metaData));
    m_nEventStatus = 0;

	m_nConnectIntervalTime = 5000;
	m_nPubMaxErrorLimit = 30;
	m_nErrorCount = 0;

}

RTMPPublisher::~RTMPPublisher()
{
	m_bRun = false;
#ifdef _WIN32
    SetEvent(m_hStopEvent);
	WaitForSingleObject(m_hMonitorThread, INFINITE);

#else
    OSReleaseSemCond(m_hStopEvent);
	m_monitorThread.waitThread();
	m_monitorThread.closeThread();
//    if(OSWaitThread(m_hMonitorThread))
//		printf("OSWaitThread failure");
//
//	if(OSCloseThread(m_hMonitorThread))
//		printf("OSCloseThread failure");
//	m_hMonitorThread = NULL;

#endif

    if (m_hDataMutex)
    {
        OSCloseMutex(m_hDataMutex);
        m_hDataMutex = NULL;
    }

    if (m_hRTMPMutex)
    {
        OSCloseMutex(m_hRTMPMutex);
        m_hRTMPMutex = NULL;
    }

#ifdef _WIN32
    if (m_hStopEvent)
        CloseHandle(m_hStopEvent);
#else
	if(m_hStopEvent) {
		OSCloseSemCond(m_hStopEvent);
		m_hStopEvent = NULL;
	}

#endif

	OSEnterMutex(m_hDataBufferMutex);
    if (m_pDataBuffer)
    {
        free(m_pDataBuffer);
        m_pDataBuffer = NULL;
    }
	OSLeaveMutex(m_hDataBufferMutex);

    if (m_hDataBufferMutex)
    {
        OSCloseMutex(m_hDataBufferMutex);
        m_hDataBufferMutex = NULL;
    }

    double dBFrameDropPercentage = double(m_nNumBFramesDumped) / MAX(1, m_nTotalVideoFrames)*100.0;
    double dPFrameDropPercentage = double(m_nNumPFramesDumped) / MAX(1, m_nTotalVideoFrames)*100.0;

    if (m_nTotalSendCount)
        printf("Average send payload: %d bytes, average send interval: %d ms",
        (DWORD)(m_nTotalSendBytes / m_nTotalSendCount), m_nTotalSendPeriod / m_nTotalSendCount);

    printf("Number of times waited to send: %d, Waited for a total of %d bytes", m_nTotalTimesWaited, m_nTotalBytesWaited);

    printf("Number of b-frames dropped: %u (%0.2g%%), Number of p-frames dropped: %u (%0.2g%%), Total %u (%0.2g%%)",
        m_nNumBFramesDumped, dBFrameDropPercentage,
        m_nNumPFramesDumped, dPFrameDropPercentage,
        m_nNumBFramesDumped + m_nNumPFramesDumped, dBFrameDropPercentage + dPFrameDropPercentage);

    printf("Number of bytes sent: %llu", m_nTotalSendBytes);
}

void RTMPPublisher::Stop()
{
    m_bStopping = true;
    //we're in the middle of connecting! wait for that to happen to avoid all manner of race conditions
//    if (m_hConnectionThread)
//    {
#ifdef _WIN32
        //the connect thread could be stalled in a blocking call, kill the socket to ensure it wakes up
        //if (WaitForSingleObject(m_hConnectionThread, 0) == WAIT_TIMEOUT)
        //{
        //    if (m_pRtmp != NULL && m_pRtmp->m_sb.sb_socket != -1)
        //    {
        //        OSCloseSocket(m_pRtmp->m_sb.sb_socket);
        //        m_pRtmp->m_sb.sb_socket = -1;
        //    }
        //}

        WaitForSingleObject(m_hConnectionThread, INFINITE);
        if(OSCloseThread(m_hConnectionThread))
			printf("OSCloseThread failure");
        m_hConnectionThread = NULL;
		printf("ConnectionThread exit success");
#else

		// 确保 CreateConnectionThread 线程已退出

#if 1

//		if(m_pRtmp != NULL && m_pRtmp->m_sb.sb_socket != -1) {
//			struct timeval ti;
//			ti.tv_sec = 1;
//			ti.tv_usec = 0;
//			setsockopt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDTIMEO, &ti, sizeof(ti));
//			close(m_pRtmp->m_sb.sb_socket);
//			m_pRtmp->m_sb.sb_socket = -1;
//		}

		OSSleep(50);

		int ncount = 0;
		while(m_bConnecting) {
			OSSleep(20);
			if(ncount++%100 == 0) {
				printf("now is connecting, wait for....");
			}

			if(ncount >= 3000)
				break;
		}
#endif
		OSSleep(10);
#if 0		//线程改为detach
	if(OSWaitThread(m_hConnectionThread))
        {
        	printf("OSWaitThread failure");
        }

        if(OSCloseThread(m_hConnectionThread))
			printf("OSCloseThread failure");
        m_hConnectionThread = NULL;
#endif

#endif

    DWORD startTime = OSGetTimeNow();

    //send all remaining buffered packets, this may block since it respects timestamps
    //FlushBufferedPackets();

    //printf("Packet flush completed in %d ms", timeGetTime() - startTime);

    //OSDebugOut (TEXT("%d queued after flush\n"), queuedPackets.Num());

//    if (m_hSendThread)
//    {

        startTime = OSGetTimeNow();
        //this marks the thread to exit after current work is done
#ifdef _WIN32
        SetEvent(m_hSendLoopExit);
#else
        OSReleaseSemCond(m_hSendLoopExit);
#endif

        //these wake up the thread
#ifdef _WIN32
        OSReleaseSemaphore(m_hSendSempahore);
        SetEvent(m_hBufferSpaceAvailableEvent);
#else
        OSReleaseSemCond(m_hSendSempahore);
        OSReleaseSemCond(m_hBufferSpaceAvailableEvent);
        OSSleep(200);
#endif

#ifdef _WIN32
        //wait 1 sec for all data to finish sending
		WaitForSingleObject(m_hSendThread, INFINITE);
#endif

//        if(OSWaitThread(m_hSendThread))
//        {
//        	printf("OSWaitThread failure");
//        }
//
//        if(OSCloseThread(m_hSendThread))
//			printf("OSCloseThread failure");
//        m_hSendThread = NULL;

		m_sendThread.waitThread();
		m_sendThread.closeThread();
		printf("SendThread exit in %d ms", OSGetTimeNow() - startTime);
//    }

    if (m_hSendSempahore)
    {
#ifdef _WIN32
        OSCloseSemaphore(m_hSendSempahore);
#else
        OSCloseSemCond(m_hSendSempahore);
#endif
        m_hSendSempahore = NULL;
    }

    //OSDebugOut (TEXT("*** ~RTMPPublisher m_hSendThread terminated (%d queued, %d buffered, %d data)\n"), queuedPackets.Num(), m_BufferedPackets.Num(), curDataBufferLen);

//    if (m_hSocketThread)
//    {
        startTime = OSGetTimeNow();

#ifdef _WIN32
        //mark the socket loop to shut down after the buffer is empty
        SetEvent(m_hSocketLoopExit);

        //wake it up in case it already is empty
        SetEvent(m_hBufferEvent);
        //wait 1 sec for it to exit
        //OSTerminateThread(m_hSocketThread, 60000);
		WaitForSingleObject(m_hSocketThread, INFINITE);
#else
        OSReleaseSemCond(m_hSocketLoopExit);

        write(m_hBufferEvent[1],  g_strEvent, sizeof(g_strEvent));
//        if(OSWaitThread(m_hSocketThread))
//        {
//        	printf("OSWaitThread failure");
//        }

#endif
		m_socketThread.waitThread();
		m_socketThread.closeThread();
//        if(OSCloseThread(m_hSocketThread))
//			printf("OSCloseThread failure");
//        m_hSocketThread = NULL;

		printf("SocketThread exit in %d ms", OSGetTimeNow() - startTime);
//    }

    //OSDebugOut (TEXT("*** ~RTMPPublisher m_hSocketThread terminated (%d queued, %d buffered, %d data)\n"), queuedPackets.Num(), m_BufferedPackets.Num(), curDataBufferLen);

    if (m_pRtmp)
    {
        if (RTMP_IsConnected(m_pRtmp))
        {
            startTime = OSGetTimeNow();

            //at this point nothing should be in the buffer, flush out what remains to the net and make it blocking
            //FlushDataBuffer();

            //disable the buffered send, so RTMP_* functions write directly to the net (and thus block)
            m_pRtmp->m_bCustomSend = 0;

            //manually shut down the stream and issue a graceful socket shutdown
            RTMP_DeleteStream(m_pRtmp);

#ifdef _WIN32
            shutdown(m_pRtmp->m_sb.sb_socket, SD_SEND);
#else
            shutdown(m_pRtmp->m_sb.sb_socket, 1);
#endif

#if 0			// 会阻塞
            //this waits for the socket shutdown to complete gracefully
            for (;;)
            {
                char buff[1024];
                int ret;

                ret = recv(m_pRtmp->m_sb.sb_socket, buff, sizeof(buff), 0);
                if (!ret)
                    break;
                else if (ret == -1)
                {
                    printf("Received error %d while waiting for graceful shutdown.", OSGetSocketError());
                    break;
                }
            }
#endif 

            printf("Final socket shutdown completed in %d ms", OSGetTimeNow() - startTime);
        }

//		if (m_pRtmp && m_pRtmp->m_sb.sb_socket != -1)
//        {
//            OSCloseSocket(m_pRtmp->m_sb.sb_socket);
//            m_pRtmp->m_sb.sb_socket = -1;
//        }

        //this closes the socket if not already done
        RTMP_Close(m_pRtmp);
    }

    m_nCurDataBufferLen = 0;

#ifdef _WIN32
    if (m_hBufferEvent)
    {
        CloseHandle(m_hBufferEvent);
        m_hBufferEvent = NULL;
    }

    if (m_hSendLoopExit)
    {
        CloseHandle(m_hSendLoopExit);
        m_hSendLoopExit = NULL;
    }

    if (m_hSocketLoopExit)
    {
        CloseHandle(m_hSocketLoopExit);
        m_hSocketLoopExit = NULL;
    }

    if (m_hSendBacklogEvent)
    {
        CloseHandle(m_hSendBacklogEvent);
        m_hSendBacklogEvent = NULL;
    }

    if (m_hBufferSpaceAvailableEvent)
    {
        CloseHandle(m_hBufferSpaceAvailableEvent);
        m_hBufferSpaceAvailableEvent = NULL;
    }

    if (m_hWriteEvent)
    {
        CloseHandle(m_hWriteEvent);
        m_hWriteEvent = NULL;
    }

#else
   if (m_hSendLoopExit)
    {
        OSCloseSemCond(m_hSendLoopExit);
        m_hSendLoopExit = NULL;
    }

    if (m_hSocketLoopExit)
    {
        OSCloseSemCond(m_hSocketLoopExit);
        m_hSocketLoopExit = NULL;
    }
	
    if (m_hBufferSpaceAvailableEvent)
    {
        OSCloseSemCond(m_hBufferSpaceAvailableEvent);
        m_hBufferSpaceAvailableEvent = NULL;
    }

#endif

    if (m_pRtmp)
    {
        if (m_pRtmp->Link.pubUser.av_val)
            free(m_pRtmp->Link.pubUser.av_val);
        if (m_pRtmp->Link.pubPasswd.av_val)
            free(m_pRtmp->Link.pubPasswd.av_val);
        RTMP_Free(m_pRtmp);
        m_pRtmp = NULL;
    }

	OSEnterMutex(m_hDataMutex);
    //--------------------------
    list<NetworkPacket>::iterator it = m_listDataPacket.begin();
    for (; it != m_listDataPacket.end(); ++it)
    {
        free(it->data);
    }
    m_listDataPacket.clear();
	OSLeaveMutex(m_hDataMutex);

    m_nCurrentBufferSize = 0;

    m_bStopping = false;
    m_bStreamStarted = false;
//    m_bConnecting = false;
    m_bConnected = false;

    m_bEncVideoSeqHeader = false;
    m_nVideoSeqHeaderLen = 0;
    m_bFirstKeyframe = true;
    m_nMinFramedropTimestsamp = 0;
    m_nLastBFrameDropTime = 0;

    m_nEventStatus = 0;
}

RTMPPublisher* RTMPPublisher::Instance()
{
    if (g_pInst == NULL)
        g_pInst = new RTMPPublisher();
    return g_pInst;
}

void RTMPPublisher::Destory()
{
    delete g_pInst;
    g_pInst = NULL;
}

int RTMPPublisher::Init(UINT sampleRate, UINT channelNum, char* strURL, RTMPSendDataCallBack pFun, void* pArgv)
{
    if (pFun == NULL || strURL == NULL)
    {
        printf("Input error");
        return false;
    }

    m_pFun = pFun;
    m_pArgv = pArgv;

    m_pDataBuffer = (char *)malloc(m_nDataBufferSize);
    if (!m_pDataBuffer)
    {
        printf("Malloc nDataBufferSize failure");
        return false;
    }

	m_hDataBufferMutex = OSCreateMutex();
    if (!m_hDataBufferMutex)
    {
        printf("Could not create hDataBufferMutex");
        return false;
    }

    m_nSampleRate = sampleRate;
    m_nChannelNum = channelNum;
    strncpy(g_strURL, strURL, sizeof(g_strURL)-1);
    m_hDataMutex = OSCreateMutex();
    if (!m_hDataMutex)
    {
        printf("Could not create DataMutex");
        return false;
    }

    m_hRTMPMutex = OSCreateMutex();
    if (!m_hRTMPMutex)
    {
        printf("Could not create RTMPMutex");
        return false;
    }

#ifdef _WIN32
    m_hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
    m_hStopEvent = OSCreateSemCond();
#endif
    if (!m_hStopEvent)
    {
        printf("Could not create m_hStopEvent");
        return false;
    }
	int ret = m_monitorThread.createThread((TASK_ENTRY_FUNC) RTMPPublisher::MonitorThread, this, osthread::eThreadType_Joinable);
    //m_hMonitorThread = OSCreateThread((TASK_ENTRY_FUNC)RTMPPublisher::MonitorThread, this, THREAD_TYPE_JOIN);
    if (ret != 0)
    {
        printf("Could not create monitor thread");
        return false;
    }

    if (!m_bConnected && !m_bConnecting && !m_bStopping)
    {
		m_connectionThread.createThread((TASK_ENTRY_FUNC) CreateConnectionThread, this);
       	//m_hConnectionThread = OSCreateThread((TASK_ENTRY_FUNC)CreateConnectionThread, this);
    }

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    return true;
}

int RTMPPublisher::Start(UINT tcpBufferSize)
{
#ifdef _WIN32
    m_hSendSempahore = OSCreateSemaphore(0, 0x7FFFFFFFL);
#else
    m_hSendSempahore = OSCreateSemCond();
#endif
    if (!m_hSendSempahore)
    {
        printf("Could not create SendSemaphore");
        return false;
    }

    m_pRtmp->m_customSendFunc = (CUSTOMSEND)RTMPPublisher::BufferedSend;
    m_pRtmp->m_customSendParam = this;
#ifndef DIRECT_SEND
    m_pRtmp->m_bCustomSend = TRUE;
#else
    m_pRtmp->m_bCustomSend = FALSE;
#endif

    //------------------------------------------

    int curTCPBufSize, curTCPBufSizeSize = sizeof(curTCPBufSize);

    if (!OSGetSocketOpt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize))
    {
        printf("SO_SNDBUF was at %u", curTCPBufSize);

        if (curTCPBufSize < int(tcpBufferSize))
        {
            if (!setsockopt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (const char *)&tcpBufferSize, sizeof(tcpBufferSize)))
            {
                if (!OSGetSocketOpt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize))
                {
                    if (curTCPBufSize != tcpBufferSize)
                        printf("Could not raise SO_SNDBUF to %u, value is now %d", tcpBufferSize, curTCPBufSize);

                    printf("SO_SNDBUF is now %d", curTCPBufSize);
                }
                else
                {
                    printf("Failed to query SO_SNDBUF, error %d", OSGetSocketError());
                }
            }
            else
            {
                printf("Failed to raise SO_SNDBUF to %u, error %d", tcpBufferSize, OSGetSocketError());
            }
        }
    }
    else
    {
        printf("Failed to query SO_SNDBUF, error %d", OSGetSocketError());
    }

    //------------------------------------------
#ifdef _WIN32
    m_hBufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_hBufferEvent)
    {
        printf("Could not create hBufferEvent");
        return false;
    }

    m_hBufferSpaceAvailableEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_hBufferSpaceAvailableEvent)
    {
        printf("Could not create hBufferSpaceAvailableEvent");
        return false;
    }

    m_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_hWriteEvent)
    {
        printf("Could not create hWriteEvent");
        return false;
    }

    m_hSendBacklogEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_hSendBacklogEvent)
    {
        printf("Could not create hSendBacklogEvent");
        return false;
    }

    m_hSendLoopExit = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_hSendLoopExit)
    {
        printf("Could not create hSendLoopExit");
        return false;
    }

    m_hSocketLoopExit = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_hSocketLoopExit)
    {
        printf("Could not create hSocketLoopExit");
        return false;
    }
#else
    int ret  = pipe(m_hBufferEvent);
    if (ret == -1)
    {
         printf("Could not create hBufferEvent pipe");
        return false;
    }

    m_hBufferSpaceAvailableEvent = OSCreateSemCond();
    if (!m_hBufferSpaceAvailableEvent)
    {
        printf("Could not create hBufferSpaceAvailableEvent");
        return false;
    }

    m_hSendLoopExit = OSCreateSemCond();
    if (!m_hSendLoopExit)
    {
        printf("Could not create hSendLoopExit");
        return false;
    }

    m_hSocketLoopExit = OSCreateSemCond();
    if (!m_hSocketLoopExit)
    {
        printf("Could not create hSocketLoopExit");
        return false;
    }
#endif
	//! 启动数据发送线程
	//    m_hSendThread = OSCreateThread((TASK_ENTRY_FUNC)RTMPPublisher::SendThread, this, THREAD_TYPE_JOIN);
//    if (!m_hSendThread)
//    {
	ret = m_sendThread.createThread((TASK_ENTRY_FUNC)RTMPPublisher::SendThread, this, osthread::eThreadType_Joinable);
	if(ret != 0) {
        printf("Could not create send thread");
        return false;
    }
	ret = m_socketThread.createThread((TASK_ENTRY_FUNC) RTMPPublisher::SocketThread, this, osthread::eThreadType_Joinable);
   // m_hSocketThread = OSCreateThread((TASK_ENTRY_FUNC)RTMPPublisher::SocketThread, this, THREAD_TYPE_JOIN);
    if (ret != 0){
        printf("Could not create send thread");
        return false;
    }

    m_nPacketWaitType = 0;
    return true;
}

void LogInterfaceType(RTMP *rtmp)
{
#ifdef _WIN32
    MIB_IPFORWARDROW    route;
    DWORD               destAddr, sourceAddr;
    CHAR                hostname[256];

    if (rtmp->Link.hostname.av_len >= sizeof(hostname)-1)
        return;

    strncpy(hostname, rtmp->Link.hostname.av_val, sizeof(hostname)-1);
    hostname[rtmp->Link.hostname.av_len] = 0;

    HOSTENT *h = gethostbyname(hostname);
    if (!h)
        return;

    destAddr = *(DWORD *)h->h_addr_list[0];

    if (rtmp->m_bindIP.addrLen == 0)
        sourceAddr = 0;
    else if (rtmp->m_bindIP.addr.ss_family == AF_INET)
        sourceAddr = (*(struct sockaddr_in *)&rtmp->m_bindIP).sin_addr.S_un.S_addr;
    else
        return; // getting route for IPv6 is far more complex, ignore for now

    if (!GetBestRoute(destAddr, sourceAddr, &route))
    {
        MIB_IFROW row;
        memset(&row, sizeof(row), 0);
        row.dwIndex = route.dwForwardIfIndex;

        if (!GetIfEntry(&row))
        {
            DWORD speed = row.dwSpeed / 1000000;
            TCHAR *type;

            if (row.dwType == IF_TYPE_ETHERNET_CSMACD)
                type = TEXT("ethernet");
            else if (row.dwType == IF_TYPE_IEEE80211)
                type = TEXT("802.11");
            else
            {
                type = NULL;
            }

            if (type)
                printf(" Interface: %S (%s, %d mbps)", row.bDescr, type, speed);
            else
                printf(" Interface: %S (%d, %d mbps)", row.bDescr, row.dwType, speed);
        }
    }
#endif
}

void RTMPPublisher::librtmpErrorCallback(int level, const char *format, va_list vl)
{
#ifdef _WIN32
    char ansiStr[1024];
    TCHAR logStr[1024];

    if (level > RTMP_LOGERROR)
        return;

    vsnprintf(ansiStr, sizeof(ansiStr)-1, format, vl);
    ansiStr[sizeof(ansiStr)-1] = 0;

    MultiByteToWideChar(CP_UTF8, 0, ansiStr, -1, (LPWSTR)g_strRTMPErrors, _countof(logStr) - 1);

    printf("librtmp error: %s", g_strRTMPErrors);
#endif
}

int test_first = 1;
int RTMPPublisher::CreateConnectionThread(RTMPPublisher *publisher)
{
	int tid = syscall(SYS_gettid);

    printf("CreateConnectionThread start, ThreadID:%u, publisher->m_hRTMPMutex = %p", tid, publisher->m_hRTMPMutex);
	OSEnterMutex(publisher->m_hRTMPMutex);

	if(publisher->m_bConnected) {
		printf("CreateConnectionThread start failed, now is connected ThreadID:%u", tid);
		OSLeaveMutex(publisher->m_hRTMPMutex);
		return -1;
	}

	if(publisher->m_bConnecting) {
		printf("CreateConnectionThread start failed, now is connecting ThreadID:%u", tid);
		OSLeaveMutex(publisher->m_hRTMPMutex);
		return -1;
	}

	publisher->m_bConnecting = true;

	publisher->InvokeCallBack(RTMP_SEND_START_CONNECT);

	publisher->m_nPrevConnectTime = OSGetTimeNow();
	publisher->m_nTotalVideoFrames = 0;
	publisher->m_nNumBFramesDumped = 0;
	publisher->m_nNumPFramesDumped = 0;
	publisher->m_nTotalTimesWaited = 0;
	publisher->m_nTotalBytesWaited = 0;
	publisher->m_nTotalSendBytes = 0;
	publisher->m_nBytesSent = 0;
	publisher->m_nTotalSendPeriod = 0;
	publisher->m_nTotalSendCount = 0;
#ifdef LINUX
//    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
//    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

#endif
    //------------------------------------------------------
    // set up URL

    bool bSuccess = false;
    char failReason[256];

    UINT tcpBufferSize;
    DWORD startTime;
    int retryTime;
    int sendBuffer;
    int len = sizeof(sendBuffer);
    int ret;
    int set = 1;
	RTMP_SEND_EVENT_TYPE connectFailReason = RTMP_SEND_CONNECTED_SUCCESS;

    publisher->m_pRtmp = RTMP_Alloc();
	RTMP_Init(publisher->m_pRtmp);

    //RTMP_LogSetLevel(RTMP_LOGERROR);
	if (!RTMP_SetupURL(publisher->m_pRtmp, g_strURL))
    {
        strcpy(failReason, "Connection.CouldNotParseURL");
		connectFailReason = RTMP_SEND_URL_PARSE_FAILURE;
        goto end;
    }

	RTMP_EnableWrite(publisher->m_pRtmp); //set it to publish

	publisher->m_pRtmp->Link.swfUrl.av_len = publisher->m_pRtmp->Link.tcUrl.av_len;
	publisher->m_pRtmp->Link.swfUrl.av_val = publisher->m_pRtmp->Link.tcUrl.av_val;
	publisher->m_pRtmp->Link.flashVer.av_val = (char*)("FMLE/3.0 (compatible; FMSc/1.0)");
	publisher->m_pRtmp->Link.flashVer.av_len = (int)strlen(publisher->m_pRtmp->Link.flashVer.av_val);

    //-----------------------------------------
    tcpBufferSize = 64 * 1024;

    if (tcpBufferSize < 8192)
        tcpBufferSize = 8192;
    else if (tcpBufferSize > 1024 * 1024)
        tcpBufferSize = 1024 * 1024;

	publisher->m_pRtmp->m_outChunkSize = 4096; //RTMP_DEFAULT_CHUNKSIZE
	publisher->m_pRtmp->m_bSendChunkSizeInfo = TRUE;
	publisher->m_pRtmp->m_bUseNagle = TRUE;

#ifdef _WIN32
    char strBindIP[64] = { "Default" };
    if (strcmp(strBindIP, "Default"))
    {
        printf("Binding to non-default IP %s", strBindIP);
        if (strchr(strBindIP, ':'))
			publisher->m_pRtmp->m_bindIP.addr.ss_family = AF_INET6;
        else
			publisher->m_pRtmp->m_bindIP.addr.ss_family = AF_INET;
		publisher->m_pRtmp->m_bindIP.addrLen = sizeof(publisher->m_pRtmp->m_bindIP.addr);
		if (WSAStringToAddress((LPTSTR)strBindIP, publisher->m_pRtmp->m_bindIP.addr.ss_family, NULL, (LPSOCKADDR)&publisher->m_pRtmp->m_bindIP.addr, &publisher->m_pRtmp->m_bindIP.addrLen) == SOCKET_ERROR)
        {
            // no localization since this should rarely/never happen
            strcpy(failReason, "WSAStringToAddress: Could not parse address");
			connectFailReason = RTMP_SEND_URL_PARSE_FAILURE;
            goto end;
        }
    }
	LogInterfaceType(publisher->m_pRtmp);
#endif

    //-----------------------------------------

    startTime = OSGetTimeNow();

	if (!RTMP_Connect(publisher->m_pRtmp, NULL))
    {
		strcpy(failReason, "Connection.CouldNotConnect");
		connectFailReason = RTMP_SEND_CONNECTED_FAILURE;
        goto end;
    }

    printf("Completed handshake with %s in %u ms.", g_strURL, OSGetTimeNow() - startTime);

	if (RTMP_ConnectStream(publisher->m_pRtmp, 0) != 1)
    {
		strcpy(failReason, "Connection.InvalidStream");
		connectFailReason = RTMP_SEND_CONNECTED_INVALID_STREAM;
        goto end;
    }
    //-----------------------------------------

	printf("connect to %s success, time:%u", g_strURL, OSGetTimeNow());

#ifndef _WIN32
    len = sizeof(sendBuffer);
	ret = OSGetSocketOpt(publisher->m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBuffer, &len);
    if (ret < 0)
    {
        printf("get socket send buffer len failure");
        strcpy(failReason, "get socket send buffer len failure");
        goto end;
    }
    else
    {
        printf("get socket send buffer len:%d", sendBuffer);
    }

    sendBuffer = 1024 * 1024 * 2; //2M
	ret = OSSetSocketOpt(publisher->m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBuffer, len);
    if (ret < 0)
    {
        printf("set socket buffer len falure");
        strcpy(failReason, "set socket buffer len falure");
        goto end;
    }
    else
    {
        printf("set socket send buffer to:%d", sendBuffer);
    }

#ifdef SO_NOSIGPIPE
	ret = OSSetSocketOpt(publisher->m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_NOSIGPIPE, (char *)&set, sizeof(int));
    if (ret < 0)
    {
        printf("set socket buffer len falure");
        strcpy(failReason, "set socket buffer len falure");
        goto end;
    }
#endif

#endif
    //-----------------------------------------
    bSuccess = true;

end:
    if (!bSuccess)
    {
		if (publisher->m_pRtmp)
        {
			RTMP_Close(publisher->m_pRtmp);
			RTMP_Free(publisher->m_pRtmp);
            publisher->m_pRtmp = NULL;
        }

#if 0 //donna
        if (failReason.IsValid())
            App->SetStreamReport(failReason);

        if (!publisher->bStopping)
            PostMessage(hwndMain, OBS_REQUESTSTOP, bCanRetry ? 0 : 1, 0);
#endif

        printf("Connection to %s failed, failReason:%s", g_strURL, failReason);
        publisher->SetStopEvent();
		publisher->InvokeCallBack(connectFailReason);
    }
    else
    {
        publisher->InvokeCallBack(RTMP_SEND_CONNECTED_SUCCESS);
		printf("finish InvokeCallBack");
        publisher->Start(tcpBufferSize);
        publisher->m_bConnected = true;
		printf("finish Start");

    }

    publisher->m_bConnecting = false;
	
	OSLeaveMutex(publisher->m_hRTMPMutex);
	printf("停止CreateConnectionThread");

    return 0;
}

int RTMPPublisher::SendThread(RTMPPublisher *publisher)
{
    publisher->SendLoop();
    return 0;
}

int RTMPPublisher::SocketThread(RTMPPublisher *publisher)
{
    publisher->SocketLoop();
    return 0;
}

int RTMPPublisher::MonitorThread(RTMPPublisher *publisher)
{
    publisher->MonitorLoop();
    return 0;
}

int RTMPPublisher::BufferedSend(RTMPSockBuf *sb, const char *buf, int len, RTMPPublisher *network)
{
    //NOTE: This function is called from the SendLoop thread, be careful of race conditions.

retrySend:

    //We may have been disconnected mid-shutdown or something, just pretend we wrote the data
    //to avoid blocking if the socket loop exited.
    if (!RTMP_IsConnected(network->m_pRtmp))
        return len;

    OSEnterMutex(network->m_hDataBufferMutex);

	if(network->m_pDataBuffer == NULL)
	{
		printf("network->m_pDataBuffer is NULL");
		OSLeaveMutex(network->m_hDataBufferMutex);
		return 0;
	}

    if (network->m_nCurDataBufferLen + len >= network->m_nDataBufferSize)
    {
        //printf("Socket buffer is full (%d / %d bytes), waiting to send %d bytes",
            //network->m_nCurDataBufferLen, network->m_nDataBufferSize, len);
        ++network->m_nTotalTimesWaited;
        network->m_nTotalBytesWaited += len;

        OSLeaveMutex(network->m_hDataBufferMutex);

#ifdef _WIN32
        int status = WaitForSingleObject(network->m_hBufferSpaceAvailableEvent, INFINITE);
		if (status == WAIT_ABANDONED || status == WAIT_FAILED || network->m_bStopping)
            return 0;
        goto retrySend;
#else
        int status = OSWaitSemCond(network->m_hBufferSpaceAvailableEvent, -1);
		if (status !=0 || network->m_bStopping)
            return 0;
        goto retrySend;
#endif
    }

    memcpy(network->m_pDataBuffer + network->m_nCurDataBufferLen, buf, len);
    network->m_nCurDataBufferLen += len;

    OSLeaveMutex(network->m_hDataBufferMutex);

#ifdef _WIN32
    SetEvent(network->m_hBufferEvent);
#else
    write(network->m_hBufferEvent[1], &g_strEvent, sizeof(g_strEvent));
#endif

    return len;
}


void RTMPPublisher::SetupSendBacklogEvent()
{
#ifdef _WIN32
    memset(&m_sendBacklogOverlapped, sizeof(m_sendBacklogOverlapped), 0);

    ResetEvent(m_hSendBacklogEvent);
    m_sendBacklogOverlapped.hEvent = m_hSendBacklogEvent;

    idealsendbacklognotify(m_pRtmp->m_sb.sb_socket, &m_sendBacklogOverlapped, NULL);
#endif
}

void RTMPPublisher::FatalSocketShutdown()
{
    //We close the socket manually to avoid trying to run cleanup code during the shutdown cycle since
    //if we're being called the socket is already in an unusable state.

//	m_bStopping = true;		// 使 CreateConnectionThread 不再被启动
	
    OSCloseSocket(m_pRtmp->m_sb.sb_socket);
    m_pRtmp->m_sb.sb_socket = -1;

    //anything buffered is invalid now
    m_nCurDataBufferLen = 0;

    SetStopEvent();

    //if (!m_bStopping)
    //{
        //if (AppConfig->GetInt(TEXT("Publish"), TEXT("ExperimentalReconnectMode")) == 1 && AppConfig->GetInt(TEXT("Publish"), TEXT("Delay")) == 0)
        // App->NetworkFailed();
        // else
        // App->PostStopMessage();
    //}
}

int RTMPPublisher::SendNoBlock(int socket_fd, const char*send_buffer, int size)
{
    int ret = -1;
	int Total = 0;
	int lenSend = 0;

	struct timeval tv;
	tv.tv_sec = 4;
	tv.tv_usec = 0;
	fd_set wset;
	while(1)
	{
		FD_ZERO(&wset);
		FD_SET(socket_fd, &wset);
		if(select(socket_fd + 1, NULL, &wset, NULL, &tv) > 0)//4秒之内可以send，即socket可以写入
		{
			 lenSend = send(socket_fd,send_buffer + Total,size -Total,0);
			 if(lenSend == -1)
			 {
				 ret = -1;
				 break;
			 }
			 Total += lenSend;
			 if(Total == size)
			 {
				 ret = size;
				 break;
			 }
		 }
		 else  //4秒之内socket还是不可以写入，认为发送失败
		 {
			 ret = -1;
			 break;
		 }
	 }
	 return ret;
}

void RTMPPublisher::SocketLoop()
{
	printf("SocketThread启动, ThreadID:%u", syscall(SYS_gettid));
    bool canWrite = false;

    int delayTime;
    int latencyPacketSize;
    DWORD lastSendTime = 0;

#ifdef _WIN32
    WSANETWORKEVENTS networkEvents;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    WSAEventSelect(m_pRtmp->m_sb.sb_socket, m_hWriteEvent, FD_READ | FD_WRITE | FD_CLOSE);

    HANDLE hObjects[3];

    hObjects[0] = m_hWriteEvent;
    hObjects[1] = m_hBufferEvent;
    hObjects[2] = m_hSendBacklogEvent;
#else
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif

    //Low latency mode works by delaying delayTime ms between calls to send() and only sending
    //a buffer as large as latencyPacketSize at once. This causes keyframes and other data bursts
    //to be sent over several sends instead of one large one.
    if (m_nLowLatencyMode == LL_MODE_AUTO)
    {
        //Auto mode aims for a constant rate of whatever the stream bitrate is and segments into
        //MTU sized packets (test packet captures indicated that despite nagling being enabled,
        //the size of the send() buffer is still important for some reason). Note that delays
        //become very short at this rate, and it can take a while for the buffer to empty after
        //a keyframe.
        delayTime = 1400.0f / (m_nLowLatencyMode / 1000.0f);
        latencyPacketSize = 1460;
    }
    else if (m_nLowLatencyMode == LL_MODE_FIXED)
    {
        //We use latencyFactor - 2 to guarantee we're always sending at a slightly higher
        //rate than the maximum expected data rate so we don't get backed up
        latencyPacketSize = m_nLowLatencyMode / (m_nLatencyFactor - 2);
        delayTime = 1000 / m_nLatencyFactor;
    }
    else
    {
        latencyPacketSize = m_nLowLatencyMode;
        delayTime = 0;
    }

#if (defined ENABLE_SENDWIN_OPTION) && (defined _WIN32)
        SetupSendBacklogEvent();
#else
        printf ("Send window optimization disabled by user.");
#endif

    for (;;)
    {
#ifdef _WIN32
        if (m_bStopping && WaitForSingleObject(m_hSocketLoopExit, 0) != WAIT_TIMEOUT)
        {
            //OSEnterMutex(m_hDataBufferMutex);
            //if (m_nCurDataBufferLen == 0)
            //{
            //    printf("Exiting on empty buffer");
            //    OSLeaveMutex(m_hDataBufferMutex);
            //    break;
            //}

            printf ("m_bStopping and m_hSocketLoopExit is true, but %d bytes remain", m_nCurDataBufferLen);
            break;
            //OSLeaveMutex(m_hDataBufferMutex);
        }

        int status = WaitForMultipleObjects(3, hObjects, FALSE, INFINITE);
        if (status == WAIT_ABANDONED || status == WAIT_FAILED)
        {
            printf("Aborting due to WaitForMultipleObjects failure");
            SetStopEvent();
            break;
        }

        if (status == WAIT_OBJECT_0)
        {
            //Socket event
            if (WSAEnumNetworkEvents(m_pRtmp->m_sb.sb_socket, NULL, &networkEvents))
            {
                printf("Aborting due to WSAEnumNetworkEvents failure, %d"), OSGetSocketError();
                SetStopEvent();
                break;
            }

            if (networkEvents.lNetworkEvents & FD_WRITE)
                canWrite = true;

            if (networkEvents.lNetworkEvents & FD_CLOSE)
            {
                InvokeCallBack(RTMP_SEND_CONNECTION_BROKEN);
                if (lastSendTime)
                {
                    DWORD diff = OSGetTimeNow() - lastSendTime;
                    printf("Received FD_CLOSE, %u ms since last send (buffer: %d / %d)", diff, m_nCurDataBufferLen, m_nDataBufferSize);
                }

                if (m_bStopping)
                    printf("Aborting due to FD_CLOSE during shutdown, %d bytes lost, error %d", m_nCurDataBufferLen, networkEvents.iErrorCode[FD_CLOSE_BIT]);
                else
                    printf("Aborting due to FD_CLOSE, error %d", networkEvents.iErrorCode[FD_CLOSE_BIT]);
                FatalSocketShutdown();
                break;
            }

            if (networkEvents.lNetworkEvents & FD_READ)
            {
                BYTE discard[16384];
                int ret, errorCode;
                bool fatalError = FALSE;

                for (;;)
                {
                    ret = recv(m_pRtmp->m_sb.sb_socket, (char *)discard, sizeof(discard), 0);
                    if (ret == -1)
                    {
                    errorCode = OSGetSocketError();

                    if (errorCode == WSAEWOULDBLOCK)
                        break;

                        fatalError = TRUE;
                    }
                    else if (ret == 0)
                    {
                        errorCode = 0;
                        fatalError = TRUE;
                    }

                    if (fatalError)
                    {
                        printf("Socket error, recv() returned %d, GetLastError() %d", ret, errorCode);
                        FatalSocketShutdown();
                        break;
                    }
                }
            }
        }
        else if (status == WAIT_OBJECT_0 + 2)
        {
            //Ideal send backlog event
            ULONG idealSendBacklog;
            if (!idealsendbacklogquery(m_pRtmp->m_sb.sb_socket, &idealSendBacklog))
            {
                int curTCPBufSize, curTCPBufSizeSize = sizeof(curTCPBufSize);
                if (!getsockopt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize))
                {
                    if (curTCPBufSize < (int)idealSendBacklog)
                    {
                        int bufferSize = (int)idealSendBacklog;
                        setsockopt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (const char *)&bufferSize, sizeof(bufferSize));
                        printf("Increasing send buffer to ISB %d (buffer: %d / %d)", idealSendBacklog, m_nCurDataBufferLen, m_nDataBufferSize);
                    }
                }
                else
                    printf("Got hSendBacklogEvent but getsockopt() returned %d", OSGetSocketError());
            }
            else
                printf("Got hSendBacklogEvent but WSAIoctl() returned %d", OSGetSocketError());

            SetupSendBacklogEvent();
            continue;
        }
#else
//		printf("001-SocketLoop m_bStopping = %d, m_hSocketLoopExit = %p.", m_bStopping, m_hSocketLoopExit);
        if (m_bStopping && OSWaitSemCond(m_hSocketLoopExit, 0) == 0)
        {
            printf ("m_bStopping and m_hSocketLoopExit is true, but %d bytes remain", m_nCurDataBufferLen);
            break;
        }

//		printf("002-SocketLoop m_bStopping = %d, m_hSocketLoopExit = %p.", m_bStopping, m_hSocketLoopExit);

		fd_set readFds;
		fd_set writeFds;
		FD_ZERO(&readFds);
		FD_ZERO(&writeFds);
//		FD_SET(m_pRtmp->m_sb.sb_socket, &readFds);
		FD_SET(m_hBufferEvent[0], &readFds);
		FD_SET(m_pRtmp->m_sb.sb_socket, &writeFds);
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int curTCPBufSize, curTCPBufSizeSize = sizeof(curTCPBufSize);
		OSGetSocketOpt(m_pRtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize);

//		printf("002---001-SocketLoop m_pRtmp->m_sb.sb_socket = %d, bufsize = %d", m_pRtmp->m_sb.sb_socket, curTCPBufSize);

        int ret = select(FD_SETSIZE, &readFds, &writeFds, NULL, &timeout);
        if (ret == -1)
        {
            printf("Aborting due to select failure");
            SetStopEvent();
            break;
        }

//		printf("002---002-SocketLoop m_pRtmp->m_sb.sb_socket = %d", m_pRtmp->m_sb.sb_socket);

        if(FD_ISSET(m_pRtmp->m_sb.sb_socket, &writeFds) || (FD_ISSET(m_hBufferEvent[0], &readFds)))
        {
            char data[256];
            read(m_hBufferEvent[0], data, sizeof(data));
            canWrite = true;
        }
#if 0
        else if (FD_ISSET(m_pRtmp->m_sb.sb_socket, &readFds))
        {
            BYTE discard[16384];
            int ret, errorCode;
            bool fatalError = FALSE;

            for (;;)
            {
                ret = recv(m_pRtmp->m_sb.sb_socket, (char *)discard, sizeof(discard), 0);
                if (ret == -1)
                {
                    if (errno == EWOULDBLOCK)
                        break;

                    fatalError = TRUE;
                }
                else if (ret == 0)
                {
                    errorCode = 0;
                    fatalError = TRUE;
                }

                if (fatalError)
                {
                    printf("Socket error, recv() returned %d, GetLastError() %d", ret, errorCode);
                    FatalSocketShutdown();
                    break;
                }
            }

        }
#endif
		else
		{
            continue;
		}
#endif

#ifndef DIRECT_SEND
        if (canWrite)
        {
            bool exitLoop = false;
            do
            {
                OSEnterMutex(m_hDataBufferMutex);

                if (!m_nCurDataBufferLen)
                {
                    //this is now an expected occasional condition due to use of auto-reset events, we could end up emptying the buffer
                    //as it's filled in a previous loop cycle, especially if using low latency mode.
                    OSLeaveMutex(m_hDataBufferMutex);
                    //printf("Trying to send, but no data available?!"));
                    break;
                }

                int ret;
                if (m_nLowLatencyMode != LL_MODE_NONE)
                {
                    int sendLength = MIN(latencyPacketSize, m_nCurDataBufferLen);
                    //ret = send(m_pRtmp->m_sb.sb_socket, (const char *)m_pDataBuffer, sendLength, 0);
                    ret = SendNoBlock(m_pRtmp->m_sb.sb_socket, (const char *)m_pDataBuffer, sendLength);
                }
                else
                {
                    //ret = send(m_pRtmp->m_sb.sb_socket, (const char *)m_pDataBuffer, m_nCurDataBufferLen, 0);
                    ret = SendNoBlock(m_pRtmp->m_sb.sb_socket, (const char *)m_pDataBuffer, m_nCurDataBufferLen);
                }

//				if(bcount++%60==0)
//					printf("SendNoBlock ret = %d", ret);

                if (ret > 0)
                {
                    if (m_nCurDataBufferLen - ret)
                        memmove(m_pDataBuffer, m_pDataBuffer + ret, m_nCurDataBufferLen - ret);
                    m_nCurDataBufferLen -= ret;

                    m_nBytesSent += ret;

                    if (lastSendTime)
                    {
                        DWORD diff = OSGetTimeNow() - lastSendTime;

                        if (diff >= 1500)
                            printf("Stalled for %u ms to write %d bytes (buffer: %d / %d), unstable connection?", diff, ret, m_nCurDataBufferLen, m_nDataBufferSize);

                        m_nTotalSendPeriod += diff;
                        m_nTotalSendBytes += ret;
                        m_nTotalSendCount++;
                    }

                    lastSendTime = OSGetTimeNow();
                    if (g_sendLogSwitch)
                        printf("Send buffer data, send time:%u, send len:%u", lastSendTime, ret);

					if(bcount++%300==0)
						printf("Send buffer data, send time:%u, send len:%u", lastSendTime, ret);
					
 #ifdef _WIN32
                    SetEvent(m_hBufferSpaceAvailableEvent);
 #else
                    OSReleaseSemCond(m_hBufferSpaceAvailableEvent);
 #endif
                }
                else
                {
                    int errorCode;
                    bool fatalError = FALSE;

					printf("xx SendNoBlock ret = %d", ret);

                    if (ret == -1)
                    {
#ifdef _WIN32
                        errorCode = OSGetSocketError();
                        if (errorCode == WSAEWOULDBLOCK)
#else
                        if (errno == EWOULDBLOCK)
#endif
                        {
                            canWrite = false;
                            printf("return WSAEWOULDBLOCK ERROR");
                            OSLeaveMutex(m_hDataBufferMutex);
                            break;
                        }

                        fatalError = TRUE;
                    }
                    else if (ret == 0)
                    {
                        errorCode = 0;
                        fatalError = TRUE;
                    }

                    if (fatalError)
                    {
                    	InvokeCallBack(RTMP_SEND_CONNECTION_BROKEN);
                        //connection closed, or connection was aborted / socket closed / etc, that's a fatal error for us.
                        printf("Socket error, send() returned %d, GetLastError() %d", ret, errorCode);
                        OSLeaveMutex(m_hDataBufferMutex);

                        FatalSocketShutdown();
						printf("SocketThread退出");
                        return;
                    }
                }

                //finish writing for now
                if (m_nCurDataBufferLen <= 1000)
                    exitLoop = true;

                OSLeaveMutex(m_hDataBufferMutex);

                if (delayTime)
                    OSSleep(delayTime);
            } while (!exitLoop);
        }
#endif

    }

	printf("SocketThread退出");
}

void RTMPPublisher::SendLoop()
{
	printf("SendThread 启动, ThreadID:%u", syscall(SYS_gettid));
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    while (WaitForSingleObject(m_hSendSempahore, INFINITE) == WAIT_OBJECT_0)
#else
    //pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    while (OSWaitSemCond(m_hSendSempahore, -1) == 0)
#endif
    {
        while (!m_bStopping)
        {
            OSEnterMutex(m_hDataMutex);
            if (m_listDataPacket.size() == 0)
            {
                OSLeaveMutex(m_hDataMutex);
                break;
            }

            NetworkPacket packetData = m_listDataPacket.front();
            m_listDataPacket.pop_front();
            OSLeaveMutex(m_hDataMutex);

            RTMPPacket packet;
            packet.m_nChannel = (packetData.type == PacketType_Audio) ? 0x5 : 0x4;
            packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
            packet.m_packetType = (packetData.type == PacketType_Audio) ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
            packet.m_nTimeStamp = packetData.timestamp;
            packet.m_nInfoField2 = m_pRtmp->m_stream_id;
            packet.m_hasAbsTimestamp = TRUE;

            packet.m_body = (char *)packetData.data+RTMP_MAX_HEADER_SIZE;
            packet.m_nBodySize = packetData.len - RTMP_MAX_HEADER_SIZE;
            packet.m_nInfoField2 = m_pRtmp->m_stream_id;

            if (g_sendLogSwitch)
                printf("Send packet to buffer,  type:%u, timestamp:%u, body size:%u",
                    packet.m_packetType, packetData.timestamp, packet.m_nBodySize);

            //QWORD sendTimeStart = OSGetTimeMicroseconds();
            if (!RTMP_SendPacket(m_pRtmp, &packet, FALSE))
            {
                //should never reach here with the new shutdown sequence.
                if (!RTMP_IsConnected(m_pRtmp))
                {
                    free(packetData.data);
                    SetStopEvent();
                    break;
                }
            }
            free(packetData.data);
        }

#ifdef _WIN32
        if (m_bStopping && WaitForSingleObject(m_hSendLoopExit, 0) == WAIT_OBJECT_0)
#else
        if (m_bStopping && OSWaitSemCond(m_hSendLoopExit, 0) == 0)
#endif
        {
            printf("RTMPPublisher::SendLoop:m_bStopping and m_hSendLoopExit is true");
            break;
        }

    }

	printf("SendThread退出");
}
void RTMPPublisher::MonitorLoop()
{
	printf("Monitor线程启动, ThreadID:%u", syscall(SYS_gettid));
    while (m_bRun)
    {
#ifdef _WIN32
        int status = WaitForSingleObject(m_hStopEvent, INFINITE);
        if (status == WAIT_ABANDONED || status == WAIT_FAILED)
#else
        int status = OSWaitSemCond(m_hStopEvent, -1);
        if (status != 0)
#endif
        {
            printf("WaitForSingleObject failure");
            OSSleep(100);
            continue;
        }

        printf("Wait a Stop Event");
        Stop();
    }
	printf("Monitor线程退出");
}

void RTMPPublisher::DropFrame(UINT id)
{
    list<NetworkPacket>::iterator it = m_listDataPacket.begin();
    advance(it, id);

    NetworkPacket dropPacket = *it;
    m_nCurrentBufferSize -= dropPacket.len;
    PacketType type = dropPacket.type;
    free(dropPacket.data);
    if (dropPacket.type < PacketType_VideoHigh)
    {
        m_nNumBFramesDumped++;
    }
    else
    {
        m_nNumPFramesDumped++;
    }

    m_listDataPacket.erase(it++);
    for (UINT i = id + 1; it != m_listDataPacket.end(); i++, ++it)
    {
        UINT distance = (i - id);
        if (it->distanceFromDroppedFrame <= distance)
            break;

        it->distanceFromDroppedFrame = distance;
    }

    list<NetworkPacket>::reverse_iterator rIt = m_listDataPacket.rbegin();
    advance(rIt, m_listDataPacket.size() - id);
    for (int i = int(id) - 1; rIt != m_listDataPacket.rend(); i--, ++rIt)
    {
        UINT distance = (id - UINT(i));
        if (rIt->distanceFromDroppedFrame <= distance)
            break;

        rIt->distanceFromDroppedFrame = distance;
    }

    bool bSetPriority = true;
    it = m_listDataPacket.begin();
    advance(it, id);
    for (; it != m_listDataPacket.end(); )
    {
        NetworkPacket &packet = *it;
        if (packet.type < PacketType_Audio)
        {
            if (type >= PacketType_VideoHigh)
            {
                if (packet.type < PacketType_VideoHighest)
                {
                    m_nCurrentBufferSize -= packet.len;
                    free(packet.data);
                    if (packet.type < PacketType_VideoHigh)
                    {
                        m_nNumBFramesDumped++;

                    }
                    else
                    {
                        m_nNumPFramesDumped++;
                    }
                    m_listDataPacket.erase(it++);
                    continue;
                }
                else
                {
                    bSetPriority = false;
                    break;
                }
            }
            else
            {
                if (packet.type >= type)
                {
                    bSetPriority = false;
                    break;
                }

            }
        }
        ++it;
    }

    if (bSetPriority)
    {
        if (type >= PacketType_VideoHigh)
            m_nPacketWaitType = PacketType_VideoHighest;
        else
        {
            if (m_nPacketWaitType < type)
                m_nPacketWaitType = type;
        }
    }
}

void RTMPPublisher::DoIFrameDelaySimple()
{
    list<NetworkPacket>::iterator it = m_listDataPacket.begin();
    m_nMinFramedropTimestsamp = m_listDataPacket.back().timestamp;
    for (; it != m_listDataPacket.end(); )
    {
        NetworkPacket packet = *it;
        if (packet.type == PacketType_VideoHigh)
        {
            free(it->data);
            m_listDataPacket.erase(it++);
			printf("Drop all the p Frame");
        }
        else
            ++it;
    }
    m_nPacketWaitType = PacketType_VideoHighest;
}

//video packet count exceeding maximum.  find lowest priority frame to dump
bool RTMPPublisher::DoIFrameDelay(bool bBFramesOnly)
{
    int curWaitType = PacketType_VideoDisposable;

    while ((!bBFramesOnly && curWaitType < PacketType_VideoHighest) ||
            (bBFramesOnly && curWaitType < PacketType_VideoHigh))
    {
        UINT bestPacket = INVALID;
        UINT bestPacketDistance = 0;

        if (curWaitType == PacketType_VideoHigh)
        {
            bool bFoundIFrame = false;
            list<NetworkPacket>::reverse_iterator rIt = m_listDataPacket.rbegin();
            int i = m_listDataPacket.size() - 1;
            for (; rIt != m_listDataPacket.rend(); ++rIt, --i)
            {
                NetworkPacket &packet = *rIt;
                if (packet.type == PacketType_Audio)
                    continue;

                if (packet.type == curWaitType)
                {
                    if (bFoundIFrame)
                    {
                        bestPacket = i;
                        break;
                    }
                    else if (bestPacket == INVALID)
                    {
                        bestPacket = i;
                    }
                }
                else if (packet.type == PacketType_VideoHighest)
                    bFoundIFrame = true;
            }
        }
        else
        {
            list<NetworkPacket>::iterator it = m_listDataPacket.begin();
            int i = 0;
            for (; it != m_listDataPacket.end(); ++it, ++i)
            {
                NetworkPacket &packet = *it;
                if (packet.type <= curWaitType)
                {
                    if (packet.distanceFromDroppedFrame > bestPacketDistance)
                    {
                        bestPacket = i;
                        bestPacketDistance = packet.distanceFromDroppedFrame;
                    }
                }
            }
        }

        if (bestPacket != INVALID)
        {
            DropFrame(bestPacket);
            return true;
        }

        curWaitType++;
    }

    return false;
}

void RTMPPublisher::InitEncoderData()
{
    char *enc = m_pMetaDataPacketBuffer + RTMP_MAX_HEADER_SIZE;
    char *pend = m_pMetaDataPacketBuffer + sizeof(m_pMetaDataPacketBuffer);
    enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
    enc = AMF_EncodeString(enc, pend, &av_onMetaData);
    *enc++ = AMF_OBJECT;

//#ifndef _WIN32
    enc = AMF_EncodeNamedNumber(enc, pend, &av_duration, 0.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_fileSize, 0.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_width, double(m_metaData.width));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_height, double(m_metaData.heigth));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_videocodecid, 7);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_videodatarate, double(m_metaData.videoDataRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_framerate, double(m_metaData.frameRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiocodecid, 10);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiodatarate, double(m_metaData.audioDataRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplerate, double(m_metaData.sampleRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplesize, m_metaData.sampleSize);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiochannels, double(m_metaData.channel));
    enc = AMF_EncodeNamedBoolean(enc, pend, &av_stereo, 1);
//#endif
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;
    m_nMetaDataPacketBufferLen = enc - m_pMetaDataPacketBuffer;
}

void RTMPPublisher::BeginPublishingInternal()
{
    // send metadata
    RTMPPacket packet;
    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INFO;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;
    packet.m_hasAbsTimestamp = TRUE;
    packet.m_body = m_pMetaDataPacketBuffer + RTMP_MAX_HEADER_SIZE;

    packet.m_nBodySize = m_nMetaDataPacketBufferLen - RTMP_MAX_HEADER_SIZE;
    if (!RTMP_SendPacket(m_pRtmp, &packet, FALSE))
    {
        printf("send metadata failure.\n");
        return;
    }
    printf("send metadata.\n");

    // send audio header
    char sampleRateNo;
    char sampleFreqInedx = 4;
    switch (m_nSampleRate)
    {
    case 5500:
        sampleRateNo = 0;
        break;
    case 11025:
        sampleRateNo = 1;
        sampleFreqInedx = 10;
        break;
    case 22050:
        sampleRateNo = 2;
        sampleFreqInedx = 7;
        break;
    case 32000:
        sampleRateNo = 3;
        //sampleFreqInedx = 5;
        sampleFreqInedx = 8;
        break;
    case 44100:
        sampleRateNo = 3;
        sampleFreqInedx = 4;
        break;
    case 48000:
        sampleRateNo = 3;
        sampleFreqInedx = 3;
        break;
    default:
        sampleRateNo = 0;
        sampleFreqInedx = 0;
    }
    char soundFormat = 10;
    m_nAudioTagHeader = (soundFormat << 4) | (sampleRateNo << 2) | 0x03;

    //char* data = (char*)malloc(RTMP_MAX_HEADER_SIZE + 4);
	char data[32];
    packet.m_nChannel = 0x05; // source channel
    packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet.m_body = data + RTMP_MAX_HEADER_SIZE;
    packet.m_nBodySize = 4;
    int i = 0;
    packet.m_body[i++] = m_nAudioTagHeader;
    packet.m_body[i++] = 0;
    packet.m_body[i++] = 2 << 3 | sampleFreqInedx >> 1;
    packet.m_body[i++] = sampleFreqInedx << 7 | m_nChannelNum << 3;
    if (!RTMP_SendPacket(m_pRtmp, &packet, FALSE))
    {
        printf("send audio seq header failure.\n");
		//free(data);
        return;
    }
	//free(data);
    printf("send audio seq header.\n");
}

void RTMPPublisher::GetFirstFrameFromBuffer(char* buf, UINT bufLen, char** frame, UINT* frameLen)
{
    *frame = NULL;
    if (bufLen < 4)
        return;

    int i = 0;
    while (i <= bufLen-4)
    {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x00 && buf[i + 3] == 0x01)
        {
            *frame = &buf[i + 4];
            i += 4;
            break;
        }
        else if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x01)
        {
            *frame = &buf[i + 3];
            i += 3;
            break;
        }
        else
            i++;
    }

    while (i <= bufLen-4)
    {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x00 && buf[i + 3] == 0x01)
        {
            *frameLen = &buf[i] - *frame;
            return;
        }
        else if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x01)
        {
            *frameLen = &buf[i] - *frame;
            return;
        }
        else
            ++i;
    }
    *frameLen = buf + bufLen - *frame;
    return;
}

void RTMPPublisher::GetIFrameFromBuffer(char* buf, UINT bufLen, char** frame, UINT* frameLen)
{
    int i = 0;
    char* p = buf;
    int len = bufLen;
    while (i < 3)
    {
        GetFirstFrameFromBuffer(p, len, frame, frameLen);
        if (*frame == NULL)
            return;

        len = p + len - *frame - *frameLen;
        p = *frame + *frameLen;
        i++;
    }
}

int RTMPPublisher::EncRTMPNetworkPacket(char* nalu, UINT naluLen, UINT naluType, bool bAudio,
    UINT timestamp, UINT composeTime, NetworkPacket& packet)
{
    int i = 0, j=0;
    char* frame;
    UINT frameLen;
    packet.timestamp = timestamp;
    if (!bAudio)
    {
        if (naluType)
        {
            if (!m_bEncVideoSeqHeader)
            {
                if (!ReadOneNaluFromBuf_GetFrame((unsigned char*)nalu, naluLen, (unsigned char**)&frame, (int*)&frameLen, NALU_SPS))
                {
                    printf("ReadOneNaluFromBuf_GetFrame SPS failure\n");
                    return false;
                }

                i += RTMP_MAX_HEADER_SIZE;
                m_pVideoSeqHeader[i++] = 0x17;
                m_pVideoSeqHeader[i++] = 0x00;
                m_pVideoSeqHeader[i++] = 0x00;
                m_pVideoSeqHeader[i++] = 0x00;
                m_pVideoSeqHeader[i++] = 0x00;
                /*AVCDecoderConfigurationRecord*/
                m_pVideoSeqHeader[i++] = 0x01;
                m_pVideoSeqHeader[i++] = frame[1];
                m_pVideoSeqHeader[i++] = frame[2];
                m_pVideoSeqHeader[i++] = frame[3];
                m_pVideoSeqHeader[i++] = 0xff;

                /*sps*/
                m_pVideoSeqHeader[i++] = 0xe1;
                m_pVideoSeqHeader[i++] = (frameLen >> 8) & 0xff;
                m_pVideoSeqHeader[i++] = frameLen & 0xff;
                memcpy(&m_pVideoSeqHeader[i], frame, frameLen);
                m_nVideoSeqHeaderLen = i + frameLen;

                m_pIFrameHeader[j++] = frameLen >> 24 & 0xff;
                m_pIFrameHeader[j++] = frameLen >> 16 & 0xff;
                m_pIFrameHeader[j++] = frameLen >> 8 & 0xff;
                m_pIFrameHeader[j++] = frameLen & 0xff;
                memcpy(&m_pIFrameHeader[j], frame, frameLen);
                j += frameLen;

                // pps
                if (!ReadOneNaluFromBuf_GetFrame((unsigned char*)nalu, naluLen, (unsigned char**)&frame, (int*)&frameLen, NALU_PPS))
                {
                    printf("ReadOneNaluFromBuf_GetFrame PPS failure\n");
                    return false;
                }

                m_pVideoSeqHeader[m_nVideoSeqHeaderLen++] = 0x01;
                m_pVideoSeqHeader[m_nVideoSeqHeaderLen++] = (frameLen >> 8) & 0xff;
                m_pVideoSeqHeader[m_nVideoSeqHeaderLen++] = (frameLen)& 0xff;
                memcpy(&m_pVideoSeqHeader[m_nVideoSeqHeaderLen], frame, frameLen);

                m_nVideoSeqHeaderLen += frameLen;
                m_bEncVideoSeqHeader = true;

                m_pIFrameHeader[j++] = frameLen >> 24 & 0xff;
                m_pIFrameHeader[j++] = frameLen >> 16 & 0xff;
                m_pIFrameHeader[j++] = frameLen >> 8 & 0xff;
                m_pIFrameHeader[j++] = frameLen & 0xff;
                memcpy(&m_pIFrameHeader[j], frame, frameLen);
                j += frameLen;
                m_nIFrameHeaderLen = j;

                RTMPPacket rpacket;
                rpacket.m_nChannel = 0x04; // source channel
                rpacket.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
                rpacket.m_packetType = RTMP_PACKET_TYPE_VIDEO;
                rpacket.m_body = (char*)m_pVideoSeqHeader + RTMP_MAX_HEADER_SIZE;
                rpacket.m_nBodySize = m_nVideoSeqHeaderLen - RTMP_MAX_HEADER_SIZE;
                rpacket.m_nTimeStamp = timestamp;
                rpacket.m_hasAbsTimestamp = true;
                if (!RTMP_SendPacket(m_pRtmp, &rpacket, FALSE))
                {
                    printf("Send sps pps faulure\n");
                    return false;
                }
                m_bEncVideoSeqHeader = true;
                printf("Send video seq header.\n");
            }

            if (!ReadOneNaluFromBuf_GetFrame((unsigned char*)nalu, naluLen, (unsigned char**)&frame, (int*)&frameLen, NALU_I_FRAME))
            {
                printf("ReadOneNaluFromBuf_GetFrame I failure\n");
                return false;
            }

            // I frame
            i = 0;
            packet.type = PacketType_VideoHighest;
            packet.len = frameLen + 9 + RTMP_MAX_HEADER_SIZE + m_nIFrameHeaderLen;
            packet.data = (char*)malloc(packet.len);
            i += RTMP_MAX_HEADER_SIZE;
            packet.data[i++] = 0x17;// 1:Iframe  7:AVC
            packet.data[i++] = 0x01;// AVC NALU
            packet.data[i++] = composeTime >> 16 &0xff;
            packet.data[i++] = composeTime >> 8 & 0xff;
            packet.data[i++] = composeTime >> 0 & 0xff;

            memcpy(&packet.data[i], m_pIFrameHeader, m_nIFrameHeaderLen);
            i += m_nIFrameHeaderLen;

            // NALU size
            packet.data[i++] = frameLen >> 24 & 0xff;
            packet.data[i++] = frameLen >> 16 & 0xff;
            packet.data[i++] = frameLen >> 8 & 0xff;
            packet.data[i++] = frameLen & 0xff;
            memcpy(&packet.data[i], frame, frameLen);
        }
        else
        {
            //P frame
            if (!ReadOneNaluFromBuf_GetFrame((unsigned char*)nalu, naluLen, (unsigned char**)&frame, (int*)&frameLen, NALU_PB_FRAME))
            {
                printf("ReadOneNaluFromBuf_GetFrame PB failure\n");
                return false;
            }

            packet.type = PacketType_VideoHigh;
            packet.len = frameLen + 9 + RTMP_MAX_HEADER_SIZE;
            packet.data = (char*)malloc(packet.len);
            i += RTMP_MAX_HEADER_SIZE;
            packet.data[i++] = 0x27;// 2:Pframe  7:AVC
            packet.data[i++] = 0x01;// AVC NALU
            packet.data[i++] = composeTime >> 16 & 0xff;
            packet.data[i++] = composeTime >> 8 & 0xff;
            packet.data[i++] = composeTime >> 0 & 0xff;

            // NALU size
            packet.data[i++] = frameLen >> 24 & 0xff;
            packet.data[i++] = frameLen >> 16 & 0xff;
            packet.data[i++] = frameLen >> 8 & 0xff;
            packet.data[i++] = frameLen & 0xff;
            memcpy(&packet.data[i], frame, frameLen);
        }

    }
    else
    {
        packet.type = PacketType_Audio;
        packet.len = naluLen + 2 - 7 + RTMP_MAX_HEADER_SIZE;
        packet.data = (char*)malloc(packet.len);
        i += RTMP_MAX_HEADER_SIZE;
        packet.data[i++] = m_nAudioTagHeader;
        packet.data[i++] = 1;
        memcpy(&packet.data[i], nalu + 7, naluLen - 7);
    }
    return true;
}

int RTMPPublisher::SendPacket(char* frame, UINT frameLen, UINT frameType, bool bAudio, UINT timestamp, UINT composeTime)
{
	UINT timeNow = OSGetTimeNow();

	//! 閲嶈繛闂撮殧1s
	if (!m_bConnected && !m_bConnecting && !m_bStopping && (timeNow - m_nPrevConnectTime) > m_nConnectIntervalTime /*&& m_nErrorCount < m_nPubMaxErrorLimit*/)
    {
    	m_nPrevConnectTime = OSGetTimeNow();
    	printf("m_nErrorCount:%d", m_nErrorCount);
//        m_hConnectionThread = OSCreateThread((TASK_ENTRY_FUNC)CreateConnectionThread, this);
		m_connectionThread.createThread((TASK_ENTRY_FUNC) CreateConnectionThread, this);
    }

    if (!m_bConnected)
    {
        if (g_sendLogSwitch)
            printf("status is wait to connect or connecting....");
        return 0;
    }

    if (m_bFirstKeyframe)
    {
        if (bAudio || !frameType)
        {
            if (g_sendLogSwitch)
                printf("finding first I frame....");
            return 0;
        }

        initVideoTimestamp = timestamp;
        m_bFirstKeyframe = false;
    }

    if (!m_bStreamStarted && !m_bStopping)
    {
        //  encode metadata
        InitEncoderData();

        // send onMetaData and audio sequence header
        BeginPublishingInternal();
        m_bStreamStarted = true;
    }

    if (!bAudio)
        m_nTotalVideoFrames++;

    int offsetTimeStamp = timestamp - initVideoTimestamp;
    if (offsetTimeStamp < 0)
    {
		printf("drop offsetTimeStamp negviate packet");
        return 0;
    }

    if(g_sendLogSwitch)
        printf("frameLen:%u, frameType:%u, bAudio:%u, timestamp:%u, composeTime:%u",
            frameLen, frameType, bAudio, timestamp, composeTime);

	if(acount++%300==0)
		printf("frameLen:%u, frameType:%u, bAudio:%u, timestamp:%u, composeTime:%u",
            frameLen, frameType, bAudio, timestamp, composeTime);

    NetworkPacket packet;
    if (!EncRTMPNetworkPacket(frame, frameLen, frameType, bAudio, offsetTimeStamp, composeTime, packet))
    {
        printf("EncRTMPNetworkPacket failure");
        return -1;
    }


#ifdef DIRECT_SEND
    RTMPPacket rPacket;
    rPacket.m_nChannel = (packet.type == PacketType_Audio) ? 0x5 : 0x4;
    rPacket.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    rPacket.m_packetType = (packet.type == PacketType_Audio) ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
    rPacket.m_nTimeStamp = packet.timestamp;
    rPacket.m_nInfoField2 = m_pRtmp->m_stream_id;
    rPacket.m_hasAbsTimestamp = TRUE;

    rPacket.m_body = (char *)packet.data + RTMP_MAX_HEADER_SIZE;
    rPacket.m_nBodySize = packet.len - RTMP_MAX_HEADER_SIZE;
    rPacket.m_nInfoField2 = m_pRtmp->m_stream_id;

    if (g_sendLogSwitch)
        printf("Direct send packet, type:%u, timestamp:%u, body size:%u",
        rPacket.m_packetType, rPacket.m_nTimeStamp, rPacket.m_nBodySize);

	if(acount++%300==0)
		printf("Direct send packet, type:%u, timestamp:%u, body size:%u",
        	rPacket.m_packetType, rPacket.m_nTimeStamp, rPacket.m_nBodySize);

    if (!RTMP_SendPacket(m_pRtmp, &rPacket, FALSE))
    {
        //should never reach here with the new shutdown sequence.
        if (!RTMP_IsConnected(m_pRtmp))
        {
            SetStopEvent();
        }
    }
    free(packet.data);
    return 0;
#endif

    OSEnterMutex(m_hDataMutex);

    // drop frame process
    if (!m_listDataPacket.empty() && m_nMinFramedropTimestsamp < m_listDataPacket.front().timestamp)
    {/*
        UINT duration = m_listDataPacket.front().timestamp - m_listDataPacket.back().timestamp;
        DWORD curTime = timeGetTime();
        if (duration > m_nDropThreshold)
        {
            m_nMinFramedropTimestsamp = m_listDataPacket.back().timestamp;
            while (DoIFrameDelay(false));
        }
        else if (duration >= m_nBFrameDropThreshold && curTime - m_nLastBFrameDropTime >= m_nDropThreshold)
        {
            while (DoIFrameDelay(true));
            m_nLastBFrameDropTime = curTime;
        }*/

        UINT duration = m_listDataPacket.back().timestamp - m_listDataPacket.front().timestamp;
        DWORD curTime = OSGetTimeNow();
        if (duration > m_nDropThreshold)
        {
            m_nMinFramedropTimestsamp = m_listDataPacket.back().timestamp;
            DoIFrameDelaySimple();
        }
    }

    bool bAddPacket = false;
    if (packet.type >= m_nPacketWaitType)
    {
        if (packet.type != PacketType_Audio)
            m_nPacketWaitType = PacketType_VideoDisposable;

        bAddPacket = true;
    }

    if (bAddPacket)
    {
        list<NetworkPacket>::reverse_iterator rit = m_listDataPacket.rbegin();
        for (; rit != m_listDataPacket.rend(); ++rit)
        {
            if (packet.timestamp > rit->timestamp)
            {
                m_listDataPacket.insert(rit.base(), packet);
                break;
            }
        }

        if (rit == m_listDataPacket.rend())
        {
            m_listDataPacket.push_front(packet);
        }
        m_nCurrentBufferSize += packet.len;
    }
    else
    {
        if (packet.type < PacketType_VideoHigh)
        {

            m_nNumBFramesDumped++;
        }
        else
        {
            m_nNumPFramesDumped++;
        }
        free(packet.data);
    }

    if (m_listDataPacket.size())
    {
#ifdef _WIN32
        if (m_hSendSempahore)
            OSReleaseSemaphore(m_hSendSempahore);
#else
        if(m_hSendSempahore)
            OSReleaseSemCond(m_hSendSempahore);
#endif
    }
    OSLeaveMutex(m_hDataMutex);

    return 0;
}

void RTMPPublisher::SetStopEvent()
{
#ifdef WIN32
        SetEvent(m_hStopEvent);
#else
        OSReleaseSemCond(m_hStopEvent);
#endif
}

void RTMPPublisher::SetMetaData(RtmpMetaData* data)
{
    m_metaData = *data;
}

void RTMPPublisher::InvokeCallBack(RTMP_SEND_EVENT_TYPE event)
{
    if (m_nEventStatus != event)
    {
        m_pFun(m_pArgv, event);
		if(event != RTMP_SEND_CONNECTED_SUCCESS)
		{
			m_nErrorCount++;
		}
    }
}

int RTMPPublisher::GetSendNetworkWidth()
{
	if (m_nTotalSendPeriod == 0)
	{
		return 0;
	}

	return m_nTotalSendBytes*8 / m_nTotalSendPeriod;
}

double RTMPPublisher::GetDropFramePercentage()
{
	double dPFrameDropPercentage = double(m_nNumPFramesDumped + m_nNumBFramesDumped) / MAX(1, m_nTotalVideoFrames)*100.0;
	return dPFrameDropPercentage;
}

void RTMPPublisher::SetConnectIntervalTime(int ms, int error)
{
	m_nConnectIntervalTime = ms;
	m_nPubMaxErrorLimit = error;
}

unsigned int RTMPPublisher::GetPublishTime()
{
	return m_nPublishTime;
}

void RTMPPublisher::InitTest()
{
    if (!m_bConnected && !m_bConnecting && !m_bStopping)
    {
//        m_hConnectionThread = OSCreateThread((TASK_ENTRY_FUNC)CreateConnectionThread, this);
		m_connectionThread.createThread((TASK_ENTRY_FUNC) CreateConnectionThread, this);
    }

    while (!m_bConnected);
}

int timeSlap = 0;
void RTMPPublisher::TestSendPacket(char* frame, UINT frameLen, UINT frameType, bool bAudio, UINT timestamp)
{
    NetworkPacket dataPacket;
    if (!EncRTMPNetworkPacket(frame, frameLen, frameType, bAudio, timestamp, 0, dataPacket))
        return;

    RTMPPacket packet;
    packet.m_nChannel = (dataPacket.type == PacketType_Audio) ? 0x5 : 0x4;
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    if (dataPacket.type == PacketType_VideoHigh || dataPacket.type == PacketType_VideoHighest)
        packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
    else
        packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet.m_nTimeStamp = timeSlap;
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;
    packet.m_hasAbsTimestamp = 0;

    packet.m_body = (char *)dataPacket.data + RTMP_MAX_HEADER_SIZE;
    packet.m_nBodySize = dataPacket.len - RTMP_MAX_HEADER_SIZE;
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;
    timeSlap += 40;

    //QWORD sendTimeStart = OSGetTimeMicroseconds();
    if (!RTMP_SendPacket(m_pRtmp, &packet, FALSE))
    {
        printf("RTMP_SendPacket error\n");
    }
}

