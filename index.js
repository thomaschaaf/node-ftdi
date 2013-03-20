var util = require('util'),
    EventEmitter = require('events').EventEmitter,
    ftdi = require('bindings')('ftdi.node'),
		FTDIDriver = ftdi.FtdiDriver,
    FTDIDevice = ftdi.FtdiDevice;

function FtdiDevice(settings) {
	if (typeof(settings) === 'number') {
		settings = { index: settings };
	}

	EventEmitter.call(this);

	this.deviceSettings = settings;

	this.FTDIDevice = new FTDIDevice(settings);
}

util.inherits(FtdiDevice, EventEmitter);

FtdiDevice.prototype.open = function(settings, callback) {
	var self = this;
	this.connectionSettings = settings;
	console.log("Settings: ", this.connectionSettings);
	this.FTDIDevice.open(this.connectionSettings, function(data) {
		self.emit('data', data);
	}, callback);
};

FtdiDevice.prototype.write = function(data, callback) {
	if (!Buffer.isBuffer(data)) {
    data = new Buffer(data);
  	}
	this.FTDIDevice.write(data, callback);

};

FtdiDevice.prototype.close = function(callback) {

};

module.exports = {

	FtdiDevice: FtdiDevice,

	find: function(vid, pid, callback) {
		if (arguments.length === 2) {
			callback = pid;
			pid = null;
		} else if (arguments.length === 1) {
			callback = vid;
			vid = null;
			pid = null;
		}

		FTDIDriver.findAll(vid, pid, function(status, devs) {
			var devices = [];

			for (var i = 0, len = devs.length; i < len; i++) {
				var devSettings = devs[i];
				devSettings.index = i;
				devices.push(new FtdiDevice(devSettings));
			}

			callback(status, devices);
		});
	}

};