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
AVRDUDE    = avrdude -v -v -v -v -c $(PROGRAMMER) -p $(DEVICE) -P usb

LIBS       = -I./tiny -I./SPI -I.
CFLAGS  =  $(LIBS) \
           -DDEBUG_LEVEL=0 -DARDUINO=1 \
           -MMD -DUSB_VID=null -DUSB_PID=null \
           -fno-exceptions -ffunction-sections -fdata-sections \
           -Wl,--gc-sections

C_SRC   := tiny/pins_arduino.c \
           tiny/WInterrupts.c \
           tiny/wiring.c \
           tiny/wiring_analog.c \
           tiny/wiring_digital.c \
           tiny/wiring_pulse.c \
           tiny/wiring_shift.c
CPP_SRC := SPI/SPI.cpp \
           tiny/main.cpp \
           tiny/Print.cpp \
           tiny/TinyDebugSerial.cpp \
           tiny/TinyDebugSerial115200.cpp \
           tiny/TinyDebugSerial38400.cpp \
           tiny/TinyDebugSerial9600.cpp \
           tiny/TinyDebugSerialErrors.cpp \
           tiny/Tone.cpp \
           tiny/WMath.cpp \
           tiny/WString.cpp
SRC     := pinchange.cpp remote.cpp RF24.cpp

OBJECTS = $(SRC:.cpp=.o) $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o) 
COMPILE = avr-gcc -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

all: program

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
	$(AVRDUDE) -e -Uefuse:w:0xFF:m -U hfuse:w:$(FUSE_H):m -U lfuse:w:$(FUSE_L):m

# rule for uploading firmware:
flash: remote.hex
	$(AVRDUDE) -U flash:w:remote.hex:i

# rule for deleting dependent files (those which can be built by Make):
clean:
	rm -f remote.hex remote.lst remote.obj remote.cof remote.list remote.map remote.eep.hex remote.elf *.o remote.s tiny/*.o avr/*.o *.d

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
	avr-ar rcs core.a $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o)
    
# file targets:

code: $(OBJECTS) core

remote.elf: code core
	$(COMPILE) -o remote.elf $(SRC:.cpp=.o) core.a -L. -lm

remote.hex: remote.elf
	rm -f remote.hex remote.eep.hex
	avr-objcopy -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 -O ihex remote.elf remote.hex
	avr-objcopy -O ihex -R .eeprom remote.elf remote.hex
	avr-size remote.hex

disasm: remote.elf
	avr-objdump -d remote.elf

cpp:
	$(COMPILE) -E remote.c
