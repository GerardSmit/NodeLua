#define BUILDING_NODELUA
#include "luastate.h"
#include "uv.h"

using namespace v8;
using namespace v8::internal;

struct node_function {
  v8::Isolate* isolate;
  Persistent<Function> callback;
};

uv_async_t async;
std::map<std::string, node_function*> functions;

struct async_baton{
  bool has_cb;
  Persistent<Function> callback;
  v8::Isolate* isolate;
  char* data;
  bool error;
  char msg[1000];
  LuaState* state;
};

struct simple_baton{
  bool has_cb;
  Persistent<Function> callback;
  v8::Isolate* isolate;
  int data;
  int result;
  LuaState* state;
};


void do_file(uv_work_t *req){
  async_baton* baton = static_cast<async_baton*>(req->data);

  if(luaL_dofile(baton->state->lua_, baton->data)){
    baton->error = true;
    sprintf(baton->msg, "Exception In File %s Has Failed:\n%s\n", baton->data, lua_tostring(baton->state->lua_, -1));
  }
}


void do_gc(uv_work_t *req){
  simple_baton* baton = static_cast<simple_baton*>(req->data);

  baton->result = lua_gc(baton->state->lua_, baton->data, 0);
}


void do_status(uv_work_t *req){
  simple_baton* baton = static_cast<simple_baton*>(req->data);

  baton->result = lua_status(baton->state->lua_);
}


void simple_after(uv_work_t *req, int status){
  simple_baton* baton = static_cast<simple_baton*>(req->data);
  v8::Isolate* isolate = baton->isolate;
  v8::HandleScope scope( isolate );

  const int argc = 1;
  Local<Value> argv[] = { Number::New(isolate, baton->result) };

  TryCatch try_catch(isolate);

  if(baton->has_cb){
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::New( baton->isolate, baton->callback );
    callback->Call( baton->isolate->GetCurrentContext()->Global(), argc, argv);
  }

  baton->callback.Reset();
  delete baton;
  delete req;

  if(try_catch.HasCaught()){
    node::FatalException(isolate, try_catch);
  }
}

void do_string(uv_work_t *req){
  async_baton* baton = static_cast<async_baton*>(req->data);

  if(luaL_dostring(baton->state->lua_, baton->data)){
    baton->error = true;
    sprintf(baton->msg, "Exception Of Lua Code Has Failed:\n%s\n", lua_tostring(baton->state->lua_, -1));
  }
}


void async_after(uv_work_t *req, int status){
  async_baton* baton = (async_baton *)req->data;
  v8::Isolate* isolate = baton->isolate;
  v8::HandleScope scope( isolate );

  Local<Value> argv[2];
  const int argc = 2;

  if(baton->error){
    argv[0] = String::NewFromUtf8(isolate, baton->msg);
    argv[1] = Local<Value>::New(isolate, Undefined(isolate));
  } else{
    argv[0] = Local<Value>::New(isolate, Undefined(isolate));
    if(lua_gettop(baton->state->lua_)){
      argv[1] = lua_to_value(isolate, baton->state->lua_, -1);
    } else{
      argv[1] = Local<Value>::New(isolate, Undefined(isolate));
    }
  }

  TryCatch try_catch(isolate);

  if(baton->has_cb){
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::New( baton->isolate, baton->callback );
    callback->Call( baton->isolate->GetCurrentContext()->Global(), argc, argv);
  }

  baton->callback.Reset();
  delete baton;
  delete req;

  if(try_catch.HasCaught()){
    node::FatalException(isolate, try_catch);
  }
}


LuaState::LuaState(){};
LuaState::~LuaState(){};


void LuaState::Init( v8::Local<v8::Object> exports, v8::Local<v8::Object> module ){
    v8::Isolate* isolate = exports->GetIsolate();

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New( isolate, New );
    tpl->SetClassName( String::NewFromUtf8( isolate, "LuaState" ) );
    tpl->InstanceTemplate()->SetInternalFieldCount( 1 );

    NODE_SET_PROTOTYPE_METHOD( tpl, "doFileSync", DoFileSync );
    NODE_SET_PROTOTYPE_METHOD( tpl, "doFile", DoFile );
    NODE_SET_PROTOTYPE_METHOD( tpl, "doStringSync", DoStringSync );
    NODE_SET_PROTOTYPE_METHOD( tpl, "doString", DoString );
    NODE_SET_PROTOTYPE_METHOD( tpl, "setGlobal", SetGlobal );
    NODE_SET_PROTOTYPE_METHOD( tpl, "getGlobal", GetGlobal );
    NODE_SET_PROTOTYPE_METHOD( tpl, "status", Status );
    NODE_SET_PROTOTYPE_METHOD( tpl, "statusSync", StatusSync );
    NODE_SET_PROTOTYPE_METHOD( tpl, "collectGarbage", CollectGarbage );
    NODE_SET_PROTOTYPE_METHOD( tpl, "collectGarbageSync", CollectGarbageSync );
    NODE_SET_PROTOTYPE_METHOD( tpl, "close", Close );
    NODE_SET_PROTOTYPE_METHOD( tpl, "getName", GetName );
    NODE_SET_PROTOTYPE_METHOD( tpl, "registerFunction", RegisterFunction );
    NODE_SET_PROTOTYPE_METHOD( tpl, "push", Push );
    NODE_SET_PROTOTYPE_METHOD( tpl, "pop", Pop );
    NODE_SET_PROTOTYPE_METHOD( tpl, "getTop", GetTop );
    NODE_SET_PROTOTYPE_METHOD( tpl, "setTop", SetTop );
    NODE_SET_PROTOTYPE_METHOD( tpl, "replace", Replace );

    exports->Set( String::NewFromUtf8( isolate, "LuaState" ), tpl->GetFunction() );
}


