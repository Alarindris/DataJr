#include <rotary.h>

Rotary r = Rotary(2, 3);
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Bounce2.h>

Bounce  button  = Bounce();

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

void SetupLCD(void){
	lcd.begin(20,4);        // 20 columns by 4 rows on display
}

void setup() {
  SetupLCD();
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(7, INPUT);
  digitalWrite(7, HIGH);
  button.attach(7);
  button.interval(5);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
}

int crap = 0;

ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {
    if(result == DIR_CW){
		crap = 1;
	}else{
		crap = -1;
	}
  }
}

void loop() {
	if(digitalRead(7) == LOW){
		lcd.setCursor(0,3);
		lcd.print((char)126);
	}else{
		lcd.setCursor(0,3);
		lcd.print(" ");
	}

	if(crap == 1){
		digitalWrite(4, HIGH);
		digitalWrite(5, LOW);
		crap = 0;
	}
	if(crap == -1){
		digitalWrite(5, HIGH);
		digitalWrite(4, LOW);
		crap = 0;
	}

}

