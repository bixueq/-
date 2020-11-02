#ifndef _RTMP_PUBLISHER_H_
#define _RTMP_PUBLISHER_H_

#ifdef _WIN32
#include <inttypes.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#endif
#include "util.h"
#include <stdio.h>
#include "log.h"
#include <list>
#include "rtmp.h"
#include "rtmp_publisher.h"
#include "osthread.h"
using namespace std;

enum PacketType
{
    PacketType_VideoDisposable,
    PacketType_VideoLow,
    PacketType_VideoHigh,
    PacketType_VideoHighest,
    PacketType_Audio
};

struct NetworkPacket
{
    char* data;
    UINT len;
    UINT timestamp;
    PacketType type;
    UINT distanceFromDroppedFrame;
};

enum LatencyMode
{
    LL_MODE_NONE = 0,
    LL_MODE_FIXED,
    LL_MODE_AUTO,
};

static const AVal av_setDataFrame = AVC((char*)"@setDataFrame");
static const AVal av_onMetaData = AVC((char*)"onMetaData");
static const AVal av_duration = AVC((char*)"duration");
static const AVal av_fileSize = AVC((char*)"fileSize");
static const AVal av_width = AVC((char*)"width");
static const AVal av_height = AVC((char*)"height");
static const AVal av_framerate = AVC((char*)"framerate");
static const AVal av_videocodecid = AVC((char*)"videocodecid");
static const AVal av_videodatarate = AVC((char*)"videodatarate");
static const AVal av_audiocodecid = AVC((char*)"audiocodecid");
static const AVal av_audiodatarate = AVC((char*)"audiodatarate");
static const AVal av_audiosamplerate = AVC((char*)"audiosamplerate");
static const AVal av_audiosamplesize = AVC((char*)"audiosamplesize");
static const AVal av_audiochannels = AVC((char*)"audiochannels");
static const AVal av_stereo = AVC((char*)"stereo");
static const AVal av_encoder = AVC((char*)"encoder");

class RTMPPublisher
{
public:
    static RTMPPublisher* Instance();
    static void Destory();
    virtual ~RTMPPublisher();
    int Init(UINT sampleRate, UINT channelNum, char* strURL, RTMPSendDataCallBack pFun, void* pArgv);
    int Start(UINT tcpBufferSize);
    void Stop();
    int SendPacket(char* frame, UINT frameLen, UINT frameType, bool bAudio, UINT timestamp, UINT composeTime);
    void SetMetaData(RtmpMetaData* data);
    void SetStopEvent();
    void InitTest();
    void InvokeCallBack(RTMP_SEND_EVENT_TYPE event);
    void TestSendPacket(char* frame, UINT frameLen, UINT frameType, bool bAudio, UINT timestamp);
	int GetSendNetworkWidth();
	double GetDropFramePercentage();
	void SetConnectIntervalTime(int ms, int error);
	unsigned int GetPublishTime();
private:
    RTMPPublisher();
    void SendLoop();
    void SocketLoop();
    void MonitorLoop();
    void SetupSendBacklogEvent();
    void FatalSocketShutdown();
    static int BufferedSend(RTMPSockBuf *sb, const char *buf, int len, RTMPPublisher *network);
    static int CreateConnectionThread(RTMPPublisher *publisher);
    static int SendThread(RTMPPublisher *publisher);
    static int SocketThread(RTMPPublisher *publisher);
    static int MonitorThread(RTMPPublisher *publisher);
    void librtmpErrorCallback(int level, const char *format, va_list vl);
    bool DoIFrameDelay(bool bBFramesOnly);
    void DropFrame(UINT id);
    void BeginPublishingInternal();
    void InitEncoderData();
    int EncRTMPNetworkPacket(char* frame, UINT frameLen, UINT frameType, bool bAudio, UINT timestamp, UINT composeTime, NetworkPacket& packet);
    void GetFirstFrameFromBuffer(char* buf, UINT bufLen, char** frame, UINT* frameLen);
    void GetIFrameFromBuffer(char* buf, UINT bufLen, char** frame, UINT* frameLen);
    void DoIFrameDelaySimple();
	int SendNoBlock(int socket_fd, const char*send_buffer, int size);

private:
    static RTMPPublisher* g_pInst;

