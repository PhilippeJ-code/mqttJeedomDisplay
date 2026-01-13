// sdCard.c
//
#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_SDCARD

#include "sdCard.h"

#include "esp_err.h"

#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "dirent.h"

#define PIN_NUM_MISO  13
#define PIN_NUM_MOSI  11
#define PIN_NUM_CLK   12
#define PIN_NUM_CS    -1

sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
sdmmc_card_t *card = NULL;

static bool bSdCard;
static const char *TAG = "Controleur";

void sdCardInit()
{
    esp_err_t ret;

    bSdCard = false;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    bSdCard = true;

}

char sdCardPresent()
{
    return bSdCard;
}

char sdCardMount()
{
    esp_err_t ret;

    const char mount_point[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        return false;
    }

    return true;
}

char sdCardUnmount()
{
    const char mount_point[] = MOUNT_POINT;
    esp_vfs_fat_sdcard_unmount(mount_point, card);

    return true;
}

#endif