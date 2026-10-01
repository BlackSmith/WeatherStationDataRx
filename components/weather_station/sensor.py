import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_PIN

from . import WeatherStation

DEPENDENCIES = ["esp32"]

CONF_WIND_STATION_ID = "wind_station_id"
CONF_RAIN_STATION_ID = "rain_station_id"
SENSOR_DEFAULTS = {
    "temperature": ("°C", "temperature", "measurement", 1),
    "humidity": ("%", "humidity", "measurement", 0),
    "wind_speed": ("m/s", "wind_speed", "measurement", 1),
    "wind_direction": ("°", "wind_direction", "measurement", 0),
    "wind_gust": ("m/s", "wind_speed", "measurement", 1),
    "rain_volume": ("mm", "precipitation", "total_increasing", 2),
    "battery_wind_station": ("%", "battery", "measurement", 0),
    "battery_rain_station": ("%", "battery", "measurement", 0),
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WeatherStation),
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_number,
            cv.Required(CONF_WIND_STATION_ID): cv.int_range(min=0, max=255),
            cv.Required(CONF_RAIN_STATION_ID): cv.int_range(min=0, max=255),
            **{
                cv.Optional(key): sensor.sensor_schema(
                    unit_of_measurement=unit,
                    device_class=device_class,
                    state_class=state_class,
                    accuracy_decimals=decimals,
                )
                for key, (unit, device_class, state_class, decimals) in SENSOR_DEFAULTS.items()
            },
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


def validate_single_receiver(config):
    receivers = [
        entry
        for entry in fv.full_config.get().get("sensor", [])
        if entry.get("platform") == "weather_station"
    ]
    if len(receivers) > 1:
        raise cv.Invalid("Only one weather_station receiver is supported per device")
    return config


FINAL_VALIDATE_SCHEMA = validate_single_receiver


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_PIN],
        config[CONF_WIND_STATION_ID],
        config[CONF_RAIN_STATION_ID],
    )
    await cg.register_component(var, config)
    for key in SENSOR_DEFAULTS:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_sensor")(sens))
