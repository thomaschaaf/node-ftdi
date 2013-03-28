#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <queue>
#include <node_buffer.h>
#include <uv.h>

#include "ftdi_constants.h"
#include "node_ftdi.h"
#include "ftdi_driver.h"

#ifndef WIN32
    #include <unistd.h>
    #include <time.h>
#else
    #include <windows.h>
#endif

using namespace std;
using namespace v8;
using namespace node;
using namespace node_ftdi;


/**********************************
 * Local defines
 **********************************/
#define EVENT_MASK (FT_EVENT_RXCHAR)
#define WAIT_TIME_MILLISECONDS      250

#define NANOSECS_PER_SECOND         1000000000
#define NANOSECS_PER_MILISECOND     1000000
#define MILISECS_PER_SECOND         1000

/**********************************
 * Local typedefs
 **********************************/
typedef struct 
{
    NodeFtdi* device;
    Persistent<Value> callback; 
    Persistent<Value> readCallback; 
    FT_STATUS status;
} OpenBaton_t;

typedef struct
{
    NodeFtdi* device;
#ifndef WIN32
    EVENT_HANDLE eh;
    struct timespec ts;
    struct timeval tp;
#else
    HANDLE hEvent;
#endif
    uint8_t* data;
    DWORD length;
    Persistent<Value> callback;
    FT_STATUS status;
} ReadBaton_t;

typedef struct  
{
    NodeFtdi* device;
    uint8_t* data;
    DWORD length;
    Persistent<Value> callback;
    FT_STATUS status;
} WriteBaton_t;

typedef struct CloseBaton_t
{
    NodeFtdi* device;
    Persistent<Value> callback;
    FT_STATUS status;
    uv_mutex_t closeMutex;

    CloseBaton_t()
    {
        uv_mutex_init(&closeMutex);
        uv_mutex_lock(&closeMutex);
    }

} CloseBaton_t;


/**********************************
 * Local Helper Functions protoypes
 **********************************/
FT_STATUS PrepareAsyncRead(ReadBaton_t *baton, FT_HANDLE handle);
void WaitForReadEvent(ReadBaton_t *baton);

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

NodeFtdi::NodeFtdi() 
{
    deviceState = DeviceState_Idle;
};

NodeFtdi::~NodeFtdi() 
{
    if(connectParams.connectString != NULL)
    {
        printf("************ Free Ressource\r\n");
        free(connectParams.connectString);
    }
};

void NodeFtdi::Initialize(v8::Handle<v8::Object> target) 
{
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("FtdiDevice"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol("write"), FunctionTemplate::New(Write)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("open"), FunctionTemplate::New(Open)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(Close)->GetFunction());

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol("FtdiDevice"), constructor);
}

Handle<Value> NodeFtdi::New(const Arguments& args) 
{
    HandleScope scope;
    Local<String> locationId = String::New(DEVICE_LOCATION_ID_TAG);
    Local<String> serial = String::New(DEVICE_SERIAL_NR_TAG);
    Local<String> index = String::New(DEVICE_INDEX_TAG);
    Local<String> description = String::New(DEVICE_DESCRIPTION_TAG);
    Local<String> vid = String::New(DEVICE_VENDOR_ID_TAG);
    Local<String> pid = String::New(DEVICE_PRODUCT_ID_TAG);

    NodeFtdi* object = new NodeFtdi();

    if(args[0]->IsObject()) 
    {
        Local<Object> obj = args[0]->ToObject();

        if(obj->Has(locationId) && obj->Get(locationId)->Int32Value() != 0) 
        {
            object->connectParams.connectId = obj->Get(locationId)->Int32Value();
            object->connectParams.connectType = ConnectType_ByLocationId;
        }
        else 
        if(obj->Has(serial)) 
        {
            ToCString(obj->Get(serial)->ToString(), &object->connectParams.connectString);
            object->connectParams.connectType = ConnectType_BySerial;
        }
        else 
        if(obj->Has(description)) 
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
        object->connectParams.pid = 0;
        if(obj->Has(vid)) 
        {
            object->connectParams.vid = obj->Get(vid)->Int32Value();
        }
        if(obj->Has(pid)) 
        {
            object->connectParams.pid = obj->Get(pid)->Int32Value();
        }
        printf("Device Info [Vid: %x, Pid: %x]\r\n", object->connectParams.vid, object->connectParams.pid);
    }
    else if(args[0]->IsNumber())
    {
        object->connectParams.connectId = (int) args[0]->NumberValue();
        object->connectParams.connectType = ConnectType_ByIndex;
    }
    else
    {
        return NodeFtdi::ThrowTypeError("new expects a object as argument");
    }
    
    object->Wrap(args.This());

    return args.This();
}

