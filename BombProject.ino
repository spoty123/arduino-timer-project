#include <LiquidCrystal_I2C.h> // Add the I2C LCD library. 

#include <Wire.h> // Add the wire library.

#include <Adafruit_Sensor.h> // Add the Adafruit Sensor library.
#include <Adafruit_ADXL345_U.h> // Add the Adafruit ADXL345 library.

#include "SparkFun_Qwiic_Keypad_Arduino_Library.h" // Add the SparkFun Qwiic keypad library.

#include <IRremote.h>

const int IRPin = 7;
IRrecv irrecv(IRPin);
decode_results results;

KEYPAD keypad1; // Create a new keypad object.
LiquidCrystal_I2C lcd(0x27, 16, 2); // Create a new LCD object.

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(); // Create a new ADXL345 object.

void(*restart) (void) = 0; // Define the arduino restart function. 

// modes:
// 0 - waiting for the bomb to be armed
// 1 - getting the time (arming)
// 2 - getting the password (arming)
// 3 - arm message & bomb ticking & getting the passowrd
// 4 - bomb exploding / defused & waiting for a restart

int mode = 0; // Define the mode varible.

char keypad_currentKey;
long keypad_lastUpdated = 0;

bool mode1_triggered = false; // Define the mode(1) trigger state varible (default to not triggered).
int mode1_currentTimer = 0; // Define the mode(1) current timer state varible (default to 0).
const int mode1_bounds[3][2] = { {4, 6}, {7, 9}, {10, 12} }; // Define the mode(1) LCD bounds array.
int mode1_cursorPosition = mode1_bounds[0][0]; // Define the mode(1) cursor position and set it to the start of the first LCD bound.
const int mode1_timerMaxValues[3][2] = { {2, 3}, {5, 9}, {5, 9} }; // Define the mode(1) timer max values.
const String mode1_timerNames[3] = {" Hours", "Minutes", "Seconds"}; // Define the mode(1) timer names array (by string).
String mode1_timers[3] = {"00", "00", "00"}; // Define the mode(1) timer current values (default to 00 on all timers).

bool mode2_triggered = false;
int mode2_cursorPosition = 4;
String mode2_password = "";

bool mode3_triggered = false;
int mode3_submode = 0;
const int mode3_bounds[2] = {7, 15};
int mode3_cursorPosition = mode3_bounds[0];
long mode3_delayCounter;
String mode3_password = "";
int mode3_accelerationValues[3];
const float mode3_minMoveTriggerValue = 4.0;
long mode3_MSTime;
String mode3_timers[3];
bool mode3_shouldBlink = false;
bool mode3_bombExploded = false;
long mode3_soundTimer;
long mode3_buzzerDelayCounter;
int mode3_soundTimerCounter = 0;
int mode3_tickState = 0;
void mode3_setTickState(long ms) {
  if (ms > 10 * 1000) {
    mode3_tickState = 0;
  } else if (ms > 2.5 * 1000) {
    mode3_tickState = 1;
  } else {
    mode3_tickState = 2;
  }
}
long mode3_getTickWait(int state) {
  long retval;
  switch(state) {
    case 0: {
      retval = 1000L;
      break;
    }
    case 1: {
      retval = 500L;
      break;
    }
    case 2: {
      retval = 250L;
      break;
    }
  }
  return retval;
}
String mode3_calculateTime(int type) {
  int retval;
  switch(type) {
    case 0: {
      retval = ((mode3_MSTime / 3600000) % 24);
      break;
    }
    case 1: {
      retval = ((mode3_MSTime / 60000) % 60);
      break;
    }
    case 2: {
      retval = ((mode3_MSTime / 1000) % 60);
      break;
    }
  }
  String realRetval = String(retval);

  if (retval < 10) {
    realRetval = String("0" + realRetval);
  }

  return realRetval;
}

bool mode3_wasBombMoved(int intialVal[], int currentVal[]) {
  bool retval = false;
  for (int i = 0; i < 3; i++) {
    if (abs(intialVal[i] - currentVal[i]) > mode3_minMoveTriggerValue) {
      retval = true;
    }
  }
  return retval;
}

bool mode4_triggered = false;

const int buzzerPin = 2;
// [0] - red
// [1] - green
// [2] - orange
const int ledPins[] = {10, 8, 9};
int ledStates[] = {LOW, LOW, LOW};

void ChangeLedState(int index, int state) {
  if (ledStates[index] != state) {
    digitalWrite(ledPins[index], state);
    ledStates[index] = state; 
  }
}

