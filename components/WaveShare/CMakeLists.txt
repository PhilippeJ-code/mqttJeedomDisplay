set(SRCS_DIR "src")

file(GLOB_RECURSE CPP_SRCS "${SRCS_DIR}/*.cpp")
file(GLOB_RECURSE C_SRCS "${SRCS_DIR}/*.c")

idf_component_register(
    SRCS
        ${C_SRCS}
        ${CPP_SRCS}
    INCLUDE_DIRS
        ${SRCS_DIR}
	REQUIRES
        driver
        esp_timer
        fatfs
        nvs_flash
        espressif__usb_host_ch34x_vcp
        espressif__usb_host_cp210x_vcp
        espressif__usb_host_ftdi_vcp
        espressif__usb_host_vcp
        espressif__esp_lcd_touch_gt911
        espressif__esp32_io_expander
)

