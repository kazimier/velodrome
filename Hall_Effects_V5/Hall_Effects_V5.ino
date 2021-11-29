
#include <OSCMessage.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <OSCMessage.h>
#include "RunningAverage.h"


// running average over 200 samples
RunningAverage myRA_1(200);   // bike one
RunningAverage myRA_2(200);  // bike two

EthernetUDP Udp;

IPAddress ip(192, 168, 0, 100);
IPAddress outIp(192, 168, 0, 1);
const unsigned int outPort = 9999;

byte mac [] = {
  0xA8, 0x61, 0x0A, 0xAE, 0x5E, 0xED };

//Hardware constants
const int PASPin_1 = 2;    // input from PAS 1
const int PASPin_2 = 3;    // input from PAS 2


const unsigned long activityTimeoutMS= 1000; // Allowed PAS signal inactivity time before turning off
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
volatile int inputEdges_1 = 0; // counter for the number of pulses since last reset
volatile unsigned long lastEdgeTime_1 = 0; // timestamp of last PAS pulse

// Variables bike 2
volatile int inputEdges_2 = 0; // counter for the number of pulses since last reset
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
    // initialize serial communication at 9600 bits per second:
  myRA_1.clear(); // explicitly start clean
  myRA_2.clear(); // explicitly start clean
}


void loop() {
  //If PAS signal is inactive for too long, turn off everything
  unsigned long curTime=millis();
  if ((curTime>lastEdgeTime_1)&&((curTime-lastEdgeTime_1)>activityTimeoutMS) || (curTime>lastEdgeTime_2)&&((curTime-lastEdgeTime_2)>activityTimeoutMS)) {
    turnOff();
    sendOSC("/Bike1/OFF", 1);
    sendOSC("/Bike2/OFF", 1);   
  }

  //If system is off, check if the impulses are active
  if ((!state)&&((millis()-lastEdgeTime_1)<activityTimeoutMS)&&((millis()-lastEdgeTime_2)<activityTimeoutMS)) {
    //if impulses are active, check if there were enough pulses from both bikes to turn on
    if ((inputEdges_1>startPulses_1)&&(inputEdges_2>startPulses_2)) {
      turnOn();
      sendOSC("/Bike1/ON", 1);
      sendOSC("/Bike2/ON", 1);      
    }
  }


  // send pulses as OSC
  sendOSC("/Bike1/Rotations/", inputEdges_1);
  sendOSC("/Bike2/Rotations/", inputEdges_2);

  // work out if anyone has won
  if (running_1 == true && inputEdges_1 >= 500000) {
    sendOSC("/Bike1/Winner", 1);
    running_1 = false;                // toggle running variable
    delay(5);
    sendOSC("/Bike1/Winner", 0);
  }
  if (running_2 == true && inputEdges_2 >= 500000) {
    sendOSC("/Bike2/Winner", 1);
    running_2 = false;                // toggle running variable
    delay(5);
    sendOSC("/Bike2/Winner", 0);
  }  

  // calculate velocities
  timer_1 = millis()-lastEdgeTime_1;
  vel_1 = (166/(timer_1)) * 2.2;  // convert time between pulses to mph
  myRA_1.addValue(vel_1);
  av_vel_1 = myRA_1.getAverage();   // moving average velocity

  timer_2 = millis()-lastEdgeTime_2;
  vel_2 = (166/(timer_2)) * 2.2;  // convert time between pulses to mph
  myRA_2.addValue(vel_2);
  av_vel_2 = myRA_2.getAverage();   // moving average velocity

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
  lastEdgeTime_1=millis();
  if (inputEdges_1<500000) {
    inputEdges_1++;

  }
}

void pulse_2() {
  lastEdgeTime_2=millis();
  if (inputEdges_2<500000) {
    inputEdges_2++;

  }
}

void sendOSC(String msg, unsigned int data) {

  OSCMessage msgOUT(msg.c_str());
  msgOUT.add(data);
  Udp.beginPacket(outIp, outPort);
  msgOUT.send(Udp);
  Udp.endPacket();
  msgOUT.empty();

}
