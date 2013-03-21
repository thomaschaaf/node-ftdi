var ftdi = require('../index');

ftdi.find(0x18d9, 0x01a0, function(err, devices) {
  var device = new ftdi.FtdiDevice(devices[0]);

  device.on('error', function() {

  });

  device.on('data', function(data) {
    console.log(arguments);
  });

  device.open({
    baudrate: 38400,
    databits: 8,
    stopbits: 1,
    parity: 'none'
  }, function(err) {

    device.write([0x03, 0x30, 0x00, 0x33], function(err) {

    });

  });

});