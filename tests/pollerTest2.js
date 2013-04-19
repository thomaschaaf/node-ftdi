var ftdi = require('../index');

var chw4000Connected = false,
    bnet9107Connected = false;

var isChw4000Polling = false,
    isBnet9107Polling = false;

setInterval(function() {
  if (isChw4000Polling) return;
  console.log('\n--------------');
  console.log(new Date(), ' "CHW-4000": -- find STARTED -- ');
  isChw4000Polling = true;
  ftdi.find(0x27f4, 0x0203, function(err, devices) {
    isChw4000Polling = false;
    console.log(new Date(), ' "CHW-4000": -- find FINISHED -- ');
    console.log(arguments);

    if (!chw4000Connected && devices.length > 0) {
      var device = new ftdi.FtdiDevice(devices[0]);
      chw4000Connected = true;

      device.on('error', function() {
      });
      device.on('data', function(data) {
        console.log(arguments);
      });

      console.log(new Date(), ' "CHW-4000": -- open STARTED -- ');
      device.open({
        baudrate: 115200,
        databits: 8,
        stopbits: 1,
        parity: 'none'
      }, function(err) {
        console.log(new Date(), ' "CHW-4000": -- open FINISHED -- ');

        console.log(new Date(), ' "CHW-4000": -- write STARTED -- ');
        device.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {
          console.log(new Date(), ' "CHW-4000": -- write FINISHED -- ');
        });
      });
    }
  });
}, 5000);

setInterval(function() {
  if (isBnet9107Polling) return;
  console.log('\n--------------');
  console.log(new Date(), ' "BNET9107": -- find STARTED -- ');
  isBnet9107Polling = true;
  ftdi.find(0x18d9, 0x01a0, function(err, devices) {
    isBnet9107Polling = false;
    console.log(new Date(), ' "BNET9107": -- find FINISHED -- ');
    console.log(arguments);

    if (!bnet9107Connected && devices.length > 0) {
      var device = new ftdi.FtdiDevice(devices[0]);
      bnet9107Connected = true;

      device.on('error', function() {
      });
      device.on('data', function(data) {
        console.log(arguments);
      });

      console.log(new Date(), ' "BNET9107": -- open STARTED -- ');
      device.open({
        baudrate: 38400,
        databits: 8,
        stopbits: 1,
        parity: 'none'
      }, function(err) {
        console.log(new Date(), ' "BNET9107": -- open FINISHED -- ');

        console.log(new Date(), ' "BNET9107": -- write STARTED -- ');
        device.write([0x03, 0x30, 0x00, 0x33], function(err) {
          console.log(new Date(), ' "BNET9107": -- write FINISHED -- ');
        });
      });
    }
  });
}, 5000);