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

