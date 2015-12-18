// This #include statement was automatically added by the Particle IDE.
#include "ThingSpeak/ThingSpeak.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkFun_Photon_Weather_Shield_Library/SparkFun_Photon_Weather_Shield_Library.h"

// Add math to get sine and cosine for wind vane
#include <math.h>

/*
  *****************************************************************************************
  **** Visit https://www.thingspeak.com to sign up for a free account and create
  **** a channel.  The video tutorial http://community.thingspeak.com/tutorials/thingspeak-channels/ 
  **** has more information. You need to change this to your channel, and your write API key
  **** IF YOU SHARE YOUR CODE WITH OTHERS, MAKE SURE YOU REMOVE YOUR WRITE API KEY!!
  **** To learn more about ThingSpeak, see the introductory video: http://www.mathworks.com/videos/introduction-to-thingspeak-107749.html
  *****************************************************************************************/
unsigned long thingspeakChannelNumber = <enter your channel number>;
char thingSpeakWriteAPIKey[] = "<enter your write key";

// Each time we loop through the main loop, we check to see if it's time to capture the sensor readings
unsigned int sensorCapturePeriod = 100;
unsigned int timeNextSensorReading;

// Each time we loop through the main loop, we check to see if it's time to publish the data we've collected
unsigned int publishPeriod = 60000;
unsigned int timeNextPublish; 

void setup() {
    initializeThingSpeak();
    initializeTempHumidityAndPressure();
    initializeRainGauge();
    initializeAnemometer();
    initializeWindVane();
    
    // Schedule the next sensor reading and publish events
    timeNextSensorReading = millis() + sensorCapturePeriod;
    timeNextPublish = millis() + publishPeriod; 
}

void loop() {

    // Capture any sensors that need to be polled (temp, humidity, pressure, wind vane)
    // The rain and wind speed sensors use interrupts, and so data is collected "in the background"
    if(timeNextSensorReading <= millis()) {
        captureTempHumidityPressure();
        captureWindVane();

        // Schedule the next sensor reading
        timeNextSensorReading = millis() + sensorCapturePeriod;
    }
    
    // Publish the data collected to Particle and to ThingSpeak
    if(timeNextPublish <= millis()) {
        
        // Get the data to be published
        float tempF = getAndResetTempF();
        float humidityRH = getAndResetHumidityRH();
        float pressureKPa = getAndResetPressurePascals() / 1000.0;
        float rainInches = getAndResetRainInches();
        float gustMPH;
        float windMPH = getAndResetAnemometerMPH(&gustMPH);
        float windDegrees = getAndResetWindVaneDegrees();
                          
        // Publish the data                    
        publishToParticle(tempF,humidityRH,pressureKPa,rainInches,windMPH,gustMPH,windDegrees);
        publishToThingSpeak(tempF,humidityRH,pressureKPa,rainInches,windMPH,gustMPH,windDegrees);

        // Schedule the next publish event
        timeNextPublish = millis() + publishPeriod;
    }
    
    delay(10);
}

void publishToParticle(float tempF,float humidityRH,float pressureKPa,float rainInches,float windMPH,float gustMPH,float windDegrees) {
    Particle.publish("weather", 
                        String::format("%0.1f°F, %0.0f%%, %0.2f kPa, %0.2f in, Avg:%0.0fmph, Gust:%0.0fmph, Dir:%0.0f°.",
                            tempF,humidityRH,pressureKPa,rainInches,windMPH,gustMPH,windDegrees),
                        60 , PRIVATE);    
}

//===========================================================
// ThingSpeak
//===========================================================
TCPClient client;

void initializeThingSpeak() {
    ThingSpeak.begin(client);
}

void publishToThingSpeak(float tempF,float humidityRH,float pressureKPa,float rainInches,float windMPH,float gustMPH,float windDegrees) {
    // To write multiple fields, you set the various fields you want to send
    ThingSpeak.setField(1,tempF);
    ThingSpeak.setField(2,humidityRH);
    ThingSpeak.setField(3,pressureKPa);
    ThingSpeak.setField(4,rainInches);
    ThingSpeak.setField(5,windMPH);
    ThingSpeak.setField(6,gustMPH);
    ThingSpeak.setField(7,windDegrees);
    
    // Then you write the fields that you've set all at once.
    ThingSpeak.writeFields(thingspeakChannelNumber, thingSpeakWriteAPIKey);
}

//===========================================================
// Temp, Humidity and Pressure
//===========================================================
// The temperature, humidity, and pressure sensors are on board
// the weather station board, and use I2C to communicate.  The sensors are read
// frequently by the main loop, and the results are averaged over the publish cycle

//Create Instance of HTU21D or SI7021 temp and humidity sensor and MPL3115A2 barometric sensor
Weather sensor;

void initializeTempHumidityAndPressure() {
    //Initialize the I2C sensors and ping them
    sensor.begin();
    //Set to Barometer Mode
    sensor.setModeBarometer();
    // Set Oversample rate
    sensor.setOversampleRate(7); 
    //Necessary register calls to enble temp, baro and alt
    sensor.enableEventFlags(); 
    
    return;
}

