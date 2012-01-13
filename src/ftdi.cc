#include <v8.h>
#include <node.h>
#include "ftdi.h"

using namespace v8;
using namespace node;
using namespace node_ftdi;

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
    
    target->Set(String::NewSymbol("Ftdi"), constructor_template->GetFunction());
}


Handle<Value> NodeFtdi::New(const Arguments& args) {
    HandleScope scope;

    if (ftdi_init(&ftdic) < 0) {
        return ThrowException(v8::Exception::Error(v8::String::New("Failed to init")));
    }
    return scope.Close(args.This());
}

Handle<Value> NodeFtdi::Open(const Arguments& args) {
    int ret = ftdi_usb_open(&ftdic, 0x0403, 0x6001);

    if(ret < 0) {
        // TODO: Use error to have a more compelling error message
        return ThrowException(Exception::Error(String::New("Unable to open device")));
    }

    return args.This();
}

Handle<Value> NodeFtdi::SetBaudrate(const Arguments& args) {
    if (args.Length() < 1 || !args[0]->IsNumber()) {
        return ThrowException(Exception::TypeError(String::New("Ftdi.setBaudrate() expects an integer")));
    }

    int baudrate = args[0]->Int32Value();
    int ret = ftdi_set_baudrate(&ftdic, baudrate);

    if(ret < 0) {
        // TODO: Use error to have a more compelling error message
        return ThrowException(Exception::Error(String::New("Unable to set baudrate")));
    }

    return args.This();
}

Handle<Value> NodeFtdi::Close(const Arguments& args) {
    int ret = ftdi_usb_close(&ftdic);

    if (ret < 0) {
        return ThrowException(v8::Exception::Error(v8::String::New("Failed to close device")));
    }

    ftdi_deinit(&ftdic);

    return args.This();
}

extern "C" void init (Handle<Object> target) {
    NodeFtdi::Initialize(target);
}
