PROJECT_NAME := remote_s110_pca10028

export OUTPUT_FILENAME
#MAKEFILE_NAME := $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
MAKEFILE_NAME := $(MAKEFILE_LIST)
MAKEFILE_DIR := $(dir $(MAKEFILE_NAME) ) 
OUTPUT_FILENAME := nrf51
LINKER_SCRIPT=./src/remote_gcc_nrf51.ld

TEMPLATE_PATH = ./libs/components/toolchain/gcc
ifeq ($(OS),Windows_NT)
include $(TEMPLATE_PATH)/Makefile.windows
else
include $(TEMPLATE_PATH)/Makefile.posix
endif

MK := mkdir
RM := rm -rf

#echo suspend
ifeq ("$(VERBOSE)","1")
NO_ECHO := 
else
NO_ECHO := @
endif

# Toolchain commands
CC       		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gcc"
AS       		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-as"
AR       		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ar" -r
LD       		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ld"
NM       		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-nm"
OBJDUMP  		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objdump"
OBJCOPY  		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objcopy"
SIZE     		:= "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-size"

#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

SOFTDEVICE_HEX = ./libs/components/softdevice/s110/hex/s110_nrf51_8.0.0_softdevice.hex

#source common to all targets
C_SOURCE_FILES += \
./src/main.c \
./src/button_status.c \
./src/rtt.c \
./libs/bsp/bsp.c \
./libs/components/ble/ble_advertising/ble_advertising.c \
./libs/components/ble/common/ble_advdata.c \
./libs/components/ble/common/ble_conn_params.c \
./libs/components/ble/common/ble_srv_common.c \
./libs/components/ble/ble_services/ble_bas/ble_bas.c \
./libs/components/ble/ble_services/ble_dfu/ble_dfu.c \
./libs/components/ble/ble_services/ble_dis/ble_dis.c \
./libs/components/ble/device_manager/device_manager_peripheral.c \
./libs/components/drivers_nrf/common/nrf_drv_common.c \
./libs/components/drivers_nrf/delay/nrf_delay.c \
./libs/components/drivers_nrf/gpiote/nrf_drv_gpiote.c \
./libs/components/drivers_nrf/ppi/nrf_drv_ppi.c \
./libs/components/drivers_nrf/pstorage/pstorage.c \
./libs/components/drivers_nrf/timer/nrf_drv_timer.c \
./libs/components/libraries/bootloader_dfu/bootloader_util.c \
./libs/components/libraries/bootloader_dfu/dfu_app_handler.c \
./libs/components/libraries/button/app_button.c \
./libs/components/libraries/scheduler/app_scheduler.c \
./libs/components/libraries/timer/app_timer.c \
./libs/components/libraries/timer/app_timer_appsh.c \
./libs/components/libraries/util/app_error.c \
./libs/components/libraries/util/nrf_assert.c \
./libs/components/softdevice/common/softdevice_handler/softdevice_handler.c \
./libs/components/toolchain/system_nrf51.c \
./libs/RTT/SEGGER_RTT.c \
./libs/RTT/SEGGER_RTT_printf.c \



#assembly files common to all targets
ASM_SOURCE_FILES  = ./libs/components/toolchain/gcc/gcc_startup_nrf51.s

#includes common to all targets
INC_PATHS  = -I./config
INC_PATHS += -I./src
INC_PATHS += -I./libs/bsp
INC_PATHS += -I./libs/RTT
INC_PATHS += -I./libs/components/ble/ble_advertising
INC_PATHS += -I./libs/components/ble/common
INC_PATHS += -I./libs/components/ble/device_manager
INC_PATHS += -I./libs/components/ble/ble_services/ble_bas
INC_PATHS += -I./libs/components/ble/ble_services/ble_dis
INC_PATHS += -I./libs/components/ble/ble_services/ble_dfu
INC_PATHS += -I./libs/components/device
INC_PATHS += -I./libs/components/drivers_nrf/common
INC_PATHS += -I./libs/components/drivers_nrf/delay
INC_PATHS += -I./libs/components/drivers_nrf/gpiote
INC_PATHS += -I./libs/components/drivers_nrf/hal
INC_PATHS += -I./libs/components/drivers_nrf/ppi
INC_PATHS += -I./libs/components/drivers_nrf/pstorage
INC_PATHS += -I./libs/components/drivers_nrf/timer
INC_PATHS += -I./libs/components/libraries/bootloader_dfu
INC_PATHS += -I./libs/components/libraries/button
INC_PATHS += -I./libs/components/libraries/scheduler
INC_PATHS += -I./libs/components/libraries/timer
INC_PATHS += -I./libs/components/libraries/trace
INC_PATHS += -I./libs/components/libraries/trace
INC_PATHS += -I./libs/components/libraries/util
INC_PATHS += -I./libs/components/softdevice/common/softdevice_handler
INC_PATHS += -I./libs/components/softdevice/s110/headers
INC_PATHS += -I./libs/components/toolchain
INC_PATHS += -I./libs/components/toolchain/gcc


OBJECT_DIRECTORY = build
LISTING_DIRECTORY = $(OBJECT_DIRECTORY)
OUTPUT_BINARY_DIRECTORY = $(OBJECT_DIRECTORY)

# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )

#flags common to all targets
CFLAGS  = -DBOARD_PCA10028
# CFLAGS  = -DBOARD_CUSTOM
CFLAGS += -DNRF51
CFLAGS += -DDEBUG
CFLAGS += -DS110
CFLAGS += -DENABLE_DEBUG_LOG_SUPPORT
CFLAGS += -DBLE_STACK_SUPPORT_REQD
# CFLAGS += -DSWI_DISABLE0
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -mcpu=cortex-m0
CFLAGS += -mthumb -mabi=aapcs --std=gnu99
CFLAGS += -Wall -Werror -O3 -g3
#CFLAGS += -Wno-unused-variable
CFLAGS += -Wno-pointer-sign
CFLAGS += -fomit-frame-pointer
CFLAGS += -mfloat-abi=soft
# keep every function in separate section. This will allow linker to dump unused functions
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
#CFLAGS += -fno-builtin --short-enums

# keep every function in separate section. This will allow linker to dump unused functions
LDFLAGS += -Xlinker -Map=$(LISTING_DIRECTORY)/$(OUTPUT_FILENAME).map
LDFLAGS += -mthumb -mabi=aapcs -L $(TEMPLATE_PATH) -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m0
# let linker to dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs -lc -lnosys

# Assembler flags
ASMFLAGS += -x assembler-with-cpp
ASMFLAGS += -DBOARD_PCA10028
ASMFLAGS += -DNRF51
ASMFLAGS += -DS110
ASMFLAGS += -DSOFTDEVICE_PRESENT
ASMFLAGS += -DBLE_STACK_SUPPORT_REQD
ASMFLAGS += -D__HEAP_SIZE=512

#default target - first one defined
default: clean nrf51

#building all targets
all: clean
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e cleanobj
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e nrf51 

#target for printing all targets
help:
	@echo following targets are available:
	@echo 	nrf51


C_SOURCE_FILE_NAMES = $(notdir $(C_SOURCE_FILES))
C_PATHS = $(call remduplicates, $(dir $(C_SOURCE_FILES) ) )
C_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SOURCE_FILES))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SOURCE_FILES) ))
ASM_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(ASM_SOURCE_FILE_NAMES:.s=.o) )

vpath %.c $(C_PATHS)
vpath %.s $(ASM_PATHS)

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

nrf51: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e finalize

## Create build directories
$(BUILD_DIRECTORIES):
	echo $(MAKEFILE_NAME)
	$(MK) $@

# Create objects from C SRC files
$(OBJECT_DIRECTORY)/%.o: %.c
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) $(CFLAGS) $(INC_PATHS) -c -o $@ $<

# Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) $(ASMFLAGS) $(INC_PATHS) -c -o $@ $<


# Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out


## Create binary .bin file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

finalize: genbin genhex echosize
# finalize: echosize

genbin:
	@echo Preparing/gen: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
genhex:
	@echo Preparing/gen: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

echosize:
	-@echo ""
	$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ""

clean:
	$(RM) $(BUILD_DIRECTORIES)

cleanobj:
	$(RM) $(BUILD_DIRECTORIES)/*.o

flash: $(MAKECMDGOALS)
	@echo Flashing: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
	nrfjprog --flash $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

erase:
	nrfjprog --erase-all

flashsd:
	nrfjprog --flash-softdevice $(SOFTDEVICE_HEX)

flashsdapp:
	srec_cat $(SOFTDEVICE_HEX) -intel $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex -intel --line-length=44 -o $(OUTPUT_BINARY_DIRECTORY)/sd_app.hex -intel
	nrfjprog --flash $(OUTPUT_BINARY_DIRECTORY)/sd_app.hex

flashall:
	srec_cat $(SOFTDEVICE_HEX) -intel $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex -intel ../bootloader/build/nrf51822_xxac.hex -intel -exclude 0x3FC00 0x3FC20 -generate 0x3FC00 0x3FC04 \
-constant-l-e 0x01 4 -generate 0x3FC04 0x3FC08 \
-constant-l-e 0x00 4 -generate 0x3FC08 0x3FC0C \
-constant-l-e 0xFE 4 -generate 0x3FC0C 0x3FC20 -constant 0x00 --line-length=44 -o $(OUTPUT_BINARY_DIRECTORY)/sd_app_bootloader.hex -intel 
# At the address 0x3FC00 the bootloader stores its information. The first byte (0x01) marks that a 
# valid application exists. The following two bytes are the CRC the application is checked against 
# to determine its validity. This hex-File writes these values to 0x00 which forces the bootloader 
# to ignore the CRC check.
# See "E. How do I program the original application via programmer (JLINK), not by DFU" at
# https://devzone.nordicsemi.com/blogs/685/common-faq-on-dfu/
	sed -i.bak 's/10FC00000100000000000000FE00000000000000F5/10FC00000100000000000000FE000000FFFFFFFFF9/' build/sd_app_bootloader.hex
	nrfjprog --flash $(OUTPUT_BINARY_DIRECTORY)/sd_app_bootloader.hex

