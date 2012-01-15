var Ftdi = require('../index');
var ftdi = new Ftdi();
ftdi.open();
ftdi.setBaudrate(19200);

// http://www.robot-electronics.co.uk/htm/usb_rly08tech.htm
// Relay 3 -> Red
// Relay 2 -> Orange
// Relay 1 -> Green

var ALL_ON      = 0x64;
var RED_ON      = 0x67;
var ORANGE_ON   = 0x66;
var GREEN_ON    = 0x65;

var ALL_OFF     = 0x6e;
var RED_OFF     = 0x71;
var ORANGE_OFF  = 0x70;
var GREEN_OFF   = 0x6f;

function write(i) {
    ftdi.write(String.fromCharCode(i));
}

function go() {
    write(ALL_OFF);
    write(GREEN_ON);
    setTimeout(function() {
        write(GREEN_OFF);
        write(ORANGE_ON);
        setTimeout(function() {
            write(ORANGE_OFF);
            write(RED_ON);
            setTimeout(go, 5000);
        }, 1000);
    }, 3000);
}

write(ALL_OFF);
write(RED_ON);