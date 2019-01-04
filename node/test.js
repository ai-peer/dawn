const process = require("process");

const dawn = require("./build/Release/dawn");

console.log(dawn);
console.log(dawn.hello(...process.argv.slice(2)));
