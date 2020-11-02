#include "log.h"
#include "rtmp.h"
#include <stdio.h>
#include <stdlib.h>

FILE *netstackdump;
FILE *netstackdump_read;
int main(int argc, char *argv[])
{
	RTMP *rtmp = RTMP_Alloc();
	RTMP_Init(rtmp);
	if (!RTMP_SetupURL(rtmp, "rtmp://192.168.8.249/live/mystream"))
	{
		printf("setup failure\n");
	}
	 RTMP_EnableWrite(rtmp); 
	
	int reTry = 20;
	while(reTry)
	{
	 	if (!RTMP_Connect(rtmp, NULL))
		{
			printf("connect failure\n");
			reTry--;
	int nSleepTime = 1000;
    struct timespec ts, rem;

    ts.tv_sec = nSleepTime / 1000;
    ts.tv_nsec = (nSleepTime % 1000) * 1000000;
    nanosleep(&ts, &rem);
	//		sleep(100);
			continue;
		}
	}

	if (!RTMP_ConnectStream(rtmp, 0))
	{
		printf("connect stream failure\n");
		
	}
	
	return 0;
}
