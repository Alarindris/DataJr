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
+ DATA1			10
, DATA2			11
- DATA3			12
. DATA4			13
/ MACH_ID		14
**************************************************************/

/**************************************************************
PINOUT TABLE
Alarm 				- A0
Display				- A4
Display				- A5
Rotary encoder A 	- D2
Rotary encoder B 	- D3
Rotary encoder R 	- D4
Rotary encoder G 	- D5
Rotary encoder B 	- D7
Relay output	 	- D8
**************************************************************/
const int RELAY_PIN = 8;
const int ROTARY_A = 3;
const int ROTARY_B = 2;
#define ALARM_PIN A0
const int BUTTON_PIN = 6;
const int RED 		= 4;
const int GREEN 	= 5;
const int BLUE 		= 7;

double vars[20] = {0};
const int OFFSET 	= 33;
const int VARTOTAL 	= 10; //must be set to number of vars to be sent/backed up
const int IN       	= 0;
const int OUT      	= 1;
const int SETPOINT 	= 2;
const int PGAIN    	= 3;
const int IGAIN    	= 4;
const int DGAIN    	= 5;
const int TUNING   	= 6;
const int HALARM   	= 7;     
const int LALARM   	= 8;
const int CALIBRATION = 9;
const int DATA1		= 10;
const int DATA2		= 11;
const int DATA3		= 12;
const int DATA4		= 13;
const int MACH_ID	= 14;


//#include <Bounce2.h>
//Bounce button = Bounce();

#include <rotary.h>
Rotary r = Rotary(ROTARY_A, ROTARY_B);
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

const unsigned long SERIAL_BAUD = 9600;
const int SERIAL_MIN_BITS = 4;

/*** PID vars ***/
const int DEFAULT_DIRECTION = 0;
int WindowSize = 10000;     //PID variables
unsigned long windowStartTime;
//int direction = REVERSE;
PID myPID(&vars[IN], &vars[OUT], &vars[SETPOINT],19432.63, 807.59,0, 0);  // create PID variable  REVERSE = COOL = 1  DIRECT = HEAT = 0

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
int multiSample = 1000;
bool outputOn = false;
bool alarmOn = false;
bool lalarmOn = false;
bool halarmOn = false;
int cursor = 0;
bool DISP = true;  //Whether or not to display temp data on LCD
bool OVERRIDE = false;