    // status
    bool m_bStopping;
    bool m_bStreamStarted;
    bool m_bConnecting;		// 注意不要在 CreateConnectionThread 线程外部进行写操作
    bool m_bConnected;

	UINT m_nPrevConnectTime;

    bool m_bRun;

    // thread
//    TASK_ID m_hSendThread;
//    TASK_ID m_hSocketThread;
//    TASK_ID m_hConnectionThread;
//    TASK_ID m_hMonitorThread;
	//! 数据发送线程句柄
	osthread m_sendThread;

	//! socket属性线程句柄
	osthread m_socketThread;

	//! 重连线程句柄
	osthread m_connectionThread;

	//! 监控线程句柄
	osthread m_monitorThread;
	bool m_bRunConnectionThread;

    // sempahore and event
#ifdef _WIN32
    SEM_ID m_hSendSempahore;
    HANDLE m_hWriteEvent;
    HANDLE m_hBufferEvent;
    HANDLE m_hBufferSpaceAvailableEvent;
    HANDLE m_hSendBacklogEvent;
    OVERLAPPED m_sendBacklogOverlapped;
    HANDLE m_hSendLoopExit;
    HANDLE m_hSocketLoopExit;
    HANDLE m_hStopEvent;
#else
    CONDSEM_ID m_hSendSempahore;
    int m_hBufferEvent[2];
    CONDSEM_ID m_hBufferSpaceAvailableEvent;
    CONDSEM_ID m_hSendLoopExit;
    CONDSEM_ID m_hSocketLoopExit;
    CONDSEM_ID m_hStopEvent;
#endif    

    // mutex
    MUTEX_ID m_hDataMutex;
    MUTEX_ID m_hDataBufferMutex;
    MUTEX_ID m_hRTMPMutex;

	UINT m_nPublishTime = 0;		// 推送时间

    // data buffer
    // queue
    list<NetworkPacket> m_listDataPacket;
    UINT m_nCurrentBufferSize;
    // send buffer
    char* m_pDataBuffer;
    UINT m_nDataBufferSize;
    UINT m_nCurDataBufferLen;
    // send buffer parameter
    LatencyMode m_nLowLatencyMode;
    UINT m_nLatencyFactor;
    
    // stat
    UINT m_nTotalVideoFrames;
    UINT m_nNumBFramesDumped;
    UINT m_nNumPFramesDumped;
    UINT m_nTotalTimesWaited;
    UINT m_nTotalBytesWaited;
    unsigned long long m_nTotalSendBytes;
    unsigned long long m_nBytesSent;
    unsigned long m_nTotalSendPeriod;
    unsigned long m_nTotalSendCount;

    // rtmp data
    RTMP* m_pRtmp;
    static char g_strURL[256];
    static char g_strRTMPErrors[1024];
    char m_pMetaDataPacketBuffer[2048];
    UINT m_nMetaDataPacketBufferLen;
    char m_pVideoSeqHeader[2048];
    UINT m_nVideoSeqHeaderLen;
    bool m_bEncVideoSeqHeader;
    char m_nAudioTagHeader;
    bool m_bFirstKeyframe;
    char m_pIFrameHeader[2048];
    UINT m_nIFrameHeaderLen;

    // drop parameter
    UINT m_nPacketWaitType;
    UINT m_nBFrameDropThreshold;
    UINT m_nDropThreshold;
    UINT m_nMinFramedropTimestsamp;
    DWORD m_nLastBFrameDropTime;

    // video and audio patameter
    UINT m_nSampleRate;
    UINT m_nChannelNum;

    RTMPSendDataCallBack m_pFun;
    void* m_pArgv;

    RtmpMetaData m_metaData;
    UINT m_nEventStatus;

	int m_nConnectIntervalTime;
	int m_nPubMaxErrorLimit;
	int m_nErrorCount;


};

#endif