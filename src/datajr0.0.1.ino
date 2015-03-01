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
**************************************************************/
#define SETPOINT    115
#define INPUT       94
#define OUTPUT      64
#define PGAIN       112
#define IGAIN       105
#define DGAIN       100


#include <Arduino.h>
#include "U8glib.h" //serial lcd library
#include <SPI.h>    //communication library
#include <PlayingWithFusion_MAX31865.h>              // core library  Playing With Fusion MAX31865 libraries
#include <PlayingWithFusion_MAX31865_STRUCT.h>       // struct library
#include <PID_v1.h>	//PID library
#include <PID_AutoTune_v0.h>
#include <avr/eeprom.h>
#include <dht11.h>

#define RelayPin 44
#define ALARM_PIN 8
#define DHT11PIN 2
#define SERIAL_BAUD 115200
#define SERIAL_MIN_BITS 4

/*** DHT11 vars ***/
dht11 DHT11;

/*** Display vars ***/
U8GLIB_ST7920_128X64 u8g(13, 11, 12, U8G_PIN_NONE);   	//create lcd display variable
 
/*** PID vars ***/
int WindowSize = 10000;			//PID variables
unsigned long windowStartTime;
int direction = DIRECT;
PID myPID(&vars[INPUT], &vars[OUTPUT], &vars[SETPOINT],19432.63, 807.59,0, direction); 	// create PID variable  REVERSE = COOL  DIRECT = HEAT
 
/*** Auto-tune vars ***/
byte ATuneModeRemember=2;
double kp=4,ki=2,kd=0;
double kpmodel=1.5, taup=100, theta[50];
double outputStart=0;
double aTuneStep=WindowSize/2, aTuneNoise=.02, aTuneStartValue=WindowSize/2;
unsigned int aTuneLookBack=20;
boolean tuning = false;
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
double hAlarm = 1000;
double lAlarm = -1000;
bool alarmOn = false;

void Alarm(bool on){
    if (on){
        digitalWrite(ALARM_PIN, HIGH);
        alarmOn = true;
    }
    else{    
        digitalWrite(ALARM_PIN, LOW);
        alarmOn = false;
    }
}

void AutoTuneHelper(boolean start){
  if(start)
    ATuneModeRemember = myPID.GetMode();
  else
    myPID.SetMode(ATuneModeRemember);
}

void changeAutoTune(){
 if(!tuning)
  {
    //Set the output to the desired starting frequency.
    vars[OUTPUT]=aTuneStartValue;
    aTune.SetNoiseBand(aTuneNoise);
    aTune.SetOutputStep(aTuneStep);
    aTune.SetLookbackSec((int)aTuneLookBack);
    AutoTuneHelper(true);
    tuning = true;
  }
  else
  { //cancel autotune
    aTune.Cancel();
    tuning = false;
    AutoTuneHelper(false);
  }
}

void DisplayTemp(double tAvg){
    double Ftemp = (tAvg * 9) / 5 + 32;
	char tempString[10];
	int gapT = millis()-windowStartTime;
	//double outTemp = vars[OUTPUT];
	dtostrf(tAvg, 3, 3, tempString);
	
    int chk = DHT11.read(DHT11PIN);
    
	do{
        u8g.setFont(u8g_font_fub14);
        u8g.setPrintPos(0, 16);
        u8g.print("Target:" + String(vars[SETPOINT]));
        u8g.setPrintPos(0, 35);
        u8g.print("C:" + String(tempString));
        /*u8g.setPrintPos(0, 30);
        u8g.print(String(Ftemp));*/
        u8g.setPrintPos(0, 54);
        u8g.print("vars[OUTPUT]:" + String(outputOn));
		/*u8g.print(String((double)DHT11.humidity));
        u8g.setPrintPos(0,55);
		u8g.print(gapT);*/
	}while( u8g.nextPage() );
}

double Fahrenheit(double celsius){
	return 1.8 * celsius + 32;
}

