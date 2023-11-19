#include <Elegoo_TFTLCD.h>
#include <pin_magic.h>
#include <registers.h>

#include <Elegoo_GFX.h>    // Core graphics library
#include <Elegoo_TFTLCD.h> // Hardware-specific library
#include <stdint.h>
#include "TouchScreen.h"

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

#define MINPRESSURE 10
#define MAXPRESSURE 1000

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
// optional
#define LCD_RESET A4

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define BOXSIZE 40

Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

struct nanoMessage{
  double curMph;
  int curFpm;
  float batteryVoltage;
  double trip;
  double odo;
};

struct config{
  bool playBootSound;
  bool odoUnit; // true == miles, false == ft
}settings;

struct dataOnScreen{
  float batVolt;
  float mph;
  int fpm;
  float trip;
  float odo;
}onscreen = {0,0,0,0,0};

float trip = 0, odo = 0;

// Bike Variables
const float bicycleWheelDia = 26;  // Wheel diameter in inches
const int noOfMagnets = 8; // Number of magnets

unsigned long startRevCount = 0;

double mph;
int fpm;

nanoMessage msg = {0};

TSPoint p,temp;

unsigned int curScreen = 1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(14400);
 
  tft.reset();
  uint16_t identifier = tft.readID();
  identifier = 0x9341;
  tft.begin(identifier);

  delay(500);
  tft.setRotation(1);

  clearScreen();
  mainScreen();
}

void checkTouch(){
  temp = ts.getPoint();

  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  p.x = map(temp.y, TS_MINY, TS_MAXY, tft.width(), 0);
  p.y = map(temp.x, TS_MINX, TS_MAXX, tft.height(), 0);

  if (temp.z > MINPRESSURE && temp.z < MAXPRESSURE) {
    if (p.x > 320-BOXSIZE){
      if(p.y <BOXSIZE){
        msg.curMph++;
      }
      else if (p.y < 2* BOXSIZE){
        msg.curFpm++;
      }
    }
  }
}

void clearScreen(){
  tft.fillScreen(BLACK);
}

void mainScreen(){
  tft.setTextColor(WHITE);  tft.setTextSize(3);
  
  // tft.setCursor(20, 20);
  // tft.print("Speeeeeeeeeeed: ");

  tft.setCursor(20, 140);
  tft.print("CUR: ");

  tft.setCursor(20, 190);
  tft.print("ODO: ");

  tft.setTextColor(YELLOW); tft.setTextSize(3);

  tft.setCursor(176, 50);
  tft.print("mph");

  tft.setCursor(176, 80);
  tft.print("fpm");

  tft.setCursor(272, 190);
  tft.print("mi");

  tft.setCursor(272, 140);
  tft.print("mi");

  tft.setCursor(86, 50);
  tft.print(" 0.0");

  tft.setCursor(86, 80);
  tft.print("   0");

  tft.setCursor(110, 140);
  tft.print("  0.0000");

  tft.setCursor(110, 190);
  tft.print("    0.00");

  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.print(" 0.0");

  // Menu Buttons
  tft.fillRect(320-BOXSIZE, 0, BOXSIZE, BOXSIZE, RED);
  tft.fillRect(320-BOXSIZE, BOXSIZE, BOXSIZE, BOXSIZE, YELLOW);
 
  // Battery
  tft.drawRect(6,6,78,22,WHITE);
  tft.drawRect(5,5,80,24,WHITE);
  tft.fillRect(83,11,6,12,WHITE);

  tft.setCursor(70, 10);
  tft.setTextSize(2);
  tft.print("V");

}

void updateNumbers() {

  tft.setTextColor(YELLOW,BLACK); tft.setTextSize(3);

  if (abs(onscreen.mph - msg.curMph) >= 0.1){
    tft.setCursor(86, 50);
    
    if(msg.curMph < 9.95){
      tft.print(" ");
    }
    tft.print(msg.curMph,1);

    onscreen.mph = msg.curMph;
  }

  if (abs(msg.curFpm  - onscreen.fpm) >= 1){
    tft.setCursor(86, 80);
    if(msg.curFpm < 10){
      tft.print("   ");
    }
    else if(msg.curFpm < 100){
      tft.print("  ");
    }
    else if(msg.curFpm < 1000){
      tft.print(" ");
    }
    tft.print(msg.curFpm);

    onscreen.fpm = msg.curFpm;
  }

  if(abs(msg.trip - onscreen.trip) >= 0.0001){
    tft.setCursor(110, 140);
    if(msg.trip < 9.99995)
      tft.print("  ");
    else if(msg.trip < 99.99995)
      tft.print(" ");
    tft.print(msg.trip, 4);

    onscreen.trip = msg.trip;
  }

  if (abs(msg.odo - onscreen.odo) >= 0.01){
    tft.setCursor(110, 190);
    if(msg.odo < 9.995)
      tft.print("    ");
    else if(msg.odo < 99.995)
      tft.print("   ");
    else if(msg.odo < 999.995)
      tft.print("  ");
    else if(msg.odo < 9999.995)
      tft.print(" ");
    tft.print(msg.odo, 2);

    onscreen.odo = msg.odo;
  }

  if (abs(onscreen.batVolt - msg.batteryVoltage) >= 0.1){
    tft.setCursor(10, 10);
    tft.setTextSize(2);
    if (msg.batteryVoltage < 9.95)
      tft.print(" ");
    tft.print(msg.batteryVoltage, 1);

    onscreen.batVolt = msg.batteryVoltage;
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  checkTouch();

  while(Serial.available()){
    Serial.readBytes((byte *) &msg, sizeof(msg));
  }

  updateNumbers();

}