void resetLeds() {
  for(int i = 0; i < sizeof(ledStates); i++) {
    ChangeLedState(i, LOW);
  }
}

void setup() {
  Serial.begin(9600);

  irrecv.enableIRIn();
 
  accel.begin();
  
  lcd.init();
  lcd.clear();
  lcd.backlight();

  lcd.setCursor(2, 0);
  lcd.print("Bomb Project");

  lcd.setCursor(3, 1);
  lcd.print("Starting..");

  tone(buzzerPin, 5000, 2500);
  delay(2500);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Press on any key");

  lcd.setCursor(1, 1);
  lcd.print("to countinue..");

  keypad1.begin();

  pinMode(buzzerPin, OUTPUT);
  for (int i = 0; i < sizeof(ledPins); i++) {
    pinMode(ledPins[i], HIGH);
  }

  ChangeLedState(1, HIGH);
}

void loop() {
  if (millis() - keypad_lastUpdated >= 20) {
    keypad1.updateFIFO();
    keypad_currentKey = keypad1.getButton();
    keypad_lastUpdated = millis();
  } else {
    keypad_currentKey = 0;
  }
  
  if (keypad_currentKey != -1 && keypad_currentKey != 0) {
    ChangeLedState(2, HIGH);
  } else {
    ChangeLedState(2, LOW);
  }

  switch (mode) {
    case 0: {
      Mode_0();
      break;
    }
    case 1: {
      Mode_1();
      break;
    }
    case 2: {
      Mode_2();
      break;
    }
    case 3: {
      Mode_3();
      break;
    }
    case 4: {
      Mode_4();
      break;
    }
  } 
}

void Mode_0() {
  if (keypad_currentKey != -1 && keypad_currentKey != 0) {
    tone(buzzerPin, 5000, 100);
    mode++;
  }
}

void Mode_1() {
  if (mode1_triggered) {
    if (keypad_currentKey != 0 && keypad_currentKey != -1) {
      tone(buzzerPin, 5000, 100);
      if (keypad_currentKey != '*' && keypad_currentKey != '#' && mode1_cursorPosition < mode1_bounds[mode1_currentTimer][1]) {
        if (mode1_cursorPosition ==  mode1_bounds[mode1_currentTimer][0] && keypad_currentKey - '0' <= mode1_timerMaxValues[mode1_currentTimer][0]) {
          lcd.print(keypad_currentKey);
          mode1_timers[mode1_currentTimer].setCharAt(0, keypad_currentKey);
          mode1_cursorPosition++;
        } else if (mode1_cursorPosition == mode1_bounds[mode1_currentTimer][1] - 1 && keypad_currentKey - '0' <= mode1_timerMaxValues[mode1_currentTimer][1]) {
          lcd.print(keypad_currentKey);
          mode1_timers[mode1_currentTimer].setCharAt(1, keypad_currentKey);
          mode1_cursorPosition++;
          lcd.noBlink();
        }
      } else if (keypad_currentKey == '*') {
        if (mode1_cursorPosition > mode1_bounds[mode1_currentTimer][0]) {
          if (mode1_cursorPosition ==  mode1_bounds[mode1_currentTimer][0] + 1) {
            mode1_timers[mode1_currentTimer].setCharAt(0, '0');
          } else {
            mode1_timers[mode1_currentTimer].setCharAt(1, '0');
          }
          lcd.setCursor(mode1_cursorPosition - 1, 1);
          lcd.print('0');
          lcd.setCursor(mode1_cursorPosition - 1, 1);
          lcd.blink();
          mode1_cursorPosition--;
        } else if (mode1_currentTimer > 0) {
          mode1_currentTimer--; 
          lcd.setCursor(0, 0);
          lcd.print(mode1_timerNames[mode1_currentTimer] + " (00-" + mode1_timerMaxValues[mode1_currentTimer][0] + mode1_timerMaxValues[mode1_currentTimer][1] + "): ");
          lcd.setCursor(mode1_bounds[mode1_currentTimer][0], 1);
          mode1_cursorPosition = mode1_bounds[mode1_currentTimer][0];
        }
      } else if (keypad_currentKey == '#') {
        if (mode1_currentTimer == 2) {
          mode++;
          lcd.noBlink();
        } else {
          mode1_currentTimer++; 
          lcd.setCursor(0, 0);
          lcd.print(mode1_timerNames[mode1_currentTimer] + " (00-" + mode1_timerMaxValues[mode1_currentTimer][0] + mode1_timerMaxValues[mode1_currentTimer][1] + "): ");
          lcd.setCursor(mode1_bounds[mode1_currentTimer][0], 1);
          mode1_cursorPosition = mode1_bounds[mode1_currentTimer][0];
          lcd.blink();
        }
      }
    }
  } else {
    mode1_triggered = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(mode1_timerNames[mode1_currentTimer] + " (00-" + mode1_timerMaxValues[mode1_currentTimer][0] + mode1_timerMaxValues[mode1_currentTimer][1] + "): ");
    lcd.setCursor(mode1_bounds[mode1_currentTimer][0], 1);
    lcd.print(mode1_timers[0] + ":" + mode1_timers[1] + ":" + mode1_timers[2]);
    lcd.setCursor(mode1_bounds[mode1_currentTimer][0], 1);
    lcd.blink();
  }
}

