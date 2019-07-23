#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <queue>
#include <uv.h>
#include <node_buffer.h>

#include "ftdi_constants.h"
#include "ftdi_device.h"
#include "ftdi_driver.h"

#ifndef WIN32
    #include <unistd.h>
    #include <sys/time.h>
#else
    #include <windows.h>
#endif

using namespace std;
using namespace v8;
using namespace node;
using namespace ftdi_device;


/**********************************
 * Local defines
 **********************************/
#define EVENT_MASK (FT_EVENT_RXCHAR)
#define WAIT_TIME_MILLISECONDS      250

#define NANOSECS_PER_SECOND         1000000000
#define NANOSECS_PER_MILISECOND     1000000
#define MILISECS_PER_SECOND         1000

#define JS_CLASS_NAME               "FtdiDevice"
#define JS_WRITE_FUNCTION           "write"
#define JS_OPEN_FUNCTION            "open"
#define JS_CLOSE_FUNCTION           "close"


/**********************************
 * Local Helper Functions protoypes
 **********************************/
UCHAR GetWordLength(int wordLength);
UCHAR GetStopBits(int stopBits);
UCHAR GetParity(const char* string);

void ToCString(Local<String> val, char ** ptr);


/**********************************
 * Local Variables
 **********************************/


/*****************************
 * CREATION Section
 *****************************/

FtdiDevice::FtdiDevice()
{
  deviceState = DeviceState_Idle;
  uv_mutex_init(&closeMutex);
};

FtdiDevice::~FtdiDevice()
{
  if(connectParams.connectString != NULL)
  {
    delete connectParams.connectString;
  }
};