float humidityRHTotal = 0.0;
unsigned int humidityRHReadingCount = 0;
float tempFTotal = 0.0;
unsigned int tempFReadingCount = 0;
float pressurePascalsTotal = 0.0;
unsigned int pressurePascalsReadingCount = 0;

void captureTempHumidityPressure() {
  // Read the humidity and pressure sensors, and update the running average
  // The running (mean) average is maintained by keeping a running sum of the observations,
  // and a count of the number of observations
  
  // Measure Relative Humidity from the HTU21D or Si7021
  float humidityRH = sensor.getRH();
  
  //If the result is reasonable, add it to the running mean
  if(humidityRH > 0 && humidityRH < 105) // It's theoretically possible to get supersaturation humidity levels over 100%
  {
      // Add the observation to the running sum, and increment the number of observations
      humidityRHTotal += humidityRH;
      humidityRHReadingCount++;
  }

  // Measure Temperature from the HTU21D or Si7021
  // Temperature is measured every time RH is requested.
  // It is faster, therefore, to read it from previous RH
  // measurement with getTemp() instead with readTemp()
  float tempF = sensor.getTempF();
  
  //If the result is reasonable, add it to the running mean
  if(tempF > -50 && tempF < 150)
  {
      // Add the observation to the running sum, and increment the number of observations
      tempFTotal += tempF;
      tempFReadingCount++;
  }

  //Measure Pressure from the MPL3115A2
  float pressurePascals = sensor.readPressure();
  
  //If the result is reasonable, add it to the running mean
  // What's reasonable? http://findanswers.noaa.gov/noaa.answers/consumer/kbdetail.asp?kbid=544
  if(pressurePascals > 80000 && pressurePascals < 110000)
  {
      // Add the observation to the running sum, and increment the number of observations
      pressurePascalsTotal += pressurePascals;
      pressurePascalsReadingCount++;
  }
  
  return;
}

float getAndResetTempF()
{
    if(tempFReadingCount == 0) {
        return 0;
    }
    float result = tempFTotal/float(tempFReadingCount);
    tempFTotal = 0.0;
    tempFReadingCount = 0;
    return result;
}

float getAndResetHumidityRH()
{
    if(humidityRHReadingCount == 0) {
        return 0;
    }
    float result = humidityRHTotal/float(humidityRHReadingCount);
    humidityRHTotal = 0.0;
    humidityRHReadingCount = 0;
    return result;
}


float getAndResetPressurePascals()
{
    if(pressurePascalsReadingCount == 0) {
        return 0;
    }
    float result = pressurePascalsTotal/float(pressurePascalsReadingCount);
    pressurePascalsTotal = 0.0;
    pressurePascalsReadingCount = 0;
    return result;
}

//===========================================================================
// Rain Guage
//===========================================================================
int RainPin = D2;
volatile unsigned int rainEventCount;
unsigned int lastRainEvent;
float RainScaleInches = 0.011; // Each pulse is .011 inches of rain

void initializeRainGauge() {
  pinMode(RainPin, INPUT_PULLUP);
  rainEventCount = 0;
  lastRainEvent = 0;
  attachInterrupt(RainPin, handleRainEvent, FALLING);
  return;
  }
  
void handleRainEvent() {
    // Count rain gauge bucket tips as they occur
    // Activated by the magnet and reed switch in the rain gauge, attached to input D2
    unsigned int timeRainEvent = millis(); // grab current time
    
    // ignore switch-bounce glitches less than 10mS after initial edge
    if(timeRainEvent - lastRainEvent < 10) {
      return;
    }
    
    rainEventCount++; //Increase this minute's amount of rain
    lastRainEvent = timeRainEvent; // set up for next event
}

float getAndResetRainInches()
{
    float result = RainScaleInches * float(rainEventCount);
    rainEventCount = 0;
    return result;
}

//===========================================================================
// Wind Speed (Anemometer)
//===========================================================================

// The Anemometer generates a frequency relative to the windspeed.  1Hz: 1.492MPH, 2Hz: 2.984MPH, etc.
// We measure the average period (elaspsed time between pulses), and calculate the average windspeed since the last recording.

int AnemometerPin = D3;
float AnemometerScaleMPH = 1.492; // Windspeed if we got a pulse every second (i.e. 1Hz)
volatile unsigned int AnemoneterPeriodTotal = 0;
volatile unsigned int AnemoneterPeriodReadingCount = 0;
volatile unsigned int GustPeriod = UINT_MAX;
unsigned int lastAnemoneterEvent = 0;

void initializeAnemometer() {
  pinMode(AnemometerPin, INPUT_PULLUP);
  AnemoneterPeriodTotal = 0;
  AnemoneterPeriodReadingCount = 0;
  GustPeriod = UINT_MAX;  //  The shortest period (and therefore fastest gust) observed
  lastAnemoneterEvent = 0;
  attachInterrupt(AnemometerPin, handleAnemometerEvent, FALLING);
  return;
  }
  
