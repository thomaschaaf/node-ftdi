var Ftdi = require('../index');
var ftdi = new Ftdi({
    'vid': 0x0403,
    'pid': 0x6001,
    'index': 0
});
ftdi.open();
ftdi.setBaudrate(19200);
ftdi.setLineProperty(Ftdi.BITS_8, Ftdi.STOP_BIT_2, Ftdi.NONE);

// Turns all relays on, on the USB RLY08
// http://www.robot-electronics.co.uk/htm/usb_rly08tech.htm
ftdi.write(String.fromCharCode(91));
console.log(ftdi.read());
ftdi.close();
