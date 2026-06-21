idf_component_register(
    SRCS "sysinfo.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer esp_driver_tsens app_update bootloader_support spi_flash
)
