/*
 Asynchronous readout interface to Shinyei Model PPD42NS Particle Sensor
 Designed for Arduino Nano (tested on Geekcreit Nano V3)
 Program by Tim Hosman
 Written December 2020
 
 Some sources
 http://www.seeedstudio.com/depot/grove-dust-sensor-p-1050.html
 http://www.sca-shinyei.com/pdf/PPD42NS.pdf
 https://quadmeup.com/read-rc-pwm-signal-with-arduino/
 https://github.com/NicoHood/PinChangeInterrupt/tree/master/src
 https://www.researchgate.net/publication/324171671_Understanding_the_Shinyei_PPD42NS_low-cost_dust_sensor
 http://www.howmuchsnow.com/arduino/airquality/grovedust/
 https://en.wikipedia.org/wiki/Air_quality_index
 
 JST Pin 1 (Green wire)  - GND   => Arduino GND
 JST Pin 2 (White wire)  - P2.5  => Arduino Digital Pin 9
 JST Pin 3 (Yellow wire) - 5V    => Arduino 5VDC
 JST Pin 4 (Black wire)  - P1.0  => Arduino Digital Pin 8
 JST Pin 5 (Red wire)    - TH2.5 => N/C

Particle Size   0.5     0.8     1.0       1.5     2.4     2.5     2.6     3.0     3.5
P1 (PM1.0) (c10 reading)        |--------------------------------------------------->
P2 (PM2.5) (c25 reading)                                  |------------------------->
AQI (PM2.5)      <----------------------------------------|

=> So we estimate AQI by taking PM1.0 - PM2.5

 */

#include <Arduino.h>
#include "PinChangeInterrupt.h"

#define P10_IN 8
#define P25_IN 9

unsigned long duration_s10, duration_s25;
unsigned long starttime;
volatile unsigned long starttime_s10, starttime_s25;
const unsigned long sampletime_ms = 30000;
volatile unsigned long lowpulseoccupancy_s10 = 0, lowpulseoccupancy_s25 = 0;
float ratio = 0;
float concentration_s10 = 0, concentration_s25 = 0, ugm3_s10 = 0, ugm3_s25 = 0;

// Assume all particles are spherical, with a density of 1.65E12 µg/m3
const float densitypm = 1.65 * pow(10, 12);
// Assume the radius of a particle in the PM2.5 channel is .44 µm, we're assuming same for PM1.0
const float rpm = 0.44 * pow(10, -6);
// Volume of a sphere = 4/3 * pi * radius^3
const float volpm = (4.0/3.0) * M_PI * pow(rpm,3);
// mass = density * volume
const float masspm = densitypm * volpm; //0.000000589

float ratio2conc(float ratio) {
      return 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
}

void s10_add(){
    // start of particle registration
     uint8_t trigger = getPinChangeInterruptTrigger(digitalPinToPCINT(P10_IN));
     if (trigger == FALLING){
      starttime_s10 = micros();
     }
     else if (trigger == RISING) { // end of particle
      lowpulseoccupancy_s10 += micros()-starttime_s10;
      starttime_s10 = 0;
     }
}

void s25_add(){
    // start of particle registration
     uint8_t trigger = getPinChangeInterruptTrigger(digitalPinToPCINT(P25_IN));
     if (trigger == FALLING){
       starttime_s25 = micros();
     }
     else if (trigger == RISING) { // end of particle
      lowpulseoccupancy_s25 += micros()-starttime_s25;
      starttime_s25 = 0;
     }
}

void setup() {
  Serial.begin(9600);
  pinMode(P25_IN,INPUT);
  pinMode(P25_IN,INPUT);
  starttime = millis();

  // Asynchronous counting of low pulses
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(P10_IN), s10_add, CHANGE);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(P25_IN), s25_add, CHANGE);
}

void loop() {

  if ((millis()-starttime) > sampletime_ms) // sampletime passed
  {
    concentration_s10 = ratio2conc(lowpulseoccupancy_s10/1000.0/sampletime_ms*100.0);  // Integer percentage 0=>100
    concentration_s25 = ratio2conc(lowpulseoccupancy_s25/1000.0/sampletime_ms*100.0);  // Integer percentage 0=>100
    ugm3_s10 = concentration_s10 * 3531.5 * masspm;
    ugm3_s25 = concentration_s25 * 3531.5 * masspm;
    Serial.print(ugm3_s10);
    Serial.print(",");
    Serial.print(ugm3_s25);
    Serial.print("\n");
    lowpulseoccupancy_s10 = 0;
    lowpulseoccupancy_s25 = 0;
    starttime = millis();
  }
}