var ftdi = require('../index');

ftdi.find(0x27f4, 0x0203, function(err, devices) {
  var device = new ftdi.FtdiDevice(devices[0]);
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

  device.on('data', function(data) {
    console.log(arguments);
  });

  device.open({
    baudrate: 115200,
    databits: 8,
    stopbits: 1,
    parity: 'none'
  }, function(err) {console.log(arguments);

    setInterval(function() {
      device.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {

      });
    }, 500);

  });

});