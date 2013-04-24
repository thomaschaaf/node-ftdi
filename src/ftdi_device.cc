#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <queue>
#include <node_buffer.h>
#include <uv.h>

#include "ftdi_constants.h"
#include "ftdi_device.h"
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
 * Local typedefs
 **********************************/
typedef struct 
{
    FtdiDevice* device;
    Persistent<Value> callback; 
    Persistent<Value> readCallback; 
    FT_STATUS status;
} OpenBaton_t;

typedef struct
{
    FtdiDevice* device;
    uint8_t* data;
    DWORD length;
    Persistent<Value> callback;
    FT_STATUS status;
} ReadBaton_t;

typedef struct  
{
    FtdiDevice* device;
    uint8_t* data;
    DWORD length;
    Persistent<Value> callback;
    FT_STATUS status;
} WriteBaton_t;

typedef struct CloseBaton_t
{
    FtdiDevice* device;
    Persistent<Value> callback;
    FT_STATUS status;

} CloseBaton_t;


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

void FtdiDevice::Initialize(v8::Handle<v8::Object> target) 
{
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol(JS_CLASS_NAME));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol(JS_WRITE_FUNCTION), FunctionTemplate::New(Write)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol(JS_OPEN_FUNCTION), FunctionTemplate::New(Open)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol(JS_CLOSE_FUNCTION), FunctionTemplate::New(Close)->GetFunction());

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol(JS_CLASS_NAME), constructor);
}

Handle<Value> FtdiDevice::New(const Arguments& args) 
{
    HandleScope scope;
    Local<String> locationId    = String::New(DEVICE_LOCATION_ID_TAG);
    Local<String> serial        = String::New(DEVICE_SERIAL_NR_TAG);
    Local<String> index         = String::New(DEVICE_INDEX_TAG);
    Local<String> description   = String::New(DEVICE_DESCRIPTION_TAG);
    Local<String> vid           = String::New(DEVICE_VENDOR_ID_TAG);
    Local<String> pid           = String::New(DEVICE_PRODUCT_ID_TAG);

    FtdiDevice* object = new FtdiDevice();

    // Check if the argument is an object
    if(args[0]->IsObject()) 
    {
        Local<Object> obj = args[0]->ToObject();

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
    else if(args[0]->IsNumber())
    {
        object->connectParams.connectId = (int) args[0]->NumberValue();
        object->connectParams.connectType = ConnectType_ByIndex;
    }
    else
    {
        return FtdiDevice::ThrowTypeError("new expects a object as argument");
    }
    
    object->Wrap(args.This());

    return args.This();
}

/*****************************
 * OPEN Section
 *****************************/
Handle<Value> FtdiDevice::Open(const Arguments& args) 
{
    HandleScope scope;

    // Get Device Object
    FtdiDevice* device = ObjectWrap::Unwrap<FtdiDevice>(args.This());
    if(device == NULL)
    {
        return FtdiDevice::ThrowLastError("No FtdiDevice object found in Java Script object");
    }

    if (args.Length() != 3) 
    {
        return FtdiDevice::ThrowTypeError("open() expects three arguments");
    }

    if (!args[0]->IsObject()) 
    {
        return FtdiDevice::ThrowTypeError("open() expects a object as first argument");
    }

    // options
    if(!args[1]->IsFunction()) 
    {
        return FtdiDevice::ThrowTypeError("open() expects a function (openFisnished) as second argument");
    }
    Local<Value> readCallback = args[1];

        // options
    if(!args[2]->IsFunction()) 
    {
        return FtdiDevice::ThrowTypeError("open() expects a function (readFinsihed) as third argument");
    }
    Local<Value> openCallback = args[2];


    // Check if device is not already open or opening
    if(device->deviceState == DeviceState_Open)
    {
        Handle<Value> argv[1];
        argv[0] = String::New(FT_STATUS_CUSTOM_ALREADY_OPEN);
        Function::Cast(*args[2])->Call(Context::GetCurrent()->Global(), 1, argv);
    }
    else
    {
        device->deviceState = DeviceState_Open;

        OpenBaton_t* baton = new OpenBaton_t();

        // Extract the connection parameters
        device->ExtractDeviceSettings(args[0]->ToObject());
        
        baton->readCallback =  Persistent<Value>::New(readCallback);
        baton->callback =  Persistent<Value>::New(openCallback);
        baton->device = device;

        uv_work_t* req = new uv_work_t();
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, OpenAsync, (uv_after_work_cb)FtdiDevice::OpenFinished);
    }

    return args.This();
}


