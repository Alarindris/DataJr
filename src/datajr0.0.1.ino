/***************************************************************************
* Processor/Platform: Arduino MEGA 2560
* Development Environment: Arduino 1.0.6
*
* Designed for use with with Playing With Fusion MAX31865 Resistance
* Temperature Device (RTD) breakout board: SEN-30202 (PT100 or PT1000)
*   ---> http://playingwithfusion.com/productview.php?pdid=25
*   ---> http://playingwithfusion.com/productview.php?pdid=26
 
* This file configures then runs a program on an Arduino Uno to read a 2-ch
* MAX31865 RTD-to-digital converter breakout board and print results to
* a serial port. Communication is via SPI built-in library.
*    - Configure Arduino Uno
*    - Configure and read resistances and statuses from MAX31865 IC 
*      - Write config registers (MAX31865 starts up in a low-power state)
*      - RTD resistance register
*      - High and low status thresholds 
*      - Fault statuses
*    - Write formatted information to serial port
*  Circuit:
*    Arduino Uno   Arduino Mega  -->  SEN-30201
*    CS0: pin  9   CS0: pin  9   -->  CS, CH0
*    CS1: pin 10   CS1: pin 10   -->  CS, CH1
*    MOSI: pin 11  MOSI: pin 51  -->  SDI (must not be changed for hardware SPI)
*    MISO: pin 12  MISO: pin 50  -->  SDO (must not be changed for hardware SPI)
*    SCK:  pin 13  SCK:  pin 52  -->  SCLK (must not be changed for hardware SPI)
*    GND           GND           -->  GND
*    5V            5V            -->  Vin (supply with same voltage as Arduino I/O, 5V)
***************************************************************************/
double vars[128] = {0};
/*************************************************************
variable table
^ PIDIn         94
@ PIDOut        64
# setpoint      35
} humidity      125
$ P             36
% I             37
! D             33
& pout          38
* iout          42
( dout          40
) isTuning      41
_ control       95
+ mode          43
- override      45
= hAlarm        61
[ lAlarm        91
] alarmstate    93
{ ambienttemp   123
? status        63
**************************************************************/
#define SETPOINT    115
#define INPUT       94
#define OUTPUT      64
#define PGAIN       36
#define IGAIN       37
#define DGAIN       33
#define TUNING      41
#define STATUS      63
#define HALARM      61      
#define LALARM      91

#include <Arduino.h>
#include "U8glib.h" //serial lcd library
#include <SPI.h>    //communication library
#include <PlayingWithFusion_MAX31865.h>              // core library  Playing With Fusion MAX31865 libraries
#include <PlayingWithFusion_MAX31865_STRUCT.h>       // struct library
#include <PID_v1.h>	//PID library
#include <PID_AutoTune_v0.h>
#include <avr/eeprom.h>


#define RelayPin 44
#define ALARM_PIN 8
#define SERIAL_BAUD 115200
#define SERIAL_MIN_BITS 4

/*** Display vars ***/
U8GLIB_ST7920_128X64 u8g(13, 11, 12, U8G_PIN_NONE);   	//create lcd display variable
 
/*** PID vars ***/
int WindowSize = 10000;			//PID variables
unsigned long windowStartTime;
int direction = REVERSE;
PID myPID(&vars[INPUT], &vars[OUTPUT], &vars[SETPOINT],19432.63, 807.59,0, direction); 	// create PID variable  REVERSE = COOL  DIRECT = HEAT
 
/*** Auto-tune vars ***/
byte ATuneModeRemember=2;
double kp=4,ki=2,kd=0;
double kpmodel=1.5, taup=100, theta[50];
double outputStart=0;
double aTuneStep=5000, aTuneNoise=.1, aTuneStartValue=WindowSize/2;
unsigned int aTuneLookBack=30;
unsigned long  modelTime, serialTime;
PID_ATune aTune(&vars[INPUT], &vars[OUTPUT]);
 
