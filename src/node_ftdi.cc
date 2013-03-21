#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <queue>
#include <node_buffer.h>

#include "ftdi_constants.h"
#include "node_ftdi.h"
#include "ftdi_driver.h"

#ifndef WIN32
    #include <unistd.h>
    #include <time.h>
#endif

using namespace std;
using namespace v8;
using namespace node;
using namespace node_ftdi;


/**********************************
 * Local defines
 **********************************/
#define EVENT_MASK (FT_EVENT_RXCHAR)
#define WAIT_TIME_SECONDS       1

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
static uv_mutex_t writeMutex;


/*****************************
 * CREATION Section
 *****************************/

NodeFtdi::NodeFtdi() 
{
    isClosing = false;
};

NodeFtdi::~NodeFtdi() 
{
    if(connectParams.connectString != NULL)
    {
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

    uv_mutex_init(&writeMutex);
}

Handle<Value> NodeFtdi::New(const Arguments& args) 
{
    HandleScope scope;
    Local<String> locationId = String::New(DEVICE_LOCATION_ID_TAG);
    Local<String> serial = String::New(DEVICE_SERIAL_NR_TAG);
    Local<String> index = String::New(DEVICE_INDEX_TAG);
    Local<String> description = String::New(DEVICE_DESCRIPTION_TAG);

    NodeFtdi* object = new NodeFtdi();
    object->connectParams.connectString = NULL;

    if(args[0]->IsObject()) 
    {
        Local<Object> obj = args[0]->ToObject();

        if(obj->Has(locationId)) 
        {
            object->connectParams.connectId = obj->Get(locationId)->Int32Value();
            object->connectParams.connectType = ConnectType_ByLocationId;
        }
        else if(obj->Has(serial)) 
        {
            ToCString(obj->Get(serial)->ToString(), &object->connectParams.connectString);
            object->connectParams.connectType = ConnectType_BySerial;
        }
        else if(obj->Has(index)) 
        {
            object->connectParams.connectId = obj->Get(index)->Int32Value();
            object->connectParams.connectType = ConnectType_ByIndex;
        }
        else if(obj->Has(description)) 
        {
            ToCString(obj->Get(description)->ToString(), &object->connectParams.connectString);
            object->connectParams.connectType = ConnectType_ByDescription;
        }
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

    printf("ConnectionType: %d\n", object->connectParams.connectType);

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
    if(connectParams.connectType == ConnectType_ByIndex)
    {
        return FT_Open(connectParams.connectId, &ftHandle);
    }
    else
    {
        PVOID arg;
        DWORD flags;

        switch(connectParams.connectType)
        {
            case ConnectType_ByDescription:
            {
                arg = (void*) connectParams.connectString;
                flags = FT_OPEN_BY_DESCRIPTION;
            }
            break;

            case ConnectType_BySerial:
            {
                arg = (void*) connectParams.connectString;
                flags = FT_OPEN_BY_SERIAL_NUMBER;
            }
            break;

            
            case ConnectType_ByLocationId:
            {
                arg = (void*) connectParams.connectId;
                flags = FT_OPEN_BY_LOCATION;
            }
            break;

            default:
            {
                return FT_INVALID_PARAMETER;
            }
        }

        return FT_OpenEx(arg, flags, &ftHandle);
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

        FT_GetQueueStatus(device->ftHandle, &RxBytes);

        if(RxBytes > 0)
        {
            baton->data = (uint8_t *)malloc(RxBytes);

            ftStatus = FT_Read(device->ftHandle, baton->data, RxBytes, &BytesReceived);
            if (ftStatus != FT_OK) 
            {
                fprintf(stderr, "Can't read from ftdi device: %d\n", ftStatus);
            }
            baton->length = BytesReceived;
            break;
        }

        if(device->isClosing)
        {
            break;
        }
    }
}

void NodeFtdi::ReadCallback(uv_work_t* req)
{
    HandleScope scope;
    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    if(baton->length != 0 && baton->callback->IsFunction())
    {
        Handle<Value> argv[1];

        Buffer *slowBuffer = Buffer::New(baton->length);
        memcpy(Buffer::Data(slowBuffer), baton->data, baton->length);
        Local<Object> globalObj = Context::GetCurrent()->Global();

        Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
        Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(baton->length), Integer::New(0) };
        Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
        argv[0] = actualBuffer;

        Function::Cast(*baton->callback)->Call(Context::GetCurrent()->Global(), 1, argv);

    }

    if(baton->length != 0)
    {
        free(baton->data);
        baton->length = 0;
    }

    if(device->isClosing)
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

    uv_mutex_lock(&writeMutex);  
    ftStatus = FT_Write(device->ftHandle, baton->data, baton->length, &bytesWritten);
    uv_mutex_unlock(&writeMutex);  

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
    CloseBaton_t* baton = new CloseBaton_t();

    // callback
    if(args[0]->IsFunction()) 
    {
        Local<Value> writeCallback = args[0];
        baton->callback = Persistent<Value>::New(writeCallback);
    }

    baton->device = device;

    uv_work_t* req = new uv_work_t();
    req->data = baton;
    uv_queue_work(uv_default_loop(), req, NodeFtdi::CloseAsync, (uv_after_work_cb)NodeFtdi::CloseFinished);

    return scope.Close(v8::Undefined());
}

void NodeFtdi::CloseAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    device->syncContext = &baton->closeMutex;
    device->isClosing = true;

    uv_mutex_lock(&baton->closeMutex);

    printf("Close Device\r\n");
    ftStatus = FT_Close(device->ftHandle);
    baton->status = ftStatus;
}

