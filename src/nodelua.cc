#include <node.h>
#include <v8.h>

#include "luastate.h"

extern "C"{
#include <lua.h>
}

using namespace v8;

void init_info_constants(v8::Isolate* isolate, v8::Local<v8::Object> module){
  Local<Object> constants = Object::New(isolate);
  constants->Set(String::NewFromUtf8(isolate, "VERSION"), String::NewFromUtf8(isolate, LUA_VERSION));
  constants->Set(String::NewFromUtf8(isolate, "VERSION_NUM"), Number::New(isolate, LUA_VERSION_NUM));
  constants->Set(String::NewFromUtf8(isolate, "COPYRIGHT"), String::NewFromUtf8(isolate, LUA_COPYRIGHT));
  constants->Set(String::NewFromUtf8(isolate, "AUTHORS"), String::NewFromUtf8(isolate, LUA_AUTHORS));
  module->Set(String::NewFromUtf8(isolate, "INFO"), constants);
}


void init_status_constants(v8::Isolate* isolate, v8::Local<v8::Object> module){
  Local<Object> constants = Object::New(isolate);
  constants->Set(String::NewFromUtf8(isolate, "YIELD"), Number::New(isolate, LUA_YIELD));
  constants->Set(String::NewFromUtf8(isolate, "ERRRUN"), Number::New(isolate, LUA_ERRRUN));
  constants->Set(String::NewFromUtf8(isolate, "ERRSYNTAX"), Number::New(isolate, LUA_ERRSYNTAX));
  constants->Set(String::NewFromUtf8(isolate, "ERRMEM"), Number::New(isolate, LUA_ERRMEM));
  constants->Set(String::NewFromUtf8(isolate, "ERRERR"), Number::New(isolate, LUA_ERRERR));
  module->Set(String::NewFromUtf8(isolate, "STATUS"), constants);
}


void init_gc_constants(v8::Isolate* isolate, v8::Local<v8::Object> module){
  Local<Object> constants = Object::New(isolate);
  constants->Set(String::NewFromUtf8(isolate, "STOP"), Number::New(isolate, LUA_GCSTOP));
  constants->Set(String::NewFromUtf8(isolate, "RESTART"), Number::New(isolate, LUA_GCRESTART));
  constants->Set(String::NewFromUtf8(isolate, "COLLECT"), Number::New(isolate, LUA_GCCOLLECT));
  constants->Set(String::NewFromUtf8(isolate, "COUNT"), Number::New(isolate, LUA_GCCOUNT));
  constants->Set(String::NewFromUtf8(isolate, "COUNTB"), Number::New(isolate, LUA_GCCOUNTB));
  constants->Set(String::NewFromUtf8(isolate, "STEP"), Number::New(isolate, LUA_GCSTEP));
  constants->Set(String::NewFromUtf8(isolate, "SETPAUSE"), Number::New(isolate, LUA_GCSETPAUSE));
  constants->Set(String::NewFromUtf8(isolate, "SETSTEPMUL"), Number::New(isolate, LUA_GCSETSTEPMUL));
  module->Set(String::NewFromUtf8(isolate, "GC"), constants);
}


void init( v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  LuaState::Init(exports, module);
  init_gc_constants(exports->GetIsolate(), exports);
  init_status_constants(exports->GetIsolate(), exports);
  init_info_constants(exports->GetIsolate(), exports);
}
NODE_MODULE(nodelua, init)