/*****************************
 * OPEN Section
 *****************************/
Handle<Value> NodeFtdi::Open(const Arguments& args) 
{
    HandleScope scope;

    // Get Object
    NodeFtdi* device = ObjectWrap::Unwrap<NodeFtdi>(args.This());
    device->deviceState = DeviceState_Open;

    if (args.Length() != 3) 
    {
        return NodeFtdi::ThrowTypeError("open() expects three arguments");
    }

    if (!args[0]->IsObject()) 
    {
        return NodeFtdi::ThrowTypeError("open() expects a object as first argument");
    }

    // options
    if(!args[1]->IsFunction()) 
    {
        return NodeFtdi::ThrowTypeError("open() expects a function as second argument");
    }
    Local<Value> readCallback = args[1];

        // options
    if(!args[2]->IsFunction()) 
    {
        return NodeFtdi::ThrowTypeError("open() expects a function as third argument");
    }
    Local<Value> openCallback = args[2];

    OpenBaton_t* baton = new OpenBaton_t();

    device->ExtractDeviceSettings(args[0]->ToObject());
    
    baton->readCallback =  Persistent<Value>::New(readCallback);
    baton->callback =  Persistent<Value>::New(openCallback);
    baton->device = device;

    uv_work_t* req = new uv_work_t();
    req->data = baton;
    uv_queue_work(uv_default_loop(), req, OpenAsync, (uv_after_work_cb)NodeFtdi::OpenFinished);

    return args.This();
}


void NodeFtdi::OpenAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    OpenBaton_t* baton = static_cast<OpenBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    ftStatus = device->OpenDevice();
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't open ftdi device: %d\n", ftStatus);
    }
    else
    {
        ftStatus = device->SetDeviceSettings();
        if (ftStatus != FT_OK) 
        {
            fprintf(stderr, "Can't Set DeviceSettings: %d\n", ftStatus);
        }
    }

    baton->status = ftStatus;
}

void NodeFtdi::OpenFinished(uv_work_t* req)
{
    OpenBaton_t* baton = static_cast<OpenBaton_t*>(req->data);
    NodeFtdi* device = baton->device;
 
    if(baton->status == FT_OK)
    {
        ReadBaton_t* readBaton = new ReadBaton_t();
        PrepareAsyncRead(readBaton, device->ftHandle);
        readBaton->device = device;
        readBaton->callback = baton->readCallback;

        uv_work_t* req = new uv_work_t();
        req->data = readBaton;
        uv_queue_work(uv_default_loop(), req, NodeFtdi::ReadDataAsync, (uv_after_work_cb)NodeFtdi::ReadCallback);
    }

    if(baton->callback->IsFunction())
    {
        Handle<Value> argv[1];
        if(baton->status != FT_OK)
        {
            argv[0] = String::New(GetStatusString(baton->status));
        }
        else
        {
            argv[0] = Undefined();
        }

        Function::Cast(*baton->callback)->Call(Context::GetCurrent()->Global(), 1, argv);
    }

    delete req;
    baton->callback.Dispose();
    delete baton;
}

