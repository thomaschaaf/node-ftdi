#ifndef NODE_FTDI_PLATFORM_H
#define NODE_FTDI_PLATFORM_H

#include <ftd2xx.h>
#include <v8.h>

struct ReadBaton 
{
  FT_HANDLE ftHandle;
  EVENT_HANDLE eh;
  uint8_t* readData;
  size_t bufferLength;
  v8::Persistent<v8::Value> callback;
  int result;
};

void ReadDataAsync(uv_work_t* req);

#endif