void Alarm(bool on){
	if (on){
		if(alarmOn == false){
			digitalWrite(RED, LOW);
			digitalWrite(ALARM_PIN, HIGH);
			alarmOn = true;
		}
	}
	else{
		if(alarmOn == true){
			digitalWrite(RED, HIGH);
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
		vars[OUT] = (double)aTuneStartValue;
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
	Serial.flush();
	Serial.println(vars[OUT]);
	Serial.flush();
}

void SerialSend(){
	Serial.println(char(0 + OFFSET));
	Serial.flush();
	Serial.println(vars[0]*10);
	Serial.flush();
	for(int i = 1; i < VARTOTAL; i++){
		Serial.println(char(i + OFFSET));
		Serial.flush();
		Serial.println(vars[i]);
		Serial.flush();
	}
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
	aTune.SetControlType(1); //0 = PI, 1 = PID
}

void SetupPID(void){
	pinMode(RELAY_PIN, OUTPUT);
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
	int winSize, dir, multi;
}settings;

void ReadVars(void){
	eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
	myPID.SetTunings(settings.p, settings.i, settings.d);
	myPID.SetControllerDirection(settings.dir);
	vars[SETPOINT] = settings.set;
	vars[CALIBRATION] = settings.ca;
	multiSample = settings.multi;
	//vars[HALARM] = settings.hA;
	//vars[LALARM] = settings.lA;
	//WindowSize = settings.winSize;
}

void SetupVars(void){
	if(isnan(vars[SETPOINT]) == 1){
		vars[SETPOINT] = 18.5;
	}
	if(isnan(vars[OUT]) == 1){
		vars[OUT] = 0.0;
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
	if(isnan(multiSample) == 1){
		multiSample = 1000;
	}
	if(isnan(settings.dir)){
		settings.dir = DEFAULT_DIRECTION;
	}
	vars[DATA1] = 0.0;
	vars[DATA2] = 0.0;
	vars[DATA3] = 0.0;
	vars[DATA4] = 0.0;
	vars[MACH_ID] = -1;
};

void WriteVars(void){
	settings.p = myPID.GetKp();
	settings.i = myPID.GetKi();
	settings.d = myPID.GetKd();
	settings.dir = myPID.GetDirection();			////THIS FUCKING THING DIRECTION REVERSE HEAT COOL 
	settings.set = vars[SETPOINT];
	settings.winSize = WindowSize;
	settings.hA = vars[HALARM];
	settings.lA = vars[LALARM];
	settings.ca = vars[CALIBRATION];
	settings.multi = multiSample;
	eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
}

void SetupLCD(void){
	lcd.begin(20,4);        // 20 columns by 4 rows on display
	lcd.clear();
	lcd.setBacklight(HIGH); // Turn on backlight, LOW for off
}

void SplashCursor(char c, int i){
	lcd.setCursor(i, 3);
	lcd.print(c);
	delay(550);
}

void Splash(void){
	lcd.setCursor(0, 0);
	lcd.print(F("Cherry Optical, Inc."));
	lcd.setCursor(2, 1);
	lcd.print(F("Producing vision"));
	lcd.setCursor(3, 2);
	lcd.print(F("to the highest"));
	lcd.setCursor(5, 3);
	lcd.print(F("definition."));
	delay(7500);
	
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(F("Data Jr. Rev. 2.1"));
	lcd.setCursor(0,1);
	lcd.print(F("C Erik Sikich 2015"));
	lcd.setCursor(0,2);
	lcd.print(F("Loading..."));
	lcd.setCursor(0,3);
	lcd.print(F("["));
	lcd.setCursor(19,3);
	lcd.print(F("]"));
	
	
	for(int i = 1; i < 19; i++){
		lcd.setCursor(i, 3);
		SplashCursor('|', i);
		SplashCursor('/', i);
		SplashCursor('-', i);
		SplashCursor(char(255), i);
	}
	
}

/******************************==MAIN_LOOP==******************************/
void setup(void) {
	SetupSPI();
	SetupLCD();
	Splash();
	Serial.begin(SERIAL_BAUD);
	ReadVars();
	SetupVars();
	SetupPID();
	SetupAutoTune();
	SetupAlarm();
	SetupEncoder();
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F(" Setpoint:"));
	lcd.setCursor(0, 2);
	lcd.print(F(" Temp. C :"));
	lcd.setCursor(0, 3);
	lcd.print(F(" Menu"));
}

void SetupEncoder(void){
	pinMode(4, OUTPUT);
	digitalWrite(4, HIGH);
	pinMode(BUTTON_PIN, INPUT);
	pinMode(5, OUTPUT);
	digitalWrite(5, HIGH);
	pinMode(7, OUTPUT);
	digitalWrite(7, HIGH);
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
		lalarmOn = true;
	}
	else if(alarmOn == true){
		Alarm(false);
		lalarmOn = false;
		halarmOn = false;
	}
}

void OutputCheck(void){
	if(vars[OUT] > millis() - windowStartTime && vars[OUT] > 1000 && outputOn == false){
		outputOn = true;
		digitalWrite(BLUE, LOW);
		digitalWrite(RELAY_PIN,HIGH);
		Serial.println(F("_"));
		Serial.println(F("on "));
	}
	if(vars[OUT] < millis() - windowStartTime && outputOn == true){
		outputOn = false;
		digitalWrite(BLUE, HIGH);
		digitalWrite(RELAY_PIN,LOW);
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
		}else{
			butDir = -1;
		}
	}
}

bool pressing = false;
bool pressed = false;

void checkPressed(void){
	if(digitalRead(BUTTON_PIN) == HIGH){
		digitalWrite(GREEN, LOW);
		pressing = true;
		butDir = 0;
	}else{
		if(pressing == true && pressed == false){
			digitalWrite(GREEN, HIGH);
			pressed = true;
			pressing = false;
		}
	}
}

int STATE = 0;
int DIG = 1;

void MainMenu(void){
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
	lcd.print(F("Cali Data"));
	lcd.setCursor(1, 3);
	lcd.print(F("NEXT"));	
}

