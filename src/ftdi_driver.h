#ifndef FTDI_DRIVER_H
#define FTDI_DRIVER_H

#include <v8.h>
#include <node.h>
#include <ftd2xx.h>

using namespace v8;
using namespace node;


void InitializeList(Handle<Object> target);

Handle<Value> FindAll(const Arguments& args);
Handle<Value> SetVidPid(const Arguments& args);

#endif