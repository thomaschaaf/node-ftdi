var util = require('util'),
    Duplex = require('events').EventEmitter,
    // Duplex = require('stream').Duplex,
    ftdi = require('bindings')('ftdi.node'),
		FTDIDriver = ftdi.FtdiDriver,
    FTDIDevice = ftdi.FtdiDevice,
    lookupVidPid = [];

function hasToSet(vid, pid) {
	for (var i = 0, len = lookupVidPid.length; i < len; i++) {
		var pair = lookupVidPid[i];
		if (pair.vid === vid && pair.pid === pid) {
			return false;
		}
	}
	return true;
}

function FtdiDevice(settings) {
	if (typeof(settings) === 'number') {
		settings = { index: settings };
	}

	Duplex.call(this);

	this.deviceSettings = settings;

	this.FTDIDevice = new FTDIDevice(settings);
}

util.inherits(FtdiDevice, Duplex);

FtdiDevice.prototype.open = function(settings, callback) {
	var self = this;
	this.connectionSettings = settings;
	this.FTDIDevice.registerDataCallback(function(data) {
		self.emit('data', data);
	});
	this.FTDIDevice.open(this.deviceSettings.serial, this.connectionSettings);
	callback();
};

FtdiDevice.prototype.write = function(data, callback) {
	if (!Buffer.isBuffer(data)) {
    data = new Buffer(data);
  }
	this.FTDIDevice.write(data);
	callback();
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

		if (vid && pid && hasToSet(vid, pid)) {
			FTDIDriver.setVidPid(vid, pid);
			lookupVidPid.push({ vid: vid, pid: pid });
		}

		FTDIDriver.findAll(function(status, devs) {
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