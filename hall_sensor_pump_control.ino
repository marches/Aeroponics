/*
Sensors, servo and pump control for PoE 2016
 
 */

#include <Servo.h>

Servo servo;
int pos1 = 0;    // variable to store the servo position
int startpos1 = 0;   // beginning angle for servo
int endpos1 = 180;  // end angle for servo
int increment1 = 1;

byte sensorInterrupt = 0;  // 0 = digital pin 2
byte hallSensorPin = 2;
byte pumpStatus = 0;

byte lowWaterPin = A0;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 4.5;
const byte ecLowerBound = 5;

volatile byte pulseCount;  
float flowRate;

float flowThreshold;
unsigned long pumpOnTime = 1500;
unsigned long pumpOffTime = 4500;
unsigned long flowCheckTime;
unsigned long lastSwitch;
unsigned long currentTime;
unsigned long timeTest;
const int pumpPin = 3;
byte waterFlow;  // 1 = water flowing, 0 =water not flowing
byte systemStatus ; // 1 = something wrong, 0 = nothing bad detected
byte floodWarning;
byte lowWaterVal;
//--------------------------------------------------------------------

byte systemCheck(){
  Serial.print("Running system check. The pump status is:  ");
  Serial.println(pumpStatus);
  if(pumpStatus == 1){
    timeTest=millis();
    if(flowCheck(timeTest) == 0){
      Serial.println(flowCheck(timeTest));
      Serial.print("Pump clogged");
      systemStatus = 1;
    }
  }
  if(pumpStatus == 0){
    timeTest = millis();
    if(flowCheck(timeTest)==1){
      Serial.println(flowCheck(timeTest));
      Serial.print("Water unexpectedly flowing");
      systemStatus = 1;
    }
  }
waterLevelCheck();
Serial.print("The flood status is:  ");
Serial.println(floodWarning);
if(floodWarning == 1){
  systemStatus = 1;
}
  return systemStatus;
}

int waterLevelCheck(){
  lowWaterVal = analogRead(lowWaterPin);
  Serial.print("The low electrical conductivity is:  ");
  Serial.println(lowWaterVal);
  if(lowWaterVal < ecLowerBound){
    floodWarning = 1;
    Serial.println("There is a leak");
  }
  if(lowWaterVal > ecLowerBound){
    floodWarning = 0;
  }
  return floodWarning;
}

int flowCheck(unsigned long flowCheckTime)

{
  Serial.print(" ");
  delay(1000);
   /*if((millis() - flowCheckTime) > 1000)    // Only process counters once per second
  { */
    detachInterrupt(sensorInterrupt);
    flowRate = ((1000.0 / (millis() - flowCheckTime)) * pulseCount) / calibrationFactor;

    pulseCount = 0;
    
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
    Serial.print("the flow rate is:  ");
    Serial.println(flowRate);
    if(flowRate < flowThreshold){
      Serial.println("No water flowing");
      waterFlow = 0; // no water flowing
    }
    if(flowRate > flowThreshold){
      Serial.println("Water flowing");
      waterFlow = 1; // water flowing
      
    } 
    return waterFlow;
  //}
}

void pulseCounter()
{
  pulseCount++;

}

void servoMove(){
      for(pos1 = startpos1; pos1 <= endpos1; pos1 += increment1) // The base servo goes from specified beginning and ending angles
    {                                  
      servo.write(pos1);              // tells servo to go to position in variable 'poss' 
        Serial.print(pos1);           // prints angle and IR value to Serial
        Serial.print(" ");
        Serial.println(";");
        delay(15);                 
    }
    for (pos1 = endpos1; pos1 >= startpos1; pos1 -= increment1) { //moves servo back to starting position
        servo.write(pos1);              
        delay(15);                       
    }
    Serial.println("SERVO MOVING");
}

void setup()
{
  Serial.begin(9600);

  servo.attach(9);  // attaches the servo on pin 10 to the servo object
  
  pinMode(hallSensorPin, INPUT);
  pinMode(lowWaterPin, INPUT);
  digitalWrite(hallSensorPin, HIGH);

  pinMode(pumpPin, OUTPUT);
  lastSwitch = 0;
  pumpStatus = 0;
  waterFlow = 0;
  systemCheck();
  flowThreshold  = 3.0;
  Serial.println(pumpStatus);
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}

void loop(){
  currentTime = millis();
  systemCheck();
  if ((currentTime - lastSwitch >= pumpOnTime) && (pumpStatus == 1) && systemStatus == 0){ 
    Serial.println(systemCheck());
    Serial.println(pumpStatus);
    digitalWrite(pumpPin, LOW);
    pumpStatus = 0;
    lastSwitch = currentTime;
    Serial.println("pump off");
    delay(3000);
  }
  if ((currentTime - lastSwitch >= pumpOffTime) && (pumpStatus == 0) && systemStatus == 0){ 
    Serial.println(systemCheck());
    Serial.println(pumpStatus);
    digitalWrite(pumpPin, HIGH);
    pumpStatus = 1;
    lastSwitch = currentTime;
    Serial.println("pump on");
    servoMove();
    delay(1000);
  }
  
  if(systemStatus == 1){
    Serial.println("Something is wrong. Fix me.");
    digitalWrite(pumpPin, LOW);
    pumpStatus = 0;
    Serial.println("pump off");
  }
//  if ((currentTime%pumpOnTime) == 0 && (currentTime%pumpOffTime) != 0) {
//    flowCheckTime = currentTime;
//    if(flowCheck(flowCheckTime) == 0){
//      digitalWrite(pumpPin, HIGH);
//    }
//  }
//  else if ((currentTime%pumpOffTime) == 0){
//    flowCheckTime = currentTime;
//    if(flowCheck(flowCheckTime) == 1){
//      digitalWrite(pumpPin, LOW);
//    }
//  }

  Serial.println();
}

