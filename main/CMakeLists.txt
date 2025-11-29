idf_component_register(
    SRCS "main.c"
    REQUIRES mqtt_sensor  
            control 
            shearch_components  
            wifi_manager 
            storage_manager 
    INCLUDE_DIRS "."
)
