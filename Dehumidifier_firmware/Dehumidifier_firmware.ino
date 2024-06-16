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

   // Variables
   String state = "overview"; // define state of the view, options: overview, settingsview, graphview
   int humidSet = 70; // Set default to 70
   int humidSeti = 70;
   boolean editingSetting = false;

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
   #define GREENBLUE   0x17b6
   
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
   tft.cp437(true);

   overview();

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
    if(state == "overview"){
        clickedSide(tsc.xpos, tsc.ypos);
    }
    if(state == "settingsview"){
        clickedSide(tsc.xpos, tsc.ypos);
        clickedSetting(tsc.xpos, tsc.ypos);
        if(editingSetting){
          String b = clickedNumpad(tsc.xpos, tsc.ypos);
          if(b == "S") humidSet = humidSeti;
          else if(b == "B"){
            String h = String(humidSeti);
            h.remove(h.length()-1);
            humidSeti = h.toInt();
          }
          else{
            String h = String(humidSeti)+String(b);
            humidSeti = h.toInt();
          }
        }
        displaySettingUpdate(humidSeti);
    }
    if(state == "graphview"){
        clickedSide(tsc.xpos, tsc.ypos);
    }
  }
  struct sens temphumid = readSHT31();
  DateTime rtctime = rtc.now();
  if(state == "overview") updateValues(rtctime, temphumid);
  saveData(rtctime, temphumid);
  delay(1000);                       // wait for a second

}

void updateValues(DateTime rtctime, struct sens temphumid){
   tft.fillRoundRect(56,16,200,40,10,BLUE);
   tft.setCursor(56,26);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Humid: " + String(temphumid.h) + "%");

   tft.fillRoundRect(56,72,200,40,10,BLUE);
   tft.setCursor(56,82);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Temp: " + String(temphumid.t) + "C");

   tft.fillRoundRect(56,128,200,40,10,BLUE);
   tft.setCursor(56,138);
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
    tap.xpos = map(tp.y, TS_BOT, TS_TOP, 0, tft.width());
    tap.ypos = map(tp.x, TS_LEFT, TS_RT, 0, tft.height());
    //Serial.println("xpos: " + String(tap.xpos) + "ypos: " + String(tap.ypos));
  }
  else{
    tap.tapped = false;
  }
  return tap;

}

