var ftdi = require('../index');

var foundDevs = [];

setInterval(function() {
  ftdi.find(0x27f4, 0x0203, function(err, devices) {
    foundDevs = devices;
    console.log(devices.length);
  });
}, 1000);

setTimeout(function() {
  var device1 = new ftdi.FtdiDevice(foundDevs[0]);

  device1.on('data', function(data) {
    // console.log(arguments);
  });

  device1.open({
    baudrate: 115200,
    databits: 8,
    stopbits: 1,
    parity: 'none'
  }, function(err) {
    console.log('opened first');
    console.log('Plug new device now!!!');
    // setInterval(function() {
    //   device1.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {

    //   });
    // }, 200);
  });
}, 4000);


// setTimeout(function() {
//   var device2 = new ftdi.FtdiDevice({ serialNumber: 'FTVTIXBE' });

//   device2.on('data', function(data) {
//     // console.log(arguments);
//   });

//   device2.open({
//     baudrate: 115200,
//     databits: 8,
//     stopbits: 1,
//     parity: 'none'
//   }, function(err) {
//     console.log('opened second');
//     // setInterval(function() {
//     //   device2.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {

//     //   });
//     // }, 200);
//   });
// }, 10000);

