# CM Hardware
BOARD_USES_CYANOGEN_HARDWARE := true
BOARD_HARDWARE_CLASS += \
    hardware/cyanogen/cmhw
TARGET_TAP_TO_WAKE_NODE := "/sys/class/tpnode/tpnode/synaptics/wake_gesture"