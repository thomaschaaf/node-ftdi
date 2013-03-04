#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

#include "node_ftdi.h"

using namespace std;
using namespace v8;
using namespace node;
using namespace node_ftdi;

struct params p;
struct ftdi_context ftdic;
Persistent<FunctionTemplate> NodeFtdi::constructor_template;

const char* ToCString(Local<String> val) {
    return *String::Utf8Value(val);
}

Handle<Value> NodeFtdi::ThrowTypeError(std::string message) {
    return ThrowException(Exception::TypeError(String::New(message.c_str())));
}

Handle<Value> NodeFtdi::ThrowLastError(std::string message) {
    Local<String> msg = String::New(message.c_str());
    Local<String> str = String::New(ftdi_get_error_string(&ftdic));

    return ThrowException(Exception::Error(String::Concat(msg, str)));
}

void NodeFtdi::Initialize(v8::Handle<v8::Object> target) {
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->SetClassName(String::NewSymbol("Ftdi"));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "setBaudrate", SetBaudrate);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "setLineProperty", SetLineProperty);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "setBitmode", SetBitmode);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "read", Read);

    NODE_SET_METHOD(constructor_template, "findAll", FindAll);
    
    target->Set(String::NewSymbol("Ftdi"), constructor_template->GetFunction());
}

Handle<Value> NodeFtdi::New(const Arguments& args) {
    HandleScope scope;
    Local<String> vid = String::New("vid");
    Local<String> pid = String::New("pid");
    Local<String> description = String::New("description");
    Local<String> serial = String::New("serial");
    Local<String> index = String::New("index");

    p.vid = FTDI_VID;
    p.pid = FTDI_PID;
    p.description = NULL;
    p.serial = NULL;
    p.index = 0;

    if(args[0]->IsObject()) {
        Local<Object> obj = args[0]->ToObject();

        if(obj->Has(vid)) {
            p.vid = obj->Get(vid)->Int32Value();
        }
        if(obj->Has(pid)) {
            p.pid = obj->Get(pid)->Int32Value();
        }
        if(obj->Has(description)) {
            p.description = ToCString(obj->Get(description)->ToString());
        }
        if(obj->Has(serial)) {
            p.serial = ToCString(obj->Get(serial)->ToString());
        }
        if(obj->Has(index)) {
            p.index = (unsigned int) obj->Get(index)->Int32Value();
        }
    }

    if (ftdi_init(&ftdic) < 0) {
        return NodeFtdi::ThrowLastError("Failed to init: ");
    }

    return scope.Close(args.This());
}

Handle<Value> NodeFtdi::Open(const Arguments& args) {
    int ret = ftdi_usb_open_desc_index(&ftdic,
        p.vid,
        p.pid,
        p.description,
        p.serial,
        p.index);

    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to open device: ");
    }

    return args.This();
}

Handle<Value> NodeFtdi::SetBaudrate(const Arguments& args) {
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

Handle<Value> NodeFtdi::SetLineProperty(const Arguments& args) {
    if (args.Length() < 3 || !args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
        return NodeFtdi::ThrowTypeError("Ftdi.setLineProperty(bits, stopbits, parity) expects an 3 integers");
    }

    int bits     = args[0]->Uint32Value();
    int stopbits = args[1]->Uint32Value();
    int parity   = args[2]->Uint32Value();

    int ret = ftdi_set_line_property(&ftdic,
        ftdi_bits_type(bits),
        ftdi_stopbits_type(stopbits),
        ftdi_parity_type(parity));

    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to set line property: ");
    }

    return args.This();
}

Handle<Value> NodeFtdi::SetBitmode(const Arguments& args) {
    if (args.Length() < 3 || !args[0]->IsNumber() || !args[1]->IsNumber()) {
        return NodeFtdi::ThrowTypeError("Ftdi.setBitmode(mask, mode) expects an 2 integers");
    }

    int mask = args[0]->Uint32Value();
    int mode = args[1]->Uint32Value();

    int ret = ftdi_set_bitmode(&ftdic, mask, mode);

    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to set bit mode: ");
    }

    return Number::New(ret);
}

Handle<Value> NodeFtdi::Write(const Arguments& args) {
    if (args.Length() < 1 || !args[0]->IsString()) {
        return NodeFtdi::ThrowTypeError("Ftdi.write() expects a string");
    }

    std::string str(*String::Utf8Value(args[0]));

    int ret = ftdi_write_data(&ftdic, (unsigned char*) str.c_str(), str.size());
    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to write to device: ");
    }

    return Number::New(ret);
}

Handle<Value> NodeFtdi::Read(const Arguments& args) {
    uint8_t buf[1];
    int ret = ftdi_read_data(&ftdic, buf, 1);
    if(ret < 0) {
        return NodeFtdi::ThrowLastError("Unable to read from device: ");
    }
    return String::New((const char*) buf);
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

    int vid = args[0]->IsUndefined() ? FTDI_VID : args[0]->Int32Value();
    int pid = args[1]->IsUndefined() ? FTDI_PID : args[1]->Int32Value();

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