NAN_METHOD(FtdiDevice::New) {
  Nan::HandleScope scope;

  Local<String> locationId    = Nan::New<String>(DEVICE_LOCATION_ID_TAG).ToLocalChecked();
  Local<String> serial        = Nan::New<String>(DEVICE_SERIAL_NR_TAG).ToLocalChecked();
  Local<String> index         = Nan::New<String>(DEVICE_INDEX_TAG).ToLocalChecked();
  Local<String> description   = Nan::New<String>(DEVICE_DESCRIPTION_TAG).ToLocalChecked();
  Local<String> vid           = Nan::New<String>(DEVICE_VENDOR_ID_TAG).ToLocalChecked();
  Local<String> pid           = Nan::New<String>(DEVICE_PRODUCT_ID_TAG).ToLocalChecked();

  FtdiDevice* object = new FtdiDevice();

  // Check if the argument is an object
  if(info[0]->IsObject())
  {
    Local<Object> obj = info[0]->ToObject();

    /**
     * Determine how to connect to the device, we have the following possibilities:
     *   1) By location id
     *   2) By serial Number
     *   3) By description
     *   4) By index
     * In this order we check for availability of the parameter and the first valid we found is taken
     */
    if(obj->Has(locationId) && obj->Get(locationId)->Int32Value() != 0)
    {
      object->connectParams.connectId = obj->Get(locationId)->Int32Value();
      object->connectParams.connectType = ConnectType_ByLocationId;
    }
    else if(obj->Has(serial) && obj->Get(serial)->ToString()->Length() > 0)
    {
      ToCString(obj->Get(serial)->ToString(), &object->connectParams.connectString);
      object->connectParams.connectType = ConnectType_BySerial;
    }
    else if(obj->Has(description) && obj->Get(description)->ToString()->Length() > 0)
    {
      ToCString(obj->Get(description)->ToString(), &object->connectParams.connectString);
      object->connectParams.connectType = ConnectType_ByDescription;
    }
    else if(obj->Has(index))
    {
      object->connectParams.connectId = obj->Get(index)->Int32Value();
      object->connectParams.connectType = ConnectType_ByIndex;
    }
    object->connectParams.vid = 0;
    if(obj->Has(vid))
    {
      object->connectParams.vid = obj->Get(vid)->Int32Value();
    }
    object->connectParams.pid = 0;
    if(obj->Has(pid))
    {
      object->connectParams.pid = obj->Get(pid)->Int32Value();
    }
  }
  // if the argument is a number we connect by index to the device
  else if(info[0]->IsNumber())
  {
    object->connectParams.connectId = (int) info[0]->NumberValue();
    object->connectParams.connectType = ConnectType_ByIndex;
  }
  else
  {
    return Nan::ThrowTypeError("new expects a object as argument");
  }

  object->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

/*****************************
 * READ Section
 *****************************/
class ReadWorker : public Nan::AsyncWorker {
 public:
  ReadWorker(Nan::Callback *callback, FtdiDevice* device)
    : Nan::AsyncWorker(callback), device(device) {}
  ~ReadWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    baton = new ReadBaton_t();
    status = FtdiDevice::ReadDataAsync(device, baton);
  }

  void WorkComplete () {
    Nan::HandleScope scope;

    if (ErrorMessage() == NULL)
      HandleOKCallback();
    else
      HandleErrorCallback();
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;

    // Execute the callback in case we have one and we have read some data
    if(status != FT_OK || (baton->length != 0))
    {
      Local<Value> argv[2];

      Local<Object> slowBuffer = Nan::CopyBuffer((char*)baton->data, baton->length).ToLocalChecked();
      Local<Object> globalObj = Nan::GetCurrentContext()->Global();
      Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(Nan::New<String>("Buffer").ToLocalChecked()));
      Handle<Value> constructorArgs[3] = { slowBuffer, Nan::New<Number>(baton->length), Nan::New<Number>(0) };
      Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
      argv[1] = actualBuffer;

      if(status != FT_OK)
      {
        argv[0] = Nan::New<String>(GetStatusString(status)).ToLocalChecked();
      }
      else
      {
        argv[0] = Local<Value>(Nan::Undefined());
      }

      callback->Call(2, argv);
    }

    if(baton->length != 0)
    {
      delete baton->data;
      baton->length = 0;
    }

    // Check if we want close the device
    if(device->deviceState == DeviceState_Closing)
    {
      // Signal that we have stoped reading
      uv_mutex_unlock(&device->closeMutex);
    }
    else
    {
      // Restart reading loop
      AsyncQueueWorkerPersistent(this);
    }

    delete baton;
  };

 private:
  FT_STATUS status;
  FtdiDevice* device;
  ReadBaton_t* baton;
};

FT_STATUS FtdiDevice::ReadDataAsync(FtdiDevice* device, ReadBaton_t* baton)
{
  FT_STATUS ftStatus;
  DWORD RxBytes;
  DWORD BytesReceived;

  // start permanent read loop
  while(1)
  {
    // Wait for data or close event
    device->WaitForReadOrCloseEvent();

    // Check Queue Status
    uv_mutex_lock(&libraryMutex);
    ftStatus = FT_GetQueueStatus(device->ftHandle, &RxBytes);
    uv_mutex_unlock(&libraryMutex);

    if(ftStatus != FT_OK)
    {
      fprintf(stderr, "Can't get queue status from ftdi device: %s\n", error_strings[ftStatus]);
      return ftStatus;
    }

    // Read available data if any
    if(RxBytes > 0)
    {
      baton->data = new uint8_t[RxBytes];

      uv_mutex_lock(&libraryMutex);
      ftStatus = FT_Read(device->ftHandle, baton->data, RxBytes, &BytesReceived);
      uv_mutex_unlock(&libraryMutex);
      if (ftStatus != FT_OK)
      {
        fprintf(stderr, "Can't read from ftdi device: %s\n", error_strings[ftStatus]);
      }

      baton->length = BytesReceived;
      return ftStatus;
    }

    // Check if we are closing the device
    if(device->deviceState == DeviceState_Closing)
    {
      uv_mutex_lock(&libraryMutex);
      FT_Purge(device->ftHandle, FT_PURGE_RX | FT_PURGE_TX);
      uv_mutex_unlock(&libraryMutex);
      return ftStatus;
    }
  }
}


