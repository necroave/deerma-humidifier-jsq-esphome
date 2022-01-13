#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"

#include <queue>

using namespace esphome;
using namespace esphome::uart;
using namespace esphome::switch_;
using namespace esphome::binary_sensor;
using namespace esphome::sensor;
using namespace esphome::select;
using namespace esphome::number;

#define MSG_PROPS "props "
#define MSG_GET_DOWN "get_down"
#define MSG_NET "net"
#define MSG_RESTORE "restore"
#define MSG_MCU_VERSION "mcu_version"
#define MSG_MODEL "model"
#define MSG_EVENT "event"
#define MSG_RESULT "result"
//switches
#define PROP_ONOFF_STATE "OnOff_State "
#define PROP_LED_ENABLED "Led_State "
#define PROP_SOUND_ENABLED "TipSound_State "
//state mode
#define PROP_HUMIDIFIER_GEAR "Humidifier_Gear "
//Binary sensors:
#define PROP_WATER_TANK_EMPTY "waterstatus "
#define PROP_WATER_TANK_INSTALLED "watertankstatus "
//Sensors:
#define PROP_CURRENT_TEMPERATURE "TemperatureValue " 
#define PROP_CURRENT_HUMIDITY "Humidity_Value "
//humidity set value
#define PROP_HUM_TARGET "HumiSet_Value "


class DeermaHumidifier : public Component, public UARTDevice {
 public:
  explicit DeermaHumidifier(UARTComponent *uart) : UARTDevice(uart){};

  void setup() override { this->send_network_status(); }

  void loop() override {
    while (this->available()) {
      uint8_t ch;
      // читаем из uart побайтово
      if (this->read_byte(&ch)) {
        // проверяем достигнут ли конец строки '\r'
        if (ch == '\r') {
          this->rx_buf_[this->rx_pos_] = 0;
          // переходим к обработке
          this->process_();
          // сбрасываем счетчик байтов входящего буфера
          this->rx_pos_ = 0;
        } else {
          // дописываем прочитаный баййт во входящий буфер и увеличиваем счетчик
          this->rx_buf_[this->rx_pos_++] = ch;
        }
      }
    }
  }

  Switch *power{};
  Switch *led{};
  Switch *buzzer{};
  BinarySensor *tank_empty{};
  BinarySensor *tank_installed{};
  Sensor *temperature_sensor{};
  Sensor *humidity_sensor{};
  Select *mode_select;
  Number *humidity_setpoint{};
  
  void send_network_status() { this->message_queue_.push("MIIO_net_change cloud"); }
//Setting power state
  void set_power_state(bool state) {
	  this->message_queue_.push(str_sprintf("Set_OnOff %u", state ? 1 : 0));
      ESP_LOGD("logging", "Set_OnOff %u", state ? 1 : 0); 
  }
//Setting Led state  
  void set_led_state(bool state) { 
       this->message_queue_.push(str_sprintf("SetLedState %u", state ? 1 : 0)); 
       ESP_LOGD("logging", "SetLedState %u", state ? 1 : 0);
  }
//Setting sound state
  void set_sound_state(bool state) {
  	  this->message_queue_.push(str_sprintf("SetTipSound_Status %u", state ? 1 : 0)); 
	  ESP_LOGD("logging", "SetTipSound_Status %u", state ? 1 : 0);
  }
//Humidifier mode
  void set_humidity_setpoint(int  state) {
	  this->message_queue_.push(str_sprintf("Set_HumidifierGears %u", state)); 
	  ESP_LOGD("logging", "Set_HumidifierGears %u", state);
  }
//test
  void set_humidifier_mode(const char *state) {
    if (strcmp(state, "low") == 0) {
	message_queue_.push(str_sprintf("Set_HumidifierGears 1"));
	ESP_LOGD("logging", "Set_HumidifierGears 1");
	} else if (strcmp(state, "medium") == 0) {
	message_queue_.push(str_sprintf("Set_HumidifierGears 2"));
	ESP_LOGD("logging", "Set_HumidifierGears 2");	
	} else if (strcmp(state, "high") == 0) {
	message_queue_.push(str_sprintf("Set_HumidifierGears 3"));
	ESP_LOGD("logging", "Set_HumidifierGears 3");	
	} else if (strcmp(state, "Humidity") == 0) {
	message_queue_.push(str_sprintf("Set_HumidifierGears 4"));
	ESP_LOGD("logging", "Set_HumidifierGears 4");	
	}
  }  
//terget humidity
  void set_humidity_target(int  state) {
	  this->message_queue_.push(str_sprintf("Set_HumiValue %u", state)); 
	  ESP_LOGD("logging", "Set_HumiValue %u", state);
  }


 protected:
  // буфер для входящих данных
  char rx_buf_[255]{};
  // счетчик входящих байт
  int rx_pos_{};
  // очередь сообщений
  std::queue<std::string> message_queue_;

  // обработка входящего буфера
  void process_() {
    if (std::strncmp(this->rx_buf_, MSG_PROPS, sizeof(MSG_PROPS) - 1) == 0) {
      this->process_props_(this->rx_buf_ + sizeof(MSG_PROPS) - 1);
    } else if (std::strncmp(this->rx_buf_, MSG_GET_DOWN, sizeof(MSG_GET_DOWN) - 1) == 0) {
      this->process_down_();
    } else if (std::strncmp(this->rx_buf_, MSG_NET, sizeof(MSG_NET) - 1) == 0) {
      this->process_net_();
    } else if (std::strncmp(this->rx_buf_, MSG_RESTORE, sizeof(MSG_RESTORE) - 1) == 0) {
      this->process_restore_();
    } else if (std::strncmp(this->rx_buf_, MSG_MCU_VERSION, sizeof(MSG_MCU_VERSION) - 1) == 0) {
      this->process_version_();
    } else if (std::strncmp(this->rx_buf_, MSG_MODEL, sizeof(MSG_MODEL) - 1) == 0) {
      this->process_model_();
    } else if (std::strncmp(this->rx_buf_, MSG_EVENT, sizeof(MSG_EVENT) - 1) == 0) {
      this->process_event_();
    } else if (std::strncmp(this->rx_buf_, MSG_RESULT, sizeof(MSG_RESULT) - 1) == 0) {
      this->process_result_();
    } else {
      this->process_unknown_();
    }
  }

