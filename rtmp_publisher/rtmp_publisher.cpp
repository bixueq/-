#include "rtmp_publisher.h"
#include "RTMPPublisher.h"

EXPORT_INT InitRTMPPublisher(RTMPStreamInfo* rtmpInfo, RTMPSendDataCallBack pFun, void* pArgv)
{
    return !(RTMPPublisher::Instance()->Init(rtmpInfo->sampleRate, rtmpInfo->channelNum, 
        rtmpInfo->strRTMPUrl, pFun, pArgv));
}

EXPORT_VOID DestroyRTMPPublisher()
{
    RTMPPublisher::Destory();
}

EXPORT_INT SendRTMPPacket(FrameInfo* frameInfo)
{
    return RTMPPublisher::Instance()->SendPacket(frameInfo->frame, frameInfo->frameLen,
        frameInfo->bKeyFrame, frameInfo->bAudio, frameInfo->rtmpts, frameInfo->pts - frameInfo->dts);
}

EXPORT_VOID SetRTMPMetaData(RtmpMetaData* metaData)
{
    RTMPPublisher::Instance()->SetMetaData(metaData);
}

EXPORT_INT GetSendNetworkWidth()
{
	return RTMPPublisher::Instance()->GetSendNetworkWidth();
}

EXPORT_DOUBLE GetDropFramePercentage()
{
	return RTMPPublisher::Instance()->GetDropFramePercentage();
}

EXPORT_VOID SetConnectIntervalTime(int ms, int error)
{
	RTMPPublisher::Instance()->SetConnectIntervalTime(ms, error);
}

unsigned int GetPublishTime()
{
	// 此时间不准，弃用
	return RTMPPublisher::Instance()->GetPublishTime();
}