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
#endif

using namespace std;
using namespace v8;
using namespace node;
using namespace node_ftdi;


/**********************************
 * Local defines
 **********************************/
#define READ_BUFFER_SIZE    64

#define EVENT_MASK (FT_EVENT_RXCHAR)
#define WAIT_TIME_MILLISECONDS      500

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
    int status;
} OpenBaton_t;

typedef struct
{
    NodeFtdi* device;
    uint8_t data[READ_BUFFER_SIZE];
    int length;
    Persistent<Value> callback;
    int status;
} ReadBaton_t;

typedef struct  
{
    NodeFtdi* device;
    uint8_t* data;
    int length;
    Persistent<Value> callback;
    int status;
} WriteBaton_t;

typedef struct CloseBaton_t
{
    NodeFtdi* device;
    Persistent<Value> callback;
    int status;
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
ftdi_bits_type GetWordLength(int wordLength);
ftdi_stopbits_type GetStopBits(int stopBits);
ftdi_parity_type GetParity(const char* string);

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
    if(connectParams.description != NULL)
    {
        free(connectParams.description);
    }
    if(connectParams.serial != NULL)
    {
        free(connectParams.serial);
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
    Local<String> serial = String::New(DEVICE_SERIAL_NR_TAG);
    Local<String> description = String::New(DEVICE_DESCRIPTION_TAG);
    Local<String> vid = String::New(DEVICE_VENDOR_ID_TAG);
    Local<String> pid = String::New(DEVICE_PRODUCT_ID_TAG);

    NodeFtdi* object = new NodeFtdi();

    if(args[0]->IsObject()) 
    {
        Local<Object> obj = args[0]->ToObject();

        object->connectParams.vid = 0;
        object->connectParams.pid = 0;

        if(obj->Has(serial)) 
        {
            ToCString(obj->Get(serial)->ToString(), &object->connectParams.serial);
        }
        if(obj->Has(description)) 
        {
            ToCString(obj->Get(description)->ToString(), &object->connectParams.description);
        }
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
    int status;
    OpenBaton_t* baton = static_cast<OpenBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    status = device->OpenDevice();
    if (status != 0) 
    {
        fprintf(stderr, "Can't open ftdi device: %d\n", status);
    }
    else
    {
        status = device->SetDeviceSettings();
        if (status != 0) 
        {
            fprintf(stderr, "Can't Set DeviceSettings: %d\n", status);
        }
    }

    baton->status = status;
}

void NodeFtdi::OpenFinished(uv_work_t* req)
{
    OpenBaton_t* baton = static_cast<OpenBaton_t*>(req->data);
    NodeFtdi* device = baton->device;
 
    if(baton->status == 0)
    {
        ReadBaton_t* readBaton = new ReadBaton_t();
        readBaton->device = device;
        readBaton->callback = baton->readCallback;

        ftdi_set_latency_timer(device->ftdi, 255);
        uv_work_t* req = new uv_work_t();
        req->data = readBaton;
        uv_queue_work(uv_default_loop(), req, NodeFtdi::ReadDataAsync, (uv_after_work_cb)NodeFtdi::ReadCallback);
    }

    if(baton->callback->IsFunction())
    {
        Handle<Value> argv[1];
        if(baton->status != 0)
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

int NodeFtdi::OpenDevice()
{
    int status;

    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    uv_mutex_lock(&libraryMutex);  
    status = ftdi_usb_open_desc(ftdi, connectParams.vid, connectParams.pid, connectParams.description, connectParams.serial);
    uv_mutex_unlock(&libraryMutex);

    printf("Ftdi Device Type: %d\r\n", ftdi->type);

    return status;
}

/*****************************
 * READ Section
 *****************************/
void NodeFtdi::ReadDataAsync(uv_work_t* req)
{
    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    NodeFtdi* device = baton->device;
    int status;

    status = ftdi_read_data(device->ftdi, baton->data, READ_BUFFER_SIZE);

    if(status >= 0)
    {
        baton->length = status;
    }
    else
    {
        baton->status = status;
    }
}

void NodeFtdi::ReadCallback(uv_work_t* req)
{
    HandleScope scope;
    ReadBaton_t* baton = static_cast<ReadBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    if(baton->status != 0 || (baton->length != 0 && baton->callback->IsFunction()))
    {
        Handle<Value> argv[2];

        Buffer *slowBuffer = Buffer::New(baton->length);
        memcpy(Buffer::Data(slowBuffer), baton->data, baton->length);
        Local<Object> globalObj = Context::GetCurrent()->Global();

        Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
        Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(baton->length), Integer::New(0) };
        Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
        argv[1] = actualBuffer;

        if(baton->status != 0)
        {
            argv[0] = String::New(GetStatusString(baton->status));
        }
        else
        {
            argv[0] = Undefined();
        }

        Function::Cast(*baton->callback)->Call(Context::GetCurrent()->Global(), 2, argv);

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

    baton->length = (int)Buffer::Length(buffer);
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
    int status;
    WriteBaton_t* baton = static_cast<WriteBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    uv_mutex_lock(&libraryMutex);  
    status = ftdi_write_data(device->ftdi, baton->data, baton->length);
    uv_mutex_unlock(&libraryMutex);  

    if(status < 0)
    {
        baton->status = status;
    }
}

void NodeFtdi::WriteFinished(uv_work_t* req)
{
    WriteBaton_t* baton = static_cast<WriteBaton_t*>(req->data);
    if(!baton->callback.IsEmpty() && baton->callback->IsFunction())
    {

        Handle<Value> argv[1];
        if(baton->status != 0)
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
    CloseBaton_t* baton = static_cast<CloseBaton_t*>(req->data);
    NodeFtdi* device = baton->device;

    device->syncContext = &baton->closeMutex;
    device->deviceState = DeviceState_Closing;

    uv_mutex_lock(&baton->closeMutex);

    uv_mutex_lock(&libraryMutex);  
    baton->status = ftdi_usb_close(device->ftdi);
    ftdi_free(device->ftdi);
    uv_mutex_unlock(&libraryMutex);  
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
        if(baton->status != 0)
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
 int NodeFtdi::SetDeviceSettings()
{
    int status;
    // Set baudrate
    uv_mutex_lock(&libraryMutex);  
    status = ftdi_set_baudrate(ftdi, deviceParams.baudRate);
    uv_mutex_unlock(&libraryMutex); 
    if (status < 0)
    {
        fprintf(stderr, "unable to set baudrate: %d (%s)\n", status, ftdi_get_error_string(ftdi));
        return status;
    }
    
    uv_mutex_lock(&libraryMutex);  
    status = ftdi_set_line_property(ftdi, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
    uv_mutex_unlock(&libraryMutex); 
    if (status < 0)
    {
        fprintf(stderr, "unable to set line parameters: %d (%s)\n", status, ftdi_get_error_string(ftdi));
        return status;
    }

    printf("Connection Settings set [Baud: %d, DataBits: %d, StopBits: %d, Parity: %d]\r\n", deviceParams.baudRate, deviceParams.wordLength, deviceParams.stopBits, deviceParams.parity);
    return status;
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

ftdi_bits_type GetWordLength(int wordLength)
{
    switch(wordLength)
    {
        case 7:
            return BITS_7;

        case 8:
        default:
            return BITS_8;
    }
}

ftdi_stopbits_type GetStopBits(int stopBits)
{
    switch(stopBits)
    {
        case 1:
            return STOP_BIT_1;

        case 2:
        default:
            return STOP_BIT_2;
    }
}

ftdi_parity_type GetParity(const char* string)
{
    if(strcmp(CONNECTION_PARITY_NONE, string) == 0)
    {
        return NONE;
    }
    else if(strcmp(CONNECTION_PARITY_ODD, string) == 0)
    {
        return ODD;
    }
    else if(strcmp(CONNECTION_PARITY_EVEN, string) == 0)
    {
        return EVEN;
    }
    return NONE;
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

extern "C" {
  void init (v8::Handle<v8::Object> target) 
  {
    InitializeList(target);
    NodeFtdi::Initialize(target);
    uv_mutex_init(&libraryMutex);
  }
}

NODE_MODULE(ftdi, init)