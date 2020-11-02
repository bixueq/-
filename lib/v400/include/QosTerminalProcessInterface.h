#pragma once


// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 QOSTERMINALPROCESS_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何其他项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// QOSTERMINALPROCESS_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef WIN32
#ifdef QOSTERMINALPROCESS_EXPORTS
#define QOSTERMINALPROCESS_API __declspec(dllexport)
#else
#define QOSTERMINALPROCESS_API __declspec(dllimport)
#endif

#else
#define QOSTERMINALPROCESS_API
#endif

//dataType 用于确定回调数据类型
//0 音视频数据
//1 udp穿透包数据 
//当 dataType 时，使用以下结构体填充 data 字段
/*
struct UDP_penetration
{
	char ip[64];
	unsigned short port;
	int	extern_len;
	char extern_data[64];

};
*/

typedef void(*InterfaceCallbackFunc)(void*thiz, unsigned char* data, unsigned int nLen, unsigned int unPTS,int dataType);

class CQosTerminalProcess;
// 此类是从 QosTerminalProcess.dll 导出的
class QOSTERMINALPROCESS_API CQosTerminalProcessInterface {
public:
	CQosTerminalProcessInterface();

	~CQosTerminalProcessInterface();

	static bool InitAll(bool haveLog, const char* outputPath);//该接口 为全局接口不作为对象接口

	//设置是否使用内部时间截 ,内部默认开启内部时间截
	void SetupUseInternalTimestamp(bool bUseInternalTimestamp);
	//设置处理模块，该接口不能多次调用
	// eRedunMethod 冗余度模式，自动或者固定,0自动，1固定
	// unRedunRatio 冗余度 当 eRedunMethod 为固定时，该值为有效值
	// unFecGroupSize 分组大小，底层使用 建议根据视频分辨率和带宽来定，分辨率、带宽越高GROUP应该设置得越大
	// unJittBuffTime 接收数据时，本地延时往外抛数据的延时时间。
	// bEnableNACK 是否开启NACK模块，该模块会导致数据延时。
	// DataCallback 数据回调函数，所接收到的远端数据通过该函数指针往 库外部抛出数据
	// DataCallbackParams DataCallback 中的 thiz 参数
	// bUseInternalTimestamp 是否使用内部时间截，为true 时 SendStreamData 的 unDts 参数为无效参数。
	// bNeedAddReadHead 在DataCallback 回调前是否需要添加上 私有reach协议头 
	// isAudio 标示该路流是否是音频流。
	bool SetupProcessModule(InterfaceCallbackFunc DataCallback, void*DataCallbackParams,bool isAudio,
		unsigned int eRedunMethod = 0, unsigned int unRedunRatio = 30, unsigned int unFecGroupSize = 28, unsigned int unJittBuffTime = 0,
		bool bEnableNACK = false, bool bNeedAddReachHead = false);

	//设置本地端的网络参数，该接口不能多次调用
	bool SetupLocalNetworkModule(const char *strLocalIP, unsigned short shLocalPort);
	//设置远端的网络参数。
	bool SetupRemoteNeworkModule(const char *strRemoteIP, unsigned short shRemotePort);

	//数据发送模式，默认为使用FUA nalu包
	//当bUseFUA 为true 时，SendStreamData 发送的为一nalu 数据，长度不能超过900字节。需要调用层将一帧数据进行拆分后再调用发送接口。并且在接收到数据的时候需要自行将数据组合成帧再使用。
	//当 bUseFUA 为false 时，SendStreamData 发送的为一帧数据。内部负责将数据切分为多个nalu 再进行发送，并且会接收端抛出数据时会抛出一帧数据。
	//当 bUseFUA 为false 时，fps参数有效，为视频帧率。
	//fps 用于发送平滑，可以比实际帧率大，但不能比实际帧率小
	bool SetupInputDataStyle(bool bUseFUA, unsigned int fps);

	//是否需要远端发送过来的穿透包信息回调给上层应用，默认为不允许
	//不允许时，将会自动将穿透包中的信息应用到远端网络模块中
	//bool SetNeedUploadRemoteParams(bool need = true);

	//发送一帧视频码流，必须附带H264起始码
	//发送一帧音频码流，建议为一帧ADTS封装包
	bool SendStreamData(unsigned int unLen, unsigned char* buf, unsigned int unDts = 0);

	void Clear(void);

public:
	//动态参数查询接口

	//UDP通讯统计数据获取
	//获得通道上下行丢包率
	void GetUpDownLostRatio(float &fUpLostRatio, float &fDownLostRatio);
	//获得音视频上下行码率
	void GetUpDownBitrate(float &fUpRate, float &fDownRate);
	//获得网络往返时延
	unsigned int GetNetWorkDelay(void);
	//获得接收流的FEC配置情况（对端FEC的配置情况）
	bool GetFecParamsForRemote(unsigned int &byGroupSize, unsigned int &byRedunSize);

	//获取当前的运行远端地址信息
	void GetCurrentRemoteParams(char* strRemoteIP, unsigned short&shRemotePort);

	//获取当前的运行本端地址信息
	void GetCurrentLocalParams(char* strLocalIP, unsigned short&shLocalPort);

	void GetUserSetParams(char* strLocalIP, unsigned short&shLocalPort, char* strRemoteIP, unsigned short&shRemotePort);//获取用户设置的参数


private:
	void CreateQosTerminalProcessObj(void);

	void DeleteQosTerminalProcessObj(void);

	CQosTerminalProcess* pQosTerminalProcess = 0x0;

	unsigned int	bAudio = 0;
	// TODO:  在此添加您的方法。
};