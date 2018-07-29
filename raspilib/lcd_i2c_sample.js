/*
 * this is a sample code of lcd_i2c.js.
 */

var LCD_I2C_ADDR = 0x27;
var SDAPIN = 12;
var SCLPIN = 20;

Raspi.init();

var i2c = new RaspiI2C(I2C.MODE_MASTER, SDAPIN, SCLPIN);
var lcd = new LCD_I2C(LCD_I2C_ADDR, i2c);

lcd.init();
lcd.setCursor(0, 0);
lcd.printStr('wa ro ta');

