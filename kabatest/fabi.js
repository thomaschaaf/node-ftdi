var ftdi = require('../ftdi');

//var dataToWrite = [0x04, 0x00, 0x02, 0x79, 0x40];
var dataToWrite = [0x03, 0x30, 0x00, 0x33];

ftdi.setVidPid(0x18d9, 0x01a0);


ftdi.find(function(devices) {console.log("1");console.log(devices)});

ftdi.find(0x1, function(devices) {console.log("2");console.log(devices)});

ftdi.find(0x1, 0x2, function(devices) {console.log("3");console.log(devices)});

ftdi.find(0x18d9, function(devices) {console.log("4");console.log(devices)});

ftdi.find(0x18d9, 0x01a0, function(devices) {console.log("5");console.log(devices)});


var devices = ftdi.find(0x18d9, 0x01a0, function(devices) 
	{
		if(devices.length > 0)
		{
			var device = ftdi.Ftdi();
			
			device.on('data', function(data)
			{
				console.log('Output:');
				console.log( data );
			});

			device.open(devices[0].serial);

			setInterval(function() 
			{
				device.write(dataToWrite);
			}, 2000);
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

