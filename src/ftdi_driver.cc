#include <stdlib.h>

#include "ftdi_driver.h"
#include "ftdi_constants.h"

using namespace v8;
using namespace node;

#define DEVICE_INFO_STRING_LENGTH 128

/**********************************
 * Local typedefs
 **********************************/
struct DeviceInfo
{
    char description[DEVICE_INFO_STRING_LENGTH];
    char manufacturer[DEVICE_INFO_STRING_LENGTH];
    char serial[DEVICE_INFO_STRING_LENGTH];
    struct DeviceInfo *next;

    DeviceInfo()
    {
        next = NULL;
    }
};

struct DeviceListBaton
{
    Persistent<Value> callback;
    struct DeviceInfo *devInfoList;
    int listLength;
    int status;
    int vid;
    int pid;

    DeviceListBaton()
    {
        devInfoList = NULL;
    }
};


/**********************************
 * Local Helper Functions protoypes
 **********************************/



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

    // NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);

    NODE_SET_METHOD(constructor_template, "findAll", FindAll);
    target->Set(String::NewSymbol("FtdiDriver"), constructor_template->GetFunction());

    uv_mutex_init(&vidPidMutex);
}

void FindAllAsync(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    int ret;
    int numDevices;
    struct ftdi_context *ftdi;
    struct ftdi_device_list *devlist, *curdev;
    int retval = EXIT_SUCCESS;

    // Lock Readout
    uv_mutex_lock(&vidPidMutex);
    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        retval = EXIT_FAILURE;
    }

    ftdi_set_interface(ftdi, INTERFACE_ANY);
    if ((numDevices = ftdi_usb_find_all(ftdi, &devlist, listBaton->vid, listBaton->pid)) < 0)
    {
        fprintf(stderr, "ftdi_usb_find_all failed: %d (%s)\n", numDevices, ftdi_get_error_string(ftdi));
        retval =  EXIT_FAILURE;
    }

    printf("Number of FTDI devices found: %d\n", numDevices);

    if(retval != EXIT_FAILURE)
    {
        listBaton->devInfoList = new DeviceInfo();
        DeviceInfo * currentItem = listBaton->devInfoList;
        for (curdev = devlist; curdev != NULL; )
        {
            if(currentItem == NULL)
            {
                currentItem = new DeviceInfo();
            }

            if ((ret = ftdi_usb_get_strings(ftdi, curdev->dev, currentItem->manufacturer, DEVICE_INFO_STRING_LENGTH, currentItem->description, DEVICE_INFO_STRING_LENGTH, currentItem->serial, DEVICE_INFO_STRING_LENGTH)) < 0)
            {
                fprintf(stderr, "ftdi_usb_get_strings failed: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
                retval = EXIT_FAILURE;
            }
            curdev = curdev->next;
            currentItem = currentItem->next;
        }
    }

    ftdi_list_free(&devlist);
    ftdi_free(ftdi);
    uv_mutex_unlock(&vidPidMutex);  

    listBaton->listLength = numDevices;              
    listBaton->status = retval;
}

void FindAllFinished(uv_work_t* req)
{
    DeviceListBaton* listBaton = static_cast<DeviceListBaton*>(req->data);

    Handle<Value> argv[2];

    if(listBaton->status == 0 && listBaton->listLength > 0)
    {

        Local<Array> array= Array::New(listBaton->listLength);
        
        DeviceInfo * currentItem;
        int index = 0;
        for (currentItem = listBaton->devInfoList; currentItem != NULL; ) 
        {
            Local<Object> obj = Object::New();
            obj->Set(String::New(DEVICE_DESCRIPTION_TAG), String::New(currentItem->description));
            obj->Set(String::New(DEVICE_SERIAL_NR_TAG), String::New(currentItem->serial));
            obj->Set(String::New(DEVICE_LOCATION_ID_TAG), String::New(currentItem->manufacturer));
            obj->Set(String::New(DEVICE_VENDOR_ID_TAG), Number::New(listBaton->vid));
            obj->Set(String::New(DEVICE_PRODUCT_ID_TAG), Number::New(listBaton->pid));
            array->Set(index++, obj);
            DeviceInfo * temp = currentItem;
            currentItem = currentItem->next;
            delete temp;
        }

        argv[1] = array;
    }
    else
    {
        argv[1] = Undefined();
    }
    argv[0] = Number::New(listBaton->status);

    Function::Cast(*listBaton->callback)->Call(Context::GetCurrent()->Global(), 2, argv);

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
