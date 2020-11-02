//
// Created by Night on 2020/8/26.
//

#pragma once

#include <cstdio>
#include <cstdint>
#include <string>

//#信令监听端口
#define SENDER_COMM_LISTEN_PORT  18002

//#近景码流
#define RECEIVER_CLOSE_SHOT_VIDEO_IN_PORT  19011     	//近景视频流接收端口
#define RECEIVER_CLOSE_SHOT_AUDIO_IN_PORT  19012     	//近景音频流接收端口

#define SENDER_CLOSE_SHOT_VIDEO_OUT_PORT   19013		//近景视频流发送端口
#define SENDER_CLOSE_SHOT_AUDIO_OUT_PORT   19014		//近景视频流发送端口

//#远景码流
#define RECEIVER_LONG_SHOT_VIDEO_IN_PORT    19015    	//远景视频流接收端口
#define RECEIVER_LONG_SHOT_AUDIO_IN_PORT    19016    	//远景音频流接收端口

#define SENDER_LONG_SHOT_VIDEO_OUT_PORT    	19017		//远景视频发送端口
#define SENDER_LONG_SHOT_AUDIO_OUT_PORT    	19018		//远景音频发送端口


#define MSGHEAD_CHECK_START_CODE  '#'
#define MSGHEAD_CHECK_END_CODE    '*'

enum MessageType{
	MsgType_Request = 0x0110,
	MsgType_Ready	  = 0x011f,
	MsgType_Stream,
	MsgType_HeartBeat,
	MsgType_Disconnect,
};

enum StremType{
  StremType_CloseShot = 0,
  StremType_LongShot,
};

struct ServerInfo{
  uint32_t 			deviceType;		   //设备类型
  int      			listenPort;		   //监听端口
  std::string 	   	version;       //版本号
  std::string    	localIp;        //本地ip
};


struct MsgHead {
  uint8_t   	check_start[4];     //#####
  uint32_t  	msgType;       	    //消息类型
  uint32_t 		deviceType;        	//设备类型
  uint32_t  	videoInPort;        //视频接收端口
  uint32_t  	audioInPort;        //音频频接收端口
  char   		requestIp[64];		//請求ip
  uint8_t   	streamTpye;			//碼流類型 近景 远景
  char 			version[100];       //版本号
  uint32_t 		iStreamIdx; 		//从文件中读出帧的流索引，从0开始
  uint32_t 		bAudio;     		//表示此帧是音频还是视频，为1时是音频，否则是视频
  uint32_t 		bKeyFrame;  		//表示此帧是否是关键帧
  int64_t 		timeTick;    		//表示此帧的时间戳
  uint32_t 		iLen;       		//表示帧长
  uint32_t 		channel;			//音频通道
  uint32_t 		samplerate; 		//音频采样率
  uint32_t 		width;				//视频宽度
  uint32_t 		height;				//视频高度
  uint8_t  		reserved[4];        //保留字节
  uint8_t  		check_end[4];       //****
};


typedef struct _MediaPacket
{
	int type;		//0--视频，1--音频
	int iStreamIdx; //从文件中读出帧的流索引，从0开始
	int bAudio;     //表示此帧是音频还是视频，为1时是音频，否则是视频
	int bKeyFrame;  //表示此帧是否是关键帧
	int64_t pts;    //表示此帧的pts,单位是ms
	int64_t dts;
	int iLen;       //表示帧长
	uint8_t* pData;//帧的数据指针
	int channel;	//音频通道
	int samplerate; //音频采样率
	int width;		//视频宽度
	int height;		//视频高度
	void* priv;		//私有数据
}MediaPacket;

static bool MsgHeadCheck(MsgHead *head)
{
	if((head == NULL) || (head->check_start[0] != MSGHEAD_CHECK_START_CODE)
		|| (head->check_start[1] != MSGHEAD_CHECK_START_CODE)
		|| (head->check_start[2] != MSGHEAD_CHECK_START_CODE)
		|| (head->check_start[3] != MSGHEAD_CHECK_START_CODE)
		|| (head->check_end[0] != MSGHEAD_CHECK_END_CODE)
		|| (head->check_end[1] != MSGHEAD_CHECK_END_CODE)
		|| (head->check_end[2] != MSGHEAD_CHECK_END_CODE)
		|| (head->check_end[3] != MSGHEAD_CHECK_END_CODE)) {
		fprintf(stderr,"the head is not head\n");
		fprintf(stderr, "[%c][%c][%c][%c][%c][%c][%c][%c]\n", head->check_start[0], head->check_start[1], head->check_start[2], head->check_start[3],
				   head->check_end[0], head->check_end[1], head->check_end[2], head->check_end[3]);
		return false;
	}
	return true;
};
