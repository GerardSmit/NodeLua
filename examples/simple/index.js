var nodelua = require('../../');

var lua = new nodelua.LuaState('simple');

lua.doString('return jit.version')
	.then(
		r => console.log('Return', r),
		e => console.log('Error', e)
	);