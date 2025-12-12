#include "camera.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "main.h"
#include "tcpclient.h"
#include "bsp_led.h"
#include "bsp_log.h"

static const char *TAG = "CAMERA"; // 日志打印标识

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_VGA,      // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 72, // 0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,      // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_LATEST,

    .fb_location = CAMERA_FB_IN_PSRAM,
};

QueueHandle_t camera_evt_queue = NULL;

uint32_t pictureCnt = 0;
uint32_t flashPictureSize = 0;

// 缓存图片数据至Flash
void SavePicToFlash(uint8_t *PicFrame, uint32_t Size)
{
    esp_err_t err;
    if (PicFrame == NULL)
        return;
    // if ((Size == 0) || (Size > 65535))
    // {
    //     printf("Jpg Size error: %ld\n", Size);
    //     return;
    // }
    if (nvsHandle == NULL)
        return;
    // 查找 user_config 分区
    const esp_partition_t *partition = esp_partition_find_first(0x40, 0, "user_config");
    if (!partition)
    {
        printf("Error: user_config partition not found\n");
        return;
    }
    printf("Found partition, address: 0x%lx, size: 0x%lx\n", partition->address, partition->size);
    // 擦除分区
    esp_err_t ret = esp_partition_erase_range(partition, 0, partition->size);
    if (ret != ESP_OK)
    {
        printf("Error: Failed to erase partition, error code: 0x%x\n", ret);
        return;
    }

    // 写入数据
    ret = esp_partition_write(partition, 0, PicFrame, Size);
    if (ret != ESP_OK)
    {
        printf("Error: Failed to write partition, error code: 0x%x\n", ret);
        return;
    }

    // // 将图片数据存储到 NVS
    // esp_err_t err = nvs_set_blob(nvsHandle, "picFrame", PicFrame, Size);
    // if (err != ESP_OK)
    // {
    //     printf("Error: Failed to set blob, error code: 0x%x\n", err);
    //     return;
    // }
    flashPictureSize = Size;
    // 设置图片已缓存标志
    uint8_t picCacheStatus = 1;
    err = nvs_set_u8(nvsHandle, "picHasCached", picCacheStatus);
    if (err != ESP_OK)
    {
        printf("Error: Failed to set cache status, error code: 0x%x\n", err);
        return;
    }
    printf("Jpg succ saved to Flash, size=%ld\n", Size);
}
// 判断Flash中是否有缓存图片
uint32_t IsPicCached(void)
{
    uint8_t IspicSaved = 0;
    uint32_t picSize = 0;
    uint8_t *buffer = NULL;

    nvs_get_u8(nvsHandle, "picHasCached", &IspicSaved);
    if (IspicSaved == 1)
    {
        // nvs_get_blob(nvsHandle,"picFrame",NULL, &picSize);
        // 读取数据
        // esp_err_t ret = esp_partition_read(partition, 0, buffer, picSize);
        // if (ret != ESP_OK)
        // {
        //     printf("Error: Failed to read partition, error code: 0x%x\n", ret);
        //     return 0;
        // }
        printf("There's a jpeg cached\n");
    }
    else
        printf("There's no jpeg cached\n");

    return IspicSaved;
}
// 读取Flash中的缓存图片
void ReadPicFromFlash(uint8_t *PicFrame, uint32_t Size)
{
    if (PicFrame == NULL)
        return;
    // 查找 user_config 分区 // wcy debug
    const esp_partition_t *partition = esp_partition_find_first(0x40, 0, "user_config");
    if (!partition)
    {
        printf("Error: user_config partition not found\n");
        return;
    }
    printf("Found partition, address: 0x%lx, size: 0x%lx\n", partition->address, partition->size);

    // 读取数据
    esp_err_t ret = esp_partition_read(partition, 0, PicFrame, Size);
    if (ret != ESP_OK)
    {
        printf("Error: Failed to read partition, error code: 0x%x\n", ret);
        return;
    }

    printf("Jpg successfully read from Flash, size=%ld\n", Size);
    
    // nvs_get_blob(nvsHandle,"picFrame",PicFrame, &Size);
}

