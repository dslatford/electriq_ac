import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@dslatford"]

electriq_ac_ns = cg.esphome_ns.namespace("electriq_ac")
ElectriqAC = electriq_ac_ns.class_(
    "ElectriqAC", climate.Climate, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    climate._CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ElectriqAC),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)
