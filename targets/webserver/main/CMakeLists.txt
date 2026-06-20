idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES wifi serial led esp_http_server esp_timer esp_driver_tsens
    EMBED_FILES "index.html" "../../../logo.svg"
)

# secrets.h lives at the repo root (two levels up from this project).
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/../..")
