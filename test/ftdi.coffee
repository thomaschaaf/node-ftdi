ON = 1
OFF = 0

async = require "async"

ftdi = require "../index"

device = null

turnBy = (miliseconds, callback) =>
  array = new Array(ON, ON, ON, ON).reverse()
  device.write [parseInt(array.join(""), 2)], (err) =>
    setTimeout =>
      array = new Array(OFF, OFF, OFF, OFF).reverse()
      device.write [parseInt(array.join(""), 2)], (err) =>
        callback()
    , miliseconds

async.series [
  (cb) =>
    ftdi.find (err, devices) =>
      device = new ftdi.FtdiDevice(devices[0])
      cb err
  ], (err) =>

    deviceOptions =
      baudrate: 9600
      databits: 8
      stopbits: 1
      parity: 'none'
      bitmode: 0x04
      bitmask: 0xf


    #device.on "error", (data) =>
    #  console.log "err", data
#
    #device.on "data", (data) =>
    #  console.log "data", data
#
    #device.on "open", () =>
    #  console.log "open"


    #console.log device
    device.open deviceOptions, (err) =>

      #console.log(err)
      turnBy 4000, () =>
        console.log "finished turning"

      #device.on "data", (data) =>
      #  console.log "data", data.toString("utf8")
