# CMakeLists.txt - src/
# MoodLink v3.0 - Main component

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp-tflite-micro
        esp_timer
        nvs_flash
        spiffs
)

# Enable C++17 (required for TFLite)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# Additional compile options
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-error
    -DESP_NN
    -DCONFIG_NN_OPTIMIZED
)
