#include <ftd2xx.h>
#include <pthread.h> 
#include <time.h>
#include <unistd.h>
#include <stdio.h>

#define WAIT_TIME_MILLISECONDS      250

#define NANOSECS_PER_SECOND         1000000000
#define NANOSECS_PER_MILISECOND     1000000
#define MILISECS_PER_SECOND         1000

typedef struct 
{
	FT_HANDLE handle;
	int vid;
	int pid;
	char* serial;
	int baud;

	pthread_t thread;
	EVENT_HANDLE rxEvent;
} DeviceParams_t;


DeviceParams_t deviceList[] = {
	{
		0,
		0x27f4,
		0x0203,
		(char*)"FTVTIXBE",
		115200,
		0,
		0,
	},
	{
		0,
		0x27f4,
		0x0203,
		(char*)"FTVTIXI5",
		115200,
		0,
		0,
	},
};

FT_STATUS OpenDevice(DeviceParams_t* device);
void* ReadData(void* ptr);
void WaitForData(DeviceParams_t* device);


FT_STATUS OpenDevice(DeviceParams_t* device)
{
	FT_STATUS status;
	FT_SetVIDPID(device->vid, device->pid);
	status = FT_OpenEx((PVOID) device->serial, FT_OPEN_BY_SERIAL_NUMBER, &(device->handle));
	
	if(status == FT_OK)
	{
		status = FT_SetDataCharacteristics(device->handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
	}

	if(status == FT_OK)
	{
		status = FT_SetBaudRate(device->handle, device->baud);
	}

	if(status == FT_OK)
	{
		pthread_mutex_init(&device->rxEvent.eMutex, NULL);
    	pthread_cond_init(&device->rxEvent.eCondVar, NULL);
    	
    	status = FT_SetEventNotification(device->handle, FT_EVENT_RXCHAR, (PVOID) &(device->rxEvent));
    
		pthread_create(&(device->thread), NULL, ReadData, (void*)device);
	}

	printf ("Open state [%x]\r\n", status);
	return status;
}


void* ReadData(void* ptr)
{
	DeviceParams_t* device = (DeviceParams_t*)ptr;
	DWORD RxBytes;
	FT_STATUS status;

	printf("Thread Started\r\n");
	
	while(true)
	{
		WaitForData(device);

		status = FT_GetQueueStatus(device->handle, &RxBytes);
		if(status != FT_OK)
        {
            fprintf(stderr, "Can't read from ftdi device: %d\n", status);
            return NULL;
        }

        printf("RX %s [%d]\r\n", device->serial, RxBytes);
        if(RxBytes > 0)
        {
        	DWORD BytesReceived;
        	char* data = new char[RxBytes];
        	
        	FT_Read(device->handle, data, RxBytes, &BytesReceived);
			
			fprintf(stderr, "%d Bytes Read\n", BytesReceived);
			delete data;
        }
	}

	return NULL;
} 


void WaitForData(DeviceParams_t* device)
{
	struct timespec ts;
    struct timeval tp;
    
    gettimeofday(&tp, NULL);

    int additionalSeconds = 0;
    int additionalMilisecs = 0;
    additionalSeconds = (WAIT_TIME_MILLISECONDS  / MILISECS_PER_SECOND);
    additionalMilisecs = (WAIT_TIME_MILLISECONDS % MILISECS_PER_SECOND);

    long additionalNanosec = tp.tv_usec * 1000 + additionalMilisecs * NANOSECS_PER_MILISECOND;
    additionalSeconds += (additionalNanosec / NANOSECS_PER_SECOND);
    additionalNanosec = (additionalNanosec % NANOSECS_PER_SECOND);

    /* Convert from timeval to timespec */
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = additionalNanosec;
    ts.tv_sec += additionalSeconds;

    pthread_mutex_lock(&device->rxEvent.eMutex);
    pthread_cond_timedwait(&device->rxEvent.eCondVar, &device->rxEvent.eMutex, &ts);
    pthread_mutex_unlock(&device->rxEvent.eMutex);
}


int main (int argc, char **argv) 
{
	printf("Open Device 1\r\n");
	OpenDevice(&deviceList[0]);
	sleep(3);
	printf("Open Device 2\r\n");
	OpenDevice(&deviceList[1]);

	while(true)
	{
		sleep(1);
	}
	pthread_join(deviceList[0].thread, NULL);
}