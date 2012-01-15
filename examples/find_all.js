var Ftdi = require('../index');
console.log(Ftdi.findAll(0x0403));
console.log(Ftdi.findAll(0x0403, 0x6001));