/*** MAX31865 vars ***/
static struct var_max31865 RTD_CH0;
struct var_max31865 *rtd_ptr;
const int CS0_PIN = 9;  // CS pin used for the connection with the sensor
						// other connections are controlled by the SPI library)
PWFusion_MAX31865_RTD rtd_ch0(CS0_PIN);  				//create rtd sensor variables
 
/*** Other vars ***/
unsigned long now = millis();
int tempTemp = 0;
double tempAvg = 0;
double tmp, tempIn, runningAvg;
int multiSample = WindowSize;
String outputOn = "off";
bool TIMEOUT = false;
bool alarmOn = false;
bool lalarmOn = false;
bool halarmOn = false;

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
  if(start)
    ATuneModeRemember = myPID.GetMode();
  else
    myPID.SetMode(ATuneModeRemember);
}

void changeAutoTune(){
 if(vars[TUNING] > 0.5)
  {
    //Set the output to the desired starting frequency.
    vars[OUTPUT]=aTuneStartValue;
    aTune.SetNoiseBand(aTuneNoise);
    aTune.SetOutputStep(aTuneStep);
    aTune.SetLookbackSec((int)aTuneLookBack);
    AutoTuneHelper(true);
    vars[TUNING] = 1;
  }
  else
  { //cancel autotune
    aTune.Cancel();
    vars[TUNING] = 0;
    AutoTuneHelper(false);
  }
}

void DisplayTemp(double tAvg){
    double Ftemp = (tAvg * 9) / 5 + 32;
	char tempString[10];
    double tempOut = vars[OUTPUT] / 100;
	dtostrf(tAvg, 3, 2, tempString);
	int boxY = 60 - int(0.6 * tempOut);
    
	do{
        for(int i = 0; i < 10; i++){
            u8g.drawPixel(111, (i * 6) + 2);
            u8g.drawPixel(124, (i * 6) + 2);
        }
        
        u8g.drawLine(0,24,107,24);
        u8g.drawRFrame(0,0,108,64,4);
        u8g.drawRFrame(110,0,16,64,4);
        
        if(int(tempOut) > 0){
            u8g.drawRBox(112, boxY + 2, 12, int(tempOut * 0.6), 4);
        }
        
        u8g.setFont(u8g_font_fur30n);
        u8g.setPrintPos(3,60);
        u8g.print(String(tempString));

        u8g.setFont(u8g_font_fub17r);
        u8g.setPrintPos(2, 21);
        u8g.print(String(vars[SETPOINT]));
        u8g.setPrintPos(65, 21);
        if(outputOn == "on"){
            u8g.print("On");
        }
        else{
            u8g.print("Off");
        }
        
    }while( u8g.nextPage() );
}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void SendPlotData(){
    digitalWrite(22, HIGH);
	Serial1.println("^");
    Serial1.println(vars[INPUT]*10);
	Serial1.println(vars[OUTPUT]);
	Serial1.flush();
    digitalWrite(22, LOW);
}

void SerialSend(){
    digitalWrite(22, HIGH);
    static int t, h, l;
    String alarm;
    (vars[TUNING] > 0.5)?t = 1:t = 0;
    (halarmOn || lalarmOn) ? alarm = "On": alarm = "Off";
    Serial1.println("#");
    Serial1.println(vars[SETPOINT]);
    Serial1.println("}");
    Serial1.println(myPID.GetKp());
    Serial1.println("%");
    Serial1.println(myPID.GetKi());
    Serial1.println("!");
    Serial1.println(myPID.GetKd());
    Serial1.println(")");
    Serial1.println((double)vars[TUNING]);
    Serial1.println("{");
    Serial1.println((double)vars[HALARM]);
    Serial1.println("[");
    Serial1.println((double)vars[LALARM]);
    Serial1.println("]");
    Serial1.println(alarm);
	//Serial1.flush();
    digitalWrite(22, LOW);
}
 
