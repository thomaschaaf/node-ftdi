#include "ftdi_driver.h"
#include <stdlib.h>

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> constructor_template;
uv_mutex_t listMutex;

struct DeviceListBaton
{
    Persistent<Value> callback;
    FT_DEVICE_LIST_INFO_NODE *devInfo;
    DWORD listLength;
    FT_STATUS status;
};

void InitializeList(Handle<Object> target) 
{
    Local<FunctionTemplate> t = FunctionTemplate::New();
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->SetClassName(String::NewSymbol("FtdiDriver"));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

    // NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);

    NODE_SET_METHOD(constructor_template, "findAll", FindAll);
    NODE_SET_METHOD(constructor_template, "setVidPid", SetVidPid);
    target->Set(String::NewSymbol("FtdiDriver"), constructor_template->GetFunction());

    uv_mutex_init(&listMutex);
}

Handle<Value> SetVidPid(const Arguments& args) 
{
    HandleScope scope;

    if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsNumber()) 
    {
        ThrowException(Exception::TypeError(String::New("SetVidPid() expects two number arguments")));
    }

    int vid = (int) args[0]->NumberValue();
    int pid = (int) args[1]->NumberValue();

    printf("Set [Vid: %x, Pid: %x]\r\n", vid, pid);
    FT_SetVIDPID(vid, pid);

   return scope.Close(v8::Undefined());
}

void FindAllAsync(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    printf("findAll \r\n");
    FT_STATUS ftStatus;
    DWORD numDevs = 0;

    // create the device information list
    uv_mutex_lock(&listMutex);
    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus == FT_OK) 
    {
        if (numDevs > 0) 
        {
            // allocate storage for list based on numDevs
            listBaton->devInfo =  (FT_DEVICE_LIST_INFO_NODE*) malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs); 
            // get the device information list
            ftStatus = FT_GetDeviceInfoList(listBaton->devInfo, &numDevs); 
            if (ftStatus != FT_OK) 
            {
                 free(listBaton->devInfo);
            }
        }
    }
    uv_mutex_unlock(&listMutex);  

    listBaton->listLength = numDevs;              
    listBaton->status = ftStatus;
}

void FindAllFinished(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    Local<Array> array= Array::New(listBaton->listLength);
    if(listBaton->status == FT_OK)
    {
        printf("NumFindDevices: %d\r\n", listBaton->listLength);
        for (DWORD i = 0; i < listBaton->listLength; i++) 
        {
            Local<Object> obj = Object::New();
            obj->Set(String::New("description"), String::New(listBaton->devInfo[i].Description));
            obj->Set(String::New("serial"), String::New(listBaton->devInfo[i].SerialNumber));
            obj->Set(String::New("locationId"), Number::New(listBaton->devInfo[i].LocId));
            obj->Set(String::New("vendorId"), Number::New( (listBaton->devInfo[i].ID >> 16) & (0xFFFF)));
            obj->Set(String::New("productId"), Number::New( (listBaton->devInfo[i].ID) & (0xFFFF)));
            array->Set(i, obj);
        }

        free(listBaton->devInfo);
    }

    Handle<Value> argv[2];
    argv[0] = Number::New(listBaton->status);
    argv[1] = array;

    Function::Cast(*listBaton->callback)->Call(Context::GetCurrent()->Global(), 2, argv);
}

Handle<Value> FindAll(const Arguments& args) 
{
    HandleScope scope;

    // callback
    if(!args[0]->IsFunction()) 
    {
        return scope.Close(v8::ThrowException(v8::Exception::TypeError(v8::String::New("First argument must be a function"))));
    }
    Local<Value> callback = args[0];

    DeviceListBaton* listBaton = new DeviceListBaton();
    listBaton->callback = Persistent<Value>::New(callback);

    uv_work_t* req = new uv_work_t();
    req->data = listBaton;
    uv_queue_work(uv_default_loop(), req, FindAllAsync, (uv_after_work_cb)FindAllFinished);

    return scope.Close(v8::Undefined());
}

