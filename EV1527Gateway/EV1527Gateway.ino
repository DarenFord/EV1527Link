/*
  An EV1527 gateway for use with domoticz and other home automation systems that use the RF Link serial protocol
  Copyright (c) 2020 Daren Ford.  All rights reserved.
    
  Last update: 26/09/2021
  Version: 2.0

  Changes: Added support for Byron protocol (doorbells) - watch out, they use alternating codes (so, two devices will be created)

  Example send command string: 10;Byron;295FD59;03;ON;

  Send commands:
  10;Byron;295FD59;3;ON;

  Loopback commands:
  11;20;45;EV1527;ID=295FD59;SWITCH=3;CMD=ON;
  11;20;45;Byron;ID=295FD59;SWITCH=3;CMD=ON;

  Loopback commands in Domoticz:
  20;45;EV1527;ID=295FD59;SWITCH=3;CMD=ON;
  20;45;Byron;ID=295FD59;SWITCH=3;CMD=ON;

  Project home: https://github.com/DarenFord/EV1527Link
  
  This is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  
*/

#include <RCSwitch.h>
#include <Bounce2.h>

//Format for protocol definitions:
//{pulselength, Sync bit, "0" bit, "1" bit}
RCSwitch::Protocol EV1527Protocol = {250, {6, 31}, {1, 3}, {3, 1}, false}; //custom protocol (not in RCSwitch.cpp)
RCSwitch::Protocol ByronProtocol =  {500, {1, 14}, {1, 3}, {3, 1}, false}; //custom protocol (not in RCSwitch.cpp)

RCSwitch rcs = RCSwitch();
String ByronProtocolName = "Byron";
String EV1527ProtocolName = "EV1527";

Bounce debouncer = Bounce();

//version definition
#define VERSION "EV1527 Gateway V2.1"

//input/output pin definitions
#define RX_IRQ 0 // Interrupt number (IRQ 0 = D2) for the 433MHz receiver data pin
#define TX_PIN 10 //433MHz ASK transmitter module (e.g. FS1000A) data pin
#define TX_LED_PIN 4
#define RX_LED_PIN 3
#define INPUT_PIN 6 //digital input - pull down to activate

//communication definitions
#define TX_PULSE_LENGTH 300 //in milliseconds
#define TX_REPEATS 15
#define TX_REPEATS_LEARNING_MODE 50
#define RX_REPEATS 3
#define RX_GUARD_TIME 1000 //in milliseconds
#define RX_LED_DURATION 1000 //in milliseconds

//global variables
int rxCounter = 0;
long lastRxCommand = 0;
int numRxCommands = 0;
uint32_t lastRxCommandReceivedTime = 0;
uint32_t rxLedOnTime = 0;
int lastInputValue = HIGH;

void setup() {
  Serial.begin(57600);
  
  pinMode(TX_LED_PIN, OUTPUT);
  pinMode(RX_LED_PIN, OUTPUT);
  RxLED(LOW);
  TxLED(LOW);

  pinMode(INPUT_PIN, INPUT_PULLUP);
  debouncer.attach(INPUT_PIN);
  debouncer.interval(5);
  
  rcs.enableReceive(RX_IRQ);  
  rcs.enableTransmit(TX_PIN);
  rcs.setPulseLength(TX_PULSE_LENGTH);
  rcs.setRepeatTransmit(TX_REPEATS);  
  SendMessage(VERSION);
}

void loop() {
  if (Serial.available() > 0) {
    String txCommand = Serial.readString();
    ParseTxCommand(txCommand);
  }

  if (rcs.available()) {
    String ProtocolName = "";
    int ProtocolNum = rcs.getReceivedProtocol();
    int BitLength = rcs.getReceivedBitlength();
    int PulseLength = rcs.getReceivedDelay();
    
    if ((ProtocolNum == 1) && (BitLength == 24) && (PulseLength > 230) && (PulseLength < 330)) {
      ProtocolName = EV1527ProtocolName;
    } else if ((ProtocolNum == 2) && (BitLength == 32) && (PulseLength > 680) && (PulseLength < 780)) { //RCSwitch actually reads the pulse length incorrectly ~707mS
      ProtocolName = ByronProtocolName;
    }
    
    if (ProtocolName.length() != 0) {
      long rxCommand = rcs.getReceivedValue();
      uint32_t rxCommandReceivedTime = millis();

      if (rxCommandReceivedTime - lastRxCommandReceivedTime > RX_GUARD_TIME) {
        numRxCommands = 0;
      }
      
      if ((rxCommand == lastRxCommand)  || (lastRxCommand == 0)) {
        numRxCommands++;        
      } else {
        numRxCommands = 0;
      }
      
      if (numRxCommands == RX_REPEATS)
      {
        String rxCommandStr = String(rxCommand, HEX);
        rxCommandStr.toUpperCase();
        String message = ParseRxCommand(rxCommandStr, ProtocolName);
        RxLED(HIGH);
        SendMessage(message);
      }

      lastRxCommand = rxCommand;
      lastRxCommandReceivedTime = rxCommandReceivedTime;
    }
    
    rcs.resetAvailable();    
  }

  if ((rxLedOnTime > 0) && ((millis() - rxLedOnTime) > RX_LED_DURATION)) {
    RxLED(LOW);
  }

  debouncer.update();  
  int value = debouncer.read();  
  if ((value != lastInputValue) && (value == LOW)) {
      EchoCommand(EV1527ProtocolName, "ID=999FF", "SWITCH=01", "CMD=ON");
  }
  lastInputValue = value;
}

