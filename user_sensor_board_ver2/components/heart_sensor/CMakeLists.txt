idf_component_register(
    SRCS    "src/max30102_driver.c" "src/heart_rate_calculator.c"
    INCLUDE_DIRS "include"
    REQUIRES driver common
)