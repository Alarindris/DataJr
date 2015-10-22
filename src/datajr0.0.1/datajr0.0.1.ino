/***************************************************************************
* Processor/Platform: Arduino Pro-Mini
* Development Environment: Arduino 1.0.6
*
* Designed for use with with Playing With Fusion MAX31865 Resistance
* Temperature Device (RTD) breakout board: SEN-30202 (PT100 or PT1000)
*   ---> http://playingwithfusion.com/productview.php?pdid=25
*   ---> http://playingwithfusion.com/productview.php?pdid=26
*
*  Circuit:
*    Arduino Uno   Arduino Mega  -->  SEN-30201
*    CS0: pin  9   CS0: pin  9   -->  CS, CH0
*    CS1: pin 10   CS1: pin 10   -->  CS, CH1
*    MOSI: pin 11  MOSI: pin 51  -->  SDI (must not be changed for hardware SPI)
*    MISO: pin 12  MISO: pin 50  -->  SDO (must not be changed for hardware SPI)
*    SCK:  pin 13  SCK:  pin 52  -->  SCLK (must not be changed for hardware SPI)
***************************************************************************/
double vars[20] = {0};
const int OFFSET = 33;
const int VARTOTAL = 10;
/*************************************************************
ASCII offset + 33
variable table
! PIDIn         0
" PIDOut        1
# setpoint      2
$ P             3
% I             4
& D             5
' ISTUNING      6
( HALARM        7
) LALARM        8
* CALIBRATION	9
**************************************************************/
const int IN       = 0;
const int OUT      = 1;
const int SETPOINT = 2;
const int PGAIN    = 3;
const int IGAIN    = 4;
const int DGAIN    = 5;
const int TUNING   = 6;
const int HALARM   = 7;     
const int LALARM   = 8;
const int CALIBRATION = 9;
#include <Bounce2.h>
Bounce button = Bounce();

#include <rotary.h>
Rotary r = Rotary(2, 3);
#include <Arduino.h>
#include <Math.h>
//#include "U8glib.h" //serial lcd library
#include <SPI.h>    //communication library
#include <PlayingWithFusion_MAX31865.h>              // core library  Playing With Fusion MAX31865 libraries
#include <PlayingWithFusion_MAX31865_STRUCT.h>       // struct library
#include <PID_v1.h>  //PID library
#include <PID_AutoTune_v0.h>
#include <avr/eeprom.h>
/*** LCD stuff ***/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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

//const int RelayPin = 2;  CHANGED TO A0
const int ALARM_PIN = 4;
const unsigned long SERIAL_BAUD = 115200;
const int SERIAL_MIN_BITS = 4;

/*** Display vars ***/
//U8GLIB_ST7920_128X64 u8g(6, 8, 7, U8G_PIN_NONE);    //SCK, SID, CS

/*** PID vars ***/
int WindowSize = 5000;     //PID variables
unsigned long windowStartTime;
//int direction = REVERSE;
PID myPID(&vars[IN], &vars[OUT], &vars[SETPOINT],19432.63, 807.59,0, 1);  // create PID variable  REVERSE = COOL = 1  DIRECT = HEAT = 0

/*** Auto-tune vars ***/
byte ATuneModeRemember=2;
double kpmodel=1.5, taup=100, theta[50];
double outputStart=0;
double aTuneStep=5000, aTuneNoise=.1, aTuneStartValue=WindowSize/2;
unsigned int aTuneLookBack=30;
unsigned long  modelTime, serialTime;
PID_ATune aTune(&vars[IN], &vars[OUT]);

/*** MAX31865 vars ***/
static struct var_max31865 RTD_CH0;
struct var_max31865 *rtd_ptr;
const int CS0_PIN = 9;  // CS pin used for the connection with the sensor
// other connections are controlled by the SPI library)
PWFusion_MAX31865_RTD rtd_ch0(CS0_PIN);         //create rtd sensor variables

/*** Other vars ***/
int tempTemp = 0;
double tempAvg = 0;
double tmp, runningAvg;
int multiSample = WindowSize;
bool outputOn = false;
bool alarmOn = false;
bool lalarmOn = false;
bool halarmOn = false;
int cursor = 0;
bool DISP = true;  //Whether or not to display temp data on LCD

void Alarm(bool on){
	if (on){
		if(alarmOn == false){
			digitalWrite(ALARM_PIN, HIGH);
			alarmOn = true;
		}
	}
	else{
		if(alarmOn == true){
			digitalWrite(ALARM_PIN, LOW);
			alarmOn = false;
			lalarmOn = false;
			halarmOn = false;
		}
	}
}

void AutoTuneHelper(boolean start){
	if(start){
		ATuneModeRemember = myPID.GetMode();
	}
	else{
		myPID.SetMode(ATuneModeRemember);
	}
}

