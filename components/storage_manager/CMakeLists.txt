idf_component_register(
    SRCS "storage_manager.c"
    REQUIRES 
        nvs_flash
    INCLUDE_DIRS "."
)
