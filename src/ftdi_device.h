#ifndef FTDI_DEVICE_H
#define FTDI_DEVICE_H

#include <v8.h>
#include <node.h>
#include <ftd2xx.h>

using namespace v8;
using namespace node;

namespace ftdi_device 
{

typedef enum 
{
    ConnectType_ByIndex,
    ConnectType_BySerial,
    ConnectType_ByDescription,
    ConnectType_ByLocationId,
} ConnectType_t;

typedef struct ConnectionParams_t
{
    ConnectType_t connectType;
    char *connectString;
    int32_t connectId;
    int pid;
    int vid;

    ConnectionParams_t()
    {
        connectString = NULL;
    }

} ConnectionParams_t;

typedef struct 
{
    int baudRate;
    UCHAR wordLength;
    UCHAR stopBits;
    UCHAR parity;
} DeviceParams_t;

typedef enum
{
    DeviceState_Idle,
    DeviceState_Open,
    DeviceState_Closing
} DeviceState_t;

class FtdiDevice : public ObjectWrap 
{
    public:
        static void Initialize(Handle<Object> target);

    protected:
        FtdiDevice();
        ~FtdiDevice();

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Open(const Arguments& args);
        static Handle<Value> Write(const Arguments& args);
        static Handle<Value> Close(const Arguments& args);

        static Handle<Value> ThrowTypeError(std::string message);
        static Handle<Value> ThrowLastError(std::string message);

    private:
        static void ReadDataAsync(uv_work_t* req);
        static void ReadCallback(uv_work_t* req);

        static void OpenAsync(uv_work_t* req);
        static void OpenFinished(uv_work_t* req);

        static void WriteAsync(uv_work_t* req);
        static void WriteFinished(uv_work_t* req);

        static void CloseAsync(uv_work_t* req);
        static void CloseFinished(uv_work_t* req);
        
        void ExtractDeviceSettings(Local<v8::Object> options);
        FT_STATUS SetDeviceSettings();
        FT_STATUS OpenDevice();

        FT_STATUS PrepareAsyncRead();
        void WaitForReadOrCloseEvent();
        void SignalCloseEvent();

        FT_HANDLE ftHandle;
        DeviceParams_t deviceParams;
        ConnectionParams_t connectParams;

        DeviceState_t deviceState;

#ifndef WIN32
        EVENT_HANDLE dataEventHandle;
#else
        HANDLE dataEventHandle;
#endif
        uv_mutex_t closeMutex;
};

}
#endif