void TEST(int line){
	lcd.setCursor(1, line);
	lcd.print(String(millis()));
}

void changeAutoTune(){
	if(vars[TUNING] < 0.5){
		
		//Set the output to the desired starting frequency.
		vars[OUT] = aTuneStartValue;
		aTune.SetNoiseBand(aTuneNoise);
		aTune.SetOutputStep(aTuneStep);
		aTune.SetLookbackSec((int)aTuneLookBack);
		AutoTuneHelper(true);
		vars[TUNING] = 1.0;
	}
	else{ //cancel autotune
		aTune.Cancel();
		vars[TUNING] = 0.0;
		AutoTuneHelper(false);
	}
}

void DisplayTemp(double tAvg){
	if(DISP){
		double Ftemp = (tAvg * 9) / 5 + 32;
		char tempString[10];
		double tempOut = vars[OUT] / 100;
		dtostrf(tAvg, 3, 2, tempString);
		const int boxY = 60 - int(0.6 * tempOut);

		lcd.setCursor(11,0);
		lcd.print(String(vars[SETPOINT]));
		lcd.setCursor(11,2);
		lcd.print(String(tempString));
	}
}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void SendPlotData(){
	Serial.println(F("^"));
	Serial.println(vars[IN]*10);
	Serial.println(vars[OUT]);
	Serial.flush();
}

void SerialSend(){
	for(int i = 0; i < VARTOTAL; i++){
		Serial.println(char(i + OFFSET));
		Serial.println(vars[i]);
	}
	//Serial.flush();
}

void SerialReceive(){

	char inChar;

	while(Serial.available() > SERIAL_MIN_BITS){
		String inputString;
		char varIndex = 0;
		
		varIndex = char(Serial.read());
		
		int intIndex = varIndex;
		
		while(inChar != '\n'){
			inputString += inChar;
			inChar = char(Serial.read());
		}
		inChar = 0;

		if(varIndex > 1){
			inputString = "";
			
			inChar = char(Serial.read());
			while (inChar != '\n'){
				inputString += inChar;
				inChar = char(Serial.read());
			}
			intIndex -= OFFSET;
			vars[intIndex] = atof(inputString.c_str());
			double varTemp = vars[intIndex];
			switch(varIndex){
			case PGAIN:
				myPID.SetP(varTemp);
				break;
			case IGAIN:
				myPID.SetI(varTemp);
				break;
			case DGAIN:
				myPID.SetD(varTemp);
				break;
			case TUNING:
				changeAutoTune();
			}
			WriteVars();
		}
	} 
}

void SetupAlarm(){
	pinMode(ALARM_PIN, OUTPUT);
}

void SetupAutoTune(void){

	if(vars[TUNING] > 0.5){
		vars[TUNING] = 0;
		changeAutoTune();
		vars[TUNING] = 1;
	}
	serialTime = 0;
}

void SetupPID(void){
	pinMode(A0, OUTPUT);
	windowStartTime = millis(); //initialize the variables we're linked to
	myPID.SetOutputLimits(0, WindowSize);  //tell the PID to range between 0 and the full window size
	myPID.SetMode(AUTOMATIC); //turn the PID on
}

void SetupSPI(void){
	SPI.begin();                            // begin SPI
	SPI.setClockDivider(SPI_CLOCK_DIV16);   // SPI speed to SPI_CLOCK_DIV16 (1MHz)
	SPI.setDataMode(SPI_MODE1);             // MAX31865 works in MODE1 or MODE3

	// initalize the chip select pin
	pinMode(CS0_PIN, OUTPUT);
	rtd_ch0.MAX31865_config();

	// give the sensor time to set up
	delay(100);
}

struct settings_t{
	double set, p, i, d, hA, lA, ca;
	int winSize, dir;
}settings;

void ReadVars(void){
	eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
	myPID.SetTunings(settings.p, settings.i, settings.d);
	myPID.SetControllerDirection(settings.dir);
	vars[SETPOINT] = settings.set;
	vars[CALIBRATION] = settings.ca;
	//vars[HALARM] = settings.hA;
	//vars[LALARM] = settings.lA;
	//WindowSize = settings.winSize;
}

void SetupVars(void){
	if(isnan(vars[SETPOINT]) == 1){
		vars[SETPOINT] = 18.5;
	}
	if(isnan(vars[OUT]) == 1){
		vars[OUT] = 0;
	}
	if(isnan(vars[IN]) == 1){
		vars[IN] = 22;
	}
	if(isnan(vars[TUNING]) == 1){
		vars[TUNING] = 0;
	}
	if(isnan(vars[HALARM]) == 1){
		vars[HALARM] = 100.0;
	}
	if(isnan(vars[LALARM]) == 1){
		vars[LALARM] = 0.0;
	}
	if(isnan(vars[PGAIN]) == 1){
		vars[PGAIN] = 4.0;
	}
	if(isnan(vars[IGAIN]) == 1){
		vars[IGAIN] = 2.0;
	}
	if(isnan(vars[DGAIN]) == 1){
		vars[DGAIN] = 0.0;
	}
	if(isnan(vars[CALIBRATION]) == 1){
		vars[CALIBRATION] = -256.552;
	}
};