void Mode_2() {
  if (mode2_triggered) {
    if (keypad_currentKey != 0 && keypad_currentKey != -1) {
      tone(buzzerPin, 5000, 100);
      if (keypad_currentKey != '*' && keypad_currentKey != '#' && mode2_cursorPosition < 12) {
        lcd.print(keypad_currentKey);
        mode2_password += keypad_currentKey;
        mode2_cursorPosition++;
        if (mode2_cursorPosition == 12)
          lcd.noBlink();
      } else if (keypad_currentKey == '*') {
        if (mode2_cursorPosition > 4) {
          lcd.setCursor(mode2_cursorPosition - 1, 1);
          lcd.print(' ');
          lcd.setCursor(mode2_cursorPosition - 1, 1);
          mode2_password = mode2_password.substring(0, mode2_password.length() - 1);
          if (mode2_cursorPosition == 12)
            lcd.blink();
          mode2_cursorPosition--;
        } else {
          mode--;
          mode1_currentTimer = 0; 
          mode1_triggered = false;
          mode1_cursorPosition = mode1_bounds[0][0];
          mode2_triggered  = false;
          lcd.noBlink();
        }
      } else if (keypad_currentKey == '#' && mode2_cursorPosition == 12) {
        mode++;
        lcd.noBlink();
      }
    }
  } else {
    mode2_triggered = true;
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("TIME");
    lcd.setCursor(5, 1);
    lcd.print("IS SET");
    tone(buzzerPin, 5000, 1000);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Defuse code (8):");
    lcd.setCursor(4, 1);
    lcd.blink();
  }
}

