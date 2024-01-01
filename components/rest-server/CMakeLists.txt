idf_component_register(SRCS "rest-server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES json esp_http_server spiffs
                    PRIV_REQUIRES captive-portal config-manager)

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/front-end/web")
if(EXISTS ${WEB_SRC_DIR}/dist)
    spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
endif()
