/*
  An EV1527 gateway for use with domoticz and other home automation systems that use the RF Link serial protocol
  Copyright (c) 2020 Daren Ford.  All rights reserved.
    
  Last update: 23/12/2020
  Version: 1.1
  
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

RCSwitch rcs = RCSwitch();
Bounce debouncer = Bounce();

//version definition
#define VERSION "EV1527 Gateway V1.1"

//input/output pin definitions
#define RX_IRQ 0 // Interrupt number (IRQ 0 = D2) for the 433MHz receiver data pin
#define TX_PIN 10 //433MHz ASK transmitter module (e.g. FS1000A) data pin
#define TX_LED_PIN 4
#define RX_LED_PIN 3
#define INPUT_PIN 6 //digital input - pull down to activate

//communication definitions
#define TX_PULSE_LENGTH 300 //in milliseconds
#define TX_REPEATS 15
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
    txCommand.toUpperCase();
    ParseTxCommand(txCommand);
  }

  if (rcs.available()) {
    if ((rcs.getReceivedProtocol() == 1) && (rcs.getReceivedBitlength() == 24)) {
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
        String message = ParseRxCommand(rxCommandStr);
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
      EchoCommand("ID=999FF", "SWITCH=01", "CMD=ON");
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

String ParseRxCommand(String rxCommandStr)
{
  String toRet = "EV1527;";

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
      } else if (cmdProtocolStr == "EV1527") {
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdDevice, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdUnit, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdAction, strtokIdx);
        if (SendCommand(cmdDevice, cmdUnit, cmdAction)) {
          RxOk();
          return;
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
      if ((cmdSubCode == 20) && (cmdProtocolStr == "EV1527")) {
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdDevice, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdUnit, strtokIdx);
        strtokIdx = strtok(NULL, ";");
        strcpy(cmdAction, strtokIdx);
        RxOk();
        EchoCommand(cmdDevice, cmdUnit, cmdAction);
        return;
      }      
      break;
    }
  } 

  RxCmdUnknown();
}

void EchoCommand(String cmdDevice, String cmdUnit, String cmdAction)
{
  SendMessage("EV1527;" + cmdDevice + ";" + cmdUnit + ";" + cmdAction);
}

bool SendCommand(String cmdDevice, String cmdUnit, String cmdAction)
{
  if (cmdAction == "ON") {
    if (cmdUnit.startsWith("0") && (cmdUnit.length() == 2)) {
          cmdUnit = cmdUnit.substring(1, 2);
    }
    
    long code = StrToHex(cmdDevice + cmdUnit);
    TxLED(HIGH);
    rcs.send(code, 24);
    TxLED(LOW);
    return true;
  }

  return false;
}

long StrToHex(String str)
{
  return strtol(str.c_str(), 0, 16);
}
