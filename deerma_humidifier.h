#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"

#include <queue>

using namespace esphome;
using namespace esphome::uart;
using namespace esphome::switch_;
using namespace esphome::binary_sensor;
using namespace esphome::sensor;
using namespace esphome::select;
using namespace esphome::number;


class DeermaHumidifier : public Component, public UARTDevice {
 public:
  explicit DeermaHumidifier(UARTComponent *uart) : UARTDevice(uart){};
  
  void setup() override { this->send_network_status(); }
  
  void loop() override { handleUart(); }
  
//Declare substitutions:
  //switches:
  Switch *power{};
  Switch *led{};
  Switch *tip_sound{};
  //sensors:
  Sensor *temperature_sensor{};
  Sensor *humidity_sensor{};
  //binary_sensors:
  BinarySensor *tank_empty{};
  BinarySensor *tank_installed{};
  //Mode Selection:
  Select *mode_select;
  //Humidity Setpoint:
  Number *humidity_setpoint{};
  
  void send_network_status() { 
   queueDownstreamMessage("MIIO_net_change cloud"); 
   //Dirty hack to obtain humidifier data
   queueDownstreamMessage("Set_OnOff 0");
  }

  static const int DOWNSTREAM_QUEUE_SIZE = 50;
  static const int DOWNSTREAM_QUEUE_ELEM_SIZE = 51;
  char downstreamQueue[DOWNSTREAM_QUEUE_SIZE][DOWNSTREAM_QUEUE_ELEM_SIZE];
  int downstreamQueueIndex = -1;
  char nextDownstreamMessage[DOWNSTREAM_QUEUE_ELEM_SIZE];
  boolean shouldUpdateState = false;
  char serialRxBuf[255];
  char serialTxBuf[255];
  uint8_t mqttRetryCounter = 0;

//Setting States of Humidifier
//Power status  
 void set_power_state(bool state) {
	if ( state == true ) {
		queueDownstreamMessage("Set_OnOff 1");
	} else {
		queueDownstreamMessage("Set_OnOff 0");
	}
 }
// LED status 
 void set_led_state(bool state) {
	if ( state == true ) {
		queueDownstreamMessage("SetLedState 1");
	} else {
		queueDownstreamMessage("SetLedState 0");
	}
 } 
//Tip Sound
 void set_sound_state(bool state) {
	if ( state == true ) {
		queueDownstreamMessage("SetTipSound_Status 1");
	} else {
		queueDownstreamMessage("SetTipSound_Status 0");
	}
 }
 //Process Humidifier Selection
  void set_humidifier_mode(const char *state) {
    if (strcmp(state, "Low") == 0) {
	queueDownstreamMessage("Set_HumidifierGears 1");
	} else if (strcmp(state, "Medium") == 0) {
	queueDownstreamMessage("Set_HumidifierGears 2");	
	} else if (strcmp(state, "High") == 0) {
	queueDownstreamMessage("Set_HumidifierGears 3");	
	} else if (strcmp(state, "Humidity") == 0) {
	queueDownstreamMessage("Set_HumidifierGears 4");
	}
  } 
 //Setting Humidity Value 
  void set_humidity_target(int state) {
    char humiditySetpointMsg[40];
    memset(humiditySetpointMsg, 0, sizeof(humiditySetpointMsg));
  
    if (state < 0) {
      state = 0;
    } else if (state > 99) {
      state = 99;
    }
  
  
    snprintf(humiditySetpointMsg, sizeof(humiditySetpointMsg), "Set_HumiValue %d", state);
  
    queueDownstreamMessage(humiditySetpointMsg);
  
    //This is required since we not always receive a prop update when setting this
    humidity_setpoint->publish_state((int)state);
  
  } 

// protected:
//  int rx_pos_{};
//  char rx_buf_[255]{};
//  std::queue<std::string> message_queue_;
  
// UART parser procedures
 void clearRxBuf() {
   //Clear everything for the next message
   memset(serialRxBuf, 0, sizeof(serialRxBuf));
 }
// UART parser procedures
 void clearTxBuf() {
   //Clear everything for the next message
   memset(serialTxBuf, 0, sizeof(serialTxBuf));
 }
// UART parser procedures 
 void clearDownstreamQueueAtIndex(int index) {
   memset(downstreamQueue[index], 0, sizeof(downstreamQueue[index]));
 }
// UART parser procedures 
 void queueDownstreamMessage(char *message) {
   if (downstreamQueueIndex >= DOWNSTREAM_QUEUE_SIZE - 1) {
     //Serial.print("Error: Queue is full. Dropping message:");
     //Serial.println(message);
 
     return;
   } else {
     downstreamQueueIndex++;
 
     snprintf(downstreamQueue[downstreamQueueIndex], sizeof(downstreamQueue[downstreamQueueIndex]), "%s", message);
   }
 }
// UART parser procedures 
 void fillNextDownstreamMessage() {
   memset(nextDownstreamMessage, 0, sizeof(nextDownstreamMessage));
 
   if (downstreamQueueIndex < 0) {
     snprintf(nextDownstreamMessage, DOWNSTREAM_QUEUE_ELEM_SIZE, "none");
 
   } else if (downstreamQueueIndex == 0) {
     snprintf(nextDownstreamMessage, DOWNSTREAM_QUEUE_ELEM_SIZE, downstreamQueue[0]);
     clearDownstreamQueueAtIndex(0);
     downstreamQueueIndex--;
 
   } else {
     /**
        This could be solved in a better way using less cycles, however, the queue should usually be mostly empty so this shouldn't matter much
     */
 
     snprintf(nextDownstreamMessage, DOWNSTREAM_QUEUE_ELEM_SIZE, downstreamQueue[0]);
 
     for (int i = 0; i < downstreamQueueIndex; i++) {
       snprintf(downstreamQueue[i], DOWNSTREAM_QUEUE_ELEM_SIZE, downstreamQueue[i + 1]);
     }
 
     clearDownstreamQueueAtIndex(downstreamQueueIndex);
     downstreamQueueIndex--;
   }
 }
 
//UART parser from humidifier MCU
 void handleUart() {
   if (Serial.available()) {
     Serial.readBytesUntil('\r', serialRxBuf, 250);
 
     char propName[30]; //30 chars for good measure
     int propValue;
 
     int sscanfResultCount;
 
 
     sscanfResultCount = sscanf(serialRxBuf, "props %s %d", propName, &propValue);
 
     if (sscanfResultCount == 2) {
       shouldUpdateState = true;
 
       if (strcmp(propName, "OnOff_State") == 0) {
 		//ESP_LOGD("setting", "OnOff_State %u", (boolean)propValue);
		process_onoff_state_((boolean)propValue);
        //ON-OFF state processing
       } else if (strcmp(propName, "Humidifier_Gear") == 0) {
 		//ESP_LOGD("processing", "Humi_Gear %u", (int)propValue);
		process_humidifier_gear_((int)propValue);
         //Processing humidifier selector
       } else if (strcmp(propName, "HumiSet_Value") == 0) {
 		ESP_LOGD("logging", "HumiSet_Value %u", (int)propValue);
		humidity_setpoint->publish_state((int)propValue);
         //Humidifier Setpoint
       } else if (strcmp(propName, "Humidity_Value") == 0) {
 		//ESP_LOGD("setting", "Humidity_Value %u", (int)propValue);
		humidity_sensor->publish_state((int)propValue);
         //Humidity Value processing
       } else if (strcmp(propName, "TemperatureValue") == 0) {
 		//ESP_LOGD("setting", "TemperatureValue %u", (int)propValue);
		temperature_sensor->publish_state((int)propValue);
         //Temperature value processing
       } else if (strcmp(propName, "TipSound_State") == 0) {
 		//ESP_LOGD("setting", "TipSound_State %u", (boolean)propValue);
		process_sound_state_((boolean)propValue);
         //Tip sound processing
       } else if (strcmp(propName, "Led_State") == 0) {
 		//ESP_LOGD("setting", "Led_State %u", (boolean)propValue);
		process_led_enabled_((boolean)propValue);
         //Led status processing
       } else if (strcmp(propName, "watertankstatus") == 0) {
 		//ESP_LOGD("setting", "watertankstatus %u", (boolean)propValue);
		tank_installed->publish_state((boolean)propValue);
         //Water tank installed
       } else if (strcmp(propName, "waterstatus") == 0) {
 		//ESP_LOGD("setting", "waterstatus %u", !(boolean)propValue);
		tank_empty->publish_state(!(boolean)propValue);
         //Water tank empty
       } else {
         //Serial.print("Received unhandled prop: ");
         //Serial.println(serialRxBuf);
       }
 
       clearRxBuf();
       Serial.print("ok\r");
 
     } else {
       if (shouldUpdateState == true) { //This prevents a spam wave of state updates since we only send them after all prop updates have been received
 	   ESP_LOGD("logging", "publishState()");
         //publishState();
       }
 
       shouldUpdateState = false;
 
 
       if (strncmp (serialRxBuf, "get_down", 8) == 0) {
         fillNextDownstreamMessage();
         snprintf(serialTxBuf, sizeof(serialTxBuf), "down %s\r", nextDownstreamMessage);
         Serial.print(serialTxBuf);
 
 
         clearTxBuf();
 
       } else if (strncmp(serialRxBuf, "net", 3) == 0) {
         Serial.print("cloud\r");
         /**
            We need to always respond with cloud because otherwise the connection to the humidifier will break for some reason
            Not sure why but it doesn't really matter
 
           if (networkConnected == true) {
 
           } else {
 
           humidifierSerial.print("uap\r");
           } **/
 
       } else if (strncmp(serialRxBuf, "restore", 7) == 0) {
		 ESP_LOGD("logging", "restore");
         //resetWifiSettingsAndReboot();
 
       } else if (
         strncmp(serialRxBuf, "mcu_version", 11) == 0 ||
         strncmp(serialRxBuf, "model", 5) == 0
       ) {
         Serial.print("ok\r");
       } else if (strncmp(serialRxBuf, "event", 5) == 0) {
         //Intentionally left blank
         //We don't need to handle events, since we already get the prop updates
       } else if (strncmp(serialRxBuf, "result", 6) == 0) {
         //Serial.println(serialRxBuf);
       } else {
         //Serial.print("Received unhandled message: ");
         //Serial.println(serialRxBuf);
       }
     }
 
 
     clearRxBuf();
   }
 }
// Process States
// Power Status:
  void process_onoff_state_(bool state) {
    if (this->power) {
      this->power->publish_state(state);
    }
  }
//LED status:
  void process_led_enabled_(bool state) {
    if (this->led) {
      this->led->publish_state(state);
    }
  }
//Buzzer status:
  void process_sound_state_(bool state) {
    if (this->tip_sound) {
      this->tip_sound->publish_state(state);
    }
  }
//Target humidifier mode  
  void process_humidifier_gear_(int value) {
	switch (value)
    {
    case 1:
      mode_select->publish_state("Low");
	  break;
	case 2:
      mode_select->publish_state("Medium");
	  break;
	case 3:
      mode_select->publish_state("High");
	  break;	
	case 4:
      mode_select->publish_state("Humidity");
	  break;
    }
  }  
};
 
DeermaHumidifier &cast(Component *c) { return *reinterpret_cast<DeermaHumidifier *>(c); }
  