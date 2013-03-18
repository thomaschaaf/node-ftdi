#ifndef NODE_FTDI_PLATFORM_H
#define NODE_FTDI_PLATFORM_H

#include <v8.h>
#include <ftd2xx.h>

using namespace v8;

struct ReadBaton 
{
  FT_HANDLE ftHandle;
#ifndef WIN32
  EVENT_HANDLE eh;
#else
  HANDLE hEvent;
#endif
  uint8_t* readData;
  size_t bufferLength;
  Persistent<Value> callback;
  int result;
};

void Platform_SetVidPid(DWORD vid, DWORD pid);
void PrepareAsyncRead(ReadBaton *baton);
void ReadDataAsync(uv_work_t* req);

#endif