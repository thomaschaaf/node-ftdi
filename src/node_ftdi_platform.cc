#include <stdlib.h>
#include <stdio.h>
#include <uv.h>

#ifndef WIN32
#include <pthread.h>
#else
#endif

#include "node_ftdi_platform.h"



#define EVENT_MASK (FT_EVENT_RXCHAR)


/**
 * Linux / Mac Functions
 */
#ifndef WIN32

void Platform_SetVidPid(DWORD vid, DWORD pid)
{
    FT_SetVIDPID(vid, pid);
}

void PrepareAsyncRead(ReadBaton *baton)
{
    FT_STATUS ftStatus;

    pthread_mutex_init(&(baton->eh).eMutex, NULL);
    pthread_cond_init(&(baton->eh).eCondVar, NULL);
    ftStatus = FT_SetEventNotification(baton->ftHandle, EVENT_MASK, (PVOID)&(baton->eh));
}

void ReadDataAsync(uv_work_t* req)
{
    ReadBaton* data = static_cast<ReadBaton*>(req->data);

    FT_STATUS ftStatus;
    DWORD RxBytes;
    DWORD BytesReceived;

    printf("Waiting for data\r\n");
    
    pthread_mutex_lock(&(data->eh).eMutex);
    pthread_cond_wait(&(data->eh).eCondVar, &(data->eh).eMutex);
    pthread_mutex_unlock(&(data->eh).eMutex);

    FT_GetQueueStatus(data->ftHandle, &RxBytes);
    printf("Status [RX: %d]\r\n", RxBytes);

    if(RxBytes > 0)
    {
        data->readData = (uint8_t *)malloc(RxBytes);

        ftStatus = FT_Read(data->ftHandle, data->readData, RxBytes, &BytesReceived);
        if (ftStatus != FT_OK) 
        {
            fprintf(stderr, "Can't read from ftdi device: %d\n", ftStatus);
        }
        data->bufferLength = BytesReceived;
    }
}

#else
void Platform_SetVidPid(DWORD vid, DWORD pid)
{
    // Not supported on Windows
}

void PrepareAsyncRead(ReadBaton *baton)
{    
    FT_STATUS ftStatus;
    
    baton->hEvent = CreateEvent(NULL, false /* auto-reset event */, false /* non-signalled state */, "");
    ftStatus = FT_SetEventNotification(baton->ftHandle, EVENT_MASK, baton->hEvent);
}

void ReadDataAsync(uv_work_t* req)
{

}
#endif