void handleAnemometerEvent() {
    // Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
     unsigned int timeAnemometerEvent = millis(); // grab current time
     
    //If there's never been an event before (first time through), then just capture it
    if(lastAnemoneterEvent != 0) {
        // Calculate time since last event
        unsigned int period = timeAnemometerEvent - lastAnemoneterEvent;
        // ignore switch-bounce glitches less than 10mS after initial edge (which implies a max windspeed of 149mph)
        if(period < 10) {
          return;
        }
        if(period < GustPeriod) {
            // If the period is the shortest (and therefore fastest windspeed) seen, capture it
            GustPeriod = period;
        }
        AnemoneterPeriodTotal += period;
        AnemoneterPeriodReadingCount++;
    }
    
    lastAnemoneterEvent = timeAnemometerEvent; // set up for next event
}

float getAndResetAnemometerMPH(float * gustMPH)
{
    if(AnemoneterPeriodReadingCount == 0)
    {
        *gustMPH = 0.0;
        return 0;
    }
    // Nonintuitive math:  We've collected the sum of the observed periods between pulses, and the number of observations.
    // Now, we calculate the average period (sum / number of readings), take the inverse and muliple by 1000 to give frequency, and then mulitply by our scale to get MPH.
    // The math below is transformed to maximize accuracy by doing all muliplications BEFORE dividing.
    float result = AnemometerScaleMPH * 1000.0 * float(AnemoneterPeriodReadingCount) / float(AnemoneterPeriodTotal);
    AnemoneterPeriodTotal = 0;
    AnemoneterPeriodReadingCount = 0;
    *gustMPH = AnemometerScaleMPH  * 1000.0 / float(GustPeriod);
    GustPeriod = UINT_MAX;
    return result;
}


//===========================================================
// Wind Vane
//===========================================================
void initializeWindVane() {
    return;
}

// For the wind vane, we need to average the unit vector components (the sine and cosine of the angle)
int WindVanePin = A0;
float windVaneCosTotal = 0.0;
float windVaneSinTotal = 0.0;
unsigned int windVaneReadingCount = 0;

void captureWindVane() {
    // Read the wind vane, and update the running average of the two components of the vector
    unsigned int windVaneRaw = analogRead(WindVanePin);
    
    float windVaneRadians = lookupRadiansFromRaw(windVaneRaw);
    if(windVaneRadians > 0 && windVaneRadians < 6.14159)
    {
        windVaneCosTotal += cos(windVaneRadians);
        windVaneSinTotal += sin(windVaneRadians);
        windVaneReadingCount++;
    }
    return;
}

float getAndResetWindVaneDegrees()
{
    if(windVaneReadingCount == 0) {
        return 0;
    }
    float avgCos = windVaneCosTotal/float(windVaneReadingCount);
    float avgSin = windVaneSinTotal/float(windVaneReadingCount);
    float result = atan(avgSin/avgCos) * 180.0 / 3.14159;
    windVaneCosTotal = 0.0;
    windVaneSinTotal = 0.0;
    windVaneReadingCount = 0;
    // atan can only tell where the angle is within 180 degrees.  Need to look at cos to tell which half of circle we're in
    if(avgCos < 0) result += 180.0;
    // atan will return negative angles in the NW quadrant -- push those into positive space.
    if(result < 0) result += 360.0;
    
   return result;
}

float lookupRadiansFromRaw(unsigned int analogRaw)
{
    // The mechanism for reading the weathervane isn't arbitrary, but effectively, we just need to look up which of the 16 positions we're in.
    if(analogRaw >= 2200 && analogRaw < 2400) return (3.14);//South
    if(analogRaw >= 2100 && analogRaw < 2200) return (3.53);//SSW
    if(analogRaw >= 3200 && analogRaw < 3299) return (3.93);//SW
    if(analogRaw >= 3100 && analogRaw < 3200) return (4.32);//WSW
    if(analogRaw >= 3890 && analogRaw < 3999) return (4.71);//West
    if(analogRaw >= 3700 && analogRaw < 3780) return (5.11);//WNW
    if(analogRaw >= 3780 && analogRaw < 3890) return (5.50);//NW
    if(analogRaw >= 3400 && analogRaw < 3500) return (5.89);//NNW
    if(analogRaw >= 3570 && analogRaw < 3700) return (0.00);//North
    if(analogRaw >= 2600 && analogRaw < 2700) return (0.39);//NNE
    if(analogRaw >= 2750 && analogRaw < 2850) return (0.79);//NE
    if(analogRaw >= 1510 && analogRaw < 1580) return (1.18);//ENE
    if(analogRaw >= 1580 && analogRaw < 1650) return (1.57);//East
    if(analogRaw >= 1470 && analogRaw < 1510) return (1.96);//ESE
    if(analogRaw >= 1900 && analogRaw < 2000) return (2.36);//SE
    if(analogRaw >= 1700 && analogRaw < 1750) return (2.74);//SSE
    if(analogRaw > 4000) return(-1); // Open circuit?  Probably means the sensor is not connected
    Particle.publish("error", String::format("Got %d from Windvane.",analogRaw), 60 , PRIVATE);
    return -1;
}