#ifndef NODE_FTDI_H
#define NODE_FTDI_H

#include <v8.h>
#include <node.h>
#include <ftdi.h>

using namespace v8;
using namespace node;

namespace node_ftdi 
{

typedef struct ConnectionParams_t
{
    char *description;
    char *serial;
    int pid;
    int vid;

    ConnectionParams_t()
    {
        description = NULL;
        serial = NULL;
    }

} ConnectionParams_t;

typedef struct 
{
    int baudRate;
    ftdi_bits_type wordLength;
    ftdi_stopbits_type stopBits;
    ftdi_parity_type parity;
} DeviceParams_t;

typedef enum
{
    DeviceState_Idle,
    DeviceState_Open,
    DeviceState_Closing
} DeviceState_t;

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
        int SetDeviceSettings();
        int OpenDevice();

        struct ftdi_context *ftdi;
        DeviceParams_t deviceParams;
        ConnectionParams_t connectParams;

        DeviceState_t deviceState;
        void* syncContext;
        uv_mutex_t closeMutex;
        bool closed;
};

}
#endif