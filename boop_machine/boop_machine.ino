/*

Copyright 2017 Rodrigo Braz Monteiro

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/


// Note: this is some really bad late night code. I apologise for it.



// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

const int shieldReset = -1;

// 0: free
// 1: free
const int buttonPin = 2;
const int dreq = 3;
const int cardCS = 4;
// 5: free
const int shieldDCS = 6;
const int shieldCS = 7;
// 8: free
const int servoPin = 9;
const int ledPin = 10;
// 11: free
// 12: free
// 13: free

constexpr int armUpAngle = -20;
constexpr int armDownAngle = 35;

enum class FilePlaying {
  None,
  Boop,
  BoopOnMe
};


// Program state
int buttonState = 0;
bool booping = false;
FilePlaying curFile = FilePlaying::None;
int curFileStep = 0;
int boopLen = 300;
unsigned long pressStartTime = 0;
unsigned long boopTime = 0;
unsigned long fileTime = 0;
unsigned long servoSetTime = 0;
uint16_t musicValue = 0;
int desiredServoPos = armUpAngle;
int servoPosSets = 0;

constexpr int signalLen = 25;


// The following times are in centiseconds (not milliseconds, as that wouldn't fit an int16) from the start of the song
int boopOnMeTimes[124] = { 1746, 1767, 1786, 1806, 1835, 1874, 1912, 1947, 1966, 1984, 2000, 2016, 2035, 2052, 2071, 2090, 2123, 2157, 2195,
                           2230, 2248, 2268, 2283, 2302, 2319, 2336, 2354, 2373, 2410, 2443, 2478, 2515, 2533, 2552, 2569, 2588, 2604, 2623,
                           2640, 2656, 2694, 2730, 2763, 2801, 2819, 2837, 2854, 2873, 2890, 2907, 2927, 2948, 2978, 3012, 3029, 3049, 3086,
                           3102, 3121, 3140, 3153, 3172, 3191, 3209, 3229, 3264, 3303, 3332, 3368, 3390, 3406, 3426, 3454, 3508, 3544, 3561,
                           3594, 3648, 3703, 3738, 3762, 3794, 3827, 3900, 3938, 3958, 3988, 4027, 4078, 4097, 4116, 4149, 4222, 4290, 4307,
                           4344, 4364, 4398, 4432, 4504, 4523, 4558, 4629, 4685, 4703, 4722, 4910, 4930, 4949, 4966, 4985, 5003, 5022, 5039,
                           5093, 5115, 5592, 5647, 5694, 6158, 6212, 6261, 6904, 7225 };


Adafruit_VS1053_FilePlayer* musicPlayer;

void initMusicPlayer()
{
  // Setup music player
  Serial.begin(9600);
  Serial.println("Boop Machine Test");
  musicPlayer = new Adafruit_VS1053_FilePlayer(shieldReset, shieldCS, shieldDCS, dreq, cardCS);

  if (!musicPlayer->begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));
  
  if (!SD.begin(cardCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }

  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer->setVolume(50, 50);
  musicPlayer->useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
}
 
void setup()
{
  // Setup button
  pinMode(buttonPin, INPUT);

  // Setup LED
  pinMode(ledPin, OUTPUT);
  analogWrite(ledPin, 255);
  delay(100);
  analogWrite(ledPin, 0);

  // Setup servo
  //myservo = new Servo();
  pinMode(servoPin, OUTPUT);
  
  // Music player
  initMusicPlayer();

  // Flash LED to signal end of boot
  analogWrite(ledPin, 255);
  delay(200);
  analogWrite(ledPin, 0);
  delay(100);
  analogWrite(ledPin, 255);
  delay(200);
  analogWrite(ledPin, 64);
}

void doSetServoPos(int n)
{
  servoSetTime = millis();

  int pulseLen = (desiredServoPos * 50 + 5) / 9 + 1000;   // "(pos * 50 + 5) / 9" = "(pos * 1000 + 90) / 180", but without overflowing the 16-bit int
  int totalLen = 20000;

  for (int i = 0; i < n; ++i) {
    digitalWrite(servoPin, HIGH);
    delayMicroseconds(pulseLen);
    digitalWrite(servoPin, LOW);
    delayMicroseconds(totalLen - pulseLen);
  }
  
  servoPosSets--;
}

void setServoPos(int pos)
{
  bool changed = pos != desiredServoPos;
  desiredServoPos = pos;
  
  if (changed) {
    unsigned long now = millis();
    unsigned long elapsed = now - servoSetTime;

    int n = 2;
    if (elapsed > (unsigned long) 500) {
       n = 8;
    } else if (elapsed > (unsigned long) 300) {
      n = 6;
    }
    servoPosSets = 1;
    doSetServoPos(n);
  }
}

void updateServo()
{
  return;
  if (servoPosSets > 0) {
    unsigned long now = millis();
    unsigned long elapsed = now - servoSetTime;
    if (elapsed >= (unsigned long)signalLen) {
      doSetServoPos(1);
    }
  }
}

void boopArm(int len)
{
  booping = true;
  boopTime = millis();
  boopLen = len;

  int nCycles = (len + 10) / 20;
  desiredServoPos = armDownAngle;
  doSetServoPos(nCycles);
  desiredServoPos = armUpAngle;
  doSetServoPos(2);

  //setServoPos(armDownAngle);
}

void boop()
{
  boopArm(600);

  curFile = FilePlaying::Boop;
  musicPlayer->startPlayingFile("boop.ogg");
}

void boopOnMe()
{
  curFile = FilePlaying::BoopOnMe;
  musicPlayer->startPlayingFile("booponme.mp3");
  curFileStep = 0;
  fileTime = millis();
}

void updateMusicBoop()
{
  constexpr int nBoops = 124;
  
  if (curFile == FilePlaying::BoopOnMe && curFileStep < nBoops) {
    const int elapsedCs = int((millis() - fileTime) / 10);
    const int next = boopOnMeTimes[curFileStep];
    if (elapsedCs >= next) {
      curFileStep++;
      
      const int following = curFileStep < nBoops ? boopOnMeTimes[curFileStep] : 30000;
      const int maxBoopTime = following - next - 50;

      int desiredBoop = next == 6904 ? 3000 : 300;
      int t = max(50, min(desiredBoop, maxBoopTime));
      boopArm(desiredBoop);
    }
  }  
}

void loop()
{
  updateServo();
  
  bool pressed = false;
  int buttonStateNow = digitalRead(buttonPin);
  if (buttonStateNow != buttonState) {
    buttonState = buttonStateNow;
    pressed = buttonState == HIGH;
    pressStartTime = millis();
  }

  bool playing = !musicPlayer->stopped();
  
  if (playing) {
    if (pressed) {
      musicPlayer->stopPlaying();
      curFile = FilePlaying::None;
    } else {
      updateMusicBoop();
    }
  } else {
    curFile = FilePlaying::None;
    if (pressed) {
      boop();
    } else if (buttonStateNow == HIGH) {
      unsigned long elapsed = millis() - pressStartTime;
      if (elapsed > 2000) {
        boopOnMe();
      }
    }
  }

  // Check if the boop is done (DEAD CODE?)
  if (booping) {
    unsigned long now = millis();
    if ((unsigned long)(now - boopTime) > (unsigned long)boopLen) {
      booping = false;
      setServoPos(armUpAngle);
      boopTime = now;
    }
  }

  // Set LED brightness
  analogWrite(ledPin, pressed ? 255 : (booping ? 255 : 64));
}

