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


ftdi.find(0x18d9, 0x01a0, function(status, devices) 
	{
		if(devices.length > 0)
		{
			var device = new ftdi.FtdiDevice(devices[0]);

			// setInterval(loop(device))
			loop(device);

		}
		else
		{
			console.log("No Device found");
		}
	});

var loop = function(device)
{
	device.on('error', function(error) 
		{
			console.log("Error: " + error)
		});

	device.on('data', function(data)
		{
			console.log('Output: ', data.length);
			console.log( data );

			device.close(function(status) 
			{
				console.log("JS Close Device");
				setTimeout(function() {loop(device);}, 5000);
				// loop(device);
			});
		});

	device.open(connectionSettings, function(status) 
		{
			device.write(dataToWrite);
		});
}





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