void WriteVars(void){
	settings.p = myPID.GetKp();
	settings.i = myPID.GetKi();
	settings.d = myPID.GetKd();
	settings.dir = 1;
	settings.set = vars[SETPOINT];
	settings.winSize = WindowSize;
	settings.hA = vars[HALARM];
	settings.lA = vars[LALARM];
	settings.ca = vars[CALIBRATION];
	eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
}

void SetupLCD(void){
	lcd.begin(20,4);        // 20 columns by 4 rows on display
	lcd.clear();
	lcd.setBacklight(HIGH); // Turn on backlight, LOW for off

	lcd.setCursor(0, 0);
	lcd.print(F(" Setpoint:"));
	lcd.setCursor(0, 2);
	lcd.print(F(" Temp. C :"));
	lcd.setCursor(0, 3);
	lcd.print(F(" Menu"));
}

/******************************==MAIN_LOOP==******************************/
void setup(void) {
	ReadVars();
	SetupVars();
	Serial.begin(SERIAL_BAUD);
	SetupPID();
	SetupAutoTune();
	SetupSPI();
	SetupAlarm();
	SetupLCD();
	pinMode(4, OUTPUT);
	pinMode(5, OUTPUT);
	button.attach(7);
	button.interval(5);
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
	sei();
}

void AlarmCheck(double t){
	if(t > vars[HALARM]){
		Alarm(true);
		halarmOn = true;
	}
	else if(t < vars[LALARM]){
		Alarm(true);
		lalarmOn = false;
	}
	else if(alarmOn == true){
		Alarm(false);
	}
}

void OutputCheck(void){
	if(vars[OUT] > millis() - windowStartTime && vars[OUT] > 1000 && outputOn == false){
		outputOn = true;
		digitalWrite(A0,HIGH);
		Serial.println(F("_"));
		Serial.println(F("on "));
	}
	if(vars[OUT] < millis() - windowStartTime && outputOn == true){
		outputOn = false;
		digitalWrite(A0,LOW);
		Serial.println(F("_"));
		Serial.println(F("off"));
	}
}

int butDir = 0;

ISR(PCINT2_vect) {
unsigned char result = r.process();
if (result) {
	if(result == DIR_CW){
		butDir = 1;
		digitalWrite(4, HIGH);
		digitalWrite(5, LOW);
	}else{
		butDir = -1;
		digitalWrite(4, LOW);
		digitalWrite(5, HIGH);
	}
}
}

bool pressing = false;
bool pressed = false;

void checkPressed(void){
	if(digitalRead(7) == LOW){
		pressing = true;
		butDir = 0;
	}else{
		if(pressing == true && pressed == false){
			pressed = true;
			pressing = false;
		}/*
		if(pressing == true && pressed == true){
			pressed = false;
			pressing = false;
		}*/
	}	
}

int STATE = 0;
int DIG = 0;

