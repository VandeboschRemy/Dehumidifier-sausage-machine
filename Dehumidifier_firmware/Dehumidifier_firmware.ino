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
   DateTime lastSave = DateTime(2024,1,1,0,0,0);
   int humidSet = 70; // Set default to 70
   int humidSeti = 70;
   float humidRampStart = 0;
   int hysteresis = 1; // The hysteresis in % on the humidity for triggering the relay.
   int FanSet = 1000; // Default fan speed 1000 rpm.
   int FanSeti = 1000;
   int ClockSegment = 0; // Cycle through the different sections of time to set
   int ClockSeti = 0;
   int ramptime = 0; // Set the time to ramp from current humidity to set value in hours.
   int ramptimei = 0;
   DateTime startRamp = DateTime(2024,1,1,0,0,0);
   boolean editingSettingH = false;
   boolean editingSettingF = false;
   boolean editingSettingC = false;
   boolean editingSettingR = false;
   double ox , oy ;
   boolean graphsredraw = true;
   boolean dehumidState = false;

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
   pinMode(23, OUTPUT);
   digitalWrite(23, true);
   
   uint16_t ID;
   ID = tft.readID();

   Serial.begin(9600);
   Serial.println(ID);

   tft.reset();
   tft.begin(ID);
   tft.setRotation(3);
   tft.cp437(true);

   Initialize sensor
   if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
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

  // Set registers to 25KHz PWM output to control the fanspeed.
  ICR5 = (F_CPU/25000)/2;
  TCCR5A = _BV(COM4C1) | _BV(WGM41);
  TCCR5B = _BV(WGM43) | _BV(CS40);
  pinMode(45, OUTPUT);
  SetFanSpeed(FanSet);

  //Serial.println("Setup done");
  overview();
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
        if(editingSettingH){
          String b = clickedNumpad(tsc.xpos, tsc.ypos);
          if(b == "S"){
            humidSet = humidSeti;
            editingSettingH = false;
          }
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
        if(editingSettingF){
          String b = clickedNumpad(tsc.xpos, tsc.ypos);
          if(b == "S"){
            FanSet = FanSeti;
            SetFanSpeed(FanSet);
            editingSettingF = false;
          }
          else if(b == "B"){
            String h = String(FanSeti);
            h.remove(h.length()-1);
            FanSeti = h.toInt();
          }
          else{
            String h = String(FanSeti)+String(b);
            FanSeti = h.toInt();
          }
        }
        if(editingSettingC){
          String b = clickedNumpad(tsc.xpos, tsc.ypos);
          if(b == "S"){
            DateTime rtctime = rtc.now();
            switch(ClockSegment){
              case 0:
                rtc.adjust(DateTime(ClockSeti, rtctime.month(), rtctime.day(), rtctime.hour(), rtctime.minute(), rtctime.second()));
                break;
              case 1:
                rtc.adjust(DateTime(rtctime.year(), ClockSeti, rtctime.day(), rtctime.hour(), rtctime.minute(), rtctime.second()));
                break;
              case 2:
                rtc.adjust(DateTime(rtctime.year(), rtctime.month(), ClockSeti, rtctime.hour(), rtctime.minute(), rtctime.second()));
                break;
              case 3:
                rtc.adjust(DateTime(rtctime.year(), rtctime.month(), rtctime.day(), ClockSeti, rtctime.minute(), rtctime.second()));
                break;
              case 4:
                rtc.adjust(DateTime(rtctime.year(), rtctime.month(), rtctime.day(), rtctime.hour(), ClockSeti, rtctime.second()));
                break;
              case 5:
                rtc.adjust(DateTime(rtctime.year(), rtctime.month(), rtctime.day(), rtctime.hour(), rtctime.minute(), ClockSeti));
                editingSettingC = false;
                break;
            }
            ClockSegment = ClockSegment + 1;
            ClockSeti = 0;
          }
          else if(b == "B"){
            String h = String(ClockSeti);
            h.remove(h.length()-1);
            ClockSeti = h.toInt();
          }
          else{
            String h = String(ClockSeti)+String(b);
            ClockSeti = h.toInt();
          }
        }
        if(editingSettingR){
          String b = clickedNumpad(tsc.xpos, tsc.ypos);
          if(b == "S"){
            ramptime = ramptimei;
            startRamp = rtc.now();
            humidRampStart = readSHT31().h;
            editingSettingR = false;
          }
          else if(b == "B"){
            String h = String(ramptimei);
            h.remove(h.length()-1);
            ramptimei = h.toInt();
          }
          else{
            String h = String(ramptimei)+String(b);
            ramptimei = h.toInt();
          }
        }
        displaySettingUpdate(humidSeti, FanSeti, ClockSegment, ClockSeti, ramptime, ramptimei);
    }
    if(state == "graphview"){
        clickedSide(tsc.xpos, tsc.ypos);
    }
  }
  struct sens temphumid = readSHT31();
  DateTime rtctime = rtc.now();
  float hs = toggleDehumid(temphumid, humidSet, humidRampStart, hysteresis, startRamp, ramptime, rtctime);
  if(state == "overview") updateValues(rtctime, temphumid, dehumidState, hs);
  if(lastSave.minute() - rtctime.minute() != 0){
    saveData(rtctime, temphumid, dehumidState);
    lastSave = rtctime;
  }
  delay(300);
}

