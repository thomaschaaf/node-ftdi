#include <v8.h>
#include <node.h>

#include "hello.h";

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> Hello::constructor_template;

// This is our bootstrap function, where we define what the module exposes
void Hello::Initialize(v8::Handle<v8::Object> target) {
    // First we create a function object (the constructor function)
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    // Then we make it persistent, so it doesn't get garbage collected (duh)
    constructor_template = Persistent<FunctionTemplate>::New(t);

    // This is optional. It sets the constructor name to "HelloWorld"
    // Example:
    //      var helloworld = new HelloWorld();
    //      console.log(helloworld.constructor.name); // "HelloWorld"
    constructor_template->SetClassName(String::NewSymbol("HelloWorld"));

    // TODO: Add comprehensive explanation
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

    // Now let's add some properties and methods to the prototype of Hello
    // This makes Hello.prototype.world point to the method World defined below
    // https://github.com/joyent/node/blob/823a443321360e089e9c6d169a6cc116e3fb50b2/src/node.h#L116
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "say", Say);

    // Makes our function available in the module under the name "HelloWorld"
    target->Set(String::NewSymbol("HelloWorld"), constructor_template->GetFunction());
}

// Called when you call "new Hello" in our code
Handle<Value> Hello::New(const Arguments& args) {
    HandleScope scope;
    return scope.Close(args.This());
}

// A method we attach to our prototype
Handle<Value> Hello::Say(const Arguments& args){
    return String::New("Hello world!");
}

extern "C" void init (Handle<Object> target) {
    // target is the node module
    Hello::Initialize(target);
}
