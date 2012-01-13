var NodeFtdi = require('./build/Release/ftdi');

var ftdi = new NodeFtdi.Ftdi();
//ftdi.open();
//ftdi.setBaudrate(19200);

var x = NodeFtdi.Ftdi.findAll();

console.log(x);