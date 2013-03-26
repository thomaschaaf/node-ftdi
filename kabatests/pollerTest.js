var ftdi = require('../index');

setInterval(function() {
  ftdi.find(0x18d9, 0x01a0, function(err, devices) {
    console.log(arguments);
  });
}, 1000);