void SmartA(void){
	lcd.clear();
	lcd.setCursor(1, 0);
	lcd.print(F("Z Before:"));
	lcd.setCursor(1, 1);
	lcd.print(F("Z After :"));
	lcd.setCursor(1, 2);
	lcd.print(F("X       :"));
	lcd.setCursor(1, 3);
	lcd.print(F("NEXT"));
}

void CaliMenu(void){
	lcd.clear();
	lcd.setCursor(1, 0);
	lcd.print(F("Smart A"));
	lcd.setCursor(1, 1);
	lcd.print(F("Smart XP"));
	lcd.setCursor(1, 3);
	lcd.print(F("BACK"));
}

void DisplayScreen(void){
	STATE = 0;
	DISP = true;
	lcd.clear();
	lcd.setCursor(1, 0);
	lcd.print(F("Setpoint:"));
	lcd.setCursor(1, 1);
	lcd.print(F("Direction:"));
	lcd.setCursor(12, 1);
	if(myPID.GetDirection() == 0){
		lcd.print(F("Heat"));
	}else{
		lcd.print(F("Cool"));
	}
	lcd.setCursor(1, 2);
	lcd.print(F("Temp C:"));
	lcd.setCursor(1, 3);
	lcd.print(F("MENU"));
	DisplayTemp(vars[INPUT]);
}

void menu(void){
	//lcd.setCursor(1, 2);
	//lcd.print(millis());
	delay(1);  //delay to catch input on 16MHz chip
	switch(STATE){
		case 0://**********************READOUT/MAIN SCREEN*********************
			CursorHandler();
			OVERRIDE = false;
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
						MainMenu();
						cursor = 0;
						break;
				}
				break;
			}
			break;
			
		case 1:
			EnterData(&vars[SETPOINT], 0, 11, 0, -1, 2, 4);
			break;
		case 2:
		case 3:
		case 4:
		case 5://******************************MENU****************************
			CursorHandler();
			OVERRIDE = false;
			lcd.setCursor(0, cursor);
			lcd.print(char(126));
			checkPressed();
			if(pressed){			
				switch(cursor){
					case 0://*****************AUTOTUNE*************************
						changeAutoTune();
						lcd.setCursor(11, 0);
						if(vars[TUNING] > 0){
							lcd.print(F("ON "));
						}else{
							lcd.print(F("OFF"));
						}
						break;
					case 1://*****************GOTO CALIBRATION*****************
						STATE = 6;
						break;
					case 2://*****************GOTO CALI DATA************************
						STATE = 7;
						cursor = 0;
						CaliMenu();
						break;
					case 3:
						STATE = 17;
						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print(F("Window: "));
						lcd.setCursor(1, 3);
						lcd.print(F("MAIN MENU"));
						pressed = false;
						cursor = 0;
						break;
				}
				pressed = false;
				//break;
			}
			break;
		case 6://*****************TEMP CALIBRATION***********************
		{
			EnterData(&vars[CALIBRATION], 1, 11, 5, 1, 2, 4);
			break;
		}
		case 7://*****************CALI DATA MENU*************************
			CursorHandler();
			checkPressed();
			if(pressed){			
				switch(cursor){
					case 0:
						STATE = 8;
						vars[MACH_ID] = 0;
						SmartA();
						cursor = 0;
						break;
					case 1:
						STATE = 15;
						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print(F("Pre"));
						lcd.setCursor(1, 1);
						lcd.print(F("Fine"));
						lcd.setCursor(1, 3);
						lcd.print(F("BACK"));
						cursor = 0;
						break;
					case 3:
						STATE = 5;
						MainMenu();
						break;
				}
				pressed = false;
			}
			break;
		case 8://*****************SMART A*********************************
			OVERRIDE = false;
			CursorHandler();
			checkPressed();
			if(pressed){
				switch(cursor){
					case 0:
						STATE = 10;
						break;
					case 1:
						STATE = 11;
						break;
					case 2:
						STATE = 12;
						break;
					case 3:
						STATE = 13;
						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print(F("Y       :"));
						lcd.setCursor(1, 1);
						lcd.print(F("Send Data"));
						lcd.setCursor(1, 2);
						lcd.print(F("MAIN MENU"));
						lcd.setCursor(1, 3);
						lcd.print(F("BACK"));
						break;
				}
				pressed = false;
			}
			break;
		case 9:
			break;
		case 10:
			EnterData(&vars[DATA1], 0, 10, 8, -1, 3, 5);
			break;
		case 11:
			EnterData(&vars[DATA2], 1, 10, 8, -1, 3, 5);
			break;
		case 12:
			EnterData(&vars[DATA3], 2, 10, 8, -1, 3, 5);
			break;
		case 13:
			OVERRIDE = false;
			CursorHandler();
			checkPressed();
			if(pressed){
				switch(cursor){
					case 0:
						STATE = 14;
						break;
					case 1:
						STATE = 16;
						break;
					case 2:
						STATE = 5;
						MainMenu();
						break;
					case 3:
						STATE = 8;
						SmartA();
						break;
				}
				pressed = false;
			}
			break;
		case 14:
			EnterData(&vars[DATA4], 0, 10, 13, -1, 3, 5);
			break;
		case 15:
			CursorHandler();
			checkPressed();
			if(pressed){
				switch(cursor){
					case 0:
						vars[MACH_ID] = 1;
						STATE = 8;
						SmartA();
						break;
					case 1:
						vars[MACH_ID] = 2;
						STATE = 8;
						SmartA();
						break;
					case 3:
						STATE = 7;
						CaliMenu();
						break;
				}
				cursor = 0;
				pressed = false;
			}
			break;
		case 16:
			for(int i = 10; i < 14; i++){  //Loops through DATA vars[]
				Serial.println(char(i + OFFSET));
				Serial.println(vars[i]);
			}
			STATE = 5;
			MainMenu();
			break;
		case 17:
			OVERRIDE = false;
			CursorHandler();
			checkPressed();
			if(pressed){
				switch(cursor){
					case 0:
						STATE = 18;
						break;
					case 3:
						STATE = 0;
						DisplayScreen();
						break;
				}
				
				pressed = false;
			}
			break;
		case 18:
			EnterDataInt(&multiSample, 0, 10, 17, -1, 2, 4);
			break;
			
	}
}

