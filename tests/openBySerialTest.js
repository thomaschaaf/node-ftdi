setTimeout(function() {
  var ftdi = require('../index');

  var chw4000 = new ftdi.FtdiDevice({ serialNumber: 'FTVTIXBE' });
  //var chw4000 = new ftdi.FtdiDevice({ locationId: 530 });

  chw4000.on('data', function(data) {
    console.log(arguments);
  });

  chw4000.open({
    baudrate: 115200,
    databits: 8,
    stopbits: 1,
    parity: 'none'
  }, function(err) {console.log(arguments);

     setInterval(function() {
      chw4000.write([0x04, 0x00, 0x02, 0x79, 0x40], function(err) {console.log(arguments);

      });
     }, 200); // 200 for serialNumber
      
  });


  setTimeout(function() {

    var bnet9107 = new ftdi.FtdiDevice({ serialNumber: '00000210' });
    //var bnet9107 = new ftdi.FtdiDevice({ locationId: 529 });
    bnet9107.on('data', function(data) {
      console.log(arguments);
    });

    bnet9107.open({
      baudrate: 38400,
      databits: 8,
      stopbits: 1,
      parity: 'none'
    }, function(err) {console.log(arguments);

      setInterval(function() {
        bnet9107.write([0x03, 0x30, 0x00, 0x33], function(err) {console.log(arguments);

        });
      }, 300);

    });

  }, 2000);
}, 300);




