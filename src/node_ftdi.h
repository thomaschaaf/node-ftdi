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

typedef enum 
{
    BatonType_Read,
    BatonType_Open,
} BatonType_t;

typedef struct 
{
    ConnectType_t connectType;
    const char *connectString;
    int32_t connectId;
} ConnectionParams_t;

typedef struct 
{
    int baudRate;
    UCHAR wordLength;
    UCHAR stopBits;
    UCHAR parity;
} DeviceParams_t;

typedef struct 
{
    Persistent<Value> callback; 
    FT_STATUS status;
} OpenBaton_t;

typedef struct  
{
#ifndef WIN32
    EVENT_HANDLE eh;
#else
    HANDLE hEvent;
#endif
    uint8_t* readData;
    int32_t bufferLength;
    Persistent<Value> callback;
    FT_STATUS status;
} ReadBaton_t;

class NodeFtdi : public ObjectWrap 
{
    public:
        static void Initialize(Handle<Object> target);
        
        void ReadDataAsync();
        FT_STATUS OpenDevice();
        FT_STATUS SetDeviceSettings();
        void SetBatonStatus(BatonType_t baton, FT_STATUS status);

    protected:
        NodeFtdi();
        ~NodeFtdi();

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Open(const Arguments& args);
        static Handle<Value> Write(const Arguments& args);

        static Handle<Value> ThrowTypeError(std::string message);
        static Handle<Value> ThrowLastError(std::string message);

        static Handle<Value> RegisterDataCallback(const Arguments& args);

    private:
        static void ReadCallback(uv_work_t* req);
        static void OpenFinished(uv_work_t* req);
        
        void ExtractDeviceSettings(Local<v8::Object> options);

        static Persistent<Object> module_handle;
        static Persistent<String> callback_symbol;

        FT_HANDLE ftHandle;
        DeviceParams_t deviceParams;
        ConnectionParams_t connectParams;
        ReadBaton_t readBaton;
        OpenBaton_t openBaton;
};

}
#endif