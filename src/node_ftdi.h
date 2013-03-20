#ifndef NODE_FTDI_H
#define NODE_FTDI_H

#include <v8.h>
#include <node.h>
#include <ftd2xx.h>

using namespace v8;
using namespace node;

namespace node_ftdi 
{

typedef enum 
{
    ConnectType_ByIndex,
    ConnectType_BySerial,
    ConnectType_ByDescription,
    ConnectType_ByLocationId,
} ConnectType_t;

typedef struct 
{
    ConnectType_t connectType;
    char *connectString;
    int32_t connectId;
} ConnectionParams_t;

typedef struct 
{
    int baudRate;
    UCHAR wordLength;
    UCHAR stopBits;
    UCHAR parity;
} DeviceParams_t;

class NodeFtdi : public ObjectWrap 
{
    public:
        static void Initialize(Handle<Object> target);

    protected:
        NodeFtdi();
        ~NodeFtdi();

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Open(const Arguments& args);
        static Handle<Value> Write(const Arguments& args);
        static Handle<Value> Close(const Arguments& args);

        static Handle<Value> ThrowTypeError(std::string message);
        static Handle<Value> ThrowLastError(std::string message);

        static Handle<Value> RegisterDataCallback(const Arguments& args);

    private:
        static void ReadDataAsync(uv_work_t* req);
        static void ReadCallback(uv_work_t* req);

        static void OpenAsync(uv_work_t* req);
        static void OpenFinished(uv_work_t* req);

        static void WriteAsync(uv_work_t* req);
        static void WriteFinished(uv_work_t* req);

        static void CloseAsync(uv_work_t* req);
        static void CloseFinished(uv_work_t* req);
        
        void ExtractDeviceSettings(Local<v8::Object> options);
        FT_STATUS SetDeviceSettings();
        FT_STATUS OpenDevice();

        FT_HANDLE ftHandle;
        DeviceParams_t deviceParams;
        ConnectionParams_t connectParams;

        bool isClosing;
        void* syncContext;
};

}
#endif