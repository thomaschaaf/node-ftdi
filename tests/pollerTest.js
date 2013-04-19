var ftdi = require('../index');

setInterval(function() {
  console.log(new Date(), ' "CHW-4000": -- find STARTED -- ');
  ftdi.find(0x27f4, 0x0203, function(err, devices) {
    console.log(new Date(), ' "CHW-4000": -- find FINISHED -- ');
    console.log(arguments);
  });
}, 1000);

setInterval(function() {
  console.log(new Date(), ' "BNET9107": -- find STARTED -- ');
  ftdi.find(0x18d9, 0x01a0, function(err, devices) {
    console.log(new Date(), ' "BNET9107": -- find FINISHED -- ');
    console.log(arguments);
  });
}, 1000);