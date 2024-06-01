#include<SPI.h>
#include <SD.h>

File myFile;

void setup() {
  pinMode(53, OUTPUT);
  pinMode(10, OUTPUT);
  //digitalWrite(53,HIGH);
  Serial.begin(9600);
  Serial.println(String(SD.begin()));
  if (!SD.begin()) {
    Serial.println(F("SD CARD FAILED, OR NOT PRESENT!"));
    while (1); // don't do anything more:
  }

  Serial.println(F("SD CARD INITIALIZED."));

  if (!SD.exists("arduino.txt")) {
    Serial.println(F("arduino.txt doesn't exist. Creating arduino.txt file..."));
    // create a new file by opening a new file and immediately close it
    myFile = SD.open("arduino.txt", FILE_WRITE);
    myFile.close();
  }

  // recheck if file is created or not
  if (SD.exists("arduino.txt"))
    Serial.println(F("arduino.txt exists on SD Card."));
  else
    Serial.println(F("arduino.txt doesn't exist on SD Card."));
}

void loop() {
  delay(1000);
}
