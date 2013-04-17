var util = require('util'),
    EventEmitter = require('events').EventEmitter,
    ftdi = require('bindings')('ftdi.node'),
    FTDIDriver = ftdi.FtdiDriver,
    FTDIDevice = ftdi.FtdiDevice;

/**
 * FtdiDevice represents your physical device.
 * On error 'error' will be emitted.
 * @param {Object || Number} settings The device settings (locationId, serial, index, description).
 */
function FtdiDevice(settings) {
  if (typeof(settings) === 'number') {
    settings = { index: settings };
  }

  EventEmitter.call(this);

  this.deviceSettings = settings;

  this.FTDIDevice = new FTDIDevice(settings);
}

util.inherits(FtdiDevice, EventEmitter);

/**
 * The open mechanism of the device.
 * On opened 'open' will be emitted and the callback will be called.
 * On error 'error' will be emitted and the callback will be called.
 * @param  {Function} callback The function, that will be called when device is opened. [optional]
 *                             `function(err){}`
 */
FtdiDevice.prototype.open = function(settings, callback) {
  var self = this;
  this.connectionSettings = settings;
  this.on('error', function(err) {
    self.close();
  });
  this.isClosing = false;
  this.FTDIDevice.open(this.connectionSettings, function(err, data) {
    if (err) {
      self.emit('error', err);
    }
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

/**
 * The write mechanism.
 * @param  {Array || Buffer} data     The data, that should be sent to device.
 * On error 'error' will be emitted and the callback will be called.
 * @param  {Function}        callback The function, that will be called when data is sent. [optional]
 *                                    `function(err){}`
 */
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

/**
 * The close mechanism of the device.
 * On closed 'close' will be emitted and the callback will be called.
 * On error 'error' will be emitted and the callback will be called.
 * @param  {Function} callback The function, that will be called when device is closed. [optional]
 *                             `function(err){}`
 */
FtdiDevice.prototype.close = function(callback) {
  var self = this;
  if (this.isClosing) {
    return;
  }
  this.isClosing = true;
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

  /**
   * Calls the callback with an array of found devices.
   * @param  {Number}   vid      The vendor id. [optional]
   * @param  {Number}   pid      The product id. [optional]
   * @param  {Function} callback The function, that will be called when finished finding.
   *                             `function(err, devices){}` devices is an array of device objects.
   */
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