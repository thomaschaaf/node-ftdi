var util = require('util'),
    EventEmitter = require('events').EventEmitter,
    ftdi = require('bindings')('ftdi.node'),
    FTDIDriver = ftdi.FtdiDriver,
    FTDIDevice = ftdi.FtdiDevice;

/**
 * 0x00 = Reset
 * 0x01 = Asynchronous Bit Bang
 * 0x02 = MPSSE (FT2232, FT2232H, FT4232H and FT232H devices only)
 * 0x04 = Synchronous Bit Bang (FT232R, FT245R, FT2232, FT2232H, FT4232H and FT232H devices only)
 * 0x08 = MCU Host Bus Emulation Mode (FT2232, FT2232H, FT4232H and FT232H devices only)
 * 0x10 = Fast Opto-Isolated Serial Mode (FT2232, FT2232H, FT4232H and FT232H devices only)
 * 0x20 = CBUS Bit Bang Mode (FT232R and FT232H devices only) 
 * 0x40 = Single Channel Synchronous 245 FIFO Mode (FT2232H and FT232H devices only)
 */
var bitmodes = {
  'reset' : 0x00,
  'async' : 0x01,
  'mpsse' : 0x02,
  'sync'  : 0x04,
  'mcu'   : 0x0B,
  'fast'  : 0x10,
  'cbus'  : 0x20,
  'single': 0x40
};

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

  if (typeof(settings.bitmode) === 'sting') {
    settings.bitmode = bitmodes[settings.bitmode];
  }

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