void FtdiDevice::OpenAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    OpenBaton_t* baton = static_cast<OpenBaton_t*>(req->data);
    FtdiDevice* device = baton->device;

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

    baton->status = ftStatus;
}

void FtdiDevice::OpenFinished(uv_work_t* req)
{
    OpenBaton_t* baton = static_cast<OpenBaton_t*>(req->data);
    FtdiDevice* device = baton->device;
 
    /**
     * If the open process was sucessful, we start the read thread
     * which waits for data events sents from the device.
     */ 
    if(baton->status == FT_OK)
    {
        baton->status = device->PrepareAsyncRead();
        if(baton->status == FT_OK)
        {
            ReadBaton_t* readBaton = new ReadBaton_t();
            readBaton->device = device;
            readBaton->callback = baton->readCallback;

            // Lock the Close mutex (it is needed to signal when async read has stoped reading)
            uv_mutex_lock(&device->closeMutex);

            uv_work_t* req = new uv_work_t();
            req->data = readBaton;
            uv_queue_work(uv_default_loop(), req, FtdiDevice::ReadDataAsync, (uv_after_work_cb)FtdiDevice::ReadCallback);
        }
        // In case read thread could not be started, dispose the callback
        else
        {
            fprintf(stderr, "Could not Initialise event notification: %s\n", error_strings[baton->status]);
            baton->readCallback.Dispose();
        }
    }

    // Check for callback function
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
        baton->callback.Dispose();
    }

    delete req;
    delete baton;
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
 * READ Section
 *****************************/
void FtdiDevice::ReadDataAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    DWORD RxBytes;
    DWORD BytesReceived;

    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    FtdiDevice* device = baton->device;

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
            baton->status = ftStatus;
            return;
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
            
            baton->status = ftStatus;
            baton->length = BytesReceived;
            return;
        }

        // Check if we are closing the device
        if(device->deviceState == DeviceState_Closing)
        {
            uv_mutex_lock(&libraryMutex);  
            FT_Purge(device->ftHandle, FT_PURGE_RX | FT_PURGE_TX);
            uv_mutex_unlock(&libraryMutex);  
            return;
        }
    }
}

void FtdiDevice::ReadCallback(uv_work_t* req)
{
    HandleScope scope;
    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    FtdiDevice* device = baton->device;

    // Execute the callback in case we have one and we have read some data
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
        delete baton->data;
        baton->length = 0;
    }

    // Check if we want close the device
    if(device->deviceState == DeviceState_Closing)
    {
        // Signal that we have stoped reading
        uv_mutex_unlock(&device->closeMutex);
        baton->callback.Dispose();
        delete baton;
        delete req;
    }
    else
    {   
        // Restart reading loop
        uv_queue_work(uv_default_loop(), req, FtdiDevice::ReadDataAsync, (uv_after_work_cb)FtdiDevice::ReadCallback);
    }
}


/*****************************
 * WRITE Section
 *****************************/
