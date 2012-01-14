
# Constructor

new Ftdi({
    'vendorID': 0x0403,
    'productID': 0x6001,
    'description': 'FT232R USB UART'
    'serial': 'A700ethE'
    'index': 0
});

# Instance methods

ftdi.open()

ftdi.setBaudrate(int)

ftdi.write(char)

ftdi.close()

# Class methods

Ftdi.findAll()