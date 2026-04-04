set(srcs
    "main.c"
    "my_platform.c"
    "motor_control.c"
    "steering.c"
    "led.c"
)

set(requires "bluepad32" "btstack" "driver")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