/*****************************
 * OPEN Section
 *****************************/
class OpenWorker : public Nan::AsyncWorker {
 public:
  OpenWorker(Nan::Callback *callback, FtdiDevice* device, Nan::Callback *callback_read)
    : Nan::AsyncWorker(callback), device(device), callback_read(callback_read) {}
  ~OpenWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    status = FtdiDevice::OpenAsync(device, callback_read);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;

    /**
     * If the open process was sucessful, we start the read thread
     * which waits for data events sents from the device.
     */
    if(status == FT_OK)
    {
      status = device->PrepareAsyncRead();
      if(status == FT_OK)
      {
        // Lock the Close mutex (it is needed to signal when async read has stoped reading)
        uv_mutex_lock(&device->closeMutex);

        AsyncQueueWorkerPersistent(new ReadWorker(callback_read, device));
      }
      // In case read thread could not be started, dispose the callback
      else
      {
        fprintf(stderr, "Could not Initialise event notification: %s\n", error_strings[status]);
      }
    }

    // Check for callback function
    if(callback != NULL)
    {
      Local<Value> argv[1];
      if(status != FT_OK)
      {
        argv[0] = Nan::New<String>(GetStatusString(status)).ToLocalChecked();
      }
      else
      {
        argv[0] = Local<Value>(Nan::Undefined());
      }

      callback->Call(1, argv);
    }
  };

 private:
  FT_STATUS status;
  FtdiDevice* device;
  Nan::Callback *callback_read;
};

NAN_METHOD(FtdiDevice::Open) {
  Nan::HandleScope scope;

  Nan::Callback *callback = NULL;
  Nan::Callback *callback_read = NULL;

  // Get Device Object
  FtdiDevice* device = Nan::ObjectWrap::Unwrap<FtdiDevice>(info.This());
  if(device == NULL)
  {
    return Nan::ThrowError("No FtdiDevice object found in Java Script object");
  }

  if (info.Length() != 3)
  {
    return Nan::ThrowError("open() expects three arguments");
  }

  if (!info[0]->IsObject())
  {
    return Nan::ThrowError("open() expects a object as first argument");
  }

  // options
  if(!info[1]->IsFunction())
  {
    return Nan::ThrowError("open() expects a function (openFisnished) as second argument");
  }
  Local<Value> readCallback = info[1];

  // options
  if(!info[2]->IsFunction())
  {
    return Nan::ThrowError("open() expects a function (readFinsihed) as third argument");
  }
  Local<Value> openCallback = info[2];


  // Check if device is not already open or opening
  if(device->deviceState == DeviceState_Open)
  {
    Local<Value> argv[1];
    argv[0] = Nan::New<String>(FT_STATUS_CUSTOM_ALREADY_OPEN).ToLocalChecked();
    callback = new Nan::Callback(info[1].As<v8::Function>());
    callback->Call(1, argv);
  }
  else
  {
    device->deviceState = DeviceState_Open;

    // Extract the connection parameters
    device->ExtractDeviceSettings(info[0]->ToObject());

    callback_read = new Nan::Callback(readCallback.As<v8::Function>());
    callback = new Nan::Callback(openCallback.As<v8::Function>());

    Nan::AsyncQueueWorker(new OpenWorker(callback, device, callback_read));
  }

  return;
}

FT_STATUS FtdiDevice::OpenAsync(FtdiDevice* device, Nan::Callback *callback_read)
{
  FT_STATUS ftStatus;

  ftStatus = device->OpenDevice();
  if (ftStatus != FT_OK)
  {
    fprintf(stderr, "Can't open ftdi device: %s\n", error_strings[ftStatus]);
  }
  else
  {
    ftStatus = device->SetDeviceSettings();
    if (ftStatus != FT_OK)
    {
      fprintf(stderr, "Can't Set DeviceSettings: %s\n", error_strings[ftStatus]);
    }
  }

  return ftStatus;
}

