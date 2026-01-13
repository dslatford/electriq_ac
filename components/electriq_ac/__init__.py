import esphome.codegen as cg
import esphome.config_validation as cv

# Define the namespace for your component
CODEOWNERS = ["@dslatford"]
electriq_ac_ns = cg.esphome_ns.namespace("electriq_ac")

# This component doesn't have its own config, only the climate platform does
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass
