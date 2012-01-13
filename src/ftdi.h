#include <v8.h>
#include <node.h>
#include <ftdi.h>

using namespace v8;
using namespace node;

namespace node_ftdi {

class NodeFtdi : ObjectWrap {
    public:
        static void Initialize(Handle<Object> target);
    protected:
        static Persistent<FunctionTemplate> constructor_template;

        NodeFtdi();
        ~NodeFtdi();

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Open(const Arguments& args);
};

}