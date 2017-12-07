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
//constexpr int armUpAngle = 30;
//constexpr int armDownAngle = 60;


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


Adafruit_VS1053_FilePlayer* musicPlayer;// = Adafruit_VS1053_FilePlayer(shieldReset, shieldCS, shieldDCS, dreq, cardCS);

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

/*
  for (int i = 0; i < 50; ++i) {
    desiredServoPos = armUpAngle;
    doSetServoPos(20);
    desiredServoPos = armDownAngle;
    doSetServoPos(20);
  }
  */
  
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

  setServoPos(armDownAngle);
}

void boop()
{
  boopArm(300);

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

// 20 - 200hz Single Pole Bandpass IIR Filter
float bassFilter(float sample) {
    static float xv[3] = {0,0,0}, yv[3] = {0,0,0};
    xv[0] = xv[1]; xv[1] = xv[2]; 
    xv[2] = sample / 9.1f;
    yv[0] = yv[1]; yv[1] = yv[2]; 
    yv[2] = (xv[2] - xv[0])
        + (-0.7960060012f * yv[0]) + (1.7903124146f * yv[1]);
    return yv[2];
}

// 10hz Single Pole Lowpass IIR Filter
float envelopeFilter(float sample) { //10hz low pass
    static float xv[2] = {0,0}, yv[2] = {0,0};
    xv[0] = xv[1]; 
    xv[1] = sample / 160.f;
    yv[0] = yv[1]; 
    yv[1] = (xv[0] + xv[1]) + (0.9875119299f * yv[0]);
    return yv[1];
}

// 1.7 - 3.0hz Single Pole Bandpass IIR Filter
float beatFilter(float sample) {
    static float xv[3] = {0,0,0}, yv[3] = {0,0,0};
    xv[0] = xv[1]; xv[1] = xv[2]; 
    xv[2] = sample / 7.015f;
    yv[0] = yv[1]; yv[1] = yv[2]; 
    yv[2] = (xv[2] - xv[0])
        + (-0.7169861741f * yv[0]) + (1.4453653501f * yv[1]);
    return yv[2];
}

void sampleMusic()
{
  static int sampleN = 0;
  
  uint16_t leftSample = musicPlayer->sciRead(0xC015);
  uint16_t rightSample = musicPlayer->sciRead(0xC016);
  uint16_t curMusicValue = (leftSample >> 1) + (rightSample >> 1);

  float fSample = float(long(curMusicValue) - 32768) * 0.015625;
  float value = bassFilter(fSample);
  value = fabs(value);

  /*
  float envelope = envelopeFilter(value);
  sampleN++;

  if (sampleN == 20) {
    float beat = beatFilter(envelope);
    //musicValue = beat > 1.0f ? 255 : 0;
    musicValue = uint16_t(fabs(beat));
    sampleN = 0;
  }
  */

  musicValue = uint16_t(fabs(value * 0.5f));
  
  //musicValue = (musicValue * 200 + (curMusicValue >> 8) * 56) >> 8;
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

  // LED state
  //analogWrite(ledPin, pressed ? 255 : 32);

  bool playing = !musicPlayer->stopped();
  
  if (playing) {
    if (pressed) {
      musicPlayer->stopPlaying();
      curFile = FilePlaying::None;
    } else {
      updateMusicBoop();
      //sampleMusic();    
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

  // Check if the boop is done
  int boopBrightness = 255;
  if (booping) {
    unsigned long now = millis();
    //boopBrightness = int(long(max(0, (boopTime + boopLen - now))) * 255 / boopLen);
    if ((unsigned long)(now - boopTime) > (unsigned long)boopLen) {
      booping = false;
      setServoPos(armUpAngle);
      boopTime = now;
    }
  }

  // Set LED brightness
  analogWrite(ledPin, pressed ? 255 : (booping ? boopBrightness : (playing ? musicValue : 64)));

  //delayMicroseconds(10);
}

