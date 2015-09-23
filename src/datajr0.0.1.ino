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
const int SETPOINT = 115;
const int IN    =  94;
const int OUT   =  64;
const int PGAIN    =  36;
const int IGAIN    =  37;
const int DGAIN    =  33;
const int TUNING   =  41;
const int STATUS   =  63;
const int HALARM   =  61;     
const int LALARM   =  91;

#include <Arduino.h>
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

const int RelayPin = 3;
const int ALARM_PIN = 4;
const unsigned long SERIAL_BAUD = 115200;
const int SERIAL_MIN_BITS = 4;

/*** Display vars ***/
//U8GLIB_ST7920_128X64 u8g(6, 8, 7, U8G_PIN_NONE);    //SCK, SID, CS
 
/*** PID vars ***/
int WindowSize = 10000;     //PID variables
unsigned long windowStartTime;
//int direction = REVERSE;
PID myPID(&vars[IN], &vars[OUT], &vars[SETPOINT],19432.63, 807.59,0, 1);  // create PID variable  REVERSE = COOL = 1  DIRECT = HEAT = 0
 
/*** Auto-tune vars ***/
byte ATuneModeRemember=2;
double kp=4,ki=2,kd=0;
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
unsigned long now = millis();
int tempTemp = 0;
double tempAvg = 0;
double tmp, tempIn, runningAvg;
int multiSample = WindowSize;
bool outputOn = false;
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
    vars[OUT]=aTuneStartValue;
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
    double tempOut = vars[OUT] / 100;
  dtostrf(tAvg, 3, 2, tempString);
  const int boxY = 60 - int(0.6 * tempOut);
    
    lcd.setCursor(11,0);
    lcd.print(String(vars[SETPOINT]));
    lcd.setCursor(11,2);
    lcd.print(String(tempString));
    
    /*
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
    */
}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void SendPlotData(){
  Serial.println(F("^"));
    Serial.println(vars[IN]*10);
  Serial.println(vars[OUT]);
  Serial.flush();
}

void SerialSend(){
    static int t, h, l;
    String alarm;
    (vars[TUNING] > 0.5)?t = 1:t = 0;
    (halarmOn || lalarmOn) ? alarm = "On": alarm = "Off";
    Serial.println(F("#"));
    Serial.println(vars[SETPOINT]);
    Serial.println(F("}"));
    Serial.println(myPID.GetKp());
    Serial.println(F("%"));
    Serial.println(myPID.GetKi());
    Serial.println(F("!"));
    Serial.println(myPID.GetKd());
    Serial.println(F(")"));
    Serial.println((double)vars[TUNING]);
    Serial.println(F("{"));
    Serial.println((double)vars[HALARM]);
    Serial.println(F("["));
    Serial.println((double)vars[LALARM]);
    Serial.println(F("]"));
    Serial.println(alarm);
  //Serial.flush();
}
 
void SerialReceive(){

    char inChar;

  while(Serial.available() > SERIAL_MIN_BITS){
        digitalWrite(23, HIGH);
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
    pinMode(ALARM_PIN, OUT);
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
  pinMode(RelayPin, OUT);
  windowStartTime = millis(); //initialize the variables we're linked to
  myPID.SetOutputLimits(0, WindowSize);  //tell the PID to range between 0 and the full window size
  myPID.SetMode(AUTOMATIC); //turn the PID on
}
 
void SetupSPI(void){
  SPI.begin();                            // begin SPI
  SPI.setClockDivider(SPI_CLOCK_DIV16);   // SPI speed to SPI_CLOCK_DIV16 (1MHz)
  SPI.setDataMode(SPI_MODE1);             // MAX31865 works in MODE1 or MODE3
  
  // initalize the chip select pin
  pinMode(CS0_PIN, OUT);
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
    vars[OUT] = 0;
    vars[IN] = 22;
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

void SetupLCD(void){
  lcd.begin(20,4);        // 20 columns by 4 rows on display

  lcd.setBacklight(HIGH); // Turn on backlight, LOW for off
  
  lcd.setCursor(0, 0);
  lcd.print(F("Setpoint:"));
  lcd.setCursor(0, 2);
  lcd.print(F("Temp. C :"));
}

/******************************==MAIN_LOOP==******************************/
void setup(void) {
    SetupVars();
    ReadVars();
  Serial.begin(SERIAL_BAUD);
  SetupPID();
  SetupAutoTune();
  SetupSPI();
    SetupAlarm();
    SetupLCD();
    pinMode(22, OUT); //serial out  <- indicator LEDs
    pinMode(23, OUT); //serial in
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
        digitalWrite(RelayPin,HIGH);
        Serial.println(F("_"));
        Serial.println(F("on "));
    }
  if(vars[OUT] < millis() - windowStartTime && outputOn == true){
        outputOn = false;
        digitalWrite(RelayPin,LOW);
        Serial.println(F("_"));
        Serial.println(F("off"));
    }
}
    
void loop(void) {
  //u8g.firstPage();  /***<-- DONT MOVE THIS??***/
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
        
        vars[IN] = runningAvg;
    
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