/*
 * Wiring:
 * Pin ADS1115:
 *    A0 = adj. Voltage
 *    A1 = adj. Current
 *    A2 = not adj. Current
 *    SCL = A3 -> A3 on Arduino
 *    SDA = A4 -> A4 on Arduino
 *    
 * Pin Arduino Pro Micro:
 *    A3 = SCL -> SDL on ADS1115
 *    A4 = SDA -> SDA on ADS1115
 * 
 *    5 -> RGB led pin R, via R220 resistor, PWM
 *    6 -> RGB led pin G, via R220 resistor, PWM
 *    7 -> Relay, adj. voltage
 *    8 -> Relay, not adj. voltage
 *    9 -> RGB led pin B, via R220 resistor, PWM
 *    10 ->  Button for Toggling adj. Voltage outlet, 10K pull-down resistor
 *    11 ->  Button for Toggling not adj. Voltage outlet, 10K pull-down resistor
 *    GND to PSU, to fix ground potiental 
 *    
 * RGB LED colors: 
 *    (0, 0, 255) Blue, blinking = Calibrating
 *    (0, 0, 255) Blue = Standby
 *    (0, 255, 0) Green = OK, on
 *    (255, 255, 0) Yellow = Be carefull
 *    (255, 143, 0) Orange, blink = Danger, close to threshold
 *    (255, 0, 0) Red = Overload
 *    
 * Continuous color change:
 *    (0, 255, 0) -> (+1, 0, 0) -> (255, 255, 0) -> (255, -1, 0) -> (255, 0, 0)
 *    
 */

#include "ACS712.h"
#include "ADS1X15.h"
#include "ZMPT101B.h"


 /* 
  *  Choose you ADC
  *  ADS1013 ADS(0x48);
  *  ADS1014 ADS(0x48);
  *  ADS1015 ADS(0x48);
  *  ADS1113 ADS(0x48);
  *  ADS1114 ADS(0x48);
  */
ADS1115 adc(0x48);

// Voltage Sensor, 0 < 250V AC, 2,5V = 250V
ZMPT101B voltageSensor(adc.readADC(0));

 /*
  *  choose you current sensor
  *  ACS712_05B
  *  ACS712_20A
  *  ACS712_30A
  */
ACS712 adjCurrentSensor(ACS712_05B, adc.readADC(1));
ACS712 nAdjCurrentSensor(ACS712_05B, adc.readADC(2));


int rLED = 5;         // RGB-led pin R, via R220 resistor, PWM
int gLED = 6;         // RGB-led pin G, via R220 resistor, PWM
int bLED = 9;         // RGB-led pin B, via R220 resistor, PWM

int adjRelay = 7;     // Relay, adj. voltage
int nAdjRelay = 8;    // Relay, not adj. voltage

int adjButton = 10;   // Button for Toggling adj. Voltage outlet, 10K pull-down resistor
int nAdjButton = 11;  // Button for Toggling not adj. Voltage outlet, 10K pull-down resistor

bool adjButtonValue = false;
bool nAdjButtonValue = false;
bool alert = false;

float adjU = 0;
float adjI = 0;
float nAdjI = 0;
float I = 0;
float nAdjU = 2.4;           // 240 V AC

float adjCurrentLimit = 2;  // Amp
int maxCurrent = 2;         // Amp
int alertCurrent = 2.5;     // Amp


void setup() {
  pinMode(rLED, OUTPUT);
  pinMode(gLED, OUTPUT);
  pinMode(bLED, OUTPUT);
  pinMode(adjRelay, OUTPUT);
  pinMode(nAdjRelay, OUTPUT);
  pinMode(adjButton, INPUT);
  pinMode(nAdjButton, INPUT);

  analogWrite(rLED, 0);
  analogWrite(gLED, 0);
  analogWrite(bLED, 0);
  digitalWrite(adjRelay, LOW);
  digitalWrite(nAdjRelay, LOW);
  digitalWrite(adjButton, LOW);
  digitalWrite(nAdjButton, LOW);

   /*
  *  0 = 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  *  1 = 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  *  2 = 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  *  3 = 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  *  4 = 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  *  5 = 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  */
  adc.setGain(5); 

  delay(100);

  initLED();
    
  voltageSensor.calibrate();  // Set 0V, voltage sensor must be without voltage

  initLED();
  
  adjCurrentSensor.calibrate();  // Set 0A, current must not be flowing in circut

  initLED();
  
  nAdjCurrentSensor.calibrate();

  // Signal setup is compleated
  initLED();
  rgbLED(0, 0, 255);
  
}

