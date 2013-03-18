#ifndef NODE_FTDI_H
#define NODE_FTDI_H

#include <v8.h>
#include <node.h>
#include <ftd2xx.h>
#include <node_buffer.h>

using namespace v8;
using namespace node;

#define FTDI_VID    0x0403
#define FTDI_PID    0x6001

namespace node_ftdi 
{

struct params 
{
    int vid;
    int pid;
    const char* description;
    const char* serial;
    unsigned int index;
};

class NodeFtdi : public ObjectWrap 
{
    public:
        static void Initialize(Handle<Object> target);
        // static Handle<Value> FindAll(const Arguments& args);
    protected:

        NodeFtdi();
        ~NodeFtdi();

        // static Handle<Value> SetBaudrate(const Arguments& args);
        // static Handle<Value> SetLineProperty(const Arguments& args);
        // static Handle<Value> SetBitmode(const Arguments& args);

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Open(const Arguments& args);
        static Handle<Value> Write(const Arguments& args);
        // static Handle<Value> Read(const Arguments& args);
        // static Handle<Value> Close(const Arguments& args);

        static Handle<Value> ThrowTypeError(std::string message);
        static Handle<Value> ThrowLastError(std::string message);

        static Handle<Value> RegisterDataCallback(const Arguments& args);

    private:
        static Persistent<Object> module_handle;
        static Persistent<String> callback_symbol;
        static void ReadCallback(uv_work_t* req);
        void SetDeviceSettings(Local<v8::Object> options);

        Persistent<Value> dataCallback;
        FT_HANDLE ftHandle;
        struct params p;
};

}
#endif