
#include <OSCMessage.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>    
#include <OSCMessage.h>
#include "RunningAverage.h"

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
unsigned long timer = 0;
int vel = 0;

// Variables
volatile int inputEdges = 0; // counter for the number of pulses since last reset
volatile unsigned long lastEdgeTime = 0; // timestamp of last PAS pulse
bool state=false; // variable holding information about the state of the output

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


  }
  
  //If system is off, check if the impulses are active
  if ((!state)&&((millis()-lastEdgeTime)<activityTimeoutMS)) {
    //if impulses are active, check if there were enough pulses to turn on
    if (inputEdges>startPulses) {
      turnOn();

    }
  }
  

    //Serial.print("Distance: ");
   // Serial.println(inputEdges*0.1667);
     OSCMessage msg("/Bike1/Rotations/");
     msg.add((float)inputEdges*0.0001);
  
//  msg.add((int32_t)inputEdges);
  
  Udp.beginPacket(outIp, outPort);
    msg.send(Udp); // send the bytes to the SLIP stream
  Udp.endPacket(); // mark the end of the OSC Packet
  msg.empty(); // free space occupied by message

  delay(5);

  timer = millis()-lastEdgeTime;
 
  vel = (166/(timer)) * 2.2;  // convert time between pulses to mph
  myRA.addValue(vel);
   Serial.println(myRA.getAverage(), 3);
   //Serial.println(vel);
}


//Turn off output, reset pulse counter and set state variable to false
void turnOff() {
  noInterrupts();
  inputEdges=0;
  state=false;
  interrupts();
}

//Turn on output and set state variable to true
void turnOn() {
  state=true;
}



//Interrupt subroutine, refresh last impulse timestamp and increment pulse counter (until 10000 is reached)
void pulse() {
  lastEdgeTime=millis();
  if (inputEdges<10000) {
    inputEdges++;

  }  
}
