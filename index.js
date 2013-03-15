var ftdi = require('bindings')('ftdi.node'),
		FTDIDriver = ftdi.FtdiDriver,
    FTDIDevice = ftdiFtdiDevice,
    util = require('util'),
    Duplex = require('events').EventEmitter;
    // Duplex = require('stream').Duplex;

function FtdiDevice(settings) {
	if (typeof(settings) === 'number') {
		settings = { index: settings };
	}

	Duplex.call(this);

	this.FTDIDevice = new FTDIDevice(settings);
}

util.inherits(FtdiDevice, Duplex);

FtdiDevice.prototype.open = function(settings, callback) {

};

FtdiDevice.prototype.write = function(data, callback) {
	if (!Buffer.isBuffer(data)) {
    data = new Buffer(data);
  }

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

		if (vid && pid) {
			FTDIDriver.setVidPid(vid, pid);
		}

		FTDIDriver.findAll(function(status, devs) {
			var devices = [];

			for (var i = 0, len = devs.length; i < len; i++) {
				devices.push(new FtdiDevice(i));
			}

			callback(status, devices);
		});
	}

};