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
#include "ZMPT101B.h"

 /*
  *  choose you current sensor
  *  ACS712_05B - 185 mV/A
  *  ACS712_20A - 100 mV/A
  *  ACS712_30A - 66 mV/A
  */
ACS712 adjCurrentSensor(ACS712_05B, A1);
ACS712 nAdjCurrentSensor(ACS712_05B, A2);

// We have ZMPT101B sensor connected to A0 pin of arduino
// Replace with your version if necessary
ZMPT101B voltageSensor(A0);

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

float adjU;
float adjI;
float nAdjI;
float I = 0;
float nAdjU = 2.4;           // 240 V AC

float adjCurrentLimit;  // Amp
int maxCurrent = 2;         // Amp
int alertCurrent = 2.5;     // Amp

unsigned int debugId = 0;   // debug id counter

void setup() {

  Serial.begin(115200);
  
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

  delay(100);

  initLED();
    
  voltageSensor.calibrate();  // Set 0V, voltage sensor must be without voltage

  initLED();
  
  adjCurrentSensor.calibrate();  // Set 0A, current must not be flowing in circut

  initLED();
  
  nAdjCurrentSensor.calibrate();  // Set 0A, current must not be flowing in circut

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

  while(adjButtonValue || nAdjButtonValue) {
        
   if ( adjButtonValue == true && nAdjButtonValue == true ) {
     if( I > maxCurrent && adjI > adjCurrentLimit ) {
       result = ( maxCurrent - adjI ) / 2;
     }
   }
   
   if ( adjButtonValue == true ) {
     result = maxCurrent / 2;
   }

   if ( nAdjButtonValue == true ) {
     result = adjCurrentLimit / 2; 
   }

    // (0, 255, 0) -> (+1, 0, 0) -> (255, 255, 0) -> (255, -1, 0) -> (255, 0, 0)
    if ( result >= 255 ){
      rgbLED( result, 255, 0);
    } else if ( 256 <= result ) {
      rgbLED( 255, 255 - result, 0);
    }
  }
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

// Debug / Calibrate 
void serialDebugger() {
  Serial.println((String)"Readout ID: " + debugId);
  Serial.println((String)"bool adjButtonValue: " + adjButtonValue );
  Serial.println((String)"bool nAdjButtonValue: " + nAdjButtonValue );
  Serial.println((String)"bool alert: " + alert );
  
  Serial.println();

  Serial.println((String)"pin A0: " + digitalRead(A0) + " V");
  Serial.println((String)"pin A1: " + digitalRead(A1) + " V");
  Serial.println((String)"pin A2: " + digitalRead(A2) + " V");

  Serial.println();
  
  Serial.println((String)"float adjU: " + adjU + " V");
  Serial.println((String)"float adjI: " + adjI + " A");
  Serial.println((String)"float nAdjI: " + nAdjI + " A");
  Serial.println((String)"float I: " + I + " A");
  Serial.println((String)"float nAdjU: " + nAdjU + " V");
    
  Serial.println();
  
  Serial.print((String)"float adjCurrentLimit: " + adjCurrentLimit + " A");
  Serial.println((String)"int maxCurrent: " + maxCurrent + " A");
  Serial.println((String)"int alertCurrent: " + alertCurrent + " A");

  Serial.println();

  Serial.println("Updated Sensor readout:");

  Serial.print((String)"voltageSensor DC: " + voltageSensor.getVoltageDC() + " V");
  Serial.print((String)"voltageSensor AC: " + voltageSensor.getVoltageAC() + " V");

  Serial.print((String)"adj. Current DC: " + adjCurrentSensor.getCurrentDC() + " A");
  Serial.print((String)"adj. Current AC: " + adjCurrentSensor.getCurrentAC() + " A");

  Serial.print((String)"Current DC: " + nAdjCurrentSensor.getCurrentDC() + " A");
  Serial.print((String)"Current AC: " + adjCurrentSensor.getCurrentAC() + " A");
  
  Serial.println("\n");
  ++debugId;
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
