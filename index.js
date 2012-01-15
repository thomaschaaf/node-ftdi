 var Ftdi = require('./build/Release/ftdi.node').Ftdi;

/** Number of bits for ftdi_set_line_property() */
Ftdi.BITS_7      = 7;
Ftdi.BITS_8      = 8;

/** Number of stop bits for ftdi_set_line_property() */
Ftdi.STOP_BIT_1  = 0;
Ftdi.STOP_BIT_15 = 1;
Ftdi.STOP_BIT_2  = 2;

/** Parity mode for ftdi_set_line_property() */
Ftdi.NONE        = 0;
Ftdi.ODD         = 1;
Ftdi.EVEN        = 2;
Ftdi.MARK        = 3;
Ftdi.SPACE       = 4;

 module.exports = Ftdi;