void menu(void){
	
	//checkPressed();
	
	switch(STATE){
		case 0:
			if(pressed){
				switch(cursor){
					case 0:
						STATE = 1;
						pressed = false;
						break;
					case 3:
						STATE = 5;
						pressed = false;
						DISP = false;
						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print(F("Autotune :"));
						lcd.setCursor(11, 0);
						if(vars[TUNING] > 0){
							lcd.print(F("ON"));
						}else{
							lcd.print(F("OFF"));
						}
						lcd.setCursor(1, 1);
						lcd.print(F("Calibrate:"));
						lcd.setCursor(11, 1);
						lcd.print(String(vars[CALIBRATION]));
						lcd.setCursor(1, 2);
						lcd.print(F("Setpoint"));
						lcd.setCursor(1, 3);
						lcd.print(F("Back"));
						cursor = 0;
						break;
				}
				break;
			}
			if(butDir != 0){

				lcd.setCursor(0,cursor);
				lcd.print(" ");
				cursor += butDir;
				if(cursor > 3){
					cursor = 0;
				}
				if(cursor < 0){
					cursor = 3;
				}
				lcd.setCursor(0, cursor);
				lcd.print(char(126));
				butDir = 0;
			}
			break;
			
		case 1:
		case 2:
		case 3:
		case 4:{
			if(pressed){
				STATE += 1;
				if(STATE > 4) STATE = 0;
				pressed = false;
				break;
			}
			
			int off = STATE;
			if(off > 2) off += 1;
			lcd.setCursor(10 + off, 0);
			lcd.cursor();
			lcd.noCursor();
			lcd.setCursor(11,0);
			if(butDir != 0){
				vars[SETPOINT] += double(butDir * pow(10.0, 2-STATE));
				lcd.print(String(vars[SETPOINT]) + " ");
			}
			butDir = 0;
			break;
		}
		case 5:
			lcd.setCursor(0, cursor);
			lcd.print(char(126));
			checkPressed();
			if(pressed){			
				switch(cursor){
					case 0:
						changeAutoTune();
						lcd.setCursor(11, 0);
						if(vars[TUNING] > 0){
							lcd.print(F("ON "));
						}else{
							lcd.print(F("OFF"));
						}
						pressed = false;
						pressing = false;
						break;
					case 1:
						break;
					case 2:
						break;
					case 3:
						STATE = 0;
						DISP = true;
						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print(F("Setpoint:"));
						lcd.setCursor(1, 2);
						lcd.print(F("Temp C:"));
						lcd.setCursor(1, 3);
						lcd.print(F("MENU"));
						DisplayTemp(vars[INPUT]);
						cursor = 0;
						break;
				}
				pressed = false;
				break;
			}
			if(butDir != 0){
				lcd.setCursor(0,cursor);
				lcd.print(" ");
				cursor += butDir;
				if(cursor > 3){
					cursor = 0;
				}
				if(cursor < 0){
					cursor = 3;
				}
				lcd.setCursor(0, cursor);
				lcd.print(char(126));
				butDir = 0;
			}
			break;
			
	}
}



void loop(void) {
	//u8g.firstPage();  /***<-- DONT MOVE THIS??***/
	rtd_ptr = &RTD_CH0;
	rtd_ch0.MAX31865_full_read(rtd_ptr);          // Update MAX31855 readings

	checkPressed();
	
	if(butDir != 0 || pressing || pressed){
		menu();
	}
	
	if(0 == RTD_CH0.status)                       // no fault, print info to serial port
	{ 
		tempTemp += 1;
		double tempIn = (double)RTD_CH0.rtd_res_raw;
		// calculate RTD resistance
		
		tmp = (tempIn / 32) + vars[CALIBRATION]; // <-sensor calibration
		
		tempAvg += tmp;
		runningAvg = tempAvg / tempTemp;
		
		vars[IN] = runningAvg;
		
		if(vars[TUNING] > 0.5){
			byte val = (aTune.Runtime());
			if (val != 0){
				vars[TUNING] = 0;
			}
			if(!(vars[TUNING] > 0.5)){ //we're done, set the tuning parameters
				vars[PGAIN] = aTune.GetKp();
				vars[IGAIN] = aTune.GetKi();
				vars[DGAIN] = aTune.GetKd();
				myPID.SetTunings(vars[PGAIN], vars[IGAIN], vars[DGAIN]);
				AutoTuneHelper(false);
				WriteVars();
			}
		}
		else{
			myPID.Compute();
		}
		
		if(millis() - windowStartTime > WindowSize){ //time to shift the Relay Window
			windowStartTime += WindowSize;
		}
		
		OutputCheck();
		
		if(tempTemp > multiSample){
			tempAvg = tempAvg / tempTemp;
			tempTemp = 0;
			AlarmCheck(tempAvg);
			DisplayTemp(tempAvg);
			SendPlotData();
			tempAvg = 0;
		}
	}
	/*else 
	{
		lcd.setCursor(0, 1);
		
		if(0x80 & RTD_CH0.status)
		{
			lcd.print(F(" High Threshold Met"));  // RTD high threshold fault
		}
		else if(0x40 & RTD_CH0.status)
		{
			lcd.print(F(" Low Threshold Met"));   // RTD low threshold fault
		}
		else if(0x20 & RTD_CH0.status)
		{
			lcd.print(F(" REFin- > 0.85 x Vbias"));   // REFin- > 0.85 x Vbias
		}
		else if(0x10 & RTD_CH0.status)
		{
			lcd.print(F(" FORCE- open"));             // REFin- < 0.85 x Vbias, FORCE- open
		}
		else if(0x08 & RTD_CH0.status)
		{
			lcd.print(F(" FORCE- open"));             // RTDin- < 0.85 x Vbias, FORCE- open
		}
		else if(0x04 & RTD_CH0.status)
		{
			lcd.print(F(" Over/Under voltage fault"));  // overvoltage/undervoltage fault
		}
		else
		{
			lcd.print(F(" Unknown fault, check connection")); // print RTD temperature heading
		}
	}  // end of fault handling*/

	SerialReceive();

	if(millis()>serialTime)
	{
		SerialSend();
		serialTime+=500;
	}
	if(millis() % 20000 == 0){
		WriteVars();
	}
}