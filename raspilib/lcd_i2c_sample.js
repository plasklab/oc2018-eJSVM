/*
 * this is a sample code of lcd_i2c.js.
 * to execute this code, you have to compile with following command
 * `$ java -jar ejsc_jar_path i2c.js lcd_i2c.js lcd_i2c_sample.js -o lcd_i2c_sample.sbc`
 */

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

