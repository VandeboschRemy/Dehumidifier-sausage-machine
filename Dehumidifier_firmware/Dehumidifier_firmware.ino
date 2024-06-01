// Authors: Remy Vandebosch & Josef Soucek
// Control software for the arduino mega2560 to control the dehumidifier setup
// Install libraries: MCUFRIEND_KBV | Adafruit_GFX | Adafruit_Touchscreen | SHT31 | RTClib | modified SD.h
//
// Arduino mega 2560
// UNO shield 3.6 inch 240*400 px TFT touchscreen with ST7793 controller chip

   #include <SPI.h>              
   #include <Adafruit_GFX.h>     // hardware-specific library
   #include <MCUFRIEND_kbv.h>    // hardware-specific library
   #include <TouchScreen.h>
   #include <Arduino.h>
   #include <Wire.h>
   #include "Adafruit_SHT31.h"
   #include <SD.h>
   #include <RTClib.h>

   MCUFRIEND_kbv tft;
   Adafruit_SHT31 sht31 = Adafruit_SHT31();
   RTC_DS3231 rtc; 

   const int XP=8,XM=A2,YP=A3,YM=9; //240x400 ID=0x7793
   const int TS_LEFT=910,TS_RT=134,TS_TOP=55,TS_BOT=935;

//PORTRAIT  CALIBRATION     240 x 400
//x = map(p.x, LEFT=910, RT=134, 0, 240)
//y = map(p.y, TOP=55, BOT=935, 0, 400)

//LANDSCAPE CALIBRATION     400 x 240
//x = map(p.y, LEFT=55, RT=935, 0, 400)
//y = map(p.x, TOP=134, BOT=910, 0, 240)


   TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
   TSPoint tp;

// some principal color definitions
// RGB 565 color picker at https://ee-programming-notepad.blogspot.com/2016/10/16-bit-color-generator-picker.html
   #define WHITE       0xFFFF
   #define BLACK       0x0000
   #define BLUE        0x001F
   #define RED         0xF800
   #define GREEN       0x07E0
   #define CYAN        0x07FF
   #define MAGENTA     0xF81F
   #define YELLOW      0xFFE0
   #define GREY        0x2108 
   #define TEXT_COLOR  0xFFFF
   #define RAINDROP    0x7BCF
   #define LEATHER     0x9346 
   
   #define DEG2RAD 0.0174532925 
   #define MINPRESSURE 200
   #define MAXPRESSURE 1000

   #define NUMPADSIZE 40
   #define NUMPADSPACE 16

void setup() {
   pinMode(LED_BUILTIN, OUTPUT);
   
   uint16_t ID;
   ID = tft.readID();

   Serial.begin(9600);

   tft.reset();
   tft.begin(ID);
   tft.setRotation(3);

   // draw basic layout
   tft.fillScreen(GREY);
   tft.drawFastVLine(200,0, 240, WHITE); // seperation line between the info and control section
   
   // draw buttons for control screen
   // 40x40 px buttons in a 3x4 grid, 16px to the edge and spacing
   tft.fillRoundRect(216,16,NUMPADSIZE,NUMPADSIZE,10,WHITE); //1
   tft.fillRoundRect(216,72,NUMPADSIZE,NUMPADSIZE,10,WHITE); //4
   tft.fillRoundRect(216,128,NUMPADSIZE,NUMPADSIZE,10,WHITE); //7
   tft.fillRoundRect(216,184,NUMPADSIZE,NUMPADSIZE,10,GREEN); //set
   tft.fillRoundRect(272,16,NUMPADSIZE,NUMPADSIZE,10,WHITE); //2
   tft.fillRoundRect(272,72,NUMPADSIZE,NUMPADSIZE,10,WHITE); //5
   tft.fillRoundRect(272,128,NUMPADSIZE,NUMPADSIZE,10,WHITE); //8
   tft.fillRoundRect(272,184,NUMPADSIZE,NUMPADSIZE,10,WHITE); //0
   tft.fillRoundRect(328,16,NUMPADSIZE,NUMPADSIZE,10,WHITE); //3
   tft.fillRoundRect(328,72,NUMPADSIZE,NUMPADSIZE,10,WHITE); //6
   tft.fillRoundRect(328,128,NUMPADSIZE,NUMPADSIZE,10,WHITE); //9
   tft.fillRoundRect(328,184,NUMPADSIZE,NUMPADSIZE,10,RED); //backspace
   tft.drawChar(226,20,49,BLACK,0,4); //1
   tft.drawChar(226,76,52,BLACK,0,4); //4
   tft.drawChar(226,132,55,BLACK,0,4); //7
   tft.drawChar(226,188,83,BLACK,0,4); //set
   tft.drawChar(282,20,50,BLACK,0,4); //2
   tft.drawChar(282,76,53,BLACK,0,4); //5
   tft.drawChar(282,132,56,BLACK,0,4); //8
   tft.drawChar(282,188,48,BLACK,0,4); //0
   tft.drawChar(338,20,51,BLACK,0,4); //3
   tft.drawChar(338,76,54,BLACK,0,4); //6
   tft.drawChar(338,132,57,BLACK,0,4); //9
   tft.drawChar(338,188,66,BLACK,0,4); //backspace

   // draw info screen
   tft.fillRoundRect(20,16,160,40,10,BLUE);
   tft.setCursor(20,26);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Humid");

   tft.fillRoundRect(20,72,160,40,10,BLUE);
   tft.setCursor(20,82);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Temp");

   tft.fillRoundRect(20,128,160,40,10,BLUE);
   tft.setCursor(20,138);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Runtime");

   //Initialize sensor
   if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  //Initialize RTC module 
  if(!rtc.begin()){
    Serial.println("couldnt start clock");
  }

  //Initialize the sd-card
  pinMode(10, OUTPUT);
  if(!SD.begin()){
   tft.setCursor(0,0);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("SD-card not present or failed");
  }

}

