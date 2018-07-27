
function I2C(mode, sdaPin, sclPin) {
  this.sdaPin = sdaPin;
  this.sclPin = sclPin;
  this.mode = mode;
  this.speed = 100; // [kbps]


  this.write = function(addr, data) {
    if (start() == I2C.FAILED) return I2C.FAILED;
    if (sendAddress(addr, I2C.WRITE) == I2C.FAILED) return I2C.FAILED;
    udelay(t);
    debugPrint('write('+data+') to ' + addr);
    if (send1byte(data) == I2C.FAILED) {
      debugPrint('failed to write the data.');
      return I2C.FAILED;
    }
    if (stop() == I2C.FAILED) return I2C.FAILED;
    return I2C.DONE;
  }

  var t = this.speed/10; // (1/(speed*1k)*1M) [us] (TODO: korewa kuso tekitou)

  var debugMode = false;
  this.setDebugMode = function(flg) {
    debugMode = flg;
  }

  var debugPrint = function(s) {
    if (debugMode) print('I2C message: '+s);
  }

  var LOW = 0;
  var HIGH = 1;
  var setSDA = function(x) {
    if (x == HIGH) {
      Raspi.gpioWrite(sdaPin, 1);
    } else if (x == LOW) {
      Raspi.gpioWrite(sdaPin, 0);
    }
  }
  var setSCL = function(x) {
    if (x == HIGH) {
      Raspi.gpioWrite(sclPin, 1);
    } else if (x == LOW) {
      Raspi.gpioWrite(sclPin, 0);
    }
  }
  var getSDA = function() {
    return Raspi.gpioRead(sdaPin);
  }
  var getSCL = function() {
    return Raspi.gpioRead(sclPin);
  }
  var udelay = Time.udelay;

  var start = function() {
    debugPrint('start condition');
    setSCL(HIGH); udelay(t);
    setSDA(LOW); udelay(t);
    return I2C.DONE;
  }
  var stop = function() {
    debugPrint('stop condition');
    // stop condition
    setSCL(HIGH); udelay(t);
    setSDA(HIGH); udelay(t);
    return I2C.DONE;
  }

  var sendAddress = function(addr, r_or_w) {
    var rwbit = 0;
    if (r_or_w == I2C.WRITE) {
      rwbit = 0x00;
    } else if (r_or_w == I2C.READ) {
      rwbit = 0x01;
    } else {
      debugPrint('start(addr, r_or_w) invalid arguments');
      return I2C.FAILED;
    }

    // send the address
    debugPrint('sending the address');
    if (send1byte(addr<<1 | rwbit) == I2C.FAILED) {
      debugPrint('failed to connect with the node(address='+addr+').');
      return I2C.FAILED;
    }
    return I2C.DONE;
  }
  var send1byte = function(data) {
    var MASK = 0x80;
    for (var i = 0; i < 8; i++) {
      var bit = (data & MASK) != 0;
      setSCL(LOW);udelay(t);
      if (bit) {
        setSDA(HIGH);
      } else {
        setSDA(LOW);
      }
      udelay(t);
      setSCL(HIGH);
      udelay(t);
      data = data << 1;
    }
    // ACK
    setSCL(LOW);
    setSDA(HIGH);
    udelay(t);
    setSCL(HIGH);
    udelay(t);
    var ackbit = getSDA();
    setSCL(LOW);
    if (ackbit != 0) {
      debugPrint('no ack');
      return I2C.FAILED;
    }
    return I2C.DONE;
  }
  
}

I2C.MODE_MASTER = 1;
I2C.MODE_SLAVE = 2;
I2C.DONE = 0;
I2C.FAILED = 1;
I2C.WRITE = 0;
I2C.READ = 1;
