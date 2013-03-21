var ftdi = require('../index');

var dataToWrite = [0x04, 0x00, 0x02, 0x79, 0x40];

// ftdi.find(function(err, devices) {}); // returns all ftdi devices
// ftdi.find(0x27f4, function(err, devices) {}); // returns all ftdi devices with matching vendor
// ftdi.find(0x27f4, 0x0203, function(err, devices) {}); // returns all ftdi devices with matching vendor and product


ftdi.find(0x27f4, 0x0203, function(err, devices) {
  var device = devices[0];

  device.on('error', function() {

  });

  device.on('data', function(data) {
    console.log(arguments);
  });

  device.open({
    baudrate: 115200,
    databits: 8,
    stopbits: 1,
    parity: 'none'
  }, function(err) {console.log(arguments);

    device.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {

    });

  });

});