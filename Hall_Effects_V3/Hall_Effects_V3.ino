
#include <OSCMessage.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <OSCMessage.h>
#include <Smoothed.h>
#include <ezButton.h>
 
// 28.5mm diameter axle on stand
// diameter = 0.0895m
// 12 magnet sensor means distance covered per trigger = 0.0075m

// delay (milliseconds) after winner until game restarts 
const int winner_delay = 20000;
// approximate distance = 750m
const unsigned long runLength = 50000;
// Allowed PAS signal inactivity time before turning off (microseconds)
const unsigned long activityTimeoutMS= 10  * 1000000;

// running average over 200 samples
Smoothed <float> myRA_1;
Smoothed <float> myRA_2;

EthernetUDP Udp;

IPAddress ip(192, 168, 0, 100);
IPAddress outIp(192, 168, 0, 64);
const unsigned int outPort = 9999;

byte mac [] = {
  0xA8, 0x61, 0x0A, 0xAE, 0x5E, 0xED };

//Hardware constants
const int PASPin_1 = 2;    // input from PAS 1
const int PASPin_2 = 3;    // input from PAS 2

// setup buttons on pins:
ezButton button1(5);
ezButton button2(6);

bool state=false; // variable holding information about the state of the output

//Software constants bike 1
const int startPulses_1 = 2; // Number of PAS pulses needed before turning on
unsigned long timer_1 = 0; // time between pulses to calculate velocity
int vel_1 = 0; // velocity
int av_vel_1 = 0; // moving average velocity

//Software constants bike 2
const int startPulses_2 = 2; // Number of PAS pulses needed before turning on
unsigned long timer_2 = 0; // time between pulses to calculate velocity
int vel_2 = 0; // velocity
int av_vel_2 = 0; // moving average velocity

// Variables bike 1
volatile unsigned long inputEdges_1 = 0; // counter for the number of pulses since last reset
volatile unsigned long lastEdgeTime_1 = 0; // timestamp of last PAS pulse

// Variables bike 2
volatile unsigned long inputEdges_2 = 0; // counter for the number of pulses since last reset
volatile unsigned long lastEdgeTime_2 = 0; // timestamp of last PAS pulse

boolean running_1 = false;
boolean running_2 = false;

void setup() {
  Serial.begin(115200);
  Ethernet.begin(mac,ip);
  Udp.begin(8888);
  pinMode(PASPin_1, INPUT); // initialize the PAS pin as a input
  pinMode(PASPin_2, INPUT); // initialize the PAS pin as a input
  attachInterrupt(digitalPinToInterrupt(PASPin_1), pulse_1, RISING); //Each rising edge on PAS pin causes an interrupt
  attachInterrupt(digitalPinToInterrupt(PASPin_2), pulse_2, RISING); //Each rising edge on PAS pin causes an interrupt
  myRA_1.begin(SMOOTHED_AVERAGE, 30);
  myRA_2.begin(SMOOTHED_AVERAGE, 30);
  // set button debounce times
  button1.setDebounceTime(50); // set debounce time to 50 milliseconds
  button2.setDebounceTime(50); // set debounce time to 50 milliseconds
}

void loop() {

  button1.loop(); // MUST call the loop() function first
  button2.loop(); // MUST call the loop() function first
  
// add reset button to restart game
// add ambient button to enter ambient mode
  int btn1State = button1.getState();
  int btn2State = button2.getState();
  if(button1.isPressed()) {
    turnOff();      // reset all counters
    turnOn();       // start game
    sendOSC("/Bike1/ON", 1);
    sendOSC("/Bike2/ON", 1);
  }    
  if(button2.isPressed()) {
    turnOff();
    sendOSC("/Ambient/", 1);
  }

  // send pulses as OSC
  sendOSC("/Bike1/Rotations/", inputEdges_1/((float) runLength));
  sendOSC("/Bike2/Rotations/", inputEdges_2/((float) runLength));
   
  Serial.println(inputEdges_1);
  // work out if anyone has won
  if (running_1 == true && inputEdges_1 >= runLength) {
    Serial.println("winner");
    sendOSC("/Bike1/Winner", 1);
    running_1 = false;                // toggle running variable
    delay(winner_delay);
    turnOff();      // reset all counters
    turnOn();       // start game
    sendOSC("/Bike1/ON", 1);
  }
  if (running_2 == true && inputEdges_2 >= runLength) {
    sendOSC("/Bike2/Winner", 1);
    running_2 = false;                // toggle running variable
    delay(winner_delay);
    turnOff();      // reset all counters
    turnOn();       // start game    
    sendOSC("/Bike1/ON", 1);
  }  

  // calculate velocities (averaging library works on integer so may need a conversion factor...)
  timer_1 = micros()-lastEdgeTime_1;
  vel_1 = (0.0075*1000000/(timer_1));  // convert time between pulses to mph
  vel_1 = (int) vel_1;
  myRA_1.add(vel_1);  // add value and return moving average velocity
  av_vel_1 = myRA_1.get();
  
  timer_2 = micros()-lastEdgeTime_2;
  vel_2 = (0.0075*1000000/timer_2);  // convert time between pulses to mph (multiplied by 10)
  myRA_2.add(vel_2);  // add value and return moving average velocity
  av_vel_2 = myRA_2.get();
  
  // send average velocity as OSC
  sendOSC("/Bike1/Speed/", av_vel_1);
  sendOSC("/Bike2/Speed/", av_vel_2);
}

//Turn off output, reset pulse counter and set state variable to false
void turnOff() {
  
  noInterrupts();
  inputEdges_1=0;
  inputEdges_2=0;  
  state=false;
  interrupts();
  running_1 = false;
  running_2 = false;
  
}

//Turn on output and set state variable to true
void turnOn() {
  state=true;
  running_1  = true;
  running_2  = true; 
}


//Interrupt subroutine, refresh last impulse timestamp and increment pulse counter (until 10000 is reached)
void pulse_1() {
  lastEdgeTime_1=micros();
  if (inputEdges_1<runLength) {
    inputEdges_1++;
  }
}

void pulse_2() {
  lastEdgeTime_2=micros();
  if (inputEdges_2<runLength) {
    inputEdges_2++;
  }
}

void receiveOSC(){
  
}

void sendOSC(String msg, float data) {

  OSCMessage msgOUT(msg.c_str());
  msgOUT.add(data);
  Udp.beginPacket(outIp, outPort);
  msgOUT.send(Udp);
  Udp.endPacket();
  msgOUT.empty();
}
