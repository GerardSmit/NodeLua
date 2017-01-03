#ifndef LUASTATE_H
#define LUASTATE_H

#include <map>
#include <node.h>
#include <node_object_wrap.h>

#include "utils.h"

extern "C"{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class LuaState : public node::ObjectWrap{
 public:
  lua_State* lua_;
  char* name_;

  static void Init( v8::Local<v8::Object> exports, v8::Local<v8::Object> module);
  static int CallFunction(lua_State* L);

 private:
  LuaState();
  ~LuaState();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetName(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void CollectGarbage(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CollectGarbageSync(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Status(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StatusSync(const v8::FunctionCallbackInfo<v8::Value>& args);


  static void DoFileSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoFile(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void DoStringSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoString(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetGlobal(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetGlobal(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void RegisterFunction(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Push(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Pop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetTop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Replace(const v8::FunctionCallbackInfo<v8::Value>& args);
};
#endif
