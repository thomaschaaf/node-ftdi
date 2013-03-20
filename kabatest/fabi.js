// var ftdi = require('../ftdi');
var ftdi = require('../index');

//var dataToWrite = [0x04, 0x00, 0x02, 0x79, 0x40];
var dataToWrite = [0x03, 0x30, 0x00, 0x33];
var connectionSettings =
{
  baudrate: 38400,
  databits: 8,
  stopbits: 1,
  parity: 'none'
}

// ftdi.setVidPid(0x18d9, 0x01a0);


// ftdi.find(function(status, devices) {console.log("1");console.log(devices)});

// ftdi.find(0x1, function(status, devices) {console.log("2");console.log(devices)});

// ftdi.find(0x1, 0x2, function(status, devices) {console.log("3");console.log(devices)});

// ftdi.find(0x18d9, function(status, devices) {console.log("4");console.log(devices)});

// ftdi.find(0x18d9, 0x01a0, function(status, devices) {console.log("5");console.log(devices)});


var devices = ftdi.find(0x18d9, 0x01a0, function(status, devices) 
	{
		if(devices.length > 0)
		{
			var device = devices[0];

			device.on('data', function(data)
			{
				console.log('Output:');
				console.log( data );

				device.close(function(status) {
					console.log("JS Close Device");
					device.open(connectionSettings, function(status) 
					{
						console.log('openResult: ' + status);
						device.write(dataToWrite, function(status){console.log('WriteResult: ' + status);});
					}
					)
				});
			});

			device.open(connectionSettings, function(status) 
			{
				console.log('openResult: ' + status);
				
				device.write(dataToWrite, function(status){console.log('WriteResult: ' + status);});

				// setInterval(function() 
				// {
				// 	device.write(dataToWrite, function(status){console.log('WriteResult: ' + status);});
				// }, 5000);});
			});
		}
		else
		{
			console.log("No Device found");
		}
	});







// var device = ftdi.Ftdi();
// var devices = ftdi.find(0x18d9, 0x01a0, function() {});
// console.log( devices );

// device.on('data', function(data)
// {
// 	console.log('Output:');
// 	console.log( data );
// });

// if(devices.length > 0)
// {
// 	device.open(devices[0].serial);


// 	setInterval(function() 
// 	{
// 		device.write(dataToWrite);
// 	}, 2000);
// }
// else
// {
// 	console.log("No Device found");
// }

