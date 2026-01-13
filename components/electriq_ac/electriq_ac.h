#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace electriq_ac {


class ElectriqAC : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  void setup() override;
//  void dump_config() override; // only used during debugging
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

 protected:
  void SendHeartbeat();
  void SendToMCU();
  void ReadMCU();
  void AcFanSpeed();
  void AcModes();
  void AcSwing();
  bool CheckIdle(uint8_t &a);

  uint8_t ac_mode_ = 0x03;
  uint8_t fan_speed_ = 0x90;
  uint8_t swing_ = 0;
  uint8_t target_temp_ = 0;
};

}  // namespace electriq_ac
}  // namespace esphome
