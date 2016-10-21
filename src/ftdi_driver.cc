#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#include "ftdi_driver.h"
#include "ftdi_constants.h"

using namespace std;
using namespace v8;
using namespace node;


/**********************************
 * Local typedefs
 **********************************/
struct DeviceListBaton
{
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


#define JS_CLASS_NAME "FtdiDriver"


/**********************************
 * Functions
 **********************************/
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

DeviceListBaton* FindAllAsync(int vid, int pid)
{
  DeviceListBaton* listBaton = new DeviceListBaton();
  FT_STATUS ftStatus;
  DWORD numDevs = 0;

  // Lock Readout
  uv_mutex_lock(&vidPidMutex);

#ifndef WIN32
  if(vid != 0 && pid != 0)
  {
    uv_mutex_lock(&libraryMutex);
    ftStatus = FT_SetVIDPID(vid, pid);
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
  return listBaton;
}

class FindAllWorker : public Nan::AsyncWorker {
 public:
  FindAllWorker(Nan::Callback *callback, int vid, int pid)
    : Nan::AsyncWorker(callback), vid(vid), pid(pid) {}
  ~FindAllWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    listBaton = FindAllAsync(vid, pid);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    Nan::HandleScope scope;

    v8::Local<v8::Value> argv[2];
    if(listBaton->status == FT_OK)
    {
      // Determine the length of the resulting list
      int resultListLength = 0;
      for (DWORD i = 0; i < listBaton->listLength; i++)
      {
        if(DeviceMatchesFilterCriteria(&listBaton->devInfo[i], vid, pid))
        {
          resultListLength++;
        }
      }

      // Create Java Script Array for the resulting devices
      Local<Array> array= Nan::New<Array>(resultListLength);

      int index = 0;
      for (DWORD i = 0; i < listBaton->listLength; i++)
      {
        // Add device to the array in case it matches the criteria
        if(DeviceMatchesFilterCriteria(&listBaton->devInfo[i], vid, pid))
        {
          Local<Object> obj = Nan::New<Object>();
          obj->Set(Nan::New<String>(DEVICE_DESCRIPTION_TAG).ToLocalChecked(), Nan::New<String>(listBaton->devInfo[i].Description).ToLocalChecked());
          obj->Set(Nan::New<String>(DEVICE_SERIAL_NR_TAG).ToLocalChecked(), Nan::New<String>(listBaton->devInfo[i].SerialNumber).ToLocalChecked());
          obj->Set(Nan::New<String>(DEVICE_LOCATION_ID_TAG).ToLocalChecked(), Nan::New<Number>(listBaton->devInfo[i].LocId));
          obj->Set(Nan::New<String>(DEVICE_INDEX_TAG).ToLocalChecked(), Nan::New<Number>(i));
          obj->Set(Nan::New<String>(DEVICE_VENDOR_ID_TAG).ToLocalChecked(), Nan::New<Number>( (listBaton->devInfo[i].ID >> 16) & (0xFFFF)));
          obj->Set(Nan::New<String>(DEVICE_PRODUCT_ID_TAG).ToLocalChecked(), Nan::New<Number>( (listBaton->devInfo[i].ID) & (0xFFFF)));
          array->Set(index++, obj);
        }
      }

      argv[0] = Nan::Undefined();
      argv[1] = array;
    }
    // something went wrong, return the error string
    else
    {
      argv[0] = Nan::New<String>(GetStatusString(listBaton->status)).ToLocalChecked();
      argv[1] = Nan::Undefined();
    }

    callback->Call(2, argv);

    if(listBaton->devInfo != NULL)
    {
      delete listBaton->devInfo;
    }
    delete listBaton;
  };

 private:
  int vid;
  int pid;
  DeviceListBaton* listBaton;
};

NAN_METHOD(FindAll) {
  Nan::HandleScope scope;

  int vid = 0;
  int pid = 0;
  Nan::Callback *callback;

  if (info.Length() != 3)
  {
    return Nan::ThrowError("Wrong number of arguments");
  }
  if (info[0]->IsNumber() && info[1]->IsNumber())
  {
    vid = (int) info[0]->NumberValue();
    pid = (int) info[1]->NumberValue();
  }

  // callback
  if(!info[2]->IsFunction())
  {
    return Nan::ThrowError("Third argument must be a function");
  }

  callback = new Nan::Callback(info[2].As<v8::Function>());

  Nan::AsyncQueueWorker(new FindAllWorker(callback, vid, pid));
  return;
}

void InitializeList(Handle<Object> target)
{
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>();
  tpl->SetClassName(Nan::New<String>(JS_CLASS_NAME).ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  tpl->Set(
      Nan::New<String>("findAll").ToLocalChecked()
    , Nan::New<FunctionTemplate>(FindAll)
  );

  target->Set(Nan::New<String>(JS_CLASS_NAME).ToLocalChecked(), tpl->GetFunction());

  uv_mutex_init(&vidPidMutex);
}