struct touchscreenClick{
  bool tapped;
  int xpos, ypos;
};

struct sens{
  float t,h;
};

void loop() {

  struct touchscreenClick tsc = clickedTouchPad();
  if(tsc.tapped){
    clickedNumpad(tsc.xpos, tsc.ypos);
  }
  struct sens temphumid = readSHT31();
  DateTime rtctime = rtc.now();
  updateValues(rtctime, temphumid);
  saveData(rtctime, temphumid);
  delay(1000);                       // wait for a second

}

void updateValues(DateTime rtctime, struct sens temphumid){
  tft.fillRoundRect(20,16,160,40,10,BLUE);
   tft.setCursor(20,26);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Humid: " + String(temphumid.h) + "%");

   tft.fillRoundRect(20,72,160,40,10,BLUE);
   tft.setCursor(20,82);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Temp: " + String(temphumid.t) + "C");

   tft.fillRoundRect(20,128,160,40,10,BLUE);
   tft.setCursor(20,138);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Runtime: " + String(rtctime.second()));
}

struct touchscreenClick clickedTouchPad(){
  struct touchscreenClick tap;

  tp = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  if(tp.z > MINPRESSURE && tp.z < MAXPRESSURE){
    tap.tapped = true;
    //reset the selection box
    tft.drawRoundRect(216,16,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(272,16,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(328,16,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(216,72,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(272,72,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(328,72,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(216,128,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(272,128,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(328,128,NUMPADSIZE,NUMPADSIZE,10,GREY);
    
    tap.xpos = map(tp.y, TS_BOT, TS_TOP, 0, tft.width());
    tap.ypos = map(tp.x, TS_LEFT, TS_RT, 0, tft.height());
    Serial.println("xpos: " + String(tap.xpos) + "ypos: " + String(tap.ypos));
  }
  else{
    tap.tapped = false;
  }
  return tap;

}

void clickedNumpad(int xpos, int ypos){
  
    if(ypos > 16 && ypos < (16+NUMPADSIZE)){ //1st row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //1
          tft.drawRoundRect(216,16,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("1");
        }
        if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //2
          tft.drawRoundRect(272,16,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("2");
        }
        if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //3
          tft.drawRoundRect(328,16,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("3");
        }
    }
    if(ypos > 72 && ypos < (72+NUMPADSIZE)){ //2nd row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //4
          tft.drawRoundRect(216,72,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("4");
        }
        if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //5
          tft.drawRoundRect(272,72,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("5");
        }
        if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //6
          tft.drawRoundRect(328,72,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("6");
        }
    }
    if(ypos > 128 && ypos < (128+NUMPADSIZE)){ //3rd row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //7
          tft.drawRoundRect(216,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("7");
        }
        if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //8
          tft.drawRoundRect(272,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("8");
        }
        if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //9
          tft.drawRoundRect(328,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("9");
        }
    }
    if(ypos > 184 && ypos < (184+NUMPADSIZE)){ //4rd row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //Set
          tft.drawRoundRect(216,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("S");
        }
        if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //0
          tft.drawRoundRect(272,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("0");
        }
        if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //Backspace
          tft.drawRoundRect(328,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          tft.fillRect(0,120,198,20,GREY);
          tft.setCursor(0,120);
          tft.println("B");
        }
    }
}

struct sens readSHT31(){
  struct sens th;
  th.t = sht31.readTemperature();
  th.h = sht31.readHumidity();
  return th;
}

void saveData(DateTime rtctime, struct sens temphumid){
  File file = SD.open("datalog.csv", FILE_WRITE);
  if(file){
    file.println(String(rtctime.second())+","+String(temphumid.h)+","+String(temphumid.t));
    file.close();
  }else{
    tft.setCursor(0,0);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println("Error saving data to file");
  }
}
