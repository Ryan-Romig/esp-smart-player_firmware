idf_component_register(SRCS "music-player.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_peripherals audio_stream input_key_service)