////////////////////////////////////////////////////
// Example code for sending data to a google form,
// via the pushingbox api, and accedpting commands 
// with the nearbus api.
//
// written by Duncan Hall
// for POE team Aeroplants
// Nov 30, 2016
////////////////////////////////////////////////////

////////////////////////////////////////////////////
//  Connectivity Configuration
////////////////////////////////////////////////////

// to POST with nearbus:
// http://nearbus.net/v1/api_vmcu_jsb/NB102938?user=duncanhall&pass=vmC@s?hLh9tKq8G3&channel=0&service=MY_NBIOS_0&value=1&method=POST&reqid=0
char nb_deviceID [] = "NB102938";
char nb_secret [] = "11223344";

char pb_deviceID [] = "v773DD30782DE970";

byte mac [6] = { 0xDE, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

////////////////////////////////////////////////////
//  Includes
////////////////////////////////////////////////////
#include <Ethernet.h>
#include <NearbusEther_v16.h>
#include <SimpleTimer.h>


////////////////////////////////////////////////////
//  Global Variables
////////////////////////////////////////////////////

// nearbus stuff:
Nearbus NB_Agent(0);

ULONG A_register[8];        // Define the Tx Buffer (Reg_A)
ULONG B_register[8];        // Define the Rx Buffer (Reg_B) 

void AuxPortServices(void) {
    NB_Agent.PortServices(); 
}

// pushingbox stuff:
char pb_server[] = "api.pushingbox.com";
EthernetClient pb_client;

// our stuff:
bool WATER_IS_LOW = false;
int EC_THRESHOLD; // TODO: give this a value
volatile byte HALL_PULSE_COUNT; // used along with interrupt to track
                                // flow over the course of a second.
const byte HALL_THRESHOLD_PUMP;
const byte HALL_THRESHOLD_LEAK;
const long HALL_DELAY = 800;
bool PUMP_IS_RUNNING = false;
bool LIGHT_STATUS = 1; // true daylight, false night light
byte PLANT_COUNTER = 0;

byte ERROR_CODE = 0;

// Timing (in ms):
long MIST_DURATION = 1200;
const long TURN_DURATION = 3000;
//long WATER_INTERVAL = 1800000; // 1800000 is 30 mins
long WATER_INTERVAL = 40000; // 1800000 is 30 mins
//const long DATA_INTERVAL = 120000; // every two minutes
const long DATA_INTERVAL = 2000; // every two seconds
float DAYLIGHT_PROPORTION = 0.5;
//const long HOUR24_LENGTH = 86400000; // ms per 24 hours
const long HOUR24_LENGTH = 8640;

SimpleTimer TIMER; // this is the timer which handles millis() stuff for us

// pins:
// 0 TX
// 1 TX
const byte HALL_PIN = 2;
const byte PUMP_PIN = 3; // ~
const byte SD_PIN = 4; //   (SD card, set to high to deselect)
// 5 ~ (ethernet?)
// 6 ~ (ethernet?)
// 7
// 8
// 9 ~
// 10 ~ (ethernet)
// 11 ~ (ethernet)
// 12   (ethernet)
// 13   (ethernet)
// 
const byte EC_PIN = A0;
// A1
// A2
// A3
// A4
// A5
 

// This is the function called when the above api call
// is passed to the Arduino. `value` in the api call
// corresponds to `setValue` here, and pRetValue can
// be used to return numbers to the caller.
void Nearbus::MyNbios_0
(byte portId, ULONG setValue, ULONG* pRetValue, byte vmcuMethod, PRT_CNTRL_STRCT* pPortControlStruct ) {
  // read more about this in the Hello_World_Ether example or
  // at http://www.nearbus.net/hello_world.html
  if( pPortControlStruct->portMode != MYNBIOS_MODE ) {
    PortModeConfig( portId, MYNBIOS_MODE );        
  }
  analogReference( DEFAULT );

  int mult = 100;

  byte code = setValue % 100;
  int val = (setValue - code) / 100;

  switch (code) {
    case 1: MIST_DURATION = val * 1000; // seconds to ms
    case 2: WATER_INTERVAL = val * 60 * 1000; // minutes to ms
    case 3: EC_THRESHOLD = val; // TODO: convert units
    case 4: DAYLIGHT_PROPORTION = val / 100.0;
  }

  *pRetValue = (ULONG) setValue;
}

void PingNB () {
  Serial.println("NB");
  int ret;
  NB_Agent.NearChannel( A_register, B_register, &ret );
//  if ( ret >= 50 ) {
//      Serial.println( "RxErr" ); 
//      // [50]  Frame Autentication Mismatch
//      // [51]  Frame Out of sequence
//      // [52]  Remote ACK Error
//      // [53]  Unsupported Command    
//  }
}

////////////////////////////////////////////////////
//  Data Logging
////////////////////////////////////////////////////

void SendData () {
  Serial.println("send_data");
  int ec_reading = UpdateEC; // TODO: change back
  CheckHall();

  PostPB(ec_reading, PUMP_IS_RUNNING, LIGHT_STATUS, ERROR_CODE);
}

void PostPB (int v0, int v1, int v2, int v3) {
  if (pb_client.connect(pb_server, 80)) {
    Serial.println("connected");

    pb_client.print("GET /pushingbox?devid=");
    pb_client.print(pb_deviceID);
    pb_client.print("&pin0=");
    pb_client.print(v0);
    pb_client.print("&pin1=");
    pb_client.print(v1);
    pb_client.print("&pin2=");
    pb_client.print(v2);
    pb_client.print("&pin3=");
    pb_client.print(v3);

// api.pushingbox.com/pushingbox?devid=v773DD30782DE970&pin0=9&pin1=9&pin2=9&pin3=9

    pb_client.println(" HTTP/1.1");
    pb_client.println("Host: api.pushingbox.com");
    pb_client.println("User-Agent: Arduino");
    pb_client.println("Connection: close");
    pb_client.println();

    Serial.println("postsent.");
 
     delay(1000);
     pb_client.stop();
  } else {
    Serial.print("failedconnect ");
    Serial.println(pb_server);
    pb_client.stop();
  }
}

int UpdateEC () {
//  Serial.println("ec");
  int conductivity = analogRead(EC_PIN);
  if (conductivity == 0) {
    WATER_IS_LOW = true;
    return (conductivity);
  } else if (conductivity < EC_THRESHOLD) {
    // TODO: run nutrient dump code if timer.
  } else {WATER_IS_LOW = false;}
  return (conductivity);
}

////////////////////////////////////////////////////
//  Lights
////////////////////////////////////////////////////

void SetDayLED () {
  Serial.println("LED");
  LIGHT_STATUS = 1;
  // set new value to LEDs
  int t = (int) HOUR24_LENGTH * DAYLIGHT_PROPORTION;
  TIMER.setTimeout(t, SetNightLED);
}

void SetNightLED () {
  Serial.println("LED n");
  LIGHT_STATUS = 0;
  // set new value to LEDs
  int t = (int) HOUR24_LENGTH * (1.0 - DAYLIGHT_PROPORTION);
  TIMER.setTimeout(t, SetDayLED);
}


////////////////////////////////////////////////////
//  Watering Sequence
////////////////////////////////////////////////////
// this set of functions loop three times (three plants)
// also included in the loop are the Hall sensor functions above

void StartTable () {
  Serial.println("Table");
  TIMER.setTimeout(WATER_INTERVAL, StartTable);
  PLANT_COUNTER++;
  if (PLANT_COUNTER <= 3) {
    Serial.print("watering plant ");
    Serial.println(PLANT_COUNTER);
    PumpOn();
  } else {
    PLANT_COUNTER = 0;
  }
}

void PumpOn () {
  Serial.println("pump_on");
  PUMP_IS_RUNNING = true;
  digitalWrite (PUMP_PIN, HIGH);

//  TIMER.setTimeout(HALL_DELAY, CheckHall);
}

void CheckHall () {
  Serial.println("Hall");
  HALL_PULSE_COUNT = 0;
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), PulseCounter, FALLING);
  long start_t = millis();
  int i = 0;
  while (millis() - start_t < 1000) {
    ; // it's unavoidable to not pause everything here...
  }
  EndHall();