int LuaState::CallFunction(lua_State* L){
  int n = lua_gettop(L);

  char * func_name = (char *)lua_tostring(L, lua_upvalueindex(1));
  std::map<std::string, node_function*>::iterator iter;
  for( iter = functions.begin(); iter != functions.end(); iter++ ) {
    if( strcmp(iter->first.c_str(), func_name) == 0 ) {
      v8::Isolate* isolate = iter->second->isolate;
      v8::HandleScope scope(isolate);

      const unsigned argc = n;
      Local<Value>* argv = new Local<Value>[argc];
      int i;
      for(i = 1; i <= n; ++i){
        argv[i - 1] = lua_to_value(isolate, L, i);
      }

      Handle<Value> ret_val = Undefined(isolate);

      v8::Local<v8::Function> callback = v8::Local<v8::Function>::New( isolate, iter->second->callback );
      ret_val = callback->Call( isolate->GetCurrentContext()->Global(), argc, argv);

      push_value_to_lua(isolate, L, ret_val);
      break;
    }
  }

  return 1;
}


void LuaState::New(const v8::FunctionCallbackInfo<v8::Value>& args ){
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  if(!args.IsConstructCall()) {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState Requires The 'new' Operator To Create An Instance")));
    return;
  }

  if(!args.Length() > 0){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState Requires 1 Argument")));
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState First Argument Must Be A String")));
    return;
  }

  LuaState* obj = new LuaState();
  obj->name_ = get_str( isolate, args[0]);
  obj->lua_ = lua_open();
  luaL_openlibs(obj->lua_);
  obj->Wrap( args.This());
}


void LuaState::GetName(const v8::FunctionCallbackInfo<v8::Value>& args ){
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>( args.This());
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, obj->name_));
}


void LuaState::DoFileSync(const v8::FunctionCallbackInfo<v8::Value>& args){
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.doFileSync Takes Only 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.doFileSync Argument 1 Must Be A String")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  char* file_name = get_str(isolate, args[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  if(luaL_dofile(obj->lua_, file_name)){
    char buf[1000];
    sprintf(buf, "Exception Of File %s Has Failed:\n%s\n", file_name, lua_tostring(obj->lua_, -1));
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, buf)));
    return;
  }

  if(lua_gettop(obj->lua_)){
    args.GetReturnValue().Set(lua_to_value(isolate, obj->lua_, -1));
  } else{
    args.GetReturnValue().SetUndefined();
  }
}


void LuaState::DoFile(const v8::FunctionCallbackInfo<v8::Value>& args){
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8( isolate, "LuaState.doFile Requires At Least 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8( isolate, "LuaState.doFile First Argument Must Be A String")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(args.Length() > 1 && !args[1]->IsFunction()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8( isolate, "LuaState.doFile Second Argument Must Be A Function")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  async_baton* baton = new async_baton();
  baton->isolate = isolate;
  baton->data = get_str(isolate, args[0]);
  baton->state = obj;
  obj->Ref();

  if(args.Length() > 1){
    baton->has_cb = true;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast( args[1] );
    baton->callback.Reset( isolate, callback );
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_file, async_after);

  args.GetReturnValue().SetUndefined();
}

void LuaState::DoStringSync(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.doStringSync Requires 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  char *lua_code = get_str(isolate, args[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  if(luaL_dostring(obj->lua_, lua_code)){
    char buf[1000];
    sprintf(buf, "Execution Of Lua Code Has Failed:\n%s\n", lua_tostring(obj->lua_, -1));
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, buf)));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(lua_gettop(obj->lua_)){
    args.GetReturnValue().Set( lua_to_value( isolate, obj->lua_, -1 ) );
  } else{
    args.GetReturnValue().SetUndefined();
  }
}


void LuaState::DoString(const v8::FunctionCallbackInfo<v8::Value>& args){
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.doString Requires At Least 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.doString: First Argument Must Be A String")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  async_baton* baton = new async_baton();
  baton->isolate = isolate;
  baton->data = get_str(isolate, args[0]);
  baton->state = obj;
  obj->Ref();

  if(args.Length() > 1 && !args[1]->IsFunction()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.doString Second Argument Must Be A Function")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(args.Length() > 1){
    baton->has_cb = true;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
    baton->callback.Reset(isolate, callback);
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_string, async_after);

  args.GetReturnValue().SetUndefined();
}


void LuaState::SetGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  if(args.Length() < 2){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.setGlobal Requires 2 Arguments")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.setGlobal Argument 1 Must Be A String")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  char *global_name = get_str(isolate, args[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());

  push_value_to_lua(isolate, obj->lua_, args[1]);
  lua_setglobal(obj->lua_, global_name);

  args.GetReturnValue().SetUndefined();
}


void LuaState::GetGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.getGlobal Requires 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.getGlobal Argument 1 Must Be A String")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  char *global_name = get_str(isolate, args[0]);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  lua_getglobal(obj->lua_, global_name);

  Local<Value> val = lua_to_value(isolate, obj->lua_, -1);

  args.GetReturnValue().Set( val );
}