void SerialReceive(){

    char inChar;

	while(Serial1.available() > SERIAL_MIN_BITS){
        digitalWrite(23, HIGH);
        String inputString;
        char varIndex = 0;
        
        varIndex = char(Serial1.read());
      
        int intIndex = varIndex;
		
        while(inChar != '\n'){
			inputString += inChar;
			inChar = char(Serial1.read());
		}
        inChar = 0;

		if(varIndex > 1){
			inputString = "";
       
            inChar = char(Serial1.read());
			while (inChar != '\n'){
                inputString += inChar;
                inChar = char(Serial1.read());
            }
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
        digitalWrite(23, LOW);

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
	pinMode(RelayPin, OUTPUT);
	windowStartTime = millis(); //initialize the variables we're linked to
	myPID.SetOutputLimits(0, WindowSize);  //tell the PID to range between 0 and the full window size
	myPID.SetMode(AUTOMATIC);	//turn the PID on
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
    double set, p, i, d, hA, lA;
    int winSize, dir;
}settings;

void ReadVars(void){
    eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
    myPID.SetTunings(settings.p, settings.i, settings.d);
    myPID.SetControllerDirection(settings.dir);
    vars[SETPOINT] = settings.set;
    //vars[HALARM] = settings.hA;
    //vars[LALARM] = settings.lA;
    //WindowSize = settings.winSize;
}

void SetupVars(void){
    vars[SETPOINT] = 24;
    vars[OUTPUT] = 0;
    vars[INPUT] = 22;
    vars[TUNING] = 0;
    vars[HALARM] = 100.0;
    vars[LALARM] = 0.0;
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
    eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
}

/******************************==MAIN_LOOP==******************************/
void setup(void) {
    SetupVars();
    ReadVars();
	Serial1.begin(SERIAL_BAUD);
	SetupPID();
	SetupAutoTune();
	SetupSPI();
    SetupAlarm();
    pinMode(22, OUTPUT); //serial out  <- indicator LEDs
    pinMode(23, OUTPUT); //serial in
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
	if(vars[OUTPUT] > millis() - windowStartTime && vars[OUTPUT] > 1000 && outputOn == "off"){
        outputOn = "on";
        digitalWrite(RelayPin,HIGH);
        digitalWrite(22, HIGH);
        Serial1.println("_");
        Serial1.println("on ");
        digitalWrite(22, LOW);
    }
	if(vars[OUTPUT] < millis() - windowStartTime && outputOn == "on"){
        outputOn = "off";
        digitalWrite(RelayPin,LOW);
        digitalWrite(22, HIGH);
        Serial1.println("_");
        Serial1.println("off");
        digitalWrite(22, LOW);
    }
}
    
void loop(void) {
	u8g.firstPage();  /***<-- DONT MOVE THIS??***/
	now = millis();
	rtd_ptr = &RTD_CH0;
	rtd_ch0.MAX31865_full_read(rtd_ptr);          // Update MAX31855 readings
    
	if(0 == RTD_CH0.status)                       // no fault, print info to serial port
	{
		tempTemp += 1;
		tempIn = (double)RTD_CH0.rtd_res_raw;
		// calculate RTD resistance
		tmp = tempIn * 400 / 32768;
		tmp = (tempIn / 32) - 256.552; // <-sensor calibration
        
		tempAvg += tmp;
		runningAvg = tempAvg / tempTemp;
		
        
        
        vars[INPUT] = runningAvg;
		
		if(vars[TUNING] > 0.5){
			byte val = (aTune.Runtime());
			if (val != 0){
				vars[TUNING] = 0;
			}
			if(!(vars[TUNING] > 0.5)){ //we're done, set the tuning parameters
				kp = aTune.GetKp();
				ki = aTune.GetKi();
				kd = aTune.GetKd();
				myPID.SetTunings(kp,ki,kd);
				AutoTuneHelper(false);
                WriteVars();
			}
		}
		else{
			myPID.Compute();
		}
		
		if(millis() - windowStartTime>WindowSize){ //time to shift the Relay Window
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
