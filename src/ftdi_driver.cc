#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ftdi_driver.h"
#include "ftdi_constants.h"

using namespace v8;
using namespace node;

/**********************************
 * Local typedefs
 **********************************/
struct DeviceListBaton
{
    Persistent<Value> callback;
    FT_DEVICE_LIST_INFO_NODE *devInfo;
    DWORD listLength;
    FT_STATUS status;
    int vid;
    int pid;

    DeviceListBaton()
    {
        devInfo = NULL;
    }
};


/**********************************
 * Local Helper Functions protoypes
 **********************************/
bool DeviceMatchesFilterCriteria(FT_DEVICE_LIST_INFO_NODE *devInfo, int filterVid, int filterPid);



/**********************************
 * Local Variables
 **********************************/
uv_mutex_t libraryMutex;
uv_mutex_t vidPidMutex;



/**********************************
 * Functions
 **********************************/
void InitializeList(Handle<Object> target) 
{
    Persistent<FunctionTemplate> constructor_template;
    Local<FunctionTemplate> t = FunctionTemplate::New();
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->SetClassName(String::NewSymbol("FtdiDriver"));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_METHOD(constructor_template, "findAll", FindAll);
    target->Set(String::NewSymbol("FtdiDriver"), constructor_template->GetFunction());

    uv_mutex_init(&vidPidMutex);
}

void FindAllAsync(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    FT_STATUS ftStatus;
    DWORD numDevs = 0;

    // Lock Readout
    uv_mutex_lock(&vidPidMutex);

#ifndef WIN32
    if(listBaton->vid != 0 && listBaton->pid != 0)
    {
        uv_mutex_lock(&libraryMutex);  
        ftStatus = FT_SetVIDPID(listBaton->vid, listBaton->pid);
        uv_mutex_unlock(&libraryMutex);  
    }
#endif
    // create the device information list
    uv_mutex_lock(&libraryMutex);  
    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    uv_mutex_unlock(&libraryMutex);
    if (ftStatus == FT_OK) 
    {
        if (numDevs > 0) 
        {
            // allocate storage for list based on numDevs
            listBaton->devInfo =  new FT_DEVICE_LIST_INFO_NODE[numDevs]; 
            memset(listBaton->devInfo, 0, sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
            
            // get the device information list
            uv_mutex_lock(&libraryMutex);
            ftStatus = FT_GetDeviceInfoList(listBaton->devInfo, &numDevs); 
            uv_mutex_unlock(&libraryMutex);

            // fallback for wrong info in several cases... when connected multiple devices and unplug one...
            for(DWORD i = 0; i < numDevs; i++)
            {
                uv_mutex_lock(&libraryMutex);
                FT_ListDevices((PVOID)i, listBaton->devInfo[i].SerialNumber, FT_LIST_BY_INDEX | FT_OPEN_BY_SERIAL_NUMBER);
                FT_ListDevices((PVOID)i, listBaton->devInfo[i].Description, FT_LIST_BY_INDEX | FT_OPEN_BY_DESCRIPTION);
                FT_ListDevices((PVOID)i, &listBaton->devInfo[i].LocId, FT_LIST_BY_INDEX | FT_OPEN_BY_LOCATION);
                uv_mutex_unlock(&libraryMutex);
            }
        }
    }
    uv_mutex_unlock(&vidPidMutex);  

    listBaton->listLength = numDevs;              
    listBaton->status = ftStatus;
}

void FindAllFinished(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    //printf("Num DevFound: %d, status: %d\r\n", listBaton->listLength, listBaton->status);

    Handle<Value> argv[2];

    if(listBaton->status == FT_OK)
    {
        // Determine the length of the resulting list 
        int resultListLength = 0;
        for (DWORD i = 0; i < listBaton->listLength; i++) 
        {
            if(DeviceMatchesFilterCriteria(&listBaton->devInfo[i], listBaton->vid, listBaton->pid))
            {
                resultListLength++;
            }
        }

        // Create Java Script Array for the resulting devices
        Local<Array> array= Array::New(resultListLength);
        
        int index = 0;
        for (DWORD i = 0; i < listBaton->listLength; i++) 
        {
            // Add device to the array in case it matches the criteria
            if(DeviceMatchesFilterCriteria(&listBaton->devInfo[i], listBaton->vid, listBaton->pid))
            {
                Local<Object> obj = Object::New();
                obj->Set(String::New(DEVICE_DESCRIPTION_TAG), String::New(listBaton->devInfo[i].Description));
                obj->Set(String::New(DEVICE_SERIAL_NR_TAG), String::New(listBaton->devInfo[i].SerialNumber));
                obj->Set(String::New(DEVICE_LOCATION_ID_TAG), Number::New(listBaton->devInfo[i].LocId));
                obj->Set(String::New(DEVICE_INDEX_TAG), Number::New(i));
                obj->Set(String::New(DEVICE_VENDOR_ID_TAG), Number::New( (listBaton->devInfo[i].ID >> 16) & (0xFFFF)));
                obj->Set(String::New(DEVICE_PRODUCT_ID_TAG), Number::New( (listBaton->devInfo[i].ID) & (0xFFFF)));
                array->Set(index++, obj);
            }
        }

        argv[0] = Undefined();
        argv[1] = array;
    }
    // something went wrong, return the error string
    else
    {
        argv[0] = String::New(GetStatusString(listBaton->status));
        argv[1] = Undefined();
    }

    Function::Cast(*listBaton->callback)->Call(Context::GetCurrent()->Global(), 2, argv);

    if(listBaton->devInfo != NULL)
    {
        delete listBaton->devInfo;
    }
    listBaton->callback.Dispose();
    delete listBaton;
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

bool DeviceMatchesFilterCriteria(FT_DEVICE_LIST_INFO_NODE *devInfo, int filterVid, int filterPid)
{
    int devVid = (devInfo->ID >> 16) & (0xFFFF);
    int devPid = (devInfo->ID) & (0xFFFF); 

    if(filterVid == 0 && filterPid == 0)
    {
        return true;
    }
    if(filterVid != 0 && filterVid != devVid)
    {
        return false;
    }
    if(filterPid != 0 && filterPid != devPid)
    {
        return false;
    }
    return true;
}

