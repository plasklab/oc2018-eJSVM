/*
 * HD44780 4bit mode
 */

LCD_LINE_1 = 0x80;

// commands
LCD_CLEARDISPLAY = 0x01;
LCD_RETURNHOME = 0x02;
LCD_ENTRYMODESET = 0x04;
LCD_DISPLAYCONTROL = 0x08;
LCD_CURSORSHIFT = 0x10;
LCD_FUNCTIONSET = 0x20;
LCD_SETCGRAMADDR = 0x40;
LCD_SETDDRAMADDR = 0x80;

// flags for back light
LCD_BACKLIGHT = 0x08;
LCD_NOBACKLIGHT = 0x00;

// flags for display control
LCD_DISPLAY_ON = 0x04;
LCD_CURSOR_ON = 0x02;
LCD_BLINK_ON = 0x01;

// flags for entry mode set
LCD_ENTRYLEFT = 0x02;
LCD_ENTRYRIGHT = 0x00;
LCD_ENTRYSHIFTDECREMENT = 0x01;
LCD_ENTRYSHIFTINCREMENT = 0x00;

// flags for function set
LCD_4BITMODE = 0x10;
LCD_2LINE = 0x08;
LCD_1LINE = 0x00;
LCD_5x10DOTS = 0x08;
LCD_5x8DOTS = 0x00;



En = 0x04; // enable bit
Rw = 0x02; // read/write bit
Rs = 0x01; // register select bit


function LCD_I2C(_lcd_i2c_addr, _i2c) {

  var lcd_i2c_addr = _lcd_i2c_addr;
  var i2c = _i2c;
  
  var displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  var displaycontrol = LCD_DISPLAY_ON;
  var backlightval = LCD_BACKLIGHT;
  var displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;

  var debugMode = false;

  /*
   * API
   */
  this.init = function() {
    debugPrint('initializing');
    delay(50);
    expanderWrite(backlightval);
    delay(1000);

    // put the LCD into 4 bit mode
    write4bits(0x03 << 4);
    udelay(4500); // wait min 4.1ms
    write4bits(0x03 << 4);
    udelay(4500); // wait min 4.1ms
    write4bits(0x03 << 4);
    udelay(150);
    write4bits(0x02 << 4);

    // function set
    displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
    command(LCD_FUNCTIONSET | displayfunction);

    debugPrint('entered 4 bit mode');

    // display on/off
    displaycontrol = LCD_DISPLAY_ON;
    //displaycontrol |= LCD_CURSOR_ON;
    //displaycontrol |= LCD_BLINK_ON;
    this.display();

    // clear it off
    this.clear();

    // set the entry mode
    // 0x02 0x01
    displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    command(LCD_ENTRYMODESET | displaymode);

    // return home
    this.home();

  }
  this.noDisplay = function() {
    displaycontrol &= ~LCD_DISPLAY_ON;
    command(LCD_DISPLAYCONTROL | displaycontrol);
  }
  this.display = function() {
    displaycontrol |= LCD_DISPLAY_ON;
    command(LCD_DISPLAYCONTROL | displaycontrol);
  }
  this.clear = function() {
    command(LCD_CLEARDISPLAY);
    udelay(2000);
  }
  this.home = function() {
    command(LCD_RETURNHOME);
    udelay(2000);
  }
  this.setCursor = function(col, row) {
    var row_offsets = [ 0x00, 0x40, 0x14, 0x54 ];
    command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
  }
  this.noBacklight = function() {
    backlightval = LCD_NOBACKLIGHT;
    expanderWrite(0);
  }
  this.backlight = function() {
    backlightval = LCD_BACKLIGHT;
    expanderWrite(0);
  }
  this.printStr = function(s) {
    for (var i = 0; i < s.length; i++) {
      write(s.charCodeAt(i));
    }
  }
  this.setDebugMode = function(flg) {
    debugMode = flg;
  }


  command = function(value) {
    debugPrint('command: '+value);
    send(value, 0);
  }
  var write = function(value) {
    debugPrint('write: '+value);
    send(value, Rs);
    return 1;
  }
  var send = function(value, mode) {
    var highnib = value & 0xf0;
    var lownib = (value<<4) & 0xf0;
    write4bits((highnib)|mode);
    write4bits((lownib)|mode);
  }
  var write4bits = function(value) {
    expanderWrite(value);
    pulseEnable(value);
  }
  var expanderWrite = function(data) {
    if (i2c.write(lcd_i2c_addr, data | backlightval) == I2C.FAILED) {
      debugPrint('error: expanderWrite');
      return;
    }
  }
  var pulseEnable = function(data) {
    expanderWrite(data | En);
    udelay(1);
    expanderWrite(data & ~En);
    udelay(50);
  }
  var delay = function(t) {
    Time.delay(t);
  }
  var udelay = function(t) {
    Time.udelay(t);
  }
  var debugPrint = function(s) {
    if (debugMode) print('LCD_I2C message: '+s);
  }

}


/*
function main() {
  var LCD_I2C_ADDR = 0x27;
  var SDAPIN = 12;
  var SCLPIN = 20;
  Raspi.init();
  var i2c = new I2C(I2C.MODE_MASTER, SDAPIN, SCLPIN);
  var lcd = new LCD_I2C(LCD_I2C_ADDR, i2c);
  // i2c.setDebugMode(true);
  // lcd.setDebugMode(true);
  lcd.init();
  lcd.setCursor(0, 0);
  lcd.printStr('KAERITAI MAN');
}

main();

*/
