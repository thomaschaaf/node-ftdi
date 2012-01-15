var Ftdi = require('../index');
var ftdi = new Ftdi({
    'vid': 0x0403,
    'pid': 0x6001,
    'index':0
});
ftdi.open();
ftdi.setBaudrate(19200);
// Turns all relays on, on the USB RLY08
// http://www.robot-electronics.co.uk/htm/usb_rly08tech.htm
ftdi.write(String.fromCharCode(110));
setTimeout(function() {
    ftdi.write(String.fromCharCode(100));
    ftdi.close();
}, 200);