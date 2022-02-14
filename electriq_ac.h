#include "esphome.h"
#include "esphome/core/helpers.h"

static const char *const TAG = "electriq_ac";

uint8_t ac_mode = 0x03;
uint8_t fan_speed = 0x90;
uint8_t swing = 0x00;
uint8_t target_temp = 0x19;

class ElectriqAC : public Component, public UARTDevice, public Climate
{
public:
  ElectriqAC(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override
  {
    this->set_interval("heartbeat", 1600, [this] { SendHeartbeat(); });
  }

  // calculate checksum and write out the serial message
  void SendToMCU()
  {
    uint8_t tuyacmd;
    tuyacmd = (ac_mode + fan_speed);
    uint8_t checksum = (0xAA + 0x03 + tuyacmd + swing + target_temp + 0x0B);
    write_array({0xAA, 0x03, tuyacmd, swing, target_temp, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, checksum});
    // we wrote something, so ensure it's published back to HA too
    this->publish_state();
  }

  // send regular heartbeat and check for any response
  // any response we read here is likely to be from the previous heartbeat. Not a huge deal to wait 1.5 seconds
  void SendHeartbeat()
  {
    write_array({0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAC});
    ReadMCU();
  }

  // Select command nibble for fan speed
  void AcFanSpeed()
  {
    switch (this->fan_mode.value())
    {
    case climate::CLIMATE_FAN_LOW:
    default:
      fan_speed = 0x90;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = 0xB0;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = 0xD0;
      break;
    }
  }

  // Select command nibble for mode
  void AcModes()
  {
    switch (this->mode)
    {
    case climate::CLIMATE_MODE_COOL:
      ac_mode = 0x01;
      break;
    case climate::CLIMATE_MODE_DRY:
      ac_mode = 0x02;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      ac_mode = 0x03;
      break;
    case climate::CLIMATE_MODE_HEAT:
      ac_mode = 0x04;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      fan_speed = 0x10;
      break;
    }
  }

  // Select command nibble for swing
  void AcSwing()
  {
    switch (this->swing_mode)
    {
    case climate::CLIMATE_SWING_OFF:
    default:
      swing = 0x00;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      swing = 0x0C;
      break;
    }
  }

  // read and parse messages from MCU serial
  void ReadMCU()
  {
    uint8_t c;
    uint8_t b[16];
    // find header byte, read further 16 bytes into array
    while (this->available())
    {
      read_byte(&c);
      if (c == 0xAA)
      {
        read_array(b, 16);
        // ESP_LOGD(TAG, "ReadMCU Mode: %s Swing: %s Temp1: %s Action: %s", format_hex_pretty(b[1]).c_str(), format_hex_pretty(b[2]).c_str(), format_hex_pretty(b[7]).c_str(), format_hex_pretty(b[11]).c_str());
        // if any more bytes available in the serial buffer, read each one to clear them out
        while (this->available())
        {
          read_byte(&c);
        }
        // Simple bitwise AND ops to get fan, mode, swing and action nibbles
        uint8_t f = (b[1] & 0xF0);
        uint8_t m = (b[1] & 0x0F);
        uint8_t s = (b[2] & 0x0F);
        uint8_t a = (b[11] & 0x0F);
        static uint8_t lcs;
        // if action is non-zero (off/idle) update action/mode
        if (a != 0x00)
        {
          switch (m)
          {
          case 0x01:
          default:
            this->action = climate::CLIMATE_ACTION_COOLING;
            this->mode = climate::CLIMATE_MODE_COOL;
            AcModes();
            ESP_LOGD(TAG, "Action: Cooling");
            break;
          case 0x02:
            this->action = climate::CLIMATE_ACTION_DRYING;
            this->mode = climate::CLIMATE_MODE_DRY;
            AcModes();
            ESP_LOGD(TAG, "Action: Drying");
            break;
          case 0x03:
            this->action = climate::CLIMATE_ACTION_FAN;
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            AcModes();
            ESP_LOGD(TAG, "Action: Fan");
            break;
          case 0x04:
            this->action = climate::CLIMATE_ACTION_HEATING;
            this->mode = climate::CLIMATE_MODE_HEAT;
            AcModes();
            ESP_LOGD(TAG, "Action: Heating");
            break;
          }
        }
        else
        {
          // check fan status for idle
          if (f != 0x10)
          {
            this->action = climate::CLIMATE_ACTION_IDLE;
            ESP_LOGD(TAG, "Action: Idling");
          }
          else
          {
            // we have to be in standby to reach here
            this->action = climate::CLIMATE_ACTION_OFF;
            this->mode = climate::CLIMATE_MODE_OFF;
            AcModes();
            ESP_LOGD(TAG, "Action: Standby");
          }
        }
        // update fan speed
        switch (f)
        {
        case 0x90:
        default:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          ESP_LOGD(TAG, "Fan: low");
          break;
        case 0xB0:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          ESP_LOGD(TAG, "Fan: medium");
          break;
        case 0xD0:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          ESP_LOGD(TAG, "Fan: high");
          break;
        }
        // update swing
        if (s == 0x0C)
        {
          this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
          ESP_LOGD(TAG, "Swing: on");
        }
        else
        {
          this->swing_mode = climate::CLIMATE_SWING_OFF;
          ESP_LOGD(TAG, "Swing: off");
        }

        this->current_temperature = b[7];
        this->target_temperature = b[3];

        // publish_state on change only
        // IMPROVE - sometimes two bytes can change with no difference in sum. Find better way.
        uint8_t cs = (b[1] + b[2] + b[3] + b[7] + b[11]);
        if (lcs != cs)
        {
          lcs = cs;
          this->publish_state();
        }
      }
    }
  }

  void control(const ClimateCall &call) override
  {
    if (call.get_mode().has_value())
    {
      ClimateMode mode = *call.get_mode();
      this->mode = mode;
      AcModes();
      // if the mode isn't standby (denoted by 0x10 fan speed), set the fan speed
      if (this->mode != climate::CLIMATE_MODE_OFF)
      {
        AcFanSpeed();
      }
      SendToMCU();
    }
    else if (call.get_target_temperature().has_value())
    {
      target_temp = *call.get_target_temperature();
      // Set fan speed nibble here to avoid unexpected switch-off on temp changes
      AcFanSpeed();
      SendToMCU();
    }
    else if (call.get_fan_mode().has_value())
    {
      ClimateFanMode fan_mode = *call.get_fan_mode();
      this->fan_mode = fan_mode;
      AcFanSpeed();
      SendToMCU();
    }
    else if (call.get_swing_mode().has_value())
    {
      ClimateSwingMode swing_mode = *call.get_swing_mode();
      this->swing_mode = swing_mode;
      AcSwing();
      // Set fan speed nibble here to avoid unexpected switch-off on temp changes
      AcFanSpeed();
      SendToMCU();
    }
  }

  ClimateTraits traits() override
  {
    auto traits = climate::ClimateTraits();
    traits.set_supports_action(true);
    traits.set_supports_two_point_target_temperature(false);
    traits.set_supports_current_temperature(true);
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(32);
    traits.set_visual_temperature_step(1);

    traits.set_supported_modes({climate::CLIMATE_MODE_OFF,
                                climate::CLIMATE_MODE_COOL,
                                climate::CLIMATE_MODE_HEAT,
                                climate::CLIMATE_MODE_DRY,
                                climate::CLIMATE_MODE_FAN_ONLY});

    traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF,
                                      climate::CLIMATE_SWING_VERTICAL});

    traits.set_supported_fan_modes({climate::CLIMATE_FAN_LOW,
                                    climate::CLIMATE_FAN_MEDIUM,
                                    climate::CLIMATE_FAN_HIGH});

    return traits;
  }
};
