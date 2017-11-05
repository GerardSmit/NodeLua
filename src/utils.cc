#include <stdlib.h>
#include "utils.h"
#include <node_object_wrap.h>

class LuaFunc : public node::ObjectWrap {
public:
    explicit LuaFunc(lua_State* lua) :
        m_lua(lua)
    {
        m_ref = luaL_ref(m_lua, LUA_REGISTRYINDEX);
    }

    ~LuaFunc()
    {
        luaL_unref(m_lua, LUA_REGISTRYINDEX, m_ref);
    }

    v8::Local<v8::Object> Wrap(v8::Isolate* isolate)
    {
        v8::Local<v8::ObjectTemplate> tpl = v8::ObjectTemplate::New(isolate);
        tpl->SetInternalFieldCount(1);
        v8::Local<v8::Object> obj = tpl->NewInstance();
        node::ObjectWrap::Wrap(obj);
        return obj;
    }

    lua_State* m_lua;
    int m_ref;
};

static void lua_call_func(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    LuaFunc* obj = node::ObjectWrap::Unwrap<LuaFunc>(args.Data().As<v8::Object>());
    lua_rawgeti(obj->m_lua, LUA_REGISTRYINDEX, obj->m_ref);
    int nargs = args.Length();
    for( int i = 0; i < nargs; i++ )
    {
        push_value_to_lua(isolate, obj->m_lua, args[i]);
    }
    if( lua_pcall(obj->m_lua, nargs, 1, 0) != 0 )
    {
        isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, lua_tostring(obj->m_lua, -1))));
        lua_pop(obj->m_lua, 1);
        args.GetReturnValue().SetUndefined();
        return;
    }
    args.GetReturnValue().Set(lua_to_value(isolate, obj->m_lua, -1));
    lua_pop(obj->m_lua, 1);
}

static v8::Local<v8::Value> stringify(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  v8::Local<v8::Object> global = isolate->GetCurrentContext()->Global();
  v8::Local<v8::Object> JSON = v8::Local<v8::Object>::Cast(global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
  v8::Local<v8::Function> stringify = v8::Local<v8::Function>::Cast(JSON->Get(v8::String::NewFromUtf8(isolate, "stringify")));
  v8::Local<v8::Value> args[] = { value };
  return v8::Local<v8::String>::Cast(stringify->Call(JSON, 1, args));
}

char * get_str(v8::Isolate* isolate, v8::Local<v8::Value> val){
  if(!val->IsString()){
    isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Argument Must Be A String")));
    return NULL;
  }

  v8::String::Utf8Value val_string(val);
  char * val_char_ptr = (char *) malloc(val_string.length() + 1);
  strcpy(val_char_ptr, *val_string);
  return val_char_ptr;
}


v8::Local<v8::Value> lua_to_value(v8::Isolate* isolate, lua_State* L, int i){
  switch(lua_type(L, i)){
  case LUA_TBOOLEAN:
    return v8::Local<v8::Boolean>::New(isolate, v8::Boolean::New(isolate, (int)lua_toboolean(L, i)));
    break;
  case LUA_TNUMBER:
    return v8::Local<v8::Number>::New(isolate, v8::Number::New(isolate, lua_tonumber(L, i)));
    break;
  case LUA_TSTRING:
    return v8::String::NewFromUtf8(isolate, (char *)lua_tostring(L, i));
    break;
  case LUA_TTABLE:
    {
      v8::Local<v8::Object> obj = v8::Object::New(isolate);
      lua_pushnil(L);
      while(lua_next(L, -2) != 0){
        v8::Local<v8::Value> value = lua_to_value(isolate, L, -1);
        lua_pop(L, 1);
        v8::Local<v8::Value> key = lua_to_value(isolate, L, -1);
        if(lua_type(L, -1) == LUA_TTABLE)
        {
          key = stringify(isolate, key);
        }
        obj->Set(key, value);
      }
      return obj;
      break;
    }
  case LUA_TFUNCTION:
    {
      lua_pushvalue(L, i);
      LuaFunc* func = new LuaFunc(L);
      return v8::Local<v8::Function>::New(isolate, v8::Function::New(isolate, lua_call_func, func->Wrap(isolate)));
      break;
    }
  case LUA_TNIL:
    {
      return v8::Local<v8::Primitive>::New(isolate, v8::Null(isolate));
      break;
    }
  default:
    return v8::Local<v8::Primitive>::New(isolate, v8::Undefined(isolate));
    break;
  }
}

void push_value_to_lua(v8::Isolate* isolate, lua_State* L, v8::Handle<v8::Value> value){
  if(value->IsString()){
    char * s = get_str(isolate, v8::Local<v8::Value>::New(isolate, value));
    lua_pushstring(L, s);
    free(s);
  }else if(value->IsNumber()){
    int i_value = value->NumberValue(isolate->GetCurrentContext()).FromJust();
    lua_pushinteger(L, i_value);
  }else if(value->IsBoolean()){
    int b_value = (int)value->BooleanValue(isolate->GetCurrentContext()).FromJust();
    lua_pushboolean(L, b_value);
  }else if(value->IsMap()) {
    lua_newtable(L);
    v8::Local<v8::Array> data = value->ToObject().As<v8::Map>()->AsArray();
    for(uint32_t i = 0; i < data->Length(); i += 2)
    {
        push_value_to_lua(isolate, L, data->Get(i));
        push_value_to_lua(isolate, L, data->Get(i + 1));
        lua_settable(L, -3);
    }
  }else if(value->IsObject()){
    lua_newtable(L);
    v8::Local<v8::Object> obj = value->ToObject();
    v8::Local<v8::Array> keys = obj->GetPropertyNames();
    for(uint32_t i = 0; i < keys->Length(); ++i){
      v8::Local<v8::Value> key = keys->Get(i);
      push_value_to_lua(isolate, L, key);
      push_value_to_lua(isolate, L, obj->Get(key));
      lua_settable(L, -3);
    }
  }else{
    lua_pushnil(L);
  }
}