void RxOk()
{
  SendMessage("OK");
}

void RxCmdUnknown()
{
  SendMessage("CMD UNKNOWN");
}

void SendMessage(String message)
{  
  String count = String(rxCounter, HEX);
  if (count.length() == 1) {
    count = "0" + count;
  }
  count.toUpperCase();
  
  Serial.print("20;");
  Serial.print(count);
  Serial.print(";");
  Serial.print(message);
  Serial.println(";");

  rxCounter++;
  if (rxCounter > 255) {
    rxCounter = 0;
  }
}

void RxLED(bool state)
{
  if (state == HIGH) {
    rxLedOnTime = millis();
    digitalWrite(RX_LED_PIN, HIGH);
  } else {
    rxLedOnTime = 0;
    digitalWrite(RX_LED_PIN, LOW);
  }
}

void TxLED(bool state)
{
  if (state == HIGH) {
    digitalWrite(TX_LED_PIN, HIGH);
  } else {
    digitalWrite(TX_LED_PIN, LOW);
  }
}

String ParseRxCommand(String rxCommandStr, String ProtocolName)
{
  String toRet = ProtocolName;
  toRet += ";";

  int i = rxCommandStr.length();
  if (i > 0) {
    char device = rxCommandStr[i - 1];
    toRet += "ID=" + rxCommandStr.substring(0, i - 1);
    toRet += ";SWITCH=0";
    toRet += String(device);
    toRet += ";CMD=ON";
  }

  return toRet;
}

void ParseTxCommand(String txCommand) 
{
  //debug code
  //Serial.print("->->");
  //Serial.println(txCommand);

  char cmdProtocol[32] = {0};
  char cmdDevice[16] = {0};
  char cmdUnit[16] = {0};
  char cmdAction[16] = {0};

  char * strtokIdx;
  strtokIdx = strtok(txCommand.c_str(), ";");
  int cmdCode = atoi(strtokIdx);

  switch (cmdCode) {
    case 10: { //transmit code
      strtokIdx = strtok(NULL, ";");
      strcpy(cmdProtocol, strtokIdx);
      String cmdProtocolStr = cmdProtocol;
      if (cmdProtocolStr == "PING") {
        SendMessage("PONG");
        return;          
      } else if (cmdProtocolStr == "VERSION") {
        SendMessage(VERSION);
        return;          
      } else if ((cmdProtocolStr == EV1527ProtocolName) || (cmdProtocolStr == ByronProtocolName)) {
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdDevice, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdUnit, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdAction, strtokIdx);

        long device = HexToLong((String)cmdDevice);
        long unit = HexToLong((String)cmdUnit);
        if ((device == 0) && (unit == 1)) {
          RxOk();
          ToggleLearningMode(cmdAction);          
          return;          
        } else {
          if (SendCommand(cmdProtocolStr, cmdDevice, cmdUnit, cmdAction)) {
            RxOk();
            return;
          }                  
        }
      }      
      break;
    }
    case 11: { //loopback code
      strtokIdx = strtok(NULL, ";");
      int cmdSubCode = atoi(strtokIdx);
      strtokIdx = strtok(NULL, ";"); //seq nr      
      strtokIdx = strtok(NULL, ";");
      strcpy(cmdProtocol, strtokIdx);
      String cmdProtocolStr = cmdProtocol;
      if ((cmdSubCode == 20) && ((cmdProtocolStr == EV1527ProtocolName) || (cmdProtocolStr == ByronProtocolName))) {
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdDevice, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdUnit, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdAction, strtokIdx);
        RxOk();
        EchoCommand(cmdProtocolStr, cmdDevice, cmdUnit, cmdAction);
        return;
      }      
      break;
    }
  } 

  RxCmdUnknown();
}

void EchoCommand(String ProtocolName, String cmdDevice, String cmdUnit, String cmdAction)
{
  SendMessage(ProtocolName + ";" + cmdDevice + ";" + cmdUnit + ";" + cmdAction);
}

bool SendCommand(String ProtocolName, String cmdDevice, String cmdUnit, String cmdAction)
{
  cmdAction.toUpperCase();
  if (cmdAction == "ON") {
    if (cmdUnit.startsWith("0") && (cmdUnit.length() == 2)) {
          cmdUnit = cmdUnit.substring(1, 2);
    }
    
    unsigned long code = HexToLong(cmdDevice + cmdUnit);
    TxLED(HIGH);

    if (ProtocolName == EV1527ProtocolName) {
        rcs.setProtocol(EV1527Protocol);
        rcs.send(code, 24);
    } else if (ProtocolName == ByronProtocolName) {
        rcs.setProtocol(ByronProtocol);
        rcs.send(code, 32);
    }
    
    TxLED(LOW);
    return true;
  }

  return false;
}

void ToggleLearningMode(String cmdAction)
{
  if (cmdAction == "ON") {
    rcs.setRepeatTransmit(TX_REPEATS_LEARNING_MODE);
  } else {
    rcs.setRepeatTransmit(TX_REPEATS);
  }
}

unsigned long HexToLong(String str)
{
  unsigned long toRet = 0;
  int len = str.length();
  for (int i = 0; i < len; i++)
  {
    String k = (String)str[i];
    int j = strtol(k.c_str(), 0, 16);
    toRet = toRet * 16;
    toRet += j;
  }
  return toRet;
}