void CursorHandler(void){
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
}

void EnterData(double *var, int row, int col, int RetState, int decOff, int prec, int digs){
	OVERRIDE = true;
	int neg = 0;
	char tempString[10];
	if(pressed){
		DIG += 1;
		if(DIG > digs){
			STATE = RetState;
			DIG = 1;
		}
		WriteVars();
		pressed = false;
		return;
	}
			
	int off = DIG;
	if(off > 2) off += 1;
	if(*var <= -10){
		neg = 1;
	}else{
		neg = 0;
	}
	lcd.setCursor(col + off + decOff + neg, row);
	lcd.cursor();
	delay(10);
	lcd.noCursor();
	lcd.setCursor(col, row);
	if(butDir != 0){
		*var += double(butDir * pow(10.0, 2-DIG));
		dtostrf(*var, 3, prec, tempString);
		lcd.print(String(tempString) + " ");
	}
	butDir = 0;	
}

void EnterDataInt(int *var, int row, int col, int RetState, int decOff, int prec, int digs){
	OVERRIDE = true;
	int neg = 0;
	char tempString[10];
	if(pressed){
		DIG += 1;
		if(DIG > digs){
			STATE = RetState;
			DIG = 1;
		}
		WriteVars();
		pressed = false;
		return;
	}
			
	int off = DIG;
	if(off > 2) off += 1;
	if(*var <= -10){
		neg = 1;
	}else{
		neg = 0;
	}
	lcd.setCursor(col + off + decOff + neg, row);
	lcd.cursor();
	delay(10);
	lcd.noCursor();
	lcd.setCursor(col, row);
	if(butDir != 0){
		*var += butDir * pow(10, digs - DIG);
		lcd.print(String(*var) + " ");
	}
	butDir = 0;	
}


void loop(void) {
	//u8g.firstPage();  /***<-- DONT MOVE THIS??***/
	
	rtd_ptr = &RTD_CH0;
	rtd_ch0.MAX31865_full_read(rtd_ptr);          // Update MAX31855 readings

	checkPressed();
	
	if(butDir != 0 || pressing || pressed || OVERRIDE){
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
			//SendPlotData();
			SerialSend();
			tempAvg = 0;
		}
	}
	
	/*{
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
	/*
	if(serialTime % multiSample == 0)
	{
		SerialSend();
	}
	++serialTime;*/
}