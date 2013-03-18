#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <queue>

#include "node_ftdi.h"
#include "node_ftdi_platform.h"
#include "ftdi_driver.h"

#ifndef WIN32
#include <unistd.h>
#endif

using namespace std;
using namespace v8;
using namespace node;
using namespace node_ftdi;


const char* ToCString(Local<String> val) 
{
    return *String::Utf8Value(val);
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

void NodeFtdi::ReadCallback(uv_work_t* req)
{
    HandleScope scope;
    ReadBaton* data = static_cast<ReadBaton*>(req->data);

    printf("ReadCallback\r\n");

    if(data->callback->IsFunction())
    {
        Handle<Value> argv[1];
        // Local<Object> array = Object::New();
        // array->SetIndexedPropertiesToExternalArrayData(data->readData, kExternalUnsignedByteArray, data->bufferLength);
        // argv[0] = array;


        Buffer *slowBuffer = node::Buffer::New(data->bufferLength);
        memcpy(Buffer::Data(slowBuffer), data->readData, data->bufferLength);
        Local<Object> globalObj = Context::GetCurrent()->Global();

        Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
        Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(data->bufferLength), Integer::New(0) };
        Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
        argv[0] = actualBuffer;

        Function::Cast(*data->callback)->Call(Context::GetCurrent()->Global(), 1, argv);

    }
    else
    {
        printf("Could not call dataCb\r\n");
    }

    free(data->readData);

    uv_queue_work(uv_default_loop(), req, ReadDataAsync, (uv_after_work_cb)NodeFtdi::ReadCallback);
}

NodeFtdi::NodeFtdi() {};
NodeFtdi::~NodeFtdi() {};

void NodeFtdi::Initialize(v8::Handle<v8::Object> target) 
{

    printf("Initialize NodeFtdi\r\n");

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("FtdiDevice"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol("write"), FunctionTemplate::New(Write)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("registerDataCallback"), FunctionTemplate::New(RegisterDataCallback)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("open"), FunctionTemplate::New(Open)->GetFunction());

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol("FtdiDevice"), constructor);
}

Handle<Value> NodeFtdi::New(const Arguments& args) 
{
    HandleScope scope;
    Local<String> vid = String::New("vid");
    Local<String> pid = String::New("pid");
    Local<String> description = String::New("description");
    Local<String> serial = String::New("serial");
    Local<String> index = String::New("index");

    NodeFtdi* object = new NodeFtdi();

    object->p.vid = FTDI_VID;
    object->p.pid = FTDI_PID;
    object->p.description = NULL;
    object->p.serial = NULL;
    object->p.index = 0;

    printf("Create New FTDI Object\r\n");

    if(args[0]->IsObject()) 
    {
        Local<Object> obj = args[0]->ToObject();

        if(obj->Has(vid)) {
            object->p.vid = obj->Get(vid)->Int32Value();
        }
        if(obj->Has(pid)) {
            object->p.pid = obj->Get(pid)->Int32Value();
        }
        if(obj->Has(description)) {
            object->p.description = ToCString(obj->Get(description)->ToString());
        }
        if(obj->Has(serial)) {
            object->p.serial = ToCString(obj->Get(serial)->ToString());
        }
        if(obj->Has(index)) {
            object->p.index = (unsigned int) obj->Get(index)->Int32Value();
        }
    }

    object->Wrap(args.This());

    return args.This();
}


Handle<Value> NodeFtdi::Open(const Arguments& args) 
{
    HandleScope scope;
    FT_STATUS ftStatus;

    // Get Object
    NodeFtdi* obj = ObjectWrap::Unwrap<NodeFtdi>(args.This());

    if (args.Length() < 1 || !args[0]->IsString()) 
    {
        return NodeFtdi::ThrowTypeError("open() expects a string as first argument");
    }
    std::string serial(*String::Utf8Value(args[0]));

      // options
    if(!args[1]->IsObject()) 
    {
        return NodeFtdi::ThrowTypeError("open() expects a object as second argument");
    }
    Local<v8::Object> options = args[1]->ToObject();

    printf("Open Device %s\r\n",  (unsigned char*) serial.c_str());

    ftStatus = FT_OpenEx((unsigned char*) serial.c_str(), FT_OPEN_BY_SERIAL_NUMBER, &(obj->ftHandle));
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't open ftdi device: %d\n", ftStatus);
    }

    obj->SetDeviceSettings(options);

    ReadBaton* baton = new ReadBaton();
    baton->ftHandle = obj->ftHandle;
    baton->callback = obj->dataCallback;

    PrepareAsyncRead(baton);

    uv_work_t* req = new uv_work_t();
    req->data = baton;
    uv_queue_work(uv_default_loop(), req, ReadDataAsync, (uv_after_work_cb)NodeFtdi::ReadCallback);

    return args.This();
}

void NodeFtdi::SetDeviceSettings(Local<v8::Object> options)
{
    FT_STATUS ftStatus;
    int baudRate = options->Get(v8::String::New("baudrate"))->ToInt32()->Int32Value();
    
    ftStatus = FT_SetDataCharacteristics(ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if (ftStatus != FT_OK) 
    {
        return;
    }

    ftStatus = FT_SetBaudRate(ftHandle, baudRate);
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't setBaudRate: %d\n", ftStatus);
    }
    printf("BaudRate set to: %d\r\n", baudRate);
}


Handle<Value> NodeFtdi::Write(const Arguments& args) 
{
    HandleScope scope;
    FT_STATUS ftStatus;
    DWORD BytesWritten;

    // Get Object
    NodeFtdi* obj = ObjectWrap::Unwrap<NodeFtdi>(args.This());

     // buffer
    if(!args[0]->IsObject() || !Buffer::HasInstance(args[0]))
    {
        return scope.Close(v8::ThrowException(Exception::TypeError(String::New("First argument must be a buffer"))));
    }
    Persistent<Object> buffer = Persistent<Object>::New(args[0]->ToObject());
    char* bufferData = Buffer::Data(buffer);
    DWORD bufferLength = Buffer::Length(buffer);

    ftStatus = FT_Write(obj->ftHandle, bufferData, bufferLength, &BytesWritten);
    if (ftStatus != FT_OK) 
    {
        fprintf(stderr, "Can't write to ftdi device: %d\n", ftStatus);
        return NodeFtdi::ThrowTypeError("Can't write to ftdi device");
    }
    printf("%d bytes written\r\n", BytesWritten);

    return scope.Close(v8::Undefined());
}

Handle<Value> NodeFtdi::RegisterDataCallback(const Arguments& args) 
{
    HandleScope scope;

    // Get Object
    NodeFtdi* obj = ObjectWrap::Unwrap<NodeFtdi>(args.This());

    printf("RegisterDataCallback\r\n");

    // callback
    if(!args[0]->IsFunction()) 
    {
        return scope.Close(v8::ThrowException(v8::Exception::TypeError(v8::String::New("First argument must be a function"))));
    }
    Local<Value> callback = args[0];

    obj->dataCallback = Persistent<Value>::New(callback);

    return scope.Close(v8::Undefined());
}

extern "C" {
  void init (v8::Handle<v8::Object> target) 
  {
    InitializeList(target);
    NodeFtdi::Initialize(target);
  }
}

NODE_MODULE(ftdi, init);