FT_STATUS NodeFtdi::OpenDevice()
{
    FT_STATUS status;

    if(connectParams.connectType == ConnectType_ByIndex)
    {
        uv_mutex_lock(&libraryMutex); 
#ifndef WIN32
        FT_SetVIDPID(connectParams.vid, connectParams.pid);
#endif
        status = FT_Open(connectParams.connectId, &ftHandle);
        uv_mutex_unlock(&libraryMutex);  
        printf("OpenDeviceByIndex [Index: %d]\r\n", connectParams.connectId);
        return status;
    }
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
                printf("OpenDevice [Flag: %d, Arg: %s]\r\n", flags, (char *)arg);
            }
            break;

            case ConnectType_BySerial:
            {
                arg = (PVOID) connectParams.connectString;
                flags = FT_OPEN_BY_SERIAL_NUMBER;
                printf("OpenDevice [Flag: %d, Arg: %s]\r\n", flags, (char *)arg);
            }
            break;

            
            case ConnectType_ByLocationId:
            {
                arg = (PVOID) connectParams.connectId;
                flags = FT_OPEN_BY_LOCATION;
                printf("OpenDevice [Flag: %d, Arg: %d]\r\n", flags, connectParams.connectId);
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
        FT_SetVIDPID(connectParams.vid, connectParams.pid);
#endif

        status = FT_OpenEx(arg, flags, &ftHandle);
        uv_mutex_unlock(&libraryMutex); 
        uv_mutex_unlock(&vidPidMutex);  
        return status;
    }

    return FT_INVALID_PARAMETER;
}

/*****************************
 * READ Section
 *****************************/
void NodeFtdi::ReadDataAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    DWORD RxBytes;
    DWORD BytesReceived;

    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    NodeFtdi* device = baton->device;
    while(1)
    {
        WaitForReadEvent(baton);
        
        // Check if we are closing the device
        if(device->deviceState == DeviceState_Closing)
        {
            uv_mutex_lock(&libraryMutex);  
            FT_Purge(device->ftHandle, FT_PURGE_RX | FT_PURGE_TX);
            uv_mutex_unlock(&libraryMutex);  
            return;
        }

        // Check Queue Status
        uv_mutex_lock(&libraryMutex);  
        ftStatus = FT_GetQueueStatus(device->ftHandle, &RxBytes);
        uv_mutex_unlock(&libraryMutex);  

        if(ftStatus != FT_OK)
        {
            fprintf(stderr, "Can't read from ftdi device: %d\n", ftStatus);
            baton->status = ftStatus;
            return;
        }

        if(RxBytes > 0)
        {
            baton->data = (uint8_t *)malloc(RxBytes);

            uv_mutex_lock(&libraryMutex);  
            ftStatus = FT_Read(device->ftHandle, baton->data, RxBytes, &BytesReceived);
            uv_mutex_unlock(&libraryMutex);  
            if (ftStatus != FT_OK) 
            {
                fprintf(stderr, "Can't read from ftdi device: %d\n", ftStatus);
            }
            
            baton->status = ftStatus;
            baton->length = BytesReceived;
            return;
        }
    }
}

void NodeFtdi::ReadCallback(uv_work_t* req)
{
    HandleScope scope;
    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    if(baton->status != FT_OK || (baton->length != 0 && baton->callback->IsFunction()))
    {
        Handle<Value> argv[2];

        Buffer *slowBuffer = Buffer::New(baton->length);
        memcpy(Buffer::Data(slowBuffer), baton->data, baton->length);
        Local<Object> globalObj = Context::GetCurrent()->Global();

        Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
        Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(baton->length), Integer::New(0) };
        Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
        argv[1] = actualBuffer;

        if(baton->status != FT_OK)
        {
            argv[0] = String::New(GetStatusString(baton->status));
        }
        else
        {
            argv[0] = Undefined();
        }

        Function::Cast(*baton->callback)->Call(Context::GetCurrent()->Global(), 2, argv);

    }

    if(baton->length != 0)
    {
        free(baton->data);
        baton->length = 0;
    }

    if(device->deviceState == DeviceState_Closing)
    {
        uv_mutex_unlock((uv_mutex_t *) device->syncContext);
        baton->callback.Dispose();
        delete baton;
        delete req;
    }
    else
    {
        uv_queue_work(uv_default_loop(), req, NodeFtdi::ReadDataAsync, (uv_after_work_cb)NodeFtdi::ReadCallback);
    }
}


/*****************************
 * WRITE Section
 *****************************/
