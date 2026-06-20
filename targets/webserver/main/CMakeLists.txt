idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES wifi serial esp_http_server
    EMBED_FILES "index.html"
)

# secrets.h lives at the repo root (two levels up from this project).
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/../..")
