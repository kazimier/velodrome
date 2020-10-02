
#include <OSCMessage.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <OSCMessage.h>
#include "RunningAverage.h"

// running average over 200 samples
RunningAverage myRA(200);

EthernetUDP Udp;

IPAddress ip(192, 168, 1, 100);
IPAddress outIp(192, 168, 1, 64);
const unsigned int outPort = 9999;

byte mac [] = {
  0xA8, 0x61, 0x0A, 0xAE, 0x5E, 0xED };

//Hardware constants
const int PASPin = 2;    // input from PAS


//Software constants
const unsigned long activityTimeoutMS = 10000; // Allowed PAS signal inactivity time before turning off
const int startPulses = 2; // Number of PAS pulses needed before turning on
unsigned long timer = 0; // time between pulses to calculate velocity
int vel = 0; // velocity
int av_vel = 0; // moving average velocity

// Variables
volatile int inputEdges = 0; // counter for the number of pulses since last reset
volatile unsigned long lastEdgeTime = 0; // timestamp of last PAS pulse
bool state=false; // variable holding information about the state of the output

boolean running = false;

void setup() {
  Serial.begin(115200);
  Ethernet.begin(mac,ip);
  Udp.begin(8888);
  pinMode(PASPin, INPUT); // initialize the PAS pin as a input
  attachInterrupt(digitalPinToInterrupt(PASPin), pulse, RISING); //Each rising edge on PAS pin causes an interrupt
    // initialize serial communication at 9600 bits per second:
  myRA.clear(); // explicitly start clean

}


void loop() {
  //If PAS signal is inactive for too long, turn off everything
  unsigned long curTime=millis();
  if ((curTime>lastEdgeTime)&&((curTime-lastEdgeTime)>activityTimeoutMS)) {
    turnOff();
    sendOSC("/Bike1/OFF", 1);

  }

  //If system is off, check if the impulses are active
  if ((!state)&&((millis()-lastEdgeTime)<activityTimeoutMS)) {
    //if impulses are active, check if there were enough pulses to turn on
    if (inputEdges>startPulses) {
      turnOn();
      sendOSC("/Bike1/ON", 1);

    }
  }


  // send pulses as OSC
  sendOSC("/Bike1/Rotations/", inputEdges);


  if (running == true && inputEdges >= 10000) {
    sendOSC("/Bike1/Winner", 1);
    running = false;                // toggle running variable
    delay(5);
    sendOSC("/Bike1/Winner", 0);

  }

  // calculate velocity
  timer = millis()-lastEdgeTime;
  vel = (166/(timer)) * 2.2;  // convert time between pulses to mph
  myRA.addValue(vel);
  av_vel = myRA.getAverage();   // moving average velocity

  // send average velocity as OSC
  sendOSC("/Bike1/Speed/", av_vel);

}


//Turn off output, reset pulse counter and set state variable to false
void turnOff() {
  noInterrupts();
  inputEdges=0;
  state=false;
  interrupts();
  running = false;
}

//Turn on output and set state variable to true
void turnOn() {
  state=true;
  running  = true;
}



//Interrupt subroutine, refresh last impulse timestamp and increment pulse counter (until 10000 is reached)
void pulse() {
  lastEdgeTime=millis();
  if (inputEdges<10000) {
    inputEdges++;

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