void updateValues(DateTime rtctime, struct sens temphumid, boolean dehumidstate, float hs){
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
   tft.println("Time: " + String(rtctime.hour())+ ":" +String(rtctime.minute())+ ":" +String(rtctime.second()));

   if(dehumidstate) tft.fillRoundRect(272,16,80,40,10,GREEN);
   else tft.fillRoundRect(272,16,80,40,10,RED);
   tft.setCursor(272,26);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println(String(hs));
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
    if (ypos > 16 && ypos < (16+40) && !editingSettingH){
      editingSettingH = true;
    }
    else if (ypos > 16 && ypos < (16+40) && editingSettingH){
      humidSet = humidSeti;
      editingSettingH = false;
    }
  }
  if(xpos > 56 && xpos < (56+130)){
    if (ypos > 72 && ypos < (72+40) && !editingSettingF){
      editingSettingF = true;
    }
    else if (ypos > 16 && ypos < (16+40) && editingSettingF){
      FanSet = FanSeti;
      editingSettingF = false;
    }
  }
  if(xpos > 56 && xpos < (56+130)){
    if (ypos > 128 && ypos < (128+40) && !editingSettingC){
      editingSettingC = true;
    }
    else if (ypos > 128 && ypos < (128+40) && editingSettingC){
      editingSettingC = false;
      ClockSegment = 0;
    }
  }
  if(xpos > 56 && xpos < (56+130)){
    if (ypos > 184 && ypos < (184+40) && !editingSettingR){
      editingSettingR = true;
    }
    else if (ypos > 184 && ypos < (184+40) && editingSettingR){
      editingSettingR = false;
      ramptime = ramptimei;
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

float toggleDehumid (struct sens temphumid, float humidSet, float humidRampStart, int hysteresis, DateTime startRamp, float ramptime, DateTime current){
  float hs = 0;
  if(ramptime != 0){
    int hourselapsed = (current.unixtime() - startRamp.unixtime())/3600; 
    float ramprate = (humidSet - humidRampStart)/(ramptime);
    if(hourselapsed <= ramptime)hs = humidRampStart + (ramprate*hourselapsed);
    else hs = humidSet;
  }
  else hs = humidSet;
  if(temphumid.h > (hs + hysteresis) and dehumidState == false){
    digitalWrite(23, false);
    delay(200);
    digitalWrite(23, true); // pulse relay to simulate button press.
    dehumidState = true;
  }
  if(temphumid.h < (hs - hysteresis) and dehumidState == true){
    digitalWrite(23, false); 
    delay(200);
    digitalWrite(23, true);
    dehumidState = false;
  }
  return hs;
}

void saveData(DateTime rtctime, struct sens temphumid, boolean dehumidstate){
  File file = SD.open("datalog.csv", FILE_WRITE);
  if(file){
    String line = String(rtctime.year())+ "-" +String(rtctime.month())+ "-" +String(rtctime.day())+ "T" +String(rtctime.hour())+ ":" +String(rtctime.minute())+ ":" +String(rtctime.second())+","+String(temphumid.h)+","+String(temphumid.t) + "," + String(dehumidstate) + ",";
    int filler = 35 - line.length();
    int i = 0;
    while(i < filler){
      line = line + "#";
      i+=1;
    }
    file.println(line);
    Serial.println(line);
    file.close();
  }else{
    tft.setCursor(0,0);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println("Error saving data to file");
  }
}

void SetFanSpeed(int rpm){
  float duty = 1;
  if(rpm <= 3000) duty = rpm/3000; // max speed is 3000 rpm
  OCR5B = (uint16_t)(ICR5*duty);
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

   tft.fillRoundRect(56,72,130,40,10,BLUE);
   tft.setCursor(56,82);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Fan: " + String(FanSet));

   tft.fillRoundRect(56,128,130,40,10,BLUE);
   tft.setCursor(56,138);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Updateclock");

   tft.fillRoundRect(56,184,130,40,10,BLUE);
   tft.setCursor(56,194);
   tft.setTextColor(WHITE);
   tft.setTextSize(2);
   tft.println("Ramp: " + String(ramptime));
}

void displaySettingUpdate(int humidSeti, int FanSeti, int ClockSegment, int ClockSeti, int ramptime, int ramptimei){
  Serial.println(editingSettingC);
  Serial.println(ClockSegment);
  Serial.println(ClockSeti);
   // draw settings screen
   if(editingSettingH){
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
   
   if(editingSettingF){
    tft.fillRoundRect(56,72,130,40,10,GREEN);
    tft.setCursor(56,82);
    tft.setTextColor(BLACK);
    tft.setTextSize(2);
    tft.println("Fan: " + String(FanSeti));
   }
   else{
    tft.fillRoundRect(56,72,130,40,10,BLUE);
    tft.setCursor(56,82);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println("Fan: " + String(FanSeti));
   }

   if(editingSettingC){
    tft.fillRoundRect(56,128,130,40,10,GREEN);
    tft.setCursor(56,138);
    tft.setTextColor(BLACK);
    tft.setTextSize(2);
    switch(ClockSegment){
      case 0:
        tft.println("Y: " + String(ClockSeti));
        break;
      case 1:
        tft.println("M: " + String(ClockSeti));
        break;
      case 2:
        tft.println("D: " + String(ClockSeti));
        break;
      case 3:
        tft.println("H: " + String(ClockSeti));
        break;
      case 4:
        tft.println("m: " + String(ClockSeti));
        break;
      case 5:
        tft.println("s: " + String(ClockSeti));
        break;
    }
   }
   else{
    tft.fillRoundRect(56,128,130,40,10,BLUE);
    tft.setCursor(56,138);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println("Update Clock");
   }
   if(editingSettingR){
    tft.fillRoundRect(56,184,130,40,10,GREEN);
    tft.setCursor(56,194);
    tft.setTextColor(BLACK);
    tft.setTextSize(2);
    tft.println("Ramp: " + String(ramptimei));
   }
   else{
    tft.fillRoundRect(56,184,130,40,10,BLUE);
    tft.setCursor(56,194);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println("Ramp: " + String(ramptimei));
   }
}

void graphview(){
  // draw basic layout
   tft.fillScreen(GREY);
   //tft.drawFastVLine(200,0, 240, WHITE); // seperation line between the info and control section

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

  graphsredraw = true;  
  File file = SD.open("datalog.csv", FILE_READ);
  if(file){
    String line;
    int linesToRead;
    int readLineInterval;
    int graphStartX = 0;
    int graphEndX;
    int graphIntervalX;
    int xMultiplier;
    String title;
    String xTitle;
    int fileLength = file.size();
    Serial.println("size " + String(fileLength));
    if(fileLength > 2220 and fileLength < 17760){ //display one hour per 10 minutes
      linesToRead = 6;
      readLineInterval = 370;
      graphEndX = 60;
      graphIntervalX = 10;
      xMultiplier = 10;
      title = "Moist sausage past hour";
      xTitle = "Time(m)";
    }
    if(fileLength > 17760 and fileLength < 372960){ // display last hours per hour
      linesToRead = 8;
      readLineInterval = 2220;
      graphEndX = 9;
      graphIntervalX = 1;
      xMultiplier = 1;
      title = "Moist sausage past 8h";
      xTitle = "Time(h)";
    }
    if(fileLength > 372960){ // display week per day
      linesToRead = 7;
      readLineInterval = 53280;
      graphEndX = 8;
      graphIntervalX = 1;
      xMultiplier = 1;
      title = "Moist sausage past week";
      xTitle = "Time(day)";
    }
    for (int i = linesToRead; i >= 0; i=i-1){
      file.seek(fileLength - ((linesToRead+1)-i)*readLineInterval);
      String line = file.readStringUntil('\n');
      char* token = strtok(line.c_str(), ",");
      token = strtok(NULL, ","); // point to the next part of the string
      int humidity = String(token).toInt();
      token = strtok(NULL, ",");
      int temp = String(token).toInt();
      Graph(tft, i*xMultiplier, humidity, 90, 200, 300, 160, graphStartX, graphEndX, graphIntervalX, 0, 100, 10, title, xTitle, "Humidity (%)", BLUE, RED, YELLOW, WHITE, BLACK, graphsredraw);
    }
    file.close();
  }
}

void Graph(MCUFRIEND_kbv &d, double x, double y, double gx, double gy, double w, double h, double xlo, double xhi, double xinc, double ylo, double yhi, double yinc, String title, String xlabel, String ylabel, unsigned int gcolor, unsigned int acolor, unsigned int pcolor, unsigned int tcolor, unsigned int bcolor, boolean &redraw) {

  double ydiv, xdiv;
  // initialize old x and old y in order to draw the first point of the graph
  // but save the transformed value
  // note my transform funcition is the same as the map function, except the map uses long and we need doubles
  //static double ox = (x - xlo) * ( w) / (xhi - xlo) + gx;
  //static double oy = (y - ylo) * (gy - h - gy) / (yhi - ylo) + gy;
  double i;
  double temp;
  int rot, newrot;

  if (redraw == true) {

    redraw = false;
    ox = (x - xlo) * ( w) / (xhi - xlo) + gx;
    oy = (y - ylo) * (gy - h - gy) / (yhi - ylo) + gy;
    // draw y scale
    for ( i = ylo; i <= yhi; i += yinc) {
      // compute the transform
      temp =  (i - ylo) * (gy - h - gy) / (yhi - ylo) + gy;

      if (i == 0) {
        d.drawLine(gx, temp, gx + w, temp, acolor);
      }
      else {
        d.drawLine(gx, temp, gx + w, temp, gcolor);
      }

      d.setTextSize(1);
      d.setTextColor(tcolor, bcolor);
      d.setCursor(gx - 40, temp);
      // precision is default Arduino--this could really use some format control
      d.println(i);
    }
    // draw x scale
    for (i = xlo; i <= xhi; i += xinc) {

      // compute the transform

      temp =  (i - xlo) * ( w) / (xhi - xlo) + gx;
      if (i == 0) {
        d.drawLine(temp, gy, temp, gy - h, acolor);
      }
      else {
        d.drawLine(temp, gy, temp, gy - h, gcolor);
      }

      d.setTextSize(1);
      d.setTextColor(tcolor, bcolor);
      d.setCursor(temp, gy + 10);
      // precision is default Arduino--this could really use some format control
      d.println(i);
    }

    //now draw the labels
    d.setTextSize(2);
    d.setTextColor(tcolor, bcolor);
    d.setCursor(gx , gy - h - 30);
    d.println(title);

    d.setTextSize(1);
    d.setTextColor(acolor, bcolor);
    d.setCursor(gx , gy + 20);
    d.println(xlabel);

    d.setTextSize(1);
    d.setTextColor(acolor, bcolor);
    d.setCursor(gx - 30, gy - h - 10);
    d.println(ylabel);


  }

  //graph drawn now plot the data
  // the entire plotting code are these few lines...
  // recall that ox and oy are initialized as static above
  x =  (x - xlo) * ( w) / (xhi - xlo) + gx;
  y =  (y - ylo) * (gy - h - gy) / (yhi - ylo) + gy;
  d.drawLine(ox, oy, x, y, pcolor);
  d.drawLine(ox, oy + 1, x, y + 1, pcolor);
  d.drawLine(ox, oy - 1, x, y - 1, pcolor);
  ox = x;
  oy = y;

}
/*
  End of graphing function
*/