Handle<Value> NodeFtdi::Write(const Arguments& args) 
{
    HandleScope scope;

     // buffer
    if(!args[0]->IsObject() || !Buffer::HasInstance(args[0]))
    {
        return scope.Close(v8::ThrowException(Exception::TypeError(String::New("First argument must be a buffer"))));
    }
    Local<Object> buffer = Local<Object>::New(args[0]->ToObject());

    // Get Object
    NodeFtdi* device = ObjectWrap::Unwrap<NodeFtdi>(args.This());
    WriteBaton_t* baton = new WriteBaton_t();

    Local<Value> writeCallback;
    // options
    if(args.Length() > 1 && args[1]->IsFunction()) 
    {
        writeCallback = args[1];
        baton->callback = Persistent<Value>::New(writeCallback);
    }

    baton->length = (DWORD)Buffer::Length(buffer);
    baton->data = (uint8_t*) malloc(baton->length);
    memcpy(baton->data, Buffer::Data(buffer), baton->length);
    baton->device = device;

    uv_work_t* req = new uv_work_t();
    req->data = baton;
    uv_queue_work(uv_default_loop(), req, NodeFtdi::WriteAsync, (uv_after_work_cb)NodeFtdi::WriteFinished);

    return scope.Close(v8::Undefined());
}

void NodeFtdi::WriteAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    DWORD bytesWritten;
    WriteBaton_t* baton = static_cast<WriteBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_Write(device->ftHandle, baton->data, baton->length, &bytesWritten);
    uv_mutex_unlock(&libraryMutex);  

    baton->status = ftStatus;
}

void NodeFtdi::WriteFinished(uv_work_t* req)
{
    WriteBaton_t* baton = static_cast<WriteBaton_t*>(req->data);
    if(!baton->callback.IsEmpty() && baton->callback->IsFunction())
    {

        Handle<Value> argv[1];
        if(baton->status != FT_OK)
        {
            argv[0] = String::New(GetStatusString(baton->status));
        }
        else
        {
            argv[0] = Undefined();
        }

        Function::Cast(*baton->callback)->Call(Context::GetCurrent()->Global(), 1, argv);
    }

    free(baton->data);
    baton->callback.Dispose();
    delete baton;
    delete req;
}

/*****************************
 * CLOSE Section
 *****************************/
Handle<Value> NodeFtdi::Close(const Arguments& args) 
{
    HandleScope scope;

    // Get Object
    NodeFtdi* device = ObjectWrap::Unwrap<NodeFtdi>(args.This());

    if(device->deviceState != DeviceState_Open)
    {
        // callback
        if(args[0]->IsFunction()) 
        {
            Handle<Value> argv[1];
            argv[0] = String::New(FT_STATUS_CUSTOM_ALREADY_CLOSING);
            Function::Cast(*args[0])->Call(Context::GetCurrent()->Global(), 1, argv);
        }
    }
    else
    {
        CloseBaton_t* baton = new CloseBaton_t();
        baton->device = device;

         // callback
        if(args[0]->IsFunction()) 
        {
            baton->callback = Persistent<Value>::New(args[0]);
        }

        uv_work_t* req = new uv_work_t();
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, NodeFtdi::CloseAsync, (uv_after_work_cb)NodeFtdi::CloseFinished);
    }
    return scope.Close(v8::Undefined());
}

void NodeFtdi::CloseAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    device->syncContext = &baton->closeMutex;
    device->deviceState = DeviceState_Closing;

    uv_mutex_lock(&baton->closeMutex);

    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_Close(device->ftHandle);
    uv_mutex_unlock(&libraryMutex);  
    baton->status = ftStatus;
}

void NodeFtdi::CloseFinished(uv_work_t* req)
{
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    printf("Close Finsihed\r\n");
    device->deviceState = DeviceState_Idle;

    if(!baton->callback.IsEmpty() && baton->callback->IsFunction())
    {
        Handle<Value> argv[1];
        if(baton->status != FT_OK)
        {
            argv[0] = String::New(GetStatusString(baton->status));
        }
        else
        {
            argv[0] = Undefined();
        }

        Function::Cast(*baton->callback)->Call(Context::GetCurrent()->Global(), 1, argv);
    }
    delete req;
    baton->callback.Dispose();
    delete baton;
}

