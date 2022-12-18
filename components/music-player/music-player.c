#include "music-player.h"
#include "audio_pipeline.h"
#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_idf_version.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "periph_adc_button.h"
#include "periph_touch.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "input_key_service.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_system.h"
#include "audio_mem.h"
#include "ogg_decoder.h"
#include <string.h>
#include "../../main/Global.h"


static const char* TAG = "MUSIC PLAYER";
static const char* selected_decoder_name = "ogg";
static const char* selected_file_to_play = "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.ogg";

audio_element_handle_t http_stream_reader, i2s_stream_writer, selected_decoder, rsp_handle;
audio_pipeline_handle_t pipeline;
audio_board_handle_t board_handle;
#define PERIPH_STACK_SIZE (8 * 1024)
#define PERIPH_TASK_PRIO (5)
#define PERIPH_TASK_CORE (0)
#define PERIPH_SET_CONFIG()                                                                                            \
    {                                                                                                                  \
        .task_stack = PERIPH_STACK_SIZE, .task_prio = PERIPH_TASK_PRIO, .task_core = PERIPH_TASK_CORE,                 \
        .extern_stack = true,                                                                                          \
    }

static struct marker {
    int pos;
    const uint8_t* start;
    const uint8_t* end;
} file_marker;
static void set_next_file_marker(uint8_t start[], uint8_t end[])
{
    file_marker.start = start;
    file_marker.end = end;
    file_marker.pos = 0;
}
int mp3_music_read_cb(audio_element_handle_t el, char* buf, int len, TickType_t wait_time, void* ctx)
{
    int read_size = file_marker.end - file_marker.start - file_marker.pos;
    if (read_size == 0) {
        return AEL_IO_DONE;
    } else if (len < read_size) {
        read_size = len;
    }
    memcpy(buf, file_marker.start + file_marker.pos, read_size);
    file_marker.pos += read_size;
    return read_size;
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t* evt, void* ctx)
{
    /* Handle touch pad events
           to start, pause, resume, finish current song and adjust volume
        */
    board_handle = (audio_board_handle_t)ctx;
    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGI(TAG, "[ * ] input key id is %d", (int)evt->data);
        switch ((int)evt->data) {
        case INPUT_KEY_USER_ID_PLAY:
            ESP_LOGI(TAG, "[ * ] [Play] input key event");
            audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
            switch (el_state) {
            case AEL_STATE_INIT:
                ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                audio_pipeline_run(pipeline);
                break;
            case AEL_STATE_RUNNING:
                ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
                audio_pipeline_pause(pipeline);
                break;
            case AEL_STATE_PAUSED:
                ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
                audio_pipeline_resume(pipeline); 
                break;
            default:
                ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
            break;
            }
        case INPUT_KEY_USER_ID_SET:
            ESP_LOGI(TAG, "[ * ] [Set] input key event");
            ESP_LOGI(TAG, "[ * ] Stopped, advancing to the next song");
            char* url = NULL;
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            audio_pipeline_terminate(pipeline);
            // sdcard_list_next(sdcard_list_handle, 1, &url);
            ESP_LOGW(TAG, "URL: %s", url);
            audio_element_set_uri(http_stream_reader, url);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_elements(pipeline);
            audio_pipeline_run(pipeline);
            break;
        case INPUT_KEY_USER_ID_VOLUP:
            ESP_LOGI(TAG, "[ * ] [Vol+] input key event");
            player_volume += 10;
            if (player_volume > 100) {
                player_volume = 100;
            }
            audio_hal_set_volume(board_handle->audio_hal, player_volume);
            ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            break;
        case INPUT_KEY_USER_ID_VOLDOWN:
            ESP_LOGI(TAG, "[ * ] [Vol-] input key event");
            player_volume -= 10;
            if (player_volume < 0) {
                player_volume = 0;
            }
            audio_hal_set_volume(board_handle->audio_hal, player_volume);
            ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            break;
        }
    }

    return ESP_OK;
}
void init_player() {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize and start peripherals");
    audio_board_key_init(set);
        ESP_LOGI(TAG, "[ 1.3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void*)board_handle);
     ESP_LOGI(TAG, "[ 1.4 ] Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);
    ESP_LOGI(TAG, "[2.1] Create http stream to read data");
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.out_rb_size = 1024 * 1024;
    http_stream_reader = http_stream_init(&http_cfg);
    //     mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.2] Create %s decoder to decode %s file", selected_decoder_name, selected_decoder_name);

    ogg_decoder_cfg_t ogg_cfg = DEFAULT_OGG_DECODER_CONFIG();
    selected_decoder = ogg_decoder_init(&ogg_cfg);

    ESP_LOGI(TAG, "[2.3] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    /* ZL38063 audio chip on board of ESP32-LyraTD-MSC does not support 44.1 kHz sampling frequency,
       so resample filter has been added to convert audio data to other rates accepted by the chip.
       You can resample the data to 16 kHz or 48 kHz.
    */
    ESP_LOGI(TAG, "[4.3] Create resample filter");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_handle = rsp_filter_init(&rsp_cfg);
    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, selected_decoder, selected_decoder_name);
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[2.5] Link it together http_stream-->%s_decoder-->i2s_stream-->[codec_chip]", selected_decoder_name);
    const char* link_tag[3] = { "http", selected_decoder_name, "i2s" };
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "[2.6] Set up  uri (http as http_stream, %s as %s_decoder, and default output is i2s)",
        selected_decoder_name, selected_decoder_name);
    audio_element_set_uri(http_stream_reader, selected_file_to_play);

        ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);
        ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGW(TAG, "[ 6 ] Press the keys to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] next song.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");
    // ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    //     set_next_file_marker();

    audio_pipeline_run(pipeline);
    while (1) {
        /* Handle event interface messages from pipeline
//            to set music info and to advance to the next song
//         */
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void*)selected_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            // Set music info for a new song to be played
            audio_element_info_t music_info = { 0 };
            audio_element_getinfo(selected_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from %s decoder, sample_rates=%d, bits=%d, ch=%d",
                selected_decoder_name, music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source == (void*)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
            audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
            if (el_state == AEL_STATE_FINISHED) {
                ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");
                // sdcard_list_next(sdcard_list_handle, 1, &url);
                // ESP_LOGW(TAG, "URL: %s", url);
                /* In previous versions, audio_pipeline_terminal() was called here. It will close all the elememnt task
                 * and when use the pipeline next time, all the tasks should be restart again. It speed too much time
                 * when we switch to another music. So we use another method to acheive this as below.
                 */
                // audio_element_set_uri(fatfs_stream_reader, url);
                while(1){
                    if(isWifiConnected == 1){
                        break;
                    }
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
                audio_pipeline_run(pipeline);
            }
            continue;
            //             }
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
        if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON
                || msg.source_type == PERIPH_ID_ADC_BTN)
            && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_ADC_BUTTON_PRESSED
                || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {
            if ((int)msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                switch (el_state) {
                case AEL_STATE_INIT:
                    ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                    audio_pipeline_run(pipeline);
                    break;
                case AEL_STATE_RUNNING:
                    ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
                    audio_pipeline_pause(pipeline);
                    break;
                case AEL_STATE_PAUSED:
                    ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
                    audio_pipeline_resume(pipeline);
                    break;
                case AEL_STATE_FINISHED:
                    ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
                    audio_pipeline_reset_ringbuffer(pipeline);
                    audio_pipeline_reset_elements(pipeline);
                    audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
                    // set_next_file_marker();
                    audio_pipeline_run(pipeline);
                    break;
                default:
                    ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
                }
            } else if ((int)msg.data == get_input_set_id()) {
                ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
                ESP_LOGI(TAG, "[ * ] Stopping audio pipeline");
                break;
            } else if ((int)msg.data == get_input_mode_id()) {
                ESP_LOGI(TAG, "[ * ] [mode] tap event");
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_terminate(pipeline);
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                // set_next_file_marker();
                audio_pipeline_run(pipeline);
            } else if ((int)msg.data == get_input_volup_id()) {
                ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            } else if ((int)msg.data == get_input_voldown_id()) {
                ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            }
        }
    }

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline and release all resources");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, http_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, selected_decoder);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying
     * event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(selected_decoder);
    esp_periph_set_destroy(set);
}
