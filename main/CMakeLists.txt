idf_component_register(
    SRCS "main.c" "safe.c" "encoder.c" "buzzer.c" "servo.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc
)