void Mode_3() {
  if (mode3_triggered) {
    sensors_event_t event; 
    accel.getEvent(&event);
    int accelerationValues[3] = {event.acceleration.x, event.acceleration.y, event.acceleration.z};
    bool didMove = mode3_wasBombMoved(mode3_accelerationValues, accelerationValues);

    if (didMove) {
        mode3_bombExploded = true;
        mode++; 
        return;
    }

    if (irrecv.decode(&results)) {
      mode3_bombExploded = true;
      mode++;
      return;
    }

    mode3_setTickState(mode3_MSTime);

    long tickWait = mode3_getTickWait(mode3_tickState);

    if (millis() - mode3_buzzerDelayCounter >= tickWait) {
      mode3_buzzerDelayCounter = millis();
      tone(buzzerPin, 2146, tickWait / 10);
      ChangeLedState(0, HIGH);
    } else {
      ChangeLedState(0, LOW);
    }
    
    if (millis() - mode3_delayCounter >= 1000L) {
      mode3_delayCounter = millis();
      mode3_MSTime = mode3_MSTime - 1000;
      mode3_timers[0] = mode3_calculateTime(0);
      mode3_timers[1] = mode3_calculateTime(1);
      mode3_timers[2] = mode3_calculateTime(2);
      lcd.noBlink();
      lcd.setCursor(mode1_bounds[0][0], 1);
      lcd.print(mode3_timers[0] + ":" + mode3_timers[1] + ":" + mode3_timers[2]);
      lcd.setCursor(mode3_cursorPosition, 0);
      if (mode3_shouldBlink)
        lcd.blink();
      if (mode3_MSTime <= 0) {
        mode3_bombExploded = true;
        mode++; // explosion
        return;
      }
    }
    
    switch (mode3_submode) {
      case 0 : {
        if (keypad_currentKey != 0 && keypad_currentKey != -1) {
          if (keypad_currentKey == '#') {
            lcd.setCursor(0, 0);
            lcd.print("                ");
            lcd.setCursor(1, 0);
            lcd.print("Code: ");
            lcd.blink();
            mode3_shouldBlink = true;
            mode3_submode++;
          }
        }
        break;
      }

      case 1: {
        if (keypad_currentKey != 0 && keypad_currentKey != -1) {
          tone(buzzerPin, 5000, 100);
          if (keypad_currentKey != '*' && keypad_currentKey != '#' && mode3_cursorPosition < mode3_bounds[1]) {
            lcd.setCursor(mode3_cursorPosition, 0);
            lcd.print(keypad_currentKey);
            mode3_password += keypad_currentKey;
            mode3_cursorPosition++;
            if (mode3_cursorPosition == mode3_bounds[1])
              lcd.noBlink();
              mode3_shouldBlink = false;
            } else if (keypad_currentKey == '*') {
              if (mode3_cursorPosition > mode3_bounds[0]) {
                lcd.setCursor(mode3_cursorPosition - 1, 0);
                lcd.print(' ');
                lcd.setCursor(mode3_cursorPosition - 1, 0);
                mode3_password = mode3_password.substring(0, mode3_password.length() - 1);
                if (mode3_cursorPosition == mode3_bounds[1]) {
                  lcd.blink();
                  mode3_shouldBlink = true;
                }
                mode3_cursorPosition--;
               }
            } else if (keypad_currentKey == '#' && mode3_cursorPosition == mode3_bounds[1]) {
              if (mode3_password == mode2_password) {
                mode++;
              } else {
                lcd.setCursor(0, 0);
                lcd.print(" INCORRECT CODE ");
                mode3_submode++;
              }
              lcd.noBlink();
              mode3_shouldBlink = false;
            }
        }
        break;
      }

      case 2: {
        if (millis() - mode3_soundTimer >= 500L) {
          mode3_soundTimer = millis();
          if(mode3_soundTimerCounter < 4) {
            tone(buzzerPin, 5000, 450);
            mode3_soundTimerCounter++;
          } else {
            lcd.setCursor(0, 0);
            lcd.print("TO DISARM -> (#)");
            mode3_soundTimerCounter = 0;
            mode3_password = "";
            mode3_cursorPosition = mode3_bounds[0];
            mode3_submode = 0;
          }
        }
        break;
      }
    }
  } else {
    mode3_triggered = true;
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("BOMB");
    lcd.setCursor(4, 1);
    lcd.print("IS ARMED");
    tone(buzzerPin, 5000, 1000);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TO DISARM -> (#)");
    lcd.setCursor(mode1_bounds[0][0], 1);
    memcpy(mode3_timers, mode1_timers, sizeof(mode3_timers));
    lcd.print(mode3_timers[0] + ":" + mode3_timers[1] + ":" + mode3_timers[2]);
    mode3_MSTime = (mode3_timers[0].toInt() * 3600000) + (mode3_timers[1].toInt() * 60000) + (mode3_timers[2].toInt() * 1000);
    sensors_event_t event; 
    accel.getEvent(&event);
    int accelerationValues[3] = {event.acceleration.x, event.acceleration.y, event.acceleration.z};
    memcpy(mode3_accelerationValues, accelerationValues, sizeof(mode3_accelerationValues));
  }
}

void Mode_4() {
  if (mode4_triggered) {
    if (keypad_currentKey != -1 && keypad_currentKey != 0) {
      tone(buzzerPin, 5000, 100);
      resetLeds();
      lcd.clear();
      delay(100);
      restart();
    } 
  } else {
    mode4_triggered = true;
    ChangeLedState(0, HIGH);
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("BOMB");
    lcd.setCursor(4, 1);
    if (mode3_bombExploded) {
      lcd.print("EXPLODED");
      for(int i = 0; i < 5; i++)
      {
        tone(buzzerPin, 2146, 100);
        delay(200);
      }
      for(int i = 0; i < 11; i++)
      {
        tone(buzzerPin, 2146, 50);
        delay(100);
      }
      int freq = 2000;
      tone(buzzerPin, freq, 250);
      delay(250);
      for(int i = 0; i < 300; i++)
      {
        tone(buzzerPin, freq, 10);
        delay(10);
        freq = freq + 10;
      }
    } else {
      lcd.print("DISABLED");
      tone(buzzerPin, 2146, 300);
      delay(350);
      tone(buzzerPin, 1946, 250);
      delay(300);
      tone(buzzerPin, 2146, 300);
      delay(350);
    }
    
    delay(2500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press on any key");
    lcd.setCursor(2, 1);
    lcd.print("to restart..");
  }
}
