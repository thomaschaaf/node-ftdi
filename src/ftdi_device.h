#ifndef FTDI_DEVICE_H
#define FTDI_DEVICE_H

#include <v8.h>
#include <node.h>
#include <node_object_wrap.h>
#include <ftd2xx.h>

#include "nan.h"

using namespace v8;
using namespace node;

namespace ftdi_device
{

typedef struct
{
  uint8_t* data;
  DWORD length;
} ReadBaton_t;

typedef struct
{
  uint8_t* data;
  DWORD length;
} WriteBaton_t;

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
  USHORT fc;
  UCHAR bitMode;
  UCHAR bitMask;
  bool hasBitSettings;
} DeviceParams_t;

typedef enum
{
  DeviceState_Idle,
  DeviceState_Open,
  DeviceState_Closing
} DeviceState_t;

class FtdiDevice : public Nan::ObjectWrap
{
  public:
    static void Initialize(Handle<Object> target);

    FtdiDevice();
    ~FtdiDevice();

    static NAN_METHOD(New);
    static NAN_METHOD(Open);
    static NAN_METHOD(Write);
    static NAN_METHOD(Close);

    static FT_STATUS OpenAsync(FtdiDevice* device, Nan::Callback *callback_read);
    static FT_STATUS ReadDataAsync(FtdiDevice* device, ReadBaton_t* baton);
    static FT_STATUS WriteAsync(FtdiDevice* device, WriteBaton_t* baton);
    static FT_STATUS CloseAsync(FtdiDevice* device);

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

inline void AsyncExecuteCompletePersistent (uv_work_t* req) {
  Nan::AsyncWorker* worker = static_cast<Nan::AsyncWorker*>(req->data);
  worker->WorkComplete();
}

inline void AsyncQueueWorkerPersistent (Nan::AsyncWorker* worker) {
  uv_queue_work(
      uv_default_loop()
    , &worker->request
    , Nan::AsyncExecute
    , (uv_after_work_cb)AsyncExecuteCompletePersistent
  );
}
#endif
