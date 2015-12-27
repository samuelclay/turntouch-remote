turntouch-remote
================

Turn Touch's interactive remote control

# Installing

To install dependencies, start here:

    https://devzone.nordicsemi.com/blogs/22/getting-started-with-nrf51-development-on-mac-os-x/

To use gcc, you will need to download:

 * nRF51 SDK
 * S110 SoftDevice
 * J-Link by Segger
 * srec_cat
 
    $ brew install srecord 
    
 * GCC ARM Embedded

    When installing GCC tools, do not use "The Unarchiver". It messes up the symlinks for 
    all of the binaries. Make sure to use "Archive Utility"

    $ mv -f ~/Downloads/gcc-arm-none-eabi-4_9-2015q2 ~/projects/code/
    $ ln -s ~/projects/code/gcc-arm-none-eabi-4_9-2015q2 /usr/local/gcc-arm-none-eabi-4_9-2015q2

 * ./bin/nrfjprog.sh into /usr/local/bin/nrfjprog


# Running

    $ make
    $ make bootloader
    $ make flash
    $ ./bin/nrfjprog.sh --rtt


# Build on Windows

## Build DFU package

    $ C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin --output Z:\remote\ble_peripheral\ble_app_template\pca10028\s110\arm5\_build\nrf51422_xxac_s110.bin Z:\remote\ble_peripheral\ble_app_template\pca10028\s110\arm5\_build\nrf51422_xxac_s110.axf

    $ & 'C:\Program Files (x86)\Nordic Semiconductor\Master Control Panel\3.10.0.14\nrf\nrfutil.exe' dfu genpkg --sd-req 0xfffe --application Z:\remote\ble_peripheral\ble_app_template\pca10028\s110\arm5\_build\nrf51422_xxac_s110.bin --application-version 0x04 --dev-revision 0x01 Z:\remote\ble_peripheral\nrf51_04.zip

### Back on Mac

    $ cp ../ble_peripheral/nrf51_*.zip ../../app/Turn\ Touch\ Remote/DFU/firmwares/

### In Xcode

In Supporting Files/defaults.plist, change `TT:firmware:version`

## Build flash hex

    $ mergehex -m Z:/remote/firmware/libs/components/softdevice/s110/hex/s110_nrf51_8.0.0_softdevice.hex Z:/remote/ble_peripheral/dfu/bootloader/pca10028/dual_bank_ble_s110/arm5/_build/nrf51422_xxac.hex Z:/remote/ble_peripheral/ble_app_template/pca10028/s110/arm5/_build/nrf51422_xxac_s110.hex -o Z:/remote/ble_peripheral/nrf51_sd_bootloader_app.hex

### `make flashwin` on Mac in firmware/

# Upgrading to latest nrf51 SDK

 1) Replace app_error.c with RTT logs
 2) Add BSP_BUTTON_ACTION_LONG_PUSH and BSP_BUTTON_ACTION_RELEASE to bsp.c
 3) 