String clickedNumpad(int xpos, int ypos){
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
    tft.drawRoundRect(216,184,NUMPADSIZE,NUMPADSIZE,10,GREEN);
    tft.drawRoundRect(272,184,NUMPADSIZE,NUMPADSIZE,10,GREY);
    tft.drawRoundRect(328,184,NUMPADSIZE,NUMPADSIZE,10,RED);
  
    if(ypos > 16 && ypos < (16+NUMPADSIZE)){ //1st row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //1
          tft.drawRoundRect(216,16,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "1";
        }
        else if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //2
          tft.drawRoundRect(272,16,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "2";
        }
        else if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //3
          tft.drawRoundRect(328,16,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "3";
        }
    }
    else if(ypos > 72 && ypos < (72+NUMPADSIZE)){ //2nd row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //4
          tft.drawRoundRect(216,72,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "4";
        }
        else if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //5
          tft.drawRoundRect(272,72,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "5";
        }
        else if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //6
          tft.drawRoundRect(328,72,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "6";
        }
    }
    else if(ypos > 128 && ypos < (128+NUMPADSIZE)){ //3rd row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //7
          tft.drawRoundRect(216,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "7";
        }
        else if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //8
          tft.drawRoundRect(272,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "8";
        }
        else if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //9
          tft.drawRoundRect(328,128,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "9";
        }
    }
    else if(ypos > 184 && ypos < (184+NUMPADSIZE)){ //4rd row
        if(xpos > 216 && xpos < (216+NUMPADSIZE)){ //Set
          tft.drawRoundRect(216,184,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "S";
        }
        else if(xpos > 272 && xpos < (272+NUMPADSIZE)){ //0
          tft.drawRoundRect(272,184,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "0";
        }
        else if(xpos > 328 && xpos < (328+NUMPADSIZE)){ //Backspace
          tft.drawRoundRect(328,184,NUMPADSIZE,NUMPADSIZE,10,GREEN);
          return "B";
        }
    }
    return "";
}

void clickedSetting(int xpos, int ypos){
  if(xpos > 56 && xpos < (56+130)){
    if (ypos > 16 && ypos < (16+40) && !editingSetting){
      editingSetting = true;
    }
    else if (ypos > 16 && ypos < (16+40) && editingSetting){
      humidSet = humidSeti;
      editingSetting = false;
    }
  }
}

void clickedSide(int xpos, int ypos){
  if(xpos > 20 && xpos < (20+NUMPADSIZE)){
    if (ypos > 16 && ypos < (16+NUMPADSIZE)){
      state = "overview";
      overview();
    }
    if(ypos > 72 && ypos < (72+NUMPADSIZE)){
      state = "settingsview";
      settingview();
    }
    if(ypos > 128 && ypos < (128+NUMPADSIZE)){
      state = "graphview";
      graphview();
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

// View changes
void overview(){
  tft.fillScreen(GREY);
  // draw selection buttons
  tft.fillRoundRect(10, 16, NUMPADSIZE, NUMPADSIZE, 10, GREENBLUE);
  tft.setCursor(20, 20);
  tft.setTextSize(2);
  tft.write(0x03);
  
  tft.fillRoundRect(10, 72, NUMPADSIZE, NUMPADSIZE, 10, GREENBLUE);
  tft.setCursor(20,76);
  tft.setTextSize(2);
  tft.write(0x23);
  
  tft.fillRoundRect(10, 128, NUMPADSIZE, NUMPADSIZE, 10, GREENBLUE);
  tft.setCursor(20, 132);
  tft.setTextSize(2);
  tft.write(0xF7);

   // draw info
   tft.fillRoundRect(56,16,200,40,10,BLUE);
   tft.setCursor(56,26);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Humid: ");

   tft.fillRoundRect(56,72,200,40,10,BLUE);
   tft.setCursor(56,82);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Temp: ");

   tft.fillRoundRect(56,128,200,40,10,BLUE);
   tft.setCursor(56,138);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Runtime: ");
}

void settingview(){
   // draw basic layout
   tft.fillScreen(GREY);
   tft.drawFastVLine(200,0, 240, WHITE); // seperation line between the info and control section

  // draw selection buttons
  tft.fillRoundRect(10, 16, NUMPADSIZE, NUMPADSIZE, 10, GREENBLUE);
  tft.setCursor(20, 20);
  tft.setTextSize(2);
  tft.write(0x03);
  
  tft.fillRoundRect(10, 72, NUMPADSIZE, NUMPADSIZE, 10, GREENBLUE);
  tft.setCursor(20,76);
  tft.setTextSize(2);
  tft.write(0x23);
  
  tft.fillRoundRect(10, 128, NUMPADSIZE, NUMPADSIZE, 10, GREENBLUE);
  tft.setCursor(20, 132);
  tft.setTextSize(2);
  tft.write(0xF7);
   
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

   // draw settings screen
   tft.fillRoundRect(56,16,130,40,10,BLUE);
   tft.setCursor(56,26);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Humid: " + String(humidSet));
}

void displaySettingUpdate(int humidSeti){
   // draw settings screen
   if(editingSetting){
    tft.fillRoundRect(56,16,130,40,10,GREEN);
    tft.setCursor(56,26);
    tft.setTextColor(BLACK);
    tft.setTextSize(2);
    tft.println("Humid: " + String(humidSeti));
   }
   else{
    tft.fillRoundRect(56,16,130,40,10,BLUE);
    tft.setCursor(56,26);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println("Humid: " + String(humidSeti));
   }
}

void graphview(){
  int humidGr[16]; //every half an hour for eight hours
  File file = SD.open("datalog.csv", FILE_READ);
  if(file){
    String line;
    fileLength = file.size();
  }
}
