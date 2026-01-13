// usb.cpp
//
#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_USB

#include <stdio.h>

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/usb_host.h"

#include "usb/cdc_acm_host.h"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"
#include "usb/vcp.hpp"
#include "usb/usb_host.h"

typedef void (*receive_event_t)(const uint8_t *data, size_t data_len);
typedef void (*connected_event_t)();
typedef void (*disconnected_event_t)();

using namespace esp_usb;

#define EXAMPLE_BAUDRATE (115200)
#define EXAMPLE_STOP_BITS (0) // 0: 1 stopbit, 1: 1.5 stopbits, 2: 2 stopbits
#define EXAMPLE_PARITY (0)    // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
#define EXAMPLE_DATA_BITS (8)

static const char *TAG = "USB";
static SemaphoreHandle_t device_disconnected_sem;

std::unique_ptr<CdcAcmDevice> vcp = nullptr;

receive_event_t receive_event = NULL;
connected_event_t connected_event = NULL;
disconnected_event_t disconnected_event = NULL;

extern "C" void usbInit()
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
}

extern "C" void usbSetReveiveEvent(receive_event_t re)
{
    receive_event = re;
}

extern "C" void usbSetConnectedEvent(connected_event_t ce)
{
    connected_event = ce;
}

extern "C" void usbSetDisconnectedEvent(disconnected_event_t de)
{
    disconnected_event = de;
}

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{

    if (receive_event != NULL)
    {
        (receive_event)(data, data_len);
    }

    return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type)
    {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %d", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default:
        break;
    }
}

static void usb_lib_task(void *arg)
{
    while (1)
    {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGI(TAG, "USB: All devices freed");
        }
    }
}

extern "C" bool usbIsConnected()
{
    return (vcp != nullptr);
}

extern "C" void usbSend(uint8_t data[], size_t size)
{
    if (vcp != nullptr)
    {
        ESP_ERROR_CHECK(vcp->tx_blocking(data, size));
        ESP_ERROR_CHECK(vcp->set_control_line_state(true, true));
    }
}

extern "C" void usbLoop(void *arg)
{

    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);
    assert(task_created == pdTRUE);

    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    VCP::register_driver<FT23x>();
    VCP::register_driver<CP210x>();
    VCP::register_driver<CH34x>();

    while (true)
    {
        const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 5000,
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .event_cb = handle_event,
            .data_cb = handle_rx,
            .user_arg = NULL,
        };

        vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev_config));

        if (vcp == nullptr)
        {
            continue;
        }
        vTaskDelay(10);

        cdc_acm_line_coding_t line_coding = {
            .dwDTERate = EXAMPLE_BAUDRATE,
            .bCharFormat = EXAMPLE_STOP_BITS,
            .bParityType = EXAMPLE_PARITY,
            .bDataBits = EXAMPLE_DATA_BITS,
        };
        ESP_ERROR_CHECK(vcp->line_coding_set(&line_coding));

        if ( connected_event != NULL )
        {
            (connected_event)();
        }

        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);

        if ( disconnected_event != NULL )
        {
            (disconnected_event)();
        }

    }
}

extern "C" void usbStart()
{
    xTaskCreate(usbLoop, "UsbLoop", 4096, NULL, 4, NULL);
}

#endif
