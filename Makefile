ARDUINO_DIR = /usr/share/arduino
ARDUINO_LIBS = LiquidCrystal/src
ARDMK_DIR = /usr/share/arduino
AVR_TOOLS_DIR = /usr
ARDUINO_CORE_PATH = /usr/share/arduino/hardware/arduino/avr/cores/arduino
BOARDS_TXT = /usr/share/arduino/hardware/arduino/avr/boards.txt
ARDUINO_VAR_PATH = /usr/share/arduino/hardware/arduino/avr/variants
BOOTLOADER_PARENT = /usr/share/arduino/hardware/arduino/avr/bootloaders


BOARD_TAG = mega
MCU = atmega2560
F_CPU = 16000000UL

include /usr/share/arduino/Arduino.mk
