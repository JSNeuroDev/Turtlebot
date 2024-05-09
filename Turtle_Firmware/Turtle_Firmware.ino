#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Dynamixel2Arduino.h>

// ---Macros---
#define NUMPIXELS 1
#define DXL_DIR_PIN1 6

// Wifi credentials
const char* ssid     = "MyWifiSSID";
const char* pw = "MyWifiPW";

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Dynamixel2Arduino dxl(Serial1, DXL_DIR_PIN1);

using namespace ControlTableItem; // Namespace (for Dynamixel control table items)

WiFiServer server(80);
byte op = 0; // Op code
byte leftMotor = 1;
byte rightMotor = 2;
byte moveType = 0; 
byte activeCommandType = 0;
bool paused = false;
bool commandRunning = false;
uint32_t pauseDuration = 0;
uint32_t pauseStartTime = 0;
float goalPositionL = 0;
float goalPositionR = 0;
float currentPositionL = 0;
float currentPositionR = 0;
float movePosition = 0;
float maxVelocity = 0;
float acceleration = 0;
float commandCue_Data[1000] = {0};
uint8_t commandCue_CmdType[1000] = {0}; // 0 = go, 1 = turn, 2 = set speed, 3 = pause
uint32_t nCommands = 0;

union { // Floating point amplitude in range 1 (max intensity) -> 0 (silent). Data in forward direction.
    byte byteArray[4];
    float floatArray[1];
} typeCast;

void setup()
{
    #if defined(NEOPIXEL_POWER)
      pinMode(NEOPIXEL_POWER, OUTPUT);
      digitalWrite(NEOPIXEL_POWER, HIGH);
    #endif
    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    pixels.setBrightness(2); // not so bright
    setLED(255, 0, 0); // Red LED, starting setup

    dxl.begin(4000000);
    dxl.setPortProtocolVersion(2.0);

    // Setup motor 1
    dxl.setBaudrate(1, 4000000);
    dxl.torqueOff(1);
    dxl.setOperatingMode(1, 4);
    dxl.torqueOn(1);

    // Setup motor 2
    dxl.setBaudrate(2, 4000000);
    dxl.torqueOff(2);
    dxl.setOperatingMode(2, 4);
    dxl.torqueOn(2);

    Serial.begin(115200);
    delay(10);
    setLED(255, 67, 51); // Orange LED, connecting to wifi
    WiFi.begin(ssid, pw);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    Serial.println(WiFi.localIP());
    server.begin();
    setLED(0, 255, 0); //Green LED, ready to connect
}

void loop(){
 WiFiClient client = server.available();   // listen for incoming clients
  if (client) {     
    resetMotorPos(leftMotor);
    resetMotorPos(rightMotor);
    setLED(0, 0, 128); //Blue LED, connected
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             
        uint8_t op = client.read();
        switch(op) {
          case 'G': // Go a relative distance
            while (client.available() == 0) {}
            moveType = client.read();
            client.readBytes(typeCast.byteArray, 4);
            movePosition = typeCast.floatArray[0];
            commandCue_CmdType[nCommands] = moveType;
            commandCue_Data[nCommands] = movePosition;
            nCommands++;
            client.write(1);
          break;
          case 'V': // Set velocity
            while (client.available() == 0) {}
            client.readBytes(typeCast.byteArray, 4);
            maxVelocity = typeCast.floatArray[0];
            commandCue_CmdType[nCommands] = 2;
            commandCue_Data[nCommands] = maxVelocity;
            nCommands++;
            client.write(1);
          break;
          case 'A': // Set acceleration
            while (client.available() == 0) {}
            client.readBytes(typeCast.byteArray, 4);
            acceleration = typeCast.floatArray[0];
            dxl.writeControlTableItem(PROFILE_ACCELERATION, leftMotor, acceleration);
            dxl.writeControlTableItem(PROFILE_ACCELERATION, rightMotor, acceleration);
            client.write(1);
          break;
          case 'W': // Wait
            while (client.available() == 0) {}
            client.readBytes(typeCast.byteArray, 4);
            pauseDuration = typeCast.floatArray[0];
            commandCue_CmdType[nCommands] = 3;
            commandCue_Data[nCommands] = pauseDuration;
            nCommands++;
            client.write(1);
          break;
          case 'X': // Stop
            dxl.torqueOff(leftMotor);
            dxl.torqueOff(rightMotor);
            resetMotorPos(leftMotor);
            resetMotorPos(rightMotor);
            nCommands = 0;
            commandRunning = false;
            client.write(1);
          break;
        }                              
      }
      runProgram();        
    }
    // close the connection:
    client.stop();
    setLED(0, 255, 0); //Green LED, ready to connect
  }
  runProgram();  
}

void runProgram() {
  if (nCommands > 0) {
    if (!commandRunning) {
      switch(commandCue_CmdType[0]) {
        case 0:
        case 1:
          startMove(commandCue_CmdType[0], commandCue_Data[0]);
        break;
        case 2:
          dxl.writeControlTableItem(PROFILE_VELOCITY, leftMotor, commandCue_Data[0]);
          dxl.writeControlTableItem(PROFILE_VELOCITY, rightMotor, commandCue_Data[0]);
        break;
        case 3:
          pauseDuration = commandCue_Data[0];
          pauseStartTime = millis();
        break;
      }
      commandRunning = true;
      activeCommandType = commandCue_CmdType[0];
      nCommands--;
      shiftCue(); 
    }
  }
  if (commandRunning) {
    switch (activeCommandType) {
      case 0:
      case 1:
        currentPositionL = dxl.getPresentPosition(leftMotor, UNIT_DEGREE);
        currentPositionR = dxl.getPresentPosition(rightMotor, UNIT_DEGREE);
        if ((abs(currentPositionL-goalPositionL) < 1) && (abs(currentPositionR-goalPositionR) < 1)) {
          delay(100); // Wait for motor to completely stop
          resetMotorPos(leftMotor);
          resetMotorPos(rightMotor);
          commandRunning = false;
        }
      break;
      case 2:
        commandRunning = false;
      break;
      case 3:
        if (millis() - pauseStartTime > pauseDuration) {
          commandRunning = false;
        }
      break;
    }
  }
}

void shiftCue() {
  for (int i = 0; i < nCommands; i++) {
    commandCue_CmdType[i] = commandCue_CmdType[i+1];
    commandCue_Data[i] = commandCue_Data[i+1];
  }
}

void resetMotorPos(uint8_t motorID) {
  dxl.torqueOff(motorID);
  dxl.clear(motorID, 0x01, 0, 1000);
  dxl.torqueOn(motorID);
}

void startMove(uint8_t mType, float mPosition) {
  currentPositionL = dxl.getPresentPosition(leftMotor, UNIT_DEGREE);
  currentPositionR = dxl.getPresentPosition(rightMotor, UNIT_DEGREE);
  goalPositionL = currentPositionL + mPosition;
  if (mType == 0) {
    goalPositionR = currentPositionR - mPosition;
  } else {
    goalPositionR = currentPositionR + mPosition;
  }
  dxl.setGoalPosition(leftMotor, goalPositionL, UNIT_DEGREE);
  dxl.setGoalPosition(rightMotor, goalPositionR, UNIT_DEGREE);
}

void setLED(uint8_t red, uint8_t green, uint8_t blue) {
  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();   // Send the updated pixel colors to the hardware.
}