FT_STATUS FtdiDevice::OpenDevice()
{
    FT_STATUS status = FT_OK;

    // For open by Index case
    if(connectParams.connectType == ConnectType_ByIndex)
    {
        uv_mutex_lock(&libraryMutex);
#ifndef WIN32
        // In case of Linux / Mac we have to set the VID PID of the
        // device we want to connect to
        status = FT_SetVIDPID(connectParams.vid, connectParams.pid);
#endif
        if(status == FT_OK)
        {
            status = FT_Open(connectParams.connectId, &ftHandle);
        }
        uv_mutex_unlock(&libraryMutex);
        return status;
    }
    // other cases
    else
    {
        PVOID arg;
        DWORD flags;

        switch(connectParams.connectType)
        {
            case ConnectType_ByDescription:
            {
                arg = (PVOID) connectParams.connectString;
                flags = FT_OPEN_BY_DESCRIPTION;
                // printf("OpenDevice [Flag: %d, Arg: %s]\r\n", flags, (char *)arg);
            }
            break;

            case ConnectType_BySerial:
            {
                arg = (PVOID) connectParams.connectString;
                flags = FT_OPEN_BY_SERIAL_NUMBER;
                // printf("OpenDevice [Flag: %d, Arg: %s]\r\n", flags, (char *)arg);
            }
            break;


            case ConnectType_ByLocationId:
            {
                arg = (PVOID) connectParams.connectId;
                flags = FT_OPEN_BY_LOCATION;
                // printf("OpenDevice [Flag: %d, Arg: %d]\r\n", flags, connectParams.connectId);
            }
            break;

            default:
            {
                return FT_INVALID_PARAMETER;
            }
        }

        uv_mutex_lock(&vidPidMutex);
        uv_mutex_lock(&libraryMutex);

#ifndef WIN32
        // In case of Linux / Mac we have to set the VID PID of the
        // device we want to connect to
        status = FT_SetVIDPID(connectParams.vid, connectParams.pid);
#endif
        if(status == FT_OK)
        {
            status = FT_OpenEx(arg, flags, &ftHandle);
        }
        uv_mutex_unlock(&libraryMutex);
        uv_mutex_unlock(&vidPidMutex);
        return status;
    }

    return FT_INVALID_PARAMETER;
}


/*****************************
 * WRITE Section
 *****************************/
class WriteWorker : public Nan::AsyncWorker {
 public:
  WriteWorker(Nan::Callback *callback, FtdiDevice* device, WriteBaton_t* baton)
    : Nan::AsyncWorker(callback), device(device), baton(baton) {}
  ~WriteWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    status = FtdiDevice::WriteAsync(device, baton);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;

    if(callback != NULL)
    {
      Local<Value> argv[1];
      if(status != FT_OK)
      {
        argv[0] = Nan::New<String>(GetStatusString(status)).ToLocalChecked();
      }
      else
      {
        argv[0] = Local<Value>(Nan::Undefined());
      }

      callback->Call(1, argv);
    }

    delete baton->data;
    delete baton;
  };

 private:
  FT_STATUS status;
  FtdiDevice* device;
  WriteBaton_t* baton;
};

NAN_METHOD(FtdiDevice::Write) {
  Nan::HandleScope scope;

  Nan::Callback *callback = NULL;

  // buffer
  if(!info[0]->IsObject() || !Buffer::HasInstance(info[0]))
  {
    return Nan::ThrowError("First argument must be a buffer");
  }
  Local<Object> buffer = info[0]->ToObject();

  // Obtain Device Object
  FtdiDevice* device = Nan::ObjectWrap::Unwrap<FtdiDevice>(info.This());
  if(device == NULL)
  {
    return Nan::ThrowError("No FtdiDevice object found in Java Script object");
  }

  Local<Value> writeCallback;
  // options
  if(info.Length() > 1 && info[1]->IsFunction())
  {
    writeCallback = info[1];
    callback = new Nan::Callback(writeCallback.As<v8::Function>());
  }

  WriteBaton_t* baton = new WriteBaton_t();
  baton->length = (DWORD)Buffer::Length(buffer);
  baton->data = new uint8_t[baton->length];
  memcpy(baton->data, Buffer::Data(buffer), baton->length);

  Nan::AsyncQueueWorker(new WriteWorker(callback, device, baton));

  return;
}