/*****************************
 * Helper Section
 *****************************/
 FT_STATUS NodeFtdi::SetDeviceSettings()
{
    FT_STATUS ftStatus;
    
    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_SetDataCharacteristics(ftHandle, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
    uv_mutex_unlock(&libraryMutex);  
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't Set FT_SetDataCharacteristics: %d\n", ftStatus);
        return ftStatus;
    }

    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_SetBaudRate(ftHandle, deviceParams.baudRate);
    uv_mutex_unlock(&libraryMutex);  
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't setBaudRate: %d\n", ftStatus);
        return ftStatus;
    }

    printf("Connection Settings set [Baud: %d, DataBits: %d, StopBits: %d, Parity: %d]\r\n", deviceParams.baudRate, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
    return ftStatus;
}

void NodeFtdi::ExtractDeviceSettings(Local<Object> options)
{
    HandleScope scope;
    Local<String> baudrate  = String::New(CONNECTION_BAUDRATE_TAG);
    Local<String> databits  = String::New(CONNECTION_DATABITS_TAG);
    Local<String> stopbits  = String::New(CONNECTION_STOPBITS_TAG);
    Local<String> parity    = String::New(CONNECTION_PARITY_TAG);

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
        free(str);
    }
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

void ToCString(Local<String> val, char ** ptr) 
{
    *ptr = (char *) malloc (val->Utf8Length() + 1);
    val->WriteAscii(*ptr, 0, -1, 0);
}

Handle<Value> NodeFtdi::ThrowTypeError(std::string message) 
{
    return ThrowException(Exception::TypeError(String::New(message.c_str())));
}

Handle<Value> NodeFtdi::ThrowLastError(std::string message) 
{
    Local<String> msg = String::New(message.c_str());

    return ThrowException(Exception::Error(msg));
}


/*****************************
 * Platform dependet Section
 *****************************/
#ifndef WIN32

FT_STATUS PrepareAsyncRead(ReadBaton_t *baton, FT_HANDLE handle)
{
    FT_STATUS status;
    pthread_mutex_init(&(baton->eh).eMutex, NULL);
    pthread_cond_init(&(baton->eh).eCondVar, NULL);
    uv_mutex_lock(&libraryMutex);  
    status = FT_SetEventNotification(handle, EVENT_MASK, (PVOID)&(baton->eh));
    uv_mutex_unlock(&libraryMutex);  
    return status;
}


void WaitForReadEvent(ReadBaton_t *baton)
{
    gettimeofday(&baton->tp, NULL);

    int additionalSeconds = 0;
    int additionalMilisecs = 0;
    additionalSeconds = (WAIT_TIME_MILLISECONDS  / MILISECS_PER_SECOND);
    additionalMilisecs = (WAIT_TIME_MILLISECONDS % MILISECS_PER_SECOND);

    long additionalNanosec = baton->tp.tv_usec * 1000 + additionalMilisecs * NANOSECS_PER_MILISECOND;
    additionalSeconds += (additionalNanosec / NANOSECS_PER_SECOND);
    additionalNanosec = (additionalNanosec % NANOSECS_PER_SECOND);

    /* Convert from timeval to timespec */
    baton->ts.tv_sec  = baton->tp.tv_sec;
    baton->ts.tv_nsec = additionalNanosec;
    baton->ts.tv_sec += additionalSeconds;

    pthread_mutex_lock(&(baton->eh).eMutex);
    pthread_cond_timedwait(&(baton->eh).eCondVar, &(baton->eh).eMutex, &baton->ts);
    pthread_mutex_unlock(&(baton->eh).eMutex);
}

#else
FT_STATUS PrepareAsyncRead(ReadBaton_t *baton, FT_HANDLE handle)
{    
    FT_STATUS status;
    baton->hEvent = CreateEvent(NULL, false /* auto-reset event */, false /* non-signalled state */, "");
    uv_mutex_lock(&libraryMutex);  
    status = FT_SetEventNotification(handle, EVENT_MASK, baton->hEvent);
    uv_mutex_unlock(&libraryMutex);  
    return status;
}

void WaitForReadEvent(ReadBaton_t *baton)
{
    WaitForSingleObject(baton->hEvent, WAIT_TIME_MILLISECONDS);
}
#endif

extern "C" {
  void init (v8::Handle<v8::Object> target) 
  {
    InitializeList(target);
    NodeFtdi::Initialize(target);
    uv_mutex_init(&libraryMutex);
  }
}

NODE_MODULE(ftdi, init)