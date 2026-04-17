
idf_component_register(
    SRCS
        "softap_example_main.c"
        "http_server.c"
        "control_logic.c"
        "pwm_driver.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        esp_http_server
        esp_driver_gpio    
        esp_driver_mcpwm
)