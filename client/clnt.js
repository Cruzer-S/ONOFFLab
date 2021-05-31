const abuf2hex = require("array-buffer-to-hex")

var buffer = new ArrayBuffer(1024);

var id =  new Uint32Array(buffer, 0, 1); // [0-3]
var passwd = new Uint8Array(buffer, 4, 32); // [4-36]

// id
id[0] = 620;

// passwd
for (i = 0; i < passwd.length; i++)
	passwd[i] = "ONOFFLAB88"[i];

var idView = new Uint32Array(buffer, 0, 1);
var testView = new Uint16Array(buffer, 2, 1);

console.log(idView[0]);
console.log(testView[0]);
