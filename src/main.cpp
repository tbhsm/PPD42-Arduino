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

Particle Size   0.5     0.8     1.0       1.5     2.4     2.5     2.6       10.0
P1 (PM1.0) (c10 reading)        |------------------------------------------------------->
P2 (PM2.5) (c25 reading)                                  |----------------------------->
AQI (PM2.5)      <----------------------------------------|          
AQI (PM10)                                                                   |---------->

So we estimate:
- AQI (PM2.5) = P1-P2
- AQI (PM10)  = P2 

Humidity correction
Humidity [%] 	Dry Correction 	Rain Correction
0-19 	        10.1 	          6.4
20-24 	      8.75 	          6.4
25-29 	      8 	            6.4
30-34 	      8 	            6.4
35-39 	      8 	            6.4
40-44 	      7 	            6.3
45-49 	      6 	            6.3
50-54 	      5.75 	          5.7
55-59 	      5.5 	          5.5
60-64 	      5.5 	          4.2
65-69 	      3.5 	          4.1
70-74 	      3.5 	          3.2
75-79 	      3.75 	          3.2
80-84 	      2.25 	          2.1
85-89 	      1.5 	          2.1
90-94 	      0.825 	        0.8
95-100 	      0.525 	        0.5

Not implemented (will be calculated on Home Assistant):
 - Humidity correction
 - AQI calculation

 */

#include <Arduino.h>
#include "PinChangeInterrupt.h"
#include <ArduinoJson.h>

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
// Assume the radius of a particle in the PM2.5 channel is .44 µm
// We're assuming same for PM1.0 (no data available) so result is probably overestimated (smaller particles)
const float rpm = 0.44 * pow(10, -6);
// Volume of a sphere = 4/3 * pi * radius^3
const float volpm = (4.0/3.0) * M_PI * pow(rpm,3);
// mass = density * volume
const float masspm = densitypm * volpm; //0.000000589

float ratio2conc(float ratio) {
      if (ratio){
        return 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
      } else return 0; // Round to 0 for aesthetic reasons
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
    // Process measurements
    concentration_s10 = ratio2conc(lowpulseoccupancy_s10/1000.0/sampletime_ms*100.0);  // convert to ms, then percentage 0-100
    concentration_s25 = ratio2conc(lowpulseoccupancy_s25/1000.0/sampletime_ms*100.0);  // convert to ms, then percentage 0-100
    ugm3_s10 = concentration_s10 * 3531.5 * masspm;
    ugm3_s25 = concentration_s25 * 3531.5 * masspm;
    
    // Send data
    // Serial.print(ugm3_s10);
    // Serial.print(",");
    // Serial.print(ugm3_s25);
    // Serial.print("\n");
    StaticJsonDocument<100> jsonBuffer;
    jsonBuffer["PM1.0_conc"] = concentration_s10;
    jsonBuffer["PM2.5_conc"] = concentration_s25;    
    jsonBuffer["PM1.0_ugm3"] = ugm3_s10;
    jsonBuffer["PM1.0-2.5_ugm3"] = ugm3_s10 - ugm3_s25;
    jsonBuffer["PM2.5_ugm3"] = ugm3_s25;
    serializeJson(jsonBuffer, Serial);
    Serial.println();

    // Reset
    lowpulseoccupancy_s10 = 0;
    lowpulseoccupancy_s25 = 0;
    starttime = millis();
  }
}