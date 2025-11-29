idf_component_register(
    SRCS "wifi_manager.c"
    REQUIRES 
        shearch_components
        control
        captive_portal
        dns_responder
        storage_manager
        esp_wifi
        esp_event
        esp_netif
    INCLUDE_DIRS "."
)
