var Ftdi = require('../index');
var ftdi = new Ftdi();
ftdi.open(0);
ftdi.setBaudrate(19200);
// Turns all relays on, on the USB RLY08
// http://www.robot-electronics.co.uk/htm/usb_rly08tech.htm
ftdi.write(String.fromCharCode(100));
ftdi.close();