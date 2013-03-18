var FTDIDriver = require('bindings')('ftdi.node').FtdiDriver;
var FTDIDevice = require('bindings')('ftdi.node').FtdiDevice;

var ftdiBinding;
var sys = require('sys'),
events = require('events');

// var deviceSettings = {
//   vid: 0x27f4,
//   pid: 0x0203
// };
var deviceSettings = {
  vid: 0x18d9,
  pid: 0x01a0
};

var connectionSettings =
{
  baudrate: 38400,
  databits: 8,
  stopbits: 1,
  parity: 'none'
}


function Ftdi() 
{
    if(false === (this instanceof Ftdi)) 
    {
        console.log("Create FTDI (JS)");
        ftdiBinding = new FTDIDevice(deviceSettings);
        var instance = new Ftdi();
        return instance;
    }
    
    events.EventEmitter.call(this);
}
sys.inherits(Ftdi, events.EventEmitter);

Ftdi.prototype.open = function(serial) 
{
	var self = this;
	ftdiBinding.registerDataCallback(function(data)
	{
		self.emit('data', data);
	});
	ftdiBinding.open(serial, connectionSettings);
  
}


Ftdi.prototype.write = function(data) 
{
  console.log('Write: ' + data);
	if (!Buffer.isBuffer(data))
	{
    	data = new Buffer(data);
  }
	return ftdiBinding.write(data);
}


/**
 * Static Section
 */
exports.setVidPid = function(vendorId, productId)
{
    FTDIDriver.setVidPid(vendorId, productId);
}

exports.find = function(arg1, arg2, arg3) 
{
  var vendorId = 0;
  var productId = 0;
  var callback

    // Check arguments
    switch (arguments.length) 
    {
      case 1:
        if(typeof(arg1) != "function")
        {
          throw new Error('Argument not a function');
        }
        callback = arg1;
        break;

      case 2:
        if(typeof(arg2) != "function")
        {
          throw new Error('Second Argument not a function');
        }
        vendorId = arg1;
        callback = arg2;
        break;

      case 3:
        if(typeof(arg3) != "function")
        {
          throw new Error('Third Argument not a function');
        }
        vendorId = arg1;
        productId = arg2;
        callback = arg3;
        break;
        
      default:
        throw new Error('Invalid number of arguments');
        break;
    }
  
    // Find All FTDI Devices
    FTDIDriver.findAll(function(status, deviceList)
    {
      var filteredDeviceList = deviceList;

      // Filter for desired Devices
      if(vendorId != 0 || productId != 0)
      {
        filteredDeviceList = new Array();
        for (var i = 0; i < deviceList.length; i++)
        {
          var device = deviceList[i];

          if(vendorId != 0 && device.vendorId != vendorId)
          {
            continue;
          }
          if(productId != 0 && device.productId != productId)
          {
            continue;
          }

          filteredDeviceList.push(device);
        }
      }

      if(callback)
      {
        callback(filteredDeviceList)
      }
    });
}

exports.Ftdi = Ftdi;