FT_STATUS FtdiDevice::WriteAsync(FtdiDevice* device, WriteBaton_t* baton)
{
  FT_STATUS ftStatus;
  DWORD bytesWritten;

  uv_mutex_lock(&libraryMutex);
  ftStatus = FT_Write(device->ftHandle, baton->data, baton->length, &bytesWritten);
  uv_mutex_unlock(&libraryMutex);

  return ftStatus;
}

/*****************************
 * CLOSE Section
 *****************************/
class CloseWorker : public Nan::AsyncWorker {
 public:
  CloseWorker(Nan::Callback *callback, FtdiDevice* device)
    : Nan::AsyncWorker(callback), device(device) {}
  ~CloseWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    status = FtdiDevice::CloseAsync(device);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;

    device->deviceState = DeviceState_Idle;

    if(callback != NULL)
    {
      Local<Value> argv[1];
      if(status != FT_OK)
      {
        argv[0] = Nan::New<String>(GetStatusString(status)).ToLocalChecked();
      }
      else
      {
        argv[0] = Local<Value>(Nan::Undefined());
      }

      callback->Call(1, argv);
    }
  };

 private:
  FT_STATUS status;
  FtdiDevice* device;
};

NAN_METHOD(FtdiDevice::Close) {
  Nan::HandleScope scope;

  Nan::Callback *callback = NULL;

  // Obtain Device Object
  FtdiDevice* device = Nan::ObjectWrap::Unwrap<FtdiDevice>(info.This());
  if(device == NULL)
  {
    return Nan::ThrowError("No FtdiDevice object found in Java Script object");
  }

  // Check the device state
  if(device->deviceState != DeviceState_Open)
  {
    // callback
    if(info[0]->IsFunction())
    {
      Local<Value> argv[1];
      argv[0] = Nan::New<String>(FT_STATUS_CUSTOM_ALREADY_CLOSING).ToLocalChecked();
      callback = new Nan::Callback(info[0].As<v8::Function>());
      callback->Call(1, argv);
    }
  }
  else
  {
    // Set Device State
    device->deviceState = DeviceState_Closing;

     // callback
    if(info[0]->IsFunction())
    {
      callback = new Nan::Callback(info[0].As<v8::Function>());
    }

    Nan::AsyncQueueWorker(new CloseWorker(callback, device));
  }

  return;
}

FT_STATUS FtdiDevice::CloseAsync(FtdiDevice* device)
{
  FT_STATUS ftStatus;

  // Send Event for Read Loop
  device->SignalCloseEvent();

  // Wait till read loop finishes
  uv_mutex_lock(&device->closeMutex);
  uv_mutex_unlock(&device->closeMutex);

  // Close the device
  uv_mutex_lock(&libraryMutex);
  ftStatus = FT_Close(device->ftHandle);
  uv_mutex_unlock(&libraryMutex);

  return ftStatus;
}


/*****************************
 * Helper Section
 *****************************/
