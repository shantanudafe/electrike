#include <LowPower.h>
#include <FastLED.h>
#include <SPI.h>
#include <SD.h>
#include <TMRpcm.h>           //  also need to include this library...

// Structure to send between arduinos
struct nanoMessage{
  double curMph;
  int curFpm;
  float batteryVoltage;
  double trip;
  double odo;
}msg;

uint32_t updatedelay = 200; // ms between updates

// Bike Variables
const float bicycleWheelDia = 26;  // Wheel diameter in inches
const int noOfMagnets = 8; // Number of magnets
unsigned long tripRevCount, odoRevCount;

// LED Config Variables
#define LED_PIN     8
#define NUM_LEDS    11
CRGB leds[NUM_LEDS];

// SD Card Setup and files
int SdCsPin = 10;
File myFile;
String msgwrite;

// Battery Detection Module
int batPin = A0;

// Relay Pin
int relayPin = 7;

bool bikePower = true;

// Button Pin
int buttonPin = 2;
const int timeForHold = 500; // Time in ms for a hold and not a press
int buttonStatus = 0;
int buttonState = 1, lastButtonState = 1;
uint32_t curHoldStart = 0;
int actionDone = 0;

// Revolution Hall effect pin
const int hallEffectPin = 6;

boolean lasthallEffectPin = LOW;
boolean currenthallEffectPin = LOW;

// Throttle reading pins
int throttlePin = A1;
int throttleValarray[10], throttleVal;

// Speaker vars
TMRpcm tmrpcm;   // create an object for use in this sketch

uint32_t currentTime = 0;
uint32_t revolutionTime = 0;

float currentMPH;
int currentFPM;

unsigned long revolutionCount = 0;
uint32_t lastRevolutionStartTime = 0;

uint32_t lastUpdateSent;

//A debouncing function that can be used for any button
boolean debounce(boolean last, int pin) {
  boolean current = digitalRead(pin);
  if (last != current) {
    delay(2);
    current = digitalRead(pin);
  }
  return current;
}

void updateButtonPress(){
  buttonState = digitalRead(buttonPin);
  buttonStatus = 0;

  if (buttonState == 0 && lastButtonState == 1){
    curHoldStart = currentTime;
    actionDone = 0;
  }

  if(currentTime - curHoldStart > timeForHold && buttonState == 0){
    buttonStatus = 2;
  }
  else if (buttonState == 1 && lastButtonState == 0 && currentTime - curHoldStart < timeForHold){
      buttonStatus = 1;
  }
  lastButtonState = buttonState;
}

void updateLEDThrottleSpeed(){
  // // Read analog pin
  throttleVal = analogRead(throttlePin);
  
  //Modify the analog value being read to be from 0 to 1023
  throttleVal = map(throttleVal, 10, 1020, 0, 1023);
  
  // Go through and fill in the leds for the speed
  for (int i = 0; i < NUM_LEDS; i++){
    if (throttleVal >= 0){
      if (i <= 5){
        leds[i] = CHSV(95, 255, map((throttleVal>=93)?93:throttleVal ,0,93,0,255));
      }
      else if (i <= 8){
        leds[i] = CHSV(20,255, map((throttleVal>=93)?93:throttleVal ,0,93,0,255));
      }
      else{
        leds[i] = CHSV(0,255, map((throttleVal>=93)?93:throttleVal ,0,93,0,255));
      }
    }
    else{
      leds[i] = CHSV(0,255, 0);
    }
    throttleVal = throttleVal - 93;
  }

  FastLED.show();
}

