# main.cpp because esp-matter is C++; lib/strip stays C and is called directly.
# The `main` component implicitly depends on every other component (esp_matter,
# chip, strip, serial, …), so no explicit REQUIRES is needed — same as the
# esp-matter examples.
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