FT_STATUS FtdiDevice::SetDeviceSettings()
{
  FT_STATUS ftStatus;

  uv_mutex_lock(&libraryMutex);
  ftStatus = FT_SetDataCharacteristics(ftHandle, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
  uv_mutex_unlock(&libraryMutex);
  if (ftStatus != FT_OK)
  {
    fprintf(stderr, "Can't Set FT_SetDataCharacteristics: %s\n", error_strings[ftStatus]);
    return ftStatus;
  }

  uv_mutex_lock(&libraryMutex);
  ftStatus = FT_SetBaudRate(ftHandle, deviceParams.baudRate);
  uv_mutex_unlock(&libraryMutex);
  if (ftStatus != FT_OK)
  {
    fprintf(stderr, "Can't setBaudRate: %s\n", error_strings[ftStatus]);
    return ftStatus;
  }

  if (deviceParams.hasBitSettings == true) {
    uv_mutex_lock(&libraryMutex);
    ftStatus = FT_SetBitMode(ftHandle, deviceParams.bitMask, deviceParams.bitMode);
    uv_mutex_unlock(&libraryMutex);
    if (ftStatus != FT_OK)
    {
      fprintf(stderr, "Can't setBitMode: %s\n", error_strings[ftStatus]);
      return ftStatus;
    }
  }

  // printf("Connection Settings set [Baud: %d, DataBits: %d, StopBits: %d, Parity: %d]\r\n", deviceParams.baudRate, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
  return ftStatus;
}

void FtdiDevice::ExtractDeviceSettings(Local<Object> options)
{
  Nan::EscapableHandleScope scope;
  Local<String> baudrate  = Nan::New<String>(CONNECTION_BAUDRATE_TAG).ToLocalChecked();
  Local<String> databits  = Nan::New<String>(CONNECTION_DATABITS_TAG).ToLocalChecked();
  Local<String> stopbits  = Nan::New<String>(CONNECTION_STOPBITS_TAG).ToLocalChecked();
  Local<String> parity    = Nan::New<String>(CONNECTION_PARITY_TAG).ToLocalChecked();
  Local<String> bitmode   = Nan::New<String>(CONNECTION_BITMODE).ToLocalChecked();
  Local<String> bitmask   = Nan::New<String>(CONNECTION_BITMASK).ToLocalChecked();

  if(options->Has(baudrate))
  {
    deviceParams.baudRate = options->Get(baudrate)->ToInt32()->Int32Value();
  }
  if(options->Has(databits))
  {
    deviceParams.wordLength = GetWordLength(options->Get(databits)->ToInt32()->Int32Value());
  }
  if(options->Has(stopbits))
  {
    deviceParams.stopBits = GetStopBits(options->Get(stopbits)->ToInt32()->Int32Value());
  }
  if(options->Has(parity))
  {
    char* str;
    ToCString(options->Get(parity)->ToString(), &str);
    deviceParams.parity = GetParity(str);
    delete[] str;
  }
  bool hasBitSettings = false;
  deviceParams.bitMode = 0;
  deviceParams.bitMask = 0;

  if(options->Has(bitmode))
  {
      deviceParams.bitMode = options->Get(bitmode)->ToInt32()->Int32Value();
      hasBitSettings = true;
  } else {
      hasBitSettings = false;
  }

  if(hasBitSettings && options->Has(bitmask))
  {
      deviceParams.bitMask = options->Get(bitmask)->ToInt32()->Int32Value();
      hasBitSettings = true;
  }

  deviceParams.hasBitSettings = hasBitSettings;
}

UCHAR GetWordLength(int wordLength)
{
  switch(wordLength)
  {
    case 7:
      return FT_BITS_7;

    case 8:
    default:
      return FT_BITS_8;
  }
}

UCHAR GetStopBits(int stopBits)
{
  switch(stopBits)
  {
    case 1:
      return FT_STOP_BITS_1;

    case 2:
    default:
      return FT_STOP_BITS_2;
  }
}

UCHAR GetParity(const char* string)
{
  if(strcmp(CONNECTION_PARITY_NONE, string) == 0)
  {
    return FT_PARITY_NONE;
  }
  else if(strcmp(CONNECTION_PARITY_ODD, string) == 0)
  {
    return FT_PARITY_ODD;
  }
  else if(strcmp(CONNECTION_PARITY_EVEN, string) == 0)
  {
    return FT_PARITY_EVEN;
  }
  return FT_PARITY_NONE;
}

/**
 *  Generates a new C String. It allocates memory for the new
 *  string. Be sure to free the memory as soon as you dont need
 *  it anymore.
 */
void ToCString(Local<String> val, char ** ptr)
{
  *ptr = new char[val->Utf8Length() + 1];
#if (NODE_MODULE_VERSION > 0x000B)
// Node 0.11+ (0.11.3 and below won't compile with these)
  val->WriteOneByte(reinterpret_cast<uint8_t*>(*ptr), 0, -1, 0);
#else
// Node 0.8 and 0.10
  val->WriteAscii(*ptr, 0, -1, 0);
#endif
}


/*****************************
 * Platform dependet Section
 *****************************/
#ifndef WIN32
FT_STATUS FtdiDevice::PrepareAsyncRead()
{
  FT_STATUS status;
  pthread_mutex_init(&dataEventHandle.eMutex, NULL);
  pthread_cond_init(&dataEventHandle.eCondVar, NULL);
  uv_mutex_lock(&libraryMutex);
  status = FT_SetEventNotification(ftHandle, EVENT_MASK, (PVOID) &dataEventHandle);
  uv_mutex_unlock(&libraryMutex);
  return status;
}

void FtdiDevice::WaitForReadOrCloseEvent()
{
  DWORD rxBytes = 0;

  pthread_mutex_lock(&dataEventHandle.eMutex);

  // Only sleep in case there is nothing to do
  if(FT_GetQueueStatus(ftHandle, &rxBytes) == FT_OK && deviceState != DeviceState_Closing && rxBytes == 0)
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

    pthread_cond_timedwait(&dataEventHandle.eCondVar, &dataEventHandle.eMutex, &ts);
  }
  pthread_mutex_unlock(&dataEventHandle.eMutex);
}