Handle<Value> FtdiDevice::Write(const Arguments& args) 
{
    HandleScope scope;

     // buffer
    if(!args[0]->IsObject() || !Buffer::HasInstance(args[0]))
    {
        return scope.Close(v8::ThrowException(Exception::TypeError(String::New("First argument must be a buffer"))));
    }
    Local<Object> buffer = Local<Object>::New(args[0]->ToObject());

    // Obtain Device Object
    FtdiDevice* device = ObjectWrap::Unwrap<FtdiDevice>(args.This());
    if(device == NULL)
    {
        return FtdiDevice::ThrowLastError("No FtdiDevice object found in Java Script object");
    }

    WriteBaton_t* baton = new WriteBaton_t();

    Local<Value> writeCallback;
    // options
    if(args.Length() > 1 && args[1]->IsFunction()) 
    {
        writeCallback = args[1];
        baton->callback = Persistent<Value>::New(writeCallback);
    }

    baton->length = (DWORD)Buffer::Length(buffer);
    baton->data = new uint8_t[baton->length];
    memcpy(baton->data, Buffer::Data(buffer), baton->length);
    baton->device = device;

    uv_work_t* req = new uv_work_t();
    req->data = baton;
    uv_queue_work(uv_default_loop(), req, FtdiDevice::WriteAsync, (uv_after_work_cb)FtdiDevice::WriteFinished);

    return scope.Close(v8::Undefined());
}

void FtdiDevice::WriteAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    DWORD bytesWritten;
    WriteBaton_t* baton = static_cast<WriteBaton_t*>(req->data);
    FtdiDevice* device = baton->device;

    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_Write(device->ftHandle, baton->data, baton->length, &bytesWritten);
    uv_mutex_unlock(&libraryMutex);  

    baton->status = ftStatus;
}

void FtdiDevice::WriteFinished(uv_work_t* req)
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
        baton->callback.Dispose();
    }

    delete baton->data;
    delete baton;
    delete req;
}

/*****************************
 * CLOSE Section
 *****************************/
Handle<Value> FtdiDevice::Close(const Arguments& args) 
{
    HandleScope scope;

    // Obtain Device Object
    FtdiDevice* device = ObjectWrap::Unwrap<FtdiDevice>(args.This());
    if(device == NULL)
    {
        return FtdiDevice::ThrowLastError("No FtdiDevice object found in Java Script object");
    }

    // Check the device state
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

        // Set Device State
        device->deviceState = DeviceState_Closing;

         // callback
        if(args[0]->IsFunction()) 
        {
            baton->callback = Persistent<Value>::New(args[0]);
        }

        uv_work_t* req = new uv_work_t();
        req->data = baton;
        uv_queue_work(uv_default_loop(), req, FtdiDevice::CloseAsync, (uv_after_work_cb)FtdiDevice::CloseFinished);
    }

    return scope.Close(v8::Undefined());
}

void FtdiDevice::CloseAsync(uv_work_t* req)
{
    FT_STATUS ftStatus;
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);
    FtdiDevice* device = baton->device;

    // Send Event for Read Loop
    device->SignalCloseEvent();

    // Wait till read loop finishes
    uv_mutex_lock(&device->closeMutex);
    uv_mutex_unlock(&device->closeMutex);

    // Close the device
    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_Close(device->ftHandle);
    uv_mutex_unlock(&libraryMutex);  
    baton->status = ftStatus;
}

void FtdiDevice::CloseFinished(uv_work_t* req)
{
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);
    FtdiDevice* device = baton->device;

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
        baton->callback.Dispose();
    }
    delete req;
    delete baton;
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

    // printf("Connection Settings set [Baud: %d, DataBits: %d, StopBits: %d, Parity: %d]\r\n", deviceParams.baudRate, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
    return ftStatus;
}

void FtdiDevice::ExtractDeviceSettings(Local<Object> options)
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
        delete str;
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

/**
 *  Generates a new C String. It allocates memory for the new
 *  string. Be sure to free the memory as soon as you dont need
 *  it anymore.
 */
void ToCString(Local<String> val, char ** ptr) 
{
    *ptr = new char[val->Utf8Length() + 1];
    val->WriteAscii(*ptr, 0, -1, 0);
}

Handle<Value> FtdiDevice::ThrowTypeError(std::string message) 
{
    return ThrowException(Exception::TypeError(String::New(message.c_str())));
}

Handle<Value> FtdiDevice::ThrowLastError(std::string message) 
{
    Local<String> msg = String::New(message.c_str());

    return ThrowException(Exception::Error(msg));
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