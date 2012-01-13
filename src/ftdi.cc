#include <stdlib.h>
#include <iostream>
#include <v8.h>
#include <node.h>
#include "ftdi.h"

using namespace std;
using namespace v8;
using namespace node;
using namespace node_ftdi;

int vid = 0x0403;
int pid = 0x6001;
struct ftdi_context ftdic;

Persistent<FunctionTemplate> NodeFtdi::constructor_template;

// This is our bootstrap function, where we define what the module exposes
void NodeFtdi::Initialize(v8::Handle<v8::Object> target) {
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->SetClassName(String::NewSymbol("Ftdi"));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "setBaudrate", SetBaudrate);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", Write);

    NODE_SET_METHOD(constructor_template, "findAll", FindAll);
    
    target->Set(String::NewSymbol("Ftdi"), constructor_template->GetFunction());
}

Handle<Value> NodeFtdi::ThrowTypeError(std::string message) {
    return ThrowException(Exception::TypeError(String::New(message.c_str())));
}

Handle<Value> NodeFtdi::ThrowLastError(std::string message) {
    Local<String> msg = String::New(message.c_str());
    Local<String> str = String::New(ftdi_get_error_string(&ftdic));

    return ThrowException(Exception::Error(String::Concat(msg, str)));
}

// new NodeFtdi(vid, pid);
Handle<Value> NodeFtdi::New(const Arguments& args) {
    HandleScope scope;

    if(!args[0]->IsUndefined()) {
        vid = args[0]->Int32Value();
    }

    if(!args[1]->IsUndefined()) {
        pid = args[1]->Int32Value();
    }

    if (ftdi_init(&ftdic) < 0) {
        return NodeFtdi::ThrowLastError("Failed to init: ");
    }
    return scope.Close(args.This());
}

// TODO: Open could also be serial based
Handle<Value> NodeFtdi::Open(const Arguments& args) {
    int ret = ftdi_usb_open(&ftdic, vid, pid);

    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to open device: ");
    }

    return args.This();
}

Handle<Value> NodeFtdi::SetBaudrate(const Arguments& args) {
    //TODO: Add check that the device is open
    if (args.Length() < 1 || !args[0]->IsNumber()) {
        return NodeFtdi::ThrowTypeError("Ftdi.setBaudrate() expects an integer");
    }

    int baudrate = args[0]->Int32Value();
    int ret = ftdi_set_baudrate(&ftdic, baudrate);

    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to set baudrate: ");
    }

    return args.This();
}

Handle<Value> NodeFtdi::Write(const Arguments& args) {
    //TODO: Add check that the device is open
    if (args.Length() < 1 || !args[0]->IsString()) {
        return NodeFtdi::ThrowTypeError("Ftdi.write() expects a String() which length == 1");
    }

    std::string str(*String::Utf8Value(args[0]));
    if(str.length() > 1) {
        return NodeFtdi::ThrowTypeError("Ftdi.write() excepts only one character");
    }

    int ret = ftdi_write_data(&ftdic, (unsigned char*) str.c_str(), 1);
    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to write to device: ");
    }

    return Boolean::New(true);
}

Handle<Value> NodeFtdi::Close(const Arguments& args) {
    int ret = ftdi_usb_close(&ftdic);
    if (ret < 0) {
        return NodeFtdi::ThrowLastError("Failed to close device: ");
    }

    ftdi_deinit(&ftdic);

    return args.This();
}

Handle<Value> NodeFtdi::FindAll(const Arguments& args) {
    int count, i = 0;
    struct ftdi_device_list *devlist, *curdev;
    char manufacturer[128], description[128], serial[128];

    //TODO Parametize vid and pid
    if ((count = ftdi_usb_find_all(&ftdic, &devlist, vid, pid)) < 0) {
        return NodeFtdi::ThrowLastError("Unable to list devices: ");
    }

    Local<Array> array = Array::New(count);

    for (curdev = devlist; curdev != NULL; i++) {
        if (ftdi_usb_get_strings(&ftdic, curdev->dev, manufacturer, 128, description, 128, serial, 128) < 0) {
            return NodeFtdi::ThrowLastError("Unable to get information on devices: ");
        }

        Local<Object> obj = Object::New();
        obj->Set(String::New("manufacturer"), String::New(manufacturer));
        obj->Set(String::New("description"), String::New(description));
        obj->Set(String::New("serial"), String::New(serial));
        array->Set(i, obj);
        curdev = curdev->next;
    }

    ftdi_list_free(&devlist);

    return array;
}

extern "C" void init (Handle<Object> target) {
    NodeFtdi::Initialize(target);
}
