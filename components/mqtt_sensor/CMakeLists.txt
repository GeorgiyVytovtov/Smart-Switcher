idf_component_register(
    SRCS "mqtt.c"
    REQUIRES 
        shearch_components
        parse
        control
        mqtt
    INCLUDE_DIRS "."
)
