idf_component_register(
    SRCS "serial.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_usb_serial_jtag
)
