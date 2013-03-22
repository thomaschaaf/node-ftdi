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
	this.FTDIDevice.open(this.connectionSettings, function(data) {
		self.emit('data', data);
	}, function(err) {
		if (err) {
			self.emit('error', err);
		} else {
			self.emit('open');
		}
		if (callback) { callback(err); }
	});
};

FtdiDevice.prototype.write = function(data, callback) {
	if (!Buffer.isBuffer(data)) {
    data = new Buffer(data);
  }
  var self = this;
	this.FTDIDevice.write(data, function(err) {
		if (err) {
			self.emit('error', err);
		}
		if (callback) { callback(err); }
	});
};

FtdiDevice.prototype.close = function(callback) {
	var self = this;
	this.FTDIDevice.close(function(err) {
		if (err) {
			self.emit('error', err);
		} else {
			self.emit('close');
		}
		self.removeAllListeners();
		if (callback) callback(err);
	});
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

		FTDIDriver.findAll(vid, pid, callback);
	}

};