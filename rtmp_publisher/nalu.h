#ifndef _NALU_H_
#define _NALU_H_

#define NALU_SPS 1
#define NALU_PPS 2
#define NALU_SEI 3
#define NALU_I_FRAME 4
#define NALU_PB_FRAME 5 
#define NALU_HEADER 6

int WriteNALU(FILE* fp, char* buf, int bufLen);

int MakeAdtsHeader(unsigned char * buf, int samplerate, int channels, int iFrameLen);

bool IsSpsPpsExist(char* buf);

void GetRawDataInfo(char* buf, unsigned int* offset, unsigned int* rawDataLen);

unsigned int GetHasSpsPpsIFrameLen(char* buf);

bool ReadOneNaluFromBuf_GetFrame(unsigned char *buf, int len, unsigned char **pframe, int *framelen, int type);

int RemoveSEI(unsigned char *buf, int len);

#endif

