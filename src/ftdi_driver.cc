#include <stdlib.h>
#include <ftd2xx.h>

#include "ftdi_driver.h"
#include "ftdi_constants.h"

using namespace v8;
using namespace node;

static uv_mutex_t listMutex;


struct DeviceListBaton
{
    Persistent<Value> callback;
    FT_DEVICE_LIST_INFO_NODE *devInfo;
    DWORD listLength;
    FT_STATUS status;
    int vid;
    int pid;
};

void InitializeList(Handle<Object> target) 
{
    Persistent<FunctionTemplate> constructor_template;
    Local<FunctionTemplate> t = FunctionTemplate::New();
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->SetClassName(String::NewSymbol("FtdiDriver"));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

    // NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);

    NODE_SET_METHOD(constructor_template, "findAll", FindAll);
    target->Set(String::NewSymbol("FtdiDriver"), constructor_template->GetFunction());

    uv_mutex_init(&listMutex);
}

void FindAllAsync(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    FT_STATUS ftStatus;
    DWORD numDevs = 0;

    // create the device information list
    uv_mutex_lock(&listMutex);

#ifndef WIN32
    if(listBaton->vid != 0 && listBaton->pid != 0)
    {
        FT_SetVIDPID(listBaton->vid, listBaton->pid);
    }
#endif

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
        for (DWORD i = 0; i < listBaton->listLength; i++) 
        {
            Local<Object> obj = Object::New();
            obj->Set(String::New(DEVICE_DESCRIPTION_TAG), String::New(listBaton->devInfo[i].Description));
            obj->Set(String::New(DEVICE_SERIAL_NR_TAG), String::New(listBaton->devInfo[i].SerialNumber));
            obj->Set(String::New(DEVICE_LOCATION_ID_TAG), Number::New(listBaton->devInfo[i].LocId));
            obj->Set(String::New(DEVICE_VENDOR_ID_TAG), Number::New( (listBaton->devInfo[i].ID >> 16) & (0xFFFF)));
            obj->Set(String::New(DEVICE_PRODUCT_ID_TAG), Number::New( (listBaton->devInfo[i].ID) & (0xFFFF)));
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

    int vid = 0;
    int pid = 0;

    if (args.Length() != 3) 
    {
        ThrowException(Exception::TypeError(String::New("Wrong number of Arguments")));
    }
    if (args[0]->IsNumber() && args[1]->IsNumber()) 
    {
        vid = (int) args[0]->NumberValue();
        pid = (int) args[1]->NumberValue();
    }

    // callback
    if(!args[2]->IsFunction()) 
    {
        return scope.Close(v8::ThrowException(v8::Exception::TypeError(v8::String::New("Third argument must be a function"))));
    }
    Local<Value> callback = args[2];

    DeviceListBaton* listBaton = new DeviceListBaton();
    listBaton->callback = Persistent<Value>::New(callback);
    listBaton->vid = vid;
    listBaton->pid = pid;

    uv_work_t* req = new uv_work_t();
    req->data = listBaton;
    uv_queue_work(uv_default_loop(), req, FindAllAsync, (uv_after_work_cb)FindAllFinished);

    return scope.Close(v8::Undefined());
}