double Kelvin(double celsius){
	return celsius + 273.15;
}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void RTDDEBUG(void){
		Serial1.print("RTD Fault, register: ");
		do{
			u8g.setFont(u8g_font_unifont);
			u8g.setPrintPos(0, 20);
			u8g.print("Register fault");
		}while( u8g.nextPage() );	
		Serial1.print(RTD_CH0.status);
		if(0x80 & RTD_CH0.status)
		{
			Serial1.println("RTD High Threshold Met");  // RTD high threshold fault
		}
		else if(0x40 & RTD_CH0.status)
		{
			Serial1.println("RTD Low Threshold Met");   // RTD low threshold fault
		}
		else if(0x20 & RTD_CH0.status)
		{
			Serial1.println("REFin- > 0.85 x Vbias");   // REFin- > 0.85 x Vbias
		}
		else if(0x10 & RTD_CH0.status)
		{
			Serial1.println("FORCE- open");             // REFin- < 0.85 x Vbias, FORCE- open
		}
			else if(0x08 & RTD_CH0.status)
		{
			Serial1.println("FORCE- open");             // RTDin- < 0.85 x Vbias, FORCE- open
		}
		else if(0x04 & RTD_CH0.status)
		{
			Serial1.println("Over/Under voltage fault");  // overvoltage/undervoltage fault
		}
		else
		{
			Serial1.println("Unknown fault, check connection"); // print RTD temperature heading
		}
	  // end of fault handling	
 }

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
    static int t;
    (tuning)?t = 1:t = 0;
    Serial1.println("#");
    Serial1.println(vars[SETPOINT]);
    Serial1.println("}");
    Serial1.println((double)DHT11.humidity);
    Serial1.println("$");
    Serial1.println(myPID.GetKp());
    Serial1.println("%");
    Serial1.println(myPID.GetKi());
    Serial1.println("!");
    Serial1.println(myPID.GetKd());
    Serial1.println(")");
    Serial1.println(t);
    Serial1.println("{");
    Serial1.println((double)DHT11.temperature);
	Serial1.flush();
    digitalWrite(22, LOW);
}
 
void SerialReceive(){
	/*
    while(Serial1.available() > SERIAL_MIN_BITS){
        digitalWrite(23, HIGH);
        String inputString;
        char varVal, inChar, varIndex;
        
        varIndex = char(Serial1.read());
        
        while(inChar != '\n'){
            inChar = char(Serial1.read());
        }
        inChar = 0;
        
        inChar = char(Serial1.read());      
        while(inChar != '\n'){
            inputString += inChar;
			inChar = char(Serial1.read()); 
        }

        vars[varIndex] = atof(inputString.c_str());
        
        switch(varIndex){
            case PGAIN:
                myPID.SetP(vars[varIndex]);
                break;
            case IGAIN:
                myPID.SetI(vars[varIndex]);
                break;
            case DGAIN:
                myPID.SetD(vars[varIndex]);
                break;
        }
        WriteVars();
        
        digitalWrite(23, LOW);
    }*/

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

            switch(varIndex){
                case PGAIN:
                    myPID.SetP(vars[varIndex]);
                    break;
                case IGAIN:
                    myPID.SetI(vars[varIndex]);
                    break;
                case DGAIN:
                    myPID.SetD(vars[varIndex]);
                    break;
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

	if(tuning){
		tuning=false;
		changeAutoTune();
		tuning=true;
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
    double set, p, i, d;
    int winSize, dir;
}settings;

void ReadVars(void){
    eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
    myPID.SetTunings(settings.p, settings.i, settings.d);
    myPID.SetControllerDirection(settings.dir);
    vars[SETPOINT] = settings.set;
    //WindowSize = settings.winSize;
}

void SetupVars(void){
    vars[SETPOINT] = 24;
    vars[OUTPUT] = 0;
    vars[INPUT] = 22;
};

void WriteVars(void){
    settings.p = myPID.GetKp();
    settings.i = myPID.GetKi();
    settings.d = myPID.GetKd();
    settings.dir = direction;
    settings.set = vars[SETPOINT];
    settings.winSize = WindowSize;
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
    if(t > hAlarm){
        Alarm(true);
    }
    else if(t < lAlarm){
        Alarm(true);
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
        Serial1.println(outputOn);
        digitalWrite(22, LOW);
    }
	if(vars[OUTPUT] < millis() - windowStartTime && outputOn == "on"){
        outputOn = "off";
        digitalWrite(RelayPin,LOW);
        digitalWrite(22, HIGH);
        Serial1.println("_");
        Serial1.println(outputOn);
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
		tmp = (tempIn / 32) - 257.152; // <-sensor calibration
        
		tempAvg += tmp;
		runningAvg = tempAvg / tempTemp;
		
        AlarmCheck(runningAvg);
        
        vars[INPUT] = runningAvg;
		
		if(tuning){
			byte val = (aTune.Runtime());
			if (val!=0){
				tuning = false;
			}
			if(!tuning){ //we're done, set the tuning parameters
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