// 联网成功后将缓存的图片主动上传服务器
int CheckJpgCaching(void)
{
    if (IsPicCached() == 0)
        return 0;
    uint32_t picSize = 61440;

    uint8_t *buf = (uint8_t *)malloc(picSize + 4);
    printf("malloc %p,size=%ld\n", buf, picSize + 4);
    ReadPicFromFlash(buf, picSize);

    int ret = TcpSend(buf, picSize);
    if (ret == ESP_OK)
    {
        uint8_t picSaved = 0;
        nvs_set_u8(nvsHandle, "picHasCached", picSaved);
        printf("TcpSend succ, cache pic to Flash,len=%ld\n", picSize);
    }
    else
    {
        printf("TcpSend error: %d, cache pic to Flash,len=%ld\n", ret, picSize);
    }
    free(buf);

    return 1;
}

// 拍照任务
void VCameraTask(void *pvParam)
{
    uint32_t i, t = 0;
    camera_fb_t *pic;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;

    while (1)
    {
        // printf("cameraTask stack %ld \r\n", (int32_t)uxTaskGetStackHighWaterMark(NULL));
        if (xQueueReceive(camera_evt_queue, &i, portMAX_DELAY))
        {
            CameraLedCtrl(LED_ON);
            vTaskDelay(600 / portTICK_RATE_MS);
            pic = esp_camera_fb_get();
            esp_camera_fb_return(pic);
            vTaskDelay(600 / portTICK_RATE_MS);
            CameraLedCtrl(LED_OFF);
            ESP_LOGI(TAG, "Pic! %p,%zu bytes,w=%d,h=%d", pic->buf, pic->len, pic->width, pic->height);
            // ESP_LOGI(TAG, "pic->buf addr: %02X", pic->buf);
            // print buf
            // printf("\r\n");
            // for(i=0; i< pic->len; i++)
            // {
            //     printf("%02X ", pic->buf[i]);
            // }
            // printf("\r\n");

            // SavePicToFlash(pic->buf, pic->len);
            printf("get3: %ld\n", esp_get_free_heap_size());

            if (pic != NULL)
            {
                if (pic->len > 0)
                {
                    // vTaskSuspendAll();
                    bool jpeg_converted = frame2jpg(pic, camera_config.jpeg_quality, &_jpg_buf, &_jpg_buf_len);
                    // xTaskResumeAll();

                    if (!jpeg_converted)
                    {
                        ESP_LOGE(TAG, "JPEG compression failed");
                    }
                    else
                    {
                        // 启动传输照片日志
                        write_log("INFO", TAG, "picture start sending...");
                        // 确保图片不会因为信号量获取失败而丢失
                        while (SendJpgToServer(_jpg_buf, _jpg_buf_len) == ESP_FAILMUTEX)
                        {
                            printf("TcpSend failed, retrying...\n");
                            vTaskDelay(5000 / portTICK_RATE_MS);
                        }
                        write_log("INFO", TAG, "picture sent successfully!");

                        printf("get4: %ld\n", esp_get_free_heap_size());
                        // TcpSend(_jpg_buf, _jpg_buf_len);
                        ESP_LOGI(TAG, "Jpg size %zu bytes", _jpg_buf_len);

                        // if (1)
                        // {
                        // for(i=0; i<_jpg_buf_len; i++)
                        // {
                        //     printf("%02X",_jpg_buf[i]);
                        //     if ((i % 16384) == 0)
                        //     {
                        //         //esp_task_wdt_reset();
                        //         vTaskDelay(50 / portTICK_RATE_MS);
                        //     }

                        // }
                        // printf("\nEnd\n");
                        // }

                        free(_jpg_buf);
                        _jpg_buf = NULL;
                    }
                }

                // esp_camera_fb_return(pic);
            }
        }
    }
}

esp_err_t InitCamera(void)
{
    vTaskDelay(1000 / portTICK_RATE_MS);

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    camera_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    return ESP_OK;
}
// 拍照函数
uint8_t TakePic(void)
{
    uint32_t i = 1;
    if (camera_evt_queue == NULL)
    {
        ESP_LOGE(TAG, "Camera event queue not initialized");
        return -1;
    }
    if (xQueueSend(camera_evt_queue, &i, 0) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to send camera event");
        return -1;
    }
    return 0;
}