void FtdiDevice::SignalCloseEvent()
{
  pthread_mutex_lock(&dataEventHandle.eMutex);
  pthread_cond_signal(&dataEventHandle.eCondVar);
  pthread_mutex_unlock(&dataEventHandle.eMutex);
}

#else

FT_STATUS FtdiDevice::PrepareAsyncRead()
{
  FT_STATUS status;
  dataEventHandle = CreateEvent(NULL, false /* auto-reset event */, false /* non-signalled state */, "");
  uv_mutex_lock(&libraryMutex);
  status = FT_SetEventNotification(ftHandle, EVENT_MASK, dataEventHandle);
  uv_mutex_unlock(&libraryMutex);
  return status;
}

void FtdiDevice::WaitForReadOrCloseEvent()
{
  WaitForSingleObject(dataEventHandle, WAIT_TIME_MILLISECONDS);
}

void FtdiDevice::SignalCloseEvent()
{
  SetEvent(dataEventHandle);
}
#endif

void FtdiDevice::Initialize(v8::Handle<v8::Object> target)
{
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New<String>(JS_CLASS_NAME).ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(Nan::New<String>(JS_WRITE_FUNCTION).ToLocalChecked(), Nan::New<FunctionTemplate>(Write));
  tpl->PrototypeTemplate()->Set(Nan::New<String>(JS_OPEN_FUNCTION).ToLocalChecked(), Nan::New<FunctionTemplate>(Open));
  tpl->PrototypeTemplate()->Set(Nan::New<String>(JS_CLOSE_FUNCTION).ToLocalChecked(), Nan::New<FunctionTemplate>(Close));

  Local<Function> constructor = tpl->GetFunction();
  target->Set(Nan::New<String>(JS_CLASS_NAME).ToLocalChecked(), constructor);
}

extern "C"
{
  void init (v8::Handle<v8::Object> target)
  {
    InitializeList(target);
    FtdiDevice::Initialize(target);
    uv_mutex_init(&libraryMutex);
  }
}

NODE_MODULE(ftdi, init)
