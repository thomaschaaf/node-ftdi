# Node-ftdi

FTDI Bindings for Node.js

# API

## Constructor

`self` **Ftdi(obj)**

``` js
// Example for USB RLY08
var ftdi = new Ftdi({
    'vendorID': 0x0403,
    'productID': 0x6001,
    'description': 'FT232R USB UART'
    'serial': 'A700ethE'
    'index': 0
});
```

Note: Somehow description makes open() fail

## Instance methods

`void` **ftdi.open()**

`self` **ftdi.setBaudrate(int)**

`self` **ftdi.setLineProperty(int, int, int)**

`int` **ftdi.write(char)**

`void` **ftdi.close()**


## Class methods

`Array` **Ftdi.findAll([vid [,pid]])**