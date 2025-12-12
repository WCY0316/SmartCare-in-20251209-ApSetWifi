set(src_dirs
            BEEP
            BSP_GPIO
            BSP_LED
            BSP_LOG
            CAMERA
            DS18B20
            HEARTBEAT
            HEAT
            TCPCLIENT
            TCPSERVER
            TIMER
            USB_COM
            WIFI_BLE_PROVISIONING
            APSETWIFI)

set(include_dirs
            BEEP
            BSP_GPIO
            BSP_LED
            BSP_LOG
            CAMERA
            DS18B20
            HEARTBEAT
            HEAT
            TCPCLIENT
            TCPSERVER
            TIMER
            USB_COM
            WIFI_BLE_PROVISIONING
            APSETWIFI)

set(requires
            driver
            freertos
            log
            esp_timer
            nvs_flash
            lwip 
            spiffs
            esp_wifi
            main
            esp_http_server
            )

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)