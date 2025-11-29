idf_component_register(
    SRCS "html_pages.c" "captive_portal.c"
    REQUIRES 
        wifi_manager
        esp_http_server
    INCLUDE_DIRS "."
)