//  TIMER.setTimeout(1000, EndHall); // call EndHall when TIMER.run() is called
                                   // and it's been at least a second.
}

void EndHall () {
  Serial.println("Hall off");
  detachInterrupt(digitalPinToInterrupt(HALL_PIN));
  if (PUMP_IS_RUNNING) {
    if (HALL_PULSE_COUNT < HALL_THRESHOLD_PUMP) {
      // TODO: send pump is clogged message to web
      Serial.println("FatalError:clogged");
      digitalWrite (PUMP_PIN, LOW);
      PUMP_IS_RUNNING = false;
      while (true) {
        ;
      }
    }
    PumpOff();
  } else {
    if (HALL_PULSE_COUNT > HALL_THRESHOLD_LEAK) {
      // TODO: unexpected water flow message
    }
  }
}

void PulseCounter () {
  // Increment the pulse counter
  HALL_PULSE_COUNT++;
}

void PumpOff () {
//  Serial.println("pump off");
  digitalWrite (PUMP_PIN, LOW);
  PUMP_IS_RUNNING = false;

  RotateTop();
}

void RotateTop () {
//  Serial.println("rotate");
  // However the servo needs to be written to???

  // on completion:
  StartTable();
}


////////////////////////////////////////////////////
//  Main
////////////////////////////////////////////////////

void setup () {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait until successful serial initialization
  }
  
  // NEARBUS INITIALIZATION //
//  Serial.println("initializingNB");
  NB_Agent.NearInit( nb_deviceID, nb_secret );
//  Serial.println("NBinitialized.");

  // ETHERNET INITIALIZATION //
  Serial.println("DHCP requesting");
  if (Ethernet.begin( mac ) == 0) {
    Serial.println("DHCP unanswered");
    while (true) {
      ;
    }
  } else {
    Serial.print("My IP is ");
    Serial.println(Ethernet.localIP());
  
    // TODO: is this necessary?
    delay(1000); // Give the Ethernet shield a second to initialize
  
    // PIN INITIALIZATION //
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(SD_PIN, OUTPUT);
    digitalWrite(SD_PIN, HIGH); // deselect SD card
  
    pinMode(EC_PIN, INPUT);
  
    // start timers:
    SetDayLED();
    TIMER.setTimeout(WATER_INTERVAL, StartTable); // timing is variable
    TIMER.setInterval(DATA_INTERVAL, SendData); // timing is static
  }
}


void loop(){
  PingNB();
  TIMER.run();
}

