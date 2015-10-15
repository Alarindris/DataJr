#include <LiquidCrystal_I2C.h>
#include "M2tk.h"
#include "C:\Users\Stock Room 2\Documents\Arduino\libraries\M2tklib\utility\m2ghnlc.h"

#define I2C_ADDR    0x27  // Define I2C Address where the PCF8574A is
// Address can be changed by soldering A0, A1, or A2
// Default is 0x27

// map the pin configuration of LCD backpack for the LiquidCristal class
const int BACKLIGHT_PIN = 3;
const int En_pin =  2;
const int Rw_pin =  1;
const int Rs_pin =  0;
const int D4_pin =  4;
const int D5_pin =  5;
const int D6_pin =  6;
const int D7_pin =  7;

LiquidCrystal_I2C lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin,BACKLIGHT_PIN, POSITIVE);

#include<openGLCD.h>
//#include "m2ghlc.h"

M2_LABEL(hello_world_label, NULL, "Hello World!");
M2tk m2(&hello_world_label, NULL, NULL, m2_gh_lc);

void setup() {
  m2_SetLiquidCrystal(&lcd, 16, 2);
}

void loop() {
  m2.draw();
  delay(500);
}