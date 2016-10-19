
TTY = /dev/arduino
ARDUINO = /opt/arduino/arduino
TEENSYDUINO = /opt/teensyduino/arduino
SRC = $(wildcard *.ino)

ARDUINO_AVR := uno mega leonardo
ADAFRUIT_AVR := metro feather32u4 protrinket3ftdi protrinket5ftdi gemma trinket3 trinket5
OTHER_BOARDS := due zero featherm0 huzzah teensy31 101
SUPPORTED_BOARDS := $(OTHER_BOARDS) $(ARDUINO_AVR) $(ADAFRUIT_AVR)

default: leonardo
upload: leonardo-upload

echo-targets:
	@echo $(SUPPORTED_BOARDS)

$(ARDUINO_AVR):
	$(ARDUINO) --board arduino:avr:$@ --verify --verbose $(SRC)

ARDUINO_AVR_UP := $(patsubst %,%-upload,$(ARDUINO_AVR))
$(ARDUINO_AVR_UP):
	$(ARDUINO) --board arduino:avr:$(subst -upload,,$@) --port $(TTY) --upload --verbose $(SRC)

$(ADAFRUIT_AVR):
	$(ARDUINO) --board adafruit:avr:$@ --verify --verbose $(SRC)

ADAFRUIT_AVR_UP := $(patsubst %,%-upload,$(ADAFRUIT_AVR))
$(ADAFRUIT_AVR_UP):
	$(ARDUINO) --board adafruit:avr:$(subst -upload,,$@) --port $(TTY) --upload --verbose $(SRC)

zero:
	$(ARDUINO) --board arduino:samd:arduino_zero_edbg --verify --verbose $(SRC)

zero-upload:
	$(ARDUINO) --board arduino:samd:arduino_zero_edbg --port $(TTY) --upload --verbose $(SRC)

featherm0:
	$(ARDUINO) --board adafruit:samd:adafruit_feather_m0 --verify --verbose $(SRC)

featherm0-upload:
	$(ARDUINO) --board adafruit:samd:adafruit_feather_m0 --port $(TTY) --upload --verbose $(SRC)

due:
	$(ARDUINO) --board arduino:sam:arduino_due_x_dbg --verify --verbose $(SRC)

due-upload:
	$(ARDUINO) --board arduino:sam:arduino_due_x_dbg --port /dev/$(shell readlink $(TTY)) --upload --verbose $(SRC)

huzzah:
	$(ARDUINO) --board esp8266:esp8266:huzzah --verify --verbose $(SRC)

huzzah-upload:
	$(ARDUINO) --board esp8266:esp8266:huzzah --port $(TTY) --upload --verbose $(SRC)

101:
	LANG=C $(ARDUINO) --board Intel:arc32:arduino_101 --verify --verbose $(SRC)

101-upload:
	LANG=C $(ARDUINO) --board Intel:arc32:arduino_101 --port $(TTY) --upload --verbose $(SRC)

teensy31:
	$(TEENSYDUINO) --board teensy:avr:teensy31 --verify --verbose $(SRC)