  void process_props_(const char *props) {
    if (std::strncmp(props, PROP_ONOFF_STATE, sizeof(PROP_ONOFF_STATE) - 1) == 0) {
      this->process_onoff_state_(atoi(props + sizeof(PROP_ONOFF_STATE) - 1) != 0);
    } else if (std::strncmp(props, PROP_HUMIDIFIER_GEAR, sizeof(PROP_HUMIDIFIER_GEAR) - 1) == 0) {
      this->process_humidifier_gear_(atoi(props + sizeof(PROP_HUMIDIFIER_GEAR) - 1));
    } else if (std::strncmp(props, PROP_WATER_TANK_EMPTY, sizeof(PROP_WATER_TANK_EMPTY) -1) == 0) {
	  this->process_water_tank_empty_(atoi(props + sizeof(PROP_WATER_TANK_EMPTY) - 1));
    } else if (std::strncmp(props, PROP_WATER_TANK_INSTALLED, sizeof(PROP_WATER_TANK_INSTALLED) -1) == 0) {
	  this->process_water_tank_status_(atoi(props + sizeof(PROP_WATER_TANK_INSTALLED) - 1));
    } else if (std::strncmp(props, PROP_LED_ENABLED, sizeof(PROP_LED_ENABLED) - 1) == 0) {
      this->process_led_enabled_(atoi(props + sizeof(PROP_LED_ENABLED) - 1) == 0);
    } else if (std::strncmp(props, PROP_SOUND_ENABLED, sizeof(PROP_SOUND_ENABLED) - 1) == 0) {
      this->process_sound_enabled_(atoi(props + sizeof(PROP_SOUND_ENABLED) - 1) == 0);
    } else if (std::strncmp(props, PROP_CURRENT_TEMPERATURE, sizeof(PROP_CURRENT_TEMPERATURE) - 1) == 0) {
      this->process_current_temperature_(atoi(props + sizeof(PROP_CURRENT_TEMPERATURE) - 1) == 0);
    } else if (std::strncmp(props, PROP_CURRENT_HUMIDITY, sizeof(PROP_CURRENT_HUMIDITY) - 1) == 0) {
      this->process_current_humidity_(atoi(props + sizeof(PROP_CURRENT_HUMIDITY) - 1) == 0);
    } else if (std::strncmp(props, PROP_HUM_TARGET, sizeof(PROP_HUM_TARGET) - 1) == 0) {
      this->process_set_humidity_(atoi(props + sizeof(PROP_HUM_TARGET) - 1) == 0);
    }
	
    // здесь продолжаем обрабатывать остальные значения PROP_

    this->write_str("ok\r");
    this->flush();
  }

  void process_down_() {
    this->write_str("down ");
    if (this->message_queue_.empty()) {
      this->write_str("none");
    } else {
      this->write_str(this->message_queue_.front().c_str());
      this->message_queue_.pop();
    }
    this->write_str("\r");
    this->flush();
  }

  void process_net_() {
    this->write_str("cloud\r");
    this->flush();
  }

  void process_version_() {
    this->write_str("ok\r");
    this->flush();
  }

  void process_model_() {
    this->write_str("ok\r");
    this->flush();
  }

  void process_restore_() {}

  void process_event_() {}

  void process_result_() {}

  void process_unknown_() {
    // здесь обрабатываем непредвиденное сообщение в буфере
    // например, можно залогировать
  }
//switches:
  void process_onoff_state_(bool state) {
    if (this->power) {
      this->power->publish_state(state);
    }
  }
  void process_led_enabled_(bool state) {
    if (this->led) {
      this->led->publish_state(state);
    }
  }
  void process_sound_enabled_(bool state) {
    if (this->buzzer) {
      this->buzzer->publish_state(state);
    }
  }  
 //Binary sensors:
  void process_water_tank_empty_(bool state){
	if (this->tank_empty) {
	  this->tank_empty->publish_state(state);
	}
  }
  void process_water_tank_status_(bool state){
	if (this->tank_installed) {
	  this->tank_installed->publish_state(state);
	}
  }  
 //Sensors:
  void process_current_temperature_(int value){
	if (this->temperature_sensor) {
	  this->temperature_sensor->publish_state(value);
	}
  } 
  void process_current_humidity_(int value){
	if (this->humidity_sensor) {
	  this->humidity_sensor->publish_state(value);
	}
  }  
 //Target mode
  void process_humidifier_gear_(int value) {
	switch (value)
    {
    case 1:
      mode_select->publish_state("low");
	  ESP_LOGD("logging", "Set_HumiValue %d", value);
      break;
	case 2:
      mode_select->publish_state("medium");
      break;
	case 3:
      mode_select->publish_state("high");
      break;	
	case 4:
      mode_select->publish_state("Humidity");
      break;
	default:
	  mode_select->publish_state("low");
	  break;
    }
  }
 //Humidity target value
   void process_set_humidity_(int  value){
	if (this->humidity_setpoint) {
	  this->humidity_setpoint->publish_state(value);
	}
  }
};
 
DeermaHumidifier &cast(Component *c) { return *reinterpret_cast<DeermaHumidifier *>(c); }
  