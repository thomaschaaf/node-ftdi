#include <v8.h>
#include <node.h>

using namespace v8;
using namespace node;

class Hello : ObjectWrap {
    public:
        static void Initialize(Handle<Object> target);

    protected:
        static Persistent<FunctionTemplate> constructor_template;

        Hello();
        ~Hello();

        static Handle<Value> New(const Arguments& args);
        static Handle<Value> Say(const Arguments& args);
};
