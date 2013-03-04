var ftdi = require('../index');

// var devices = ftdi.findAll(0x27f4, 0x0203);

// var dataToWrite = [0x04, 0x00, 0x02, 0x79, 0x40];

// console.log(devices);

// var device = new ftdi({
//     'vid': 0x27f4,
//     'pid': 0x0203,
//     'index': 0
// });

// device.open();
// device.setBaudrate(115200);
// device.setLineProperty(ftdi.BITS_8, ftdi.STOP_BIT_1, ftdi.NONE);

ftdi.find(function(err, devices) {}); // returns all ftdi devices
ftdi.find(0x27f4, function(err, devices) {}); // returns all ftdi devices with matching vendor
ftdi.find(0x27f4, 0x0203, function(err, devices) {}); // returns all ftdi devices with matching vendor and product


ftdi.find(0x27f4, 0x0203, function(err, devices) {
  var device = devices[0];
  // or
  // var device = new ftdi.FtdiPort(serialnumber, locationId);
  // or
  // var device = new ftdi.FtdiSerialPort(portName, {
  //   baudrate: 115200,
  //   databits: 8,
  //   stopbits: 1,
  //   parity: 'none'
  // });

  device.on('error', function() {

  });

  device.open(function(err) {

    device.on('data', function(data) {

    });

    device.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {

    });

  });

});