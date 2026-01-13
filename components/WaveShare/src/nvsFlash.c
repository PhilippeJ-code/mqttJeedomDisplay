// nvs.c
//
#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_NVS

#include "nvs_flash.h"
#include "nvs.h"

void nvsInit()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
}

uint16_t nvsRead(char* name)
{
    esp_err_t err;
    nvs_handle_t my_handle;

    uint16_t value = 0;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err)); 
    } else {
        err = nvs_get_u16(my_handle, name, &value);
        nvs_close(my_handle);
    }
    return value;
}

void nvsWrite(char* name, uint16_t value)
{
    esp_err_t err;
    nvs_handle_t my_handle;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        err = nvs_set_u16(my_handle, name, value);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

#endif