var nodelua = require('../../');

var luastate = new nodelua.LuaState('files');

luastate.setGlobal('test', 'some value');

var file_name = __dirname + '/test.lua';
luastate.doFile(file_name).then(console.log, console.error);