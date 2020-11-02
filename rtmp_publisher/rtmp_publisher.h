#ifndef _RTMP_SENDER_H_
#define _RTMP_SENDER_H_

typedef struct
{
    unsigned int frateRate;
    unsigned int width;
    unsigned int hight;
    unsigned int sampleRate;
    unsigned int channelNum;
    char strRTMPUrl[256];
}RTMPStreamInfo;

typedef struct
{
    char* frame;
    unsigned int frameLen;
    unsigned int bAudio;
    unsigned int bKeyFrame;
    unsigned int pts;
    unsigned int dts;
    unsigned int rtmpts;
}FrameInfo;

typedef struct
{
    unsigned int width;
    unsigned int heigth;
    unsigned int videoDataRate;
    unsigned int frameRate;
    unsigned int audioDataRate;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channel;
}RtmpMetaData;

enum RTMP_SEND_EVENT_TYPE {

    RTMP_SEND_CONNECTED_SUCCESS = 1,
	RTMP_SEND_START_CONNECT,
	RTMP_SEND_URL_PARSE_FAILURE,
	RTMP_SEND_CONNECTED_FAILURE,
	RTMP_SEND_CONNECTED_INVALID_STREAM,
    RTMP_SEND_CONNECTION_BROKEN,
};


typedef void(*RTMPSendDataCallBack)(void* pArgv, RTMP_SEND_EVENT_TYPE evnt);

#ifdef _WIN32
#define EXPORT_INT __declspec(dllexport) int
#define EXPORT_VOID __declspec(dllexport) void
#define EXPORT_DOUBLE __declspec(dllexport) double
#else
#define EXPORT_INT  int
#define EXPORT_VOID  void
#define EXPORT_DOUBLE double
#endif

EXPORT_INT InitRTMPPublisher(RTMPStreamInfo* rtmpInfo, RTMPSendDataCallBack pFun, void* pArgv);

EXPORT_VOID DestroyRTMPPublisher();

EXPORT_INT SendRTMPPacket(FrameInfo* frameInfo);

EXPORT_VOID SetRTMPMetaData(RtmpMetaData* metaData);

EXPORT_INT GetSendNetworkWidth();

EXPORT_DOUBLE GetDropFramePercentage();

EXPORT_VOID SetConnectIntervalTime(int ms, int error);

unsigned int GetPublishTime();

#endif