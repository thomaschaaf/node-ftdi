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

class FindAllWorker : public NanAsyncWorker {
 public:
  FindAllWorker(NanCallback *callback, int vid, int pid)
    : NanAsyncWorker(callback), vid(vid), pid(pid) {}
  ~FindAllWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    cout << "findallworker";
    listBaton = FindAllAsync(vid, pid);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    NanScope();

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
      Local<Array> array= NanNew<Array>(resultListLength);

      int index = 0;
      for (DWORD i = 0; i < listBaton->listLength; i++)
      {
        // Add device to the array in case it matches the criteria
        if(DeviceMatchesFilterCriteria(&listBaton->devInfo[i], vid, pid))
        {
          Local<Object> obj = NanNew<Object>();
          obj->Set(NanNew<String>(DEVICE_DESCRIPTION_TAG), NanNew<String>(listBaton->devInfo[i].Description));
          obj->Set(NanNew<String>(DEVICE_SERIAL_NR_TAG), NanNew<String>(listBaton->devInfo[i].SerialNumber));
          obj->Set(NanNew<String>(DEVICE_LOCATION_ID_TAG), NanNew<Number>(listBaton->devInfo[i].LocId));
          obj->Set(NanNew<String>(DEVICE_INDEX_TAG), NanNew<Number>(i));
          obj->Set(NanNew<String>(DEVICE_VENDOR_ID_TAG), NanNew<Number>( (listBaton->devInfo[i].ID >> 16) & (0xFFFF)));
          obj->Set(NanNew<String>(DEVICE_PRODUCT_ID_TAG), NanNew<Number>( (listBaton->devInfo[i].ID) & (0xFFFF)));
          array->Set(index++, obj);
        }
      }

      argv[0] = NanUndefined();
      argv[1] = array;
    }
    // something went wrong, return the error string
    else
    {
      argv[0] = NanNew<String>(GetStatusString(listBaton->status));
      argv[1] = NanUndefined();
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
  NanScope();

  cout << "findallmethod";

  int vid = 0;
  int pid = 0;
  NanCallback *callback;

  if (args.Length() != 3)
  {
    return NanThrowError("Wrong number of arguments");
  }
  if (args[0]->IsNumber() && args[1]->IsNumber())
  {
    vid = (int) args[0]->NumberValue();
    pid = (int) args[1]->NumberValue();
  }

  // callback
  if(!args[2]->IsFunction())
  {
    return NanThrowError("Third argument must be a function");
  }

  callback = new NanCallback(args[2].As<v8::Function>());

  NanAsyncQueueWorker(new FindAllWorker(callback, vid, pid));
  NanReturnUndefined();
}

void InitializeList(Handle<Object> target)
{
  Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>();
  tpl->SetClassName(NanNew<String>(JS_CLASS_NAME));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  tpl->Set(
      NanNew<String>("findAll")
    , NanNew<FunctionTemplate>(FindAll)->GetFunction()
  );

  target->Set(NanNew<String>(JS_CLASS_NAME), tpl->GetFunction());

  uv_mutex_init(&vidPidMutex);
}