void initLED() {
  blueLED();
  delay(500);
  blackLED();
  delay(500);
}

void alertLED() {
  rgbLED(255, 0, 0);
}

void blueLED() {
  rgbLED(0, 0, 255);
}

void blackLED() {
  rgbLED(0, 0, 0);
}

void overload() {
    digitalWrite(adjRelay, LOW);
    digitalWrite(nAdjRelay, LOW);
    alertLED();
    alert = true;
}

void safe() {
  int n = 0;  // Counter
  
  calcAdjCurrentLimit();
  
  if(I > alertCurrent) {
    overload();
  }

  if(adjI > adjCurrentLimit) {
    overload();    
  }

  if(adjI > maxCurrent || I > maxCurrent || (adjI + I)> maxCurrent) {
    for(int i = 0; i > 5; ++i) {
      blackLED();
      delay(500);
      alertLED();
      delay(500);
      messurment();
      ++n;   
    }
  }
  
  if(n >= 4) {
    overload();
  }
  
}

void calcAdjCurrentLimit() {
  
  if(adjU < 30) {
    adjCurrentLimit = 0,25;
  } else if (30 < adjU && adjU < 240 ) {
    adjCurrentLimit = (( adjU * ( 2000 / 240 ) ) * 0.001 ); // note: mA -> A
  } else  {
    adjCurrentLimit = 2;
  }
  
}

void colorCalc() {
  
float result = 0;
unsigned int resolution = 65535; // 2 bytes / 0 to 65,535

  while(adjButtonValue || nAdjButtonValue) {
        
   if ( adjButtonValue == true && nAdjButtonValue == true ) {
     if( I > maxCurrent && adjI > adjCurrentLimit ) {
       result = ( maxCurrent - adjI ) / resolution;
     }
   }
   
   if ( adjButtonValue == true ) {
     result = maxCurrent / resolution;
   }

   if ( nAdjButtonValue == true ) {
     result = adjCurrentLimit / resolution; 
   }

    // (0, 255, 0) -> (+1, 0, 0) -> (255, 255, 0) -> (255, -1, 0) -> (255, 0, 0)
    if ( result >= 32767 ){
      rgbLED( getColorR(result), 255, 0);
    } else if ( 32768 <= result ) {
      rgbLED( 255, getColorR(result), 0);
    }
  }
}

int getColorR(int value) {
  unsigned int unit = 32767 / 255;
  return (int)(value / unit);
}

int getColorG(int value) {
  unsigned int unit = 65535 - 32767 / 255;
  return (int)(255 - ( value / unit ));
}

void rgbLED(int r, int g, int b) {
  analogWrite(rLED, r);
  analogWrite(gLED, g);
  analogWrite(bLED, b);
}


void messurment() {
  adjU = voltageSensor.getVoltageAC();
  adjI = adjCurrentSensor.getCurrentAC();
  nAdjI = nAdjCurrentSensor.getCurrentAC();
  I = adjI + nAdjI;
  calcAdjCurrentLimit();
}

void adjButtonToggle() {
  bool state = adjButtonValue;
  if(state == true) {
    adjButtonValue == false;
  } else {
    adjButtonValue == true;
  }
}

void nAdjButtonToggle() {
  bool state = nAdjButtonValue;
  if(state == true) {
    nAdjButtonValue == false;
  } else {
    nAdjButtonValue == true;
  }
}

void relays() {
    while(adjButtonValue) {
    digitalWrite(adjRelay, HIGH);
  }

  while(nAdjButtonValue) {
    digitalWrite(nAdjRelay, HIGH);
  }
}

void loop() {
  messurment();
  safe();

  int buttonA = digitalRead(adjButton);  // read input value
  if (buttonA == HIGH) {
    adjButtonToggle();
    alert = false;
  }

  int buttonB = digitalRead(nAdjButton);  // read input value
  if (buttonB == HIGH) {
    nAdjButtonToggle();
    alert = false;
  }

  relays();

  if (adjButtonValue || nAdjButtonValue) {
    colorCalc(); 
  } else if (alert == true) {
    alertLED();
  } else {
    blueLED();
  }
  
  delay(100);
}
