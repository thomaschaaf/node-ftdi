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
    
    target->Set(String::NewSymbol("Ftdi"), constructor_template->GetFunction());
}


Handle<Value> NodeFtdi::New(const Arguments& args) {
    HandleScope scope;

    if (ftdi_init(&ftdic) < 0) {
        return v8::ThrowException(v8::Exception::Error(v8::String::New("Failed to init")));
    }
    return scope.Close(args.This());
}

Handle<Value> NodeFtdi::Open(const Arguments& args) {
    int ret = ftdi_usb_open(&ftdic, 0x0403, 0x6001);

    if(ret < 0) {
        // TODO: Not a TypeError
        // TODO: Use error to have a more compelling error message
        return ThrowException(Exception::Error(String::New("Unable to open device")));
    }

    return args.This();
}

extern "C" void init (Handle<Object> target) {
    NodeFtdi::Initialize(target);
}
