 var Ftdi = require('./build/Release/ftdi.node').Ftdi;

// Number of bits for ftdi_set_line_property()
Ftdi.BITS_7      = 7;
Ftdi.BITS_8      = 8;

// Number of stop bits for ftdi_set_line_property()
Ftdi.STOP_BIT_1  = 0;
Ftdi.STOP_BIT_15 = 1;
Ftdi.STOP_BIT_2  = 2;

// Parity mode for ftdi_set_line_property()
Ftdi.NONE        = 0;
Ftdi.ODD         = 1;
Ftdi.EVEN        = 2;
Ftdi.MARK        = 3;
Ftdi.SPACE       = 4;

Ftdi.BITMODE_RESET  = 0x00;    // switch off bitbang mode, back to regular serial/FIFO
Ftdi.BITMODE_BITBANG= 0x01;    // classical asynchronous bitbang mode, introduced with B-type chips
Ftdi.BITMODE_MPSSE  = 0x02;    // MPSSE mode, available on 2232x chips
Ftdi.BITMODE_SYNCBB = 0x04;    // synchronous bitbang mode, available on 2232x and R-type chips
Ftdi.BITMODE_MCU    = 0x08;    // MCU Host Bus Emulation mode, available on 2232x chips

// CPU-style fifo mode gets set via EEPROM
Ftdi.BITMODE_OPTO   = 0x10;    // Fast Opto-Isolated Serial Interface Mode, available on 2232x chips
Ftdi.BITMODE_CBUS   = 0x20;    // Bitbang on CBUS pins of R-type chips, configure in EEPROM before
Ftdi.BITMODE_SYNCFF = 0x40;    // Single Channel Synchronous FIFO mode, available on 2232H chips

module.exports = Ftdi;