#include <v8.h>
#include <node.h>
#include <ftdi.h>

using namespace v8;
using namespace node;

namespace node_ftdi {

struct params {
    int vid;
    int pid;
    const char* description;
    const char* serial;
    unsigned int index;
};

class NodeFtdi : ObjectWrap {
    public:
        static void Initialize(Handle<Object> target);
    protected:
        static Persistent<FunctionTemplate> constructor_template;

        NodeFtdi();
        ~NodeFtdi();

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Open(const Arguments& args);
        static Handle<Value> SetBaudrate(const Arguments& args);
        static Handle<Value> Write(const Arguments& args);
        static Handle<Value> Close(const Arguments& args);
        static Handle<Value> FindAll(const Arguments& args);

        static Handle<Value> ThrowTypeError(std::string message);
        static Handle<Value> ThrowLastError(std::string message);
};

}