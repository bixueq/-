#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include "YCatLog.h"
#endif
#include "nalu.h"
#include "util.h"

int naluHeader = 0x01000000;

int WriteNALU(FILE* fp, char* buf, int bufLen)
{
    if (fwrite(&naluHeader, 1, sizeof(naluHeader), fp) < 0)
        return -1;

    if (fwrite(buf, 1, bufLen, fp) < 0)
        return -1;
    return 0;
}

int adts_sample_rates_1[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0 };
int FindAdtsSRIndex(int sr)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (sr == adts_sample_rates_1[i])
            return i;
    }
    return 16 - 1;
}

int MakeAdtsHeader(unsigned char * buf, int samplerate, int channels, int iFrameLen)
{
    int profile = 1;
    int sr_index = FindAdtsSRIndex(samplerate);
    int skip = 7;
    int framesize = skip + iFrameLen;

    if (!buf || iFrameLen <= 0)
        return 0;
    memset(buf, 0, 7);
    buf[0] += 0xFF; /* 8b: syncword */

    buf[1] += 0xF0; /* 4b: syncword */
    /* 1b: mpeg id = 0 */
    /* 2b: layer = 0 */
    buf[1] += 1; /* 1b: protection absent */

    buf[2] += ((profile << 6) & 0xC0); /* 2b: profile */
    buf[2] += ((sr_index << 2) & 0x3C); /* 4b: sampling_frequency_index */
    /* 1b: private = 0 */
    buf[2] += ((channels >> 2) & 0x1); /* 1b: channel_configuration */

    buf[3] += ((channels << 6) & 0xC0); /* 2b: channel_configuration */
    /* 1b: original */
    /* 1b: home */
    /* 1b: copyright_id */
    /* 1b: copyright_id_start */
    buf[3] += ((framesize >> 11) & 0x3); /* 2b: aac_frame_length */

    buf[4] += ((framesize >> 3) & 0xFF); /* 8b: aac_frame_length */

    buf[5] += ((framesize << 5) & 0xE0); /* 3b: aac_frame_length */
    buf[5] += ((0x7FF >> 6) & 0x1F); /* 5b: adts_buffer_fullness */

    buf[6] += ((0x7FF << 2) & 0x3F); /* 6b: adts_buffer_fullness */
    /* 2b: num_raw_data_blocks */

    return 7;
}

bool IsSpsPpsExist(char* buf)
{
    if ((buf[4] & 0x1f) == 7)
        return true;
    else
        return false;
}

void GetRawDataInfo(char* buf, unsigned int* offset, unsigned int* rawDataLen)
{
    char* pStr = buf;
    int spsLen = ntohl(*((int*)pStr));
    pStr += 4;  // sps len
    pStr += spsLen; // sps body
    int ppsLen = ntohl(*((int*)pStr));
    pStr += 4; // pps len
    pStr += ppsLen; // pps body
    *rawDataLen = ntohl(*((int*)pStr));
    pStr += 4; // raw data len
    *offset = pStr - buf;
}

unsigned int GetHasSpsPpsIFrameLen(char* buf)
{
    char* pStr = buf;
    int spsLen = ntohl(*((int*)pStr));
    pStr += 4;
    pStr += spsLen;
    int ppsLen = ntohl(*((int*)pStr));
    pStr += 4;
    pStr += ppsLen;
    int rawLen = ntohl(*((int*)pStr));
    pStr += 4;
    pStr += rawLen;
    return pStr - buf;
}

void GetFirstFrameFromBuffer(char* buf, unsigned int bufLen, char** frame, unsigned int* frameLen)
{
    *frame = NULL;
    if (bufLen < 4)
        return;

    int i = 0;
    while (i <= bufLen - 4)
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

    while (i <= bufLen - 4)
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

void GetIFrameFromBuffer(char* buf, unsigned int bufLen, char** frame, unsigned int* frameLen)
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

/*
bool ReadOneNaluFromBuf_GetFrame(unsigned char *buf, int len, unsigned char **pframe, int *framelen, int type)
{
    //4个标识情况
    int i = 0, frametype, i_ref_idc, framesize;
    int num_index = 0;
    while (i<len - 4)
    {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x00 && buf[i + 3] == 0x01)
        {
            i += 4;
            int pos = i;
            bool bFind = false;
            while (pos<len - 4)
            {
                if (buf[pos++] == 0x00 && buf[pos++] == 0x00 && buf[pos++] == 0x00 && buf[pos++] == 0x01)
                {
                    bFind = true;
                    break;
                }
            }

            frametype = buf[i] & 0x1f;            
            if (frametype == type)
            {
                if (bFind)
                    framesize = (pos - 4) - i;
                else
                {
                    framesize = len - i;
                }
                   

                *pframe = &buf[i];
                *framelen = framesize;
                unsigned char* pTest = *pframe + framesize;
                return true;
            }

        }
        i++;

    }
    return false;
}*/

bool ReadOneNaluFromBuf_GetFrame(unsigned char *buf, int len, unsigned char **pframe, int *framelen, int type)
{
    //4个标识情况
    int i = 0, j = 0, frametype, nalutype, i_ref_idc, framesize;
    int num_index = 0;
    while (i<len - 4)
    {
        if ((buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x00 && buf[i + 3] == 0x01) ||
            (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x01))
        {
            if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x00 && buf[i + 3] == 0x01)
                j = 4;
            if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x01)
                j = 3;

            i += j;
            frametype = buf[i] & 0x1f;
            switch (type)
            {
                case NALU_SPS:
                    nalutype = 7;
                    break;
                case NALU_PPS:
                    nalutype = 8;
                    break;
                case NALU_SEI:
                    nalutype = 6;
                    break;
                case NALU_I_FRAME:
                    nalutype = 5;
                    break;
                case NALU_PB_FRAME:
                    break;
                default:
                    return false;
            }

            if (type == NALU_PB_FRAME) // SPS,PPS,SEI,I 必须满足frametype == type， P,B满足frametype不等于5,6,7,8,9
            {
				if ((frametype == 5) || (frametype == 6) || (frametype == 7) || (frametype == 8) || (frametype == 9))
                {
                    i++;
                    continue;
                }
            }
            else
            {
                if (frametype != nalutype)
                {
                    i++;
                    continue;
                }
            }

            int pos = i;
            bool bFind = false;
            while (pos<len - 4)
            {
                if ((buf[pos] == 0x00 && buf[pos + 1] == 0x00 && buf[pos + 2] == 0x00 && buf[pos + 3] == 0x01) ||
                    (buf[pos] == 0x00 && buf[pos + 1] == 0x00 && buf[pos + 2] == 0x01))
                {
                    if (buf[pos] == 0x00 && buf[pos + 1] == 0x00 && buf[pos + 2] == 0x00 && buf[pos + 3] == 0x01)
                        j = 4;
                    if (buf[pos] == 0x00 && buf[pos + 1] == 0x00 && buf[pos + 2] == 0x01)
                        j = 3;

                    pos +=j;
                    bFind = true;
                    break;
                }
                pos++;
            }

            if (bFind)
                framesize = (pos - j) - i;
            else
            {
                framesize = len - i;
            }


            *pframe = &buf[i];
            *framelen = framesize;
            unsigned char* pTest = *pframe + framesize;
            return true;
        }
        i++;

    }
    return false;
}

int RemoveSEI(unsigned char *buf, int len)
{
    int i = 0;
    while (i < len-2)
    {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x01)
        {
            break;
        }
        i++;
    }

    if (i == len - 2)
    {
        return 0;
    }
        
    return i+3;
}