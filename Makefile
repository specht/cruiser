BOARD_TAG    = uno
ARDUINO_PORT = /dev/ttyUSB0 # Change to your own tty interface

USER_LIB_PATH += /home/michael/Arduino/libraries/
ARDUINO_LIBS = Gamebuino SPI

include /usr/share/arduino/Arduino.mk # This is where arduino-mk installed

run:
	~/programming/gbsim/build/gbsim build-uno/cruiser.elf
	
