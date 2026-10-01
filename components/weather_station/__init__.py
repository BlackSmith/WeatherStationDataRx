import esphome.codegen as cg

weather_station_ns = cg.esphome_ns.namespace("weather_station")
WeatherStation = weather_station_ns.class_("WeatherStation", cg.Component)