void LuaState::Close(const v8::FunctionCallbackInfo<v8::Value>& args){
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  lua_close(obj->lua_);

  args.GetReturnValue().SetUndefined();
}


void LuaState::Status(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  simple_baton* baton = new simple_baton();
  baton->state = obj;
  obj->Ref();

  if(args.Length() > 0 && !args[0]->IsFunction()) {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.status First Argument Must Be A Function")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(args.Length() > 0) {
    baton->has_cb = true;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[0]);
    baton->callback.Reset(isolate, callback);
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_status, simple_after);

  args.GetReturnValue().SetUndefined();
}

void LuaState::StatusSync(const v8::FunctionCallbackInfo<v8::Value>& args){
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  int status = lua_status(obj->lua_);

  args.GetReturnValue().Set(Number::New(isolate, status));
}


void LuaState::CollectGarbage(const v8::FunctionCallbackInfo<v8::Value>& args){
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.collectGarbage Requires 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsNumber()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaSatte.collectGarbage Argument 1 Must Be A Number, try nodelua.GC.[TYPE]")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  int type = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();

  simple_baton* baton = new simple_baton();
  baton->data = type;
  baton->state = obj;
  obj->Ref();

  if(args.Length() > 1 && !args[1]->IsFunction()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.collectGarbage Second Argument Must Be A Function")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(args.Length() > 1){
    baton->has_cb = true;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
    baton->callback.Reset(isolate, callback);
  }

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_gc, simple_after);

  args.GetReturnValue().SetUndefined();
}


void LuaState::CollectGarbageSync(const v8::FunctionCallbackInfo<v8::Value>& args){
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.collectGarbageSync Requires 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsNumber()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaSatte.collectGarbageSync Argument 1 Must Be A Number, try nodelua.GC.[TYPE]")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  int type = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();
  int gc = lua_gc(obj->lua_, type, 0);

  args.GetReturnValue().Set(Number::New(isolate, gc));
}


void LuaState::RegisterFunction(const v8::FunctionCallbackInfo<v8::Value>& args){
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "nodelua.registerFunction Must Have 2 Arguments")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsString()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "nodelua.registerFunction Argument 1 Must Be A String")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[1]->IsFunction()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "nodelua.registerFunction Argument 2 Must Be A Function")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
  
  char* func_name = get_str(isolate, args[0]);
  Local<String> func_key = String::Concat(String::NewFromUtf8(isolate, func_name), String::NewFromUtf8(isolate, ":"));
  func_key = String::Concat(func_key, String::NewFromUtf8(isolate, obj->name_));
  node_function* func = new node_function();
  func->isolate = isolate;
  func->callback.Reset(isolate, callback);
  char* name = get_str(isolate, func_key);
  functions[name] = func;

  lua_pushstring(obj->lua_, name);
  free(name);
  lua_pushcclosure(obj->lua_, CallFunction, 1);
  lua_setglobal(obj->lua_, func_name);

  args.GetReturnValue().SetUndefined();
}


void LuaState::Push(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.push Requires 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());

  push_value_to_lua(isolate, obj->lua_, args[0]);

  args.GetReturnValue().SetUndefined();
}


void LuaState::Pop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  int pop_n = 1;
  if(args.Length() > 0 && args[0]->IsNumber()){
    pop_n = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  lua_pop(obj->lua_, pop_n);

  args.GetReturnValue().SetUndefined();
}


void LuaState::GetTop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  int n = lua_gettop(obj->lua_);

  args.GetReturnValue().Set(Number::New(isolate, n));
}


void LuaState::SetTop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  int set_n = 0;
  if(args.Length() > 0 && args[0]->IsNumber()){
    set_n = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();
  }

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  lua_settop(obj->lua_, set_n);

  args.GetReturnValue().SetUndefined();
}


void LuaState::Replace(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if(args.Length() < 1){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.replace Requires 1 Argument")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  if(!args[0]->IsNumber()){
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "LuaState.replace Argument 1 Must Be A Number")));
    args.GetReturnValue().SetUndefined();
    return;
  }

  int index = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();

  LuaState* obj = ObjectWrap::Unwrap<LuaState>(args.This());
  lua_replace(obj->lua_, index);

  args.GetReturnValue().SetUndefined();
}