void updateSpeed(){
  // Read revolution Hall sensor
  currenthallEffectPin = digitalRead(hallEffectPin);
  
  if (lasthallEffectPin == HIGH && currenthallEffectPin == LOW) {
    // Increase wheel revolution count
    tripRevCount++;
    odoRevCount++;

    msg.trip = tripRevCount * 3.14159 * bicycleWheelDia/(noOfMagnets*63360);
    msg.odo = odoRevCount * 3.14159 * bicycleWheelDia/(noOfMagnets*63360);

    revolutionTime = currentTime - lastRevolutionStartTime;
    // Compute current speed in kilometers per hour based on time it took to complete last wheel revolution
    msg.curMph = 3.14159 * (3600000 * bicycleWheelDia)/(63360 * noOfMagnets * revolutionTime);
    msg.curFpm = 3.14159 * (60000 * bicycleWheelDia)/(12 * noOfMagnets * revolutionTime);
    
    lastRevolutionStartTime = currentTime;
  }
  lasthallEffectPin = currenthallEffectPin;
}

void updateVoltage(){
  msg.batteryVoltage = analogRead(batPin) * (25.0/1024.0);
}

void readODOFile(){
  if (SD.exists("ODO.txt")){
    myFile = SD.open("ODO.txt", O_READ);
    msgwrite = "";
    while(myFile.available()){
      msgwrite = msgwrite + char(myFile.read());
    }
    odoRevCount = msgwrite.toInt();
    tripRevCount = 0;
    myFile.close();
  }
}

void writeODOFile(){
  if (SD.exists("ODO.txt")){
    myFile = SD.open("ODO.txt", O_WRITE);
    msgwrite = String(odoRevCount);
    myFile.write(msgwrite.c_str(), msgwrite.length());
    myFile.close();
  }
}

void wakeUp()
{
  // Just a handler for the pin interrupt.
  detachInterrupt(0);
  bikePower = true;
  digitalWrite(relayPin, bikePower);
  tmrpcm.play("bootup.wav");
  while(tmrpcm.isPlaying()){}
  if (SD.begin(SdCsPin)) {
    readODOFile();
  }
  while(!digitalRead(buttonPin)){}
}

void setup() {
  // put your setup code here, to run once:
  // Configure digital input pins for push buttons and Hall sensor
  Serial.begin(14400);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  pinMode (hallEffectPin, INPUT);
  pinMode (relayPin, OUTPUT);
  pinMode(throttlePin, INPUT);
  pinMode (buttonPin, INPUT_PULLUP);

  tmrpcm.speakerPin = 9; //5,6,11 or 46 on Mega, 9 on Uno, Nano, etc
  tmrpcm.quality(1);
  digitalWrite(relayPin, bikePower);
  
  if (SD.begin(SdCsPin)) {
    readODOFile();
  }
  else{
    Serial.println("Sd Fail");
  }

  tmrpcm.play("bootup.wav");
  while(tmrpcm.isPlaying()){}
  
}

void loop() {
  // Get current millis
  currentTime = millis();
  updateLEDThrottleSpeed();
  updateSpeed();
  updateVoltage();
  updateButtonPress();

  if (millis() > lastUpdateSent + updatedelay){
    //Serial.write((byte*)&msg, sizeof(msg));
    Serial.print("BatV: ");Serial.println(msg.batteryVoltage);
    lastUpdateSent = currentTime;
  }

  // If theres no movement for 1 second
  if (millis() > lastRevolutionStartTime + 1000){
    msg.curFpm = 0;
    msg.curMph = 0;
  }

  // Turn on or off the bike on hold
  if (buttonStatus == 2 && actionDone == 0){
    bikePower = !bikePower;
    if(bikePower){
      wakeUp();
    }
    else{
      tmrpcm.play("bootdown.wav");
      while(tmrpcm.isPlaying()){}
      writeODOFile();
      digitalWrite(relayPin, bikePower);
      while(!digitalRead(buttonPin)){}
      delay(100);
      attachInterrupt(0, wakeUp, LOW);
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    }
    actionDone = 1;
  }

  // Play horn when button is pressed
  if (buttonStatus == 1 && !tmrpcm.isPlaying()){
    tmrpcm.play("horn.wav");
  }
  
  if(!tmrpcm.isPlaying()){
    digitalWrite(tmrpcm.speakerPin, LOW);
  }

}
