#include <uv.h>
#include <stdlib.h>
#include <stdio.h>

#include "node_ftdi_platform.h"

#ifndef WIN32
#include <pthread.h>

#else

#endif

#ifndef WIN32
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
oid ReadDataAsync(uv_work_t* req)
{

}
#endif