idf_component_register(
    SRCS "control.c"
    REQUIRES driver
            shearch_components 
            wifi_manager
            mqtt_sensor
    INCLUDE_DIRS "."
)
