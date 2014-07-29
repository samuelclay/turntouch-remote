# Turn Touch Remote Firmware Markfile
# Author: Samuel Clay
# Date: 2013-12-17
# Copyright: (c) 2013 by Turn Touch / NewsBlur
# Based on work by Christian Starkjohann, 2008-04-07

DEVICE     = attiny84
PROGRAMMER ?= usbtiny
F_CPU      = 8000000	# in Hz
FUSE_L     = 0xFF
FUSE_H     = 0xD7
FUSE_E     = 0xFF
AVRDUDE_FUSE = avrdude -v -v -v -v -c $(PROGRAMMER) -p $(DEVICE) -P usb
AVRDUDE    = $(AVRDUDE_FUSE) -B1

LIBS       = -I./libs/tiny 
LIBS       += -I./libs/SPI
LIBS       += -I./libs/RF24
LIBS       += -I./libs

CFLAGS  =  $(LIBS) \
           -DDEBUG_LEVEL=0 -DARDUINO=1 \
           -MMD -DUSB_VID=null -DUSB_PID=null \
           -fno-exceptions -ffunction-sections -fdata-sections \
           -Wl,--gc-sections

C_SRC   := libs/tiny/pins_arduino.c \
           libs/tiny/WInterrupts.c \
           libs/tiny/wiring.c \
           libs/tiny/wiring_analog.c \
           libs/tiny/wiring_digital.c \
           libs/tiny/wiring_pulse.c \
           libs/tiny/wiring_shift.c
CPP_SRC := libs/SPI/SPI.cpp \
           libs/RF24/RF24.cpp \
           libs/tiny/main.cpp \
           libs/tiny/Print.cpp \
           libs/tiny/TinyDebugSerial.cpp \
           libs/tiny/TinyDebugSerial115200.cpp \
           libs/tiny/TinyDebugSerial38400.cpp \
           libs/tiny/TinyDebugSerial9600.cpp \
           libs/tiny/TinyDebugSerialErrors.cpp \
           libs/tiny/Tone.cpp \
           libs/tiny/WMath.cpp \
           libs/tiny/WString.cpp
SRC     := src/pinchange.cpp src/remote.cpp

OBJECTS = $(SRC:.cpp=.o) $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o) 
COMPILE = avr-gcc -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

all: clean remote.hex program

# symbolic targets:
help:
	@echo "Use one of the following:"
	@echo "make program ... to flash fuses and firmware"
	@echo "make fuse ...... to flash the fuses"
	@echo "make flash ..... to flash the firmware (use this on metaboard)"
	@echo "make hex ....... to build remote.hex"
	@echo "make clean ..... to delete objects and hex file"

hex: remote.hex

program: fuse flash

# rule for programming fuse bits:
fuse:
	@[ "$(FUSE_H)" != "" -a "$(FUSE_L)" != "" ] || \
		{ echo "*** Edit Makefile and choose values for FUSE_L and FUSE_H!"; exit 1; }
	$(AVRDUDE_FUSE) -e -U lfuse:w:$(FUSE_L):m -U hfuse:w:$(FUSE_H):m -U efuse:w:$(FUSE_E):m

# rule for uploading firmware:
flash: remote.hex
	$(AVRDUDE) -U flash:w:build/remote.hex:i

# rule for deleting dependent files (those which can be built by Make):
clean:
	rm -f build/* *.o **/*.o **/**/*.o *.d **/*.d **/**/*.d **/**/*.lst

# Generic rule for compiling C files:
.c.o:
	$(COMPILE) -g -c $< -o $@

.cpp.o:
	$(COMPILE) -g -c $< -o $@

# Generic rule for assembling Assembler source files:
.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

# Generic rule for compiling C to assembler, used for debugging only.
.c.s:
	$(COMPILE) -S $< -o $@

core:
	avr-ar rcs build/core.a $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o)
    
# file targets:

code: $(OBJECTS) core

remote.elf: code core
	$(COMPILE) -o build/remote.elf $(SRC:.cpp=.o) build/core.a -L. -lm

remote.hex: remote.elf
	rm -f build/remote.hex build/remote.eep.hex
	avr-objcopy -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 -O ihex build/remote.elf build/remote.hex
	avr-objcopy -O ihex -R .eeprom build/remote.elf build/remote.hex
	avr-size build/remote.hex

disasm: remote.elf
	avr-objdump -d build/remote.elf

cpp:
	$(COMPILE) -E build/remote.c
