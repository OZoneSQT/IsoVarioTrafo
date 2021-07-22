/*
 * Wiring:
 * Pin ADS1115:
 *    A0 = adj. Voltage
 *    A1 = adj. Current
 *    A2 = not adj. Current
 *    
 * Pin Arduino Pro Micro: 
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
float nAdjU = 0.24;         // 0,24 = 24 V  2,4 = 240 V

float adjCurrentLimit;  // Amp
int maxCurrent = 2;         // Amp
int alertCurrent = 2.5;     // Amp

void setup() {

  Serial.begin(115200);
  Serial.println("Start initializing... ");
  
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
  Serial.println("Voltage sensor calibrated");
  initLED();
  
  adjCurrentSensor.calibrate();  // Set 0A, current must not be flowing in circut
  Serial.println("Adjustable current sensor calibrated");

  initLED();
  
  nAdjCurrentSensor.calibrate();  // Set 0A, current must not be flowing in circut
  Serial.println("Not-adjustable current sensor calibrated");

  // Signal setup is compleated
  initLED();
  rgbLED(0, 0, 255);

  Serial.println("Ended initializing.\n");
}

void initLED() {
  blueLED();
  delay(500);
  blackLED();
  delay(500);
}

void alertLED() {
  rgbLED(255, 0, 0);
  Serial.println("Alert LED - ON (RED)");
}

void blueLED() {
  rgbLED(0, 0, 255);
  Serial.println("Standby LED - ON (Blue)");
}

void blackLED() {
  rgbLED(0, 0, 0);
  Serial.println("LED - OFF");
}

void overload() {
    digitalWrite(adjRelay, LOW);
    digitalWrite(nAdjRelay, LOW);
    alertLED();
    alert = true;
    Serial.println("Relays OFF, AlertLED; ON - Overload");
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
  Serial.print("Calculateing adjustable current limit...");
  if(adjU < 30) {
    adjCurrentLimit = 0,25;
  } else if (30 < adjU && adjU < 240 ) {
    adjCurrentLimit = (( adjU * ( 2000 / 240 ) ) * 0.001 ); // note: mA -> A
  } else  {
    adjCurrentLimit = 2;
  }
  Serial.print((String)"adjCurrentLimit: " + adjCurrentLimit + " A");
}

void colorCalc() {
float result = 0;
  Serial.print("Calculateing LED color...");
  Serial.println((String)"bool adjButtonValue: " + adjButtonValue );
  Serial.println((String)"bool nAdjButtonValue: " + nAdjButtonValue );
  
  while(adjButtonValue || nAdjButtonValue) {
   Serial.print("Do nou calculate if adjButtonValue or nAdjButtonValue is = false");     
   if ( adjButtonValue == true && nAdjButtonValue == true ) {
     if( I > maxCurrent && adjI > adjCurrentLimit ) {
       result = ( maxCurrent - adjI ) / 2;
       Serial.println((String)"adjButtonValue == true && nAdjButtonValue == true => " + result );
     }
   }
   
   if ( adjButtonValue == true ) {
     result = maxCurrent / 2;
     Serial.println((String)"adjButtonValue == true => " + result );
   }

   if ( nAdjButtonValue == true ) {
     result = adjCurrentLimit / 2; 
     Serial.println((String)"nAdjButtonValue == true => " + result );
   }

    // (0, 255, 0) -> (+1, 0, 0) -> (255, 255, 0) -> (255, -1, 0) -> (255, 0, 0)
    if ( result >= 255 ){
      rgbLED( result, 255, 0);
      Serial.println((String)"rgbLED( " + result + ", 255, 0 )" );
    } else if ( 256 <= result ) {
      rgbLED( 255, 255 - result, 0);
      Serial.println((String)"rgbLED( 255, " + result + ", 0 )" );
    }
  }
}

void rgbLED(int r, int g, int b) {
  analogWrite(rLED, r);
  analogWrite(gLED, g);
  analogWrite(bLED, b);
  Serial.println((String)"analogWrite(rLED, r) = " + r );
  Serial.println((String)"analogWrite(gLED, g) = " + g );
  Serial.println((String)"analogWrite(bLED, b) = " + b );
}

void messurment() { 
  Serial.println((String)"pin A0: " + digitalRead(A0) + " V");
  adjU = voltageSensor.getVoltageDC();
  Serial.println((String)"adjU: (DC) = " + adjU + " V");

  Serial.println((String)"pin A1: " + digitalRead(A1) + " V");
  adjI = adjCurrentSensor.getCurrentDC();
  Serial.println((String)"adjI: (DC) = " + adjI + " A");

  Serial.println((String)"pin A2: " + digitalRead(A2) + " V");
  nAdjI = nAdjCurrentSensor.getCurrentDC();
  Serial.println((String)"nAdjI: (DC) = " + nAdjI + " A");
  
  I = adjI + nAdjI;
  Serial.println((String)"adjI + nAdjI = I: = " + I + " A");
  
  calcAdjCurrentLimit();

  Serial.print((String)"float adjCurrentLimit = " + adjCurrentLimit + " A");
  Serial.println((String)"int maxCurrent = " + maxCurrent + " A");
  Serial.println((String)"int alertCurrent = " + alertCurrent + " A");
  
}

void adjButtonToggle() {
  bool state = adjButtonValue;
  if(state == true) {
    adjButtonValue == false;
    Serial.println((String)"digitalRead(10) (adjButton) = " + digitalRead(10));
  } else {
    adjButtonValue == true;
    Serial.println((String)"digitalRead(10) (adjButton) = " + digitalRead(10));
  }
}

void nAdjButtonToggle() {
  bool state = nAdjButtonValue;
  if(state == true) {
    nAdjButtonValue == false;
    Serial.println((String)"digitalRead(11) (nAdjButton) = " + digitalRead(11));
  } else {
    nAdjButtonValue == true;
    Serial.println((String)"digitalRead(11) (nAdjButton) = " + digitalRead(11));
  }
}

void relays() {
    while(adjButtonValue) {
    digitalWrite(adjRelay, HIGH);
    Serial.println((String)"digitalWrite(7) (adjRelay) is HIGH? = " + digitalRead(11));
  }

  while(nAdjButtonValue) {
    digitalWrite(nAdjRelay, HIGH);
    Serial.println((String)"digitalWrite(8) (nAdjRelay) is HIGH? = " + digitalRead(8));
  }
}

void loop() {
  Serial.println("New loop: ");
  Serial.println("messurment()");
  messurment();
  Serial.println("safe()");
  safe();

  Serial.println("read adjButton");
  int buttonA = digitalRead(adjButton);  // read input value
  if (buttonA == HIGH) {
    adjButtonToggle();
    alert = false;
  }

  Serial.println("nAdjButton");
  int buttonB = digitalRead(nAdjButton);  // read input value
  if (buttonB == HIGH) {
    nAdjButtonToggle();
    alert = false;
  }

  Serial.println("relays()");
  relays();

  Serial.println("LEDs");
  if (adjButtonValue || nAdjButtonValue) {
    colorCalc(); 
  } else if (alert == true) {
    alertLED();
  } else {
    blueLED();
  }
  
  delay(100);
  
  Serial.println("Loop ended");
  
  Serial.println("\n - - - \n");
    
}