void NodeFtdi::CloseFinished(uv_work_t* req)
{
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);

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
    
    ftStatus = FT_SetDataCharacteristics(ftHandle, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't Set FT_SetDataCharacteristics: %d\n", ftStatus);
        return ftStatus;
    }

    ftStatus = FT_SetBaudRate(ftHandle, deviceParams.baudRate);
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
    if(strcasecmp(CONNECTION_PARITY_NONE, string) == 0)
    {
        return FT_PARITY_NONE;
    }
    else if(strcasecmp(CONNECTION_PARITY_ODD, string) == 0)
    {
        return FT_PARITY_ODD;
    }
    else if(strcasecmp(CONNECTION_PARITY_EVEN, string) == 0)
    {
        return FT_PARITY_EVEN;
    }
    return FT_PARITY_NONE;
}

void ToCString(Local<String> val, char ** ptr) 
{
    *ptr = (char *) malloc (val->Utf8Length());
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
    pthread_mutex_init(&(baton->eh).eMutex, NULL);
    pthread_cond_init(&(baton->eh).eCondVar, NULL);
    return FT_SetEventNotification(handle, EVENT_MASK, (PVOID)&(baton->eh));
}

void WaitForReadEvent(ReadBaton_t *baton)
{

    gettimeofday(&baton->tp, NULL);
    /* Convert from timeval to timespec */
    baton->ts.tv_sec  = baton->tp.tv_sec;
    baton->ts.tv_nsec = baton->tp.tv_usec * 1000;
    baton->ts.tv_sec += WAIT_TIME_SECONDS;

    pthread_mutex_lock(&(baton->eh).eMutex);
    pthread_cond_timedwait(&(baton->eh).eCondVar, &(baton->eh).eMutex, &baton->ts);
    pthread_mutex_unlock(&(baton->eh).eMutex);
}

#else
FT_STATUS PrepareAsyncRead(ReadBaton_t *baton, FT_HANDLE handle)
{    
    baton->hEvent = CreateEvent(NULL, false /* auto-reset event */, false /* non-signalled state */, "");
    return FT_SetEventNotification(handle, EVENT_MASK, baton->hEvent);
}

void WaitForReadEvent(ReadBaton_t *baton)
{
    WaitForSingleObject(baton->hEvent, INFINITE);
}
#endif

extern "C" {
  void init (v8::Handle<v8::Object> target) 
  {
    InitializeList(target);
    NodeFtdi::Initialize(target);
  }
}

NODE_MODULE(ftdi, init)