# Turn Touch remote firmware, schematic, and board layout
> The design files used for the nrf51-based Turn Touch remote.

<a href="https://turntouch.com"><img src="https://s3.amazonaws.com/static.newsblur.com/turntouch/blog/1*1_w7IlHISYWdQjIGPcxRkQ.jpeg" width="400"><br />Available at turntouch.com</a>

[![License][license-image]][license-url]
[![Platform](https://img.shields.io/cocoapods/p/LFAlertController.svg?style=flat)](http://cocoapods.org/pods/LFAlertController)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

## Features

[Turn Touch](https://turntouch.com) is a smart home remote control carved out of beautiful mahogany wood. This is a remote control for Hue lights, Sonos speakers, and all the other connected devices in your home. Turn Touch works on iOS and macOS, giving you control of your music, videos, presentations, and more. 

- [x] **Extraordinary design**: This isn't yet another plastic remote. Turn Touch is carved from solid wood and worked to a show-stopping, textured finish. 
- [x] **Well connected**: Turn Touch connects to most smart devices in your home that speak WiFi. And this is just the first version. Over time, there's no telling what you'll be able to control.
- [x] **Always ready**: Turn Touch is always on, always connected, and always ready to take your breath away. Once you connect your Turn Touch via Bluetooth it stays connected and always available, even without the Turn Touch app actively running. And there's no delay, when you press a button the result is instant.
- [x] **Built to last**: Not only will your Turn Touch stand up to drops and shock — its entire circuit board can be inexpensively replaced with newer, future wireless tech. You won't have to buy a new remote every time Apple comes out with a new phone. And instead of flimsy plastic clasps, a set of 8 strong magnets invisibly hold the remote together. 

## Requirements

- macOS or Windows
- nrf51 Development Kit (($39 on Digikey)[https://www.digikey.com/product-detail/en/nordic-semiconductor-asa/NRF51-DK/1490-1038-ND/5022449])
- EAGLE for schematic and board layout
- ARM toolchain for firmware

## Installation

To install dependencies, start here:

    https://devzone.nordicsemi.com/blogs/22/getting-started-with-nrf51-development-on-mac-os-x/

To use gcc, you will need to download:

 * nRF51 SDK
 * S110 SoftDevice
 * J-Link by Segger
 * srec_cat

    ```$ brew install srecord```

 * GCC ARM Embedded

    When installing GCC tools, do not use "The Unarchiver". It messes up the symlinks for
    all of the binaries. Make sure to use "Archive Utility"
    ```
    $ mv -f ~/Downloads/gcc-arm-none-eabi-4_9-2015q2 ~/projects/code/
    $ ln -s ~/projects/code/gcc-arm-none-eabi-4_9-2015q2 /usr/local/gcc-arm-none-eabi-4_9-2015q2
    ```

 * ./bin/nrfjprog.sh into /usr/local/bin/nrfjprog


### Running

    $ make
    $ make bootloader
    $ make flash
    $ ./bin/nrfjprog.sh --rtt

### Build on Windows

#### Build DFU package

    $ C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin --output Z:\remote\ble_peripheral\ble_app_template\pca10028\s110\arm5\_build\nrf51422_xxac_s110.bin Z:\remote\ble_peripheral\ble_app_template\pca10028\s110\arm5\_build\nrf51422_xxac_s110.axf

    $ & 'C:\Program Files (x86)\Nordic Semiconductor\Master Control Panel\3.10.0.14\nrf\nrfutil.exe' dfu genpkg --sd-req 0xfffe --application Z:\remote\ble_peripheral\ble_app_template\pca10028\s110\arm5\_build\nrf51422_xxac_s110.bin --application-version 0x06 --dev-revision 0x01 Z:\remote\ble_peripheral\nrf51_06.zip

##### Back on Mac

    $ cp ../ble_peripheral/nrf51_*.zip ../../app/Turn\ Touch\ Mac/DFU/firmwares/
    $ cp ../ble_peripheral/nrf51_*.zip ../../ios/Turn\ Touch\ iOS/DFU/

##### In Xcode (in both turntouch-mac/turntouch-ios repos)

In Supporting Files/defaults.plist, change `TT:firmware:version`

#### Build flash hex

    $ mergehex -m Z:/remote/firmware/libs/components/softdevice/s110/hex/s110_nrf51_8.0.0_softdevice.hex Z:/remote/ble_peripheral/dfu/bootloader/pca10028/dual_bank_ble_s110/arm5/_build/nrf51422_xxac.hex Z:/remote/ble_peripheral/ble_app_template/pca10028/s110/arm5/_build/nrf51422_xxac_s110.hex -o Z:/remote/ble_peripheral/nrf51_sd_bootloader_app.hex

#### `make flashwin` on Mac in firmware/

### Upgrading to latest nrf51 SDK

 1) Replace app_error.c with RTT logs
 2) Add BSP_BUTTON_ACTION_LONG_PUSH and BSP_BUTTON_ACTION_RELEASE to bsp.c

## Contribute

We would love you for the contribution to the **Turn Touch remote firmware**. Feel free to submit a Pull Request with the improvements or additions you've made.

## Turn Touch open source repositories and documentation

Everything about Turn Touch is open source and well documented. Here are all of Turn Touch's repos:

* [Turn Touch Mac OS app](https://github.com/samuelclay/turntouch-mac/)
* [Turn Touch iOS app](https://github.com/samuelclay/turntouch-ios/)
* [Turn Touch remote firmware and board layout](https://github.com/samuelclay/turntouch-remote/)
* [Turn Touch enclosure, CAD, and CAM](https://github.com/samuelclay/turntouch-enclosure/)

The entire Turn Touch build process is also documented:
* <a href="http://ofbrooklyn.com/2019/01/3/building-turntouch/">Everything you need to build your own Turn Touch smart remote<br /><img src="https://s3.amazonaws.com/static.newsblur.com/turntouch/blog/1*1_w7IlHISYWdQjIGPcxRkQ.jpeg" width="400"></a>
* <a href="http://ofbrooklyn.com/2019/01/3/building-turntouch/#firmware">Step one: Laying out the buttons and writing the firmware <br /><img src="https://s3.amazonaws.com/static.newsblur.com/turntouch/blog/1*K_ERgjIZiFIX5yDOa5JJ7g.png" width="400"></a>
* <a href="http://ofbrooklyn.com/2019/01/3/building-turntouch/#cad">Step two: Designing the remote to have perfect button clicks <br /><img src="https://s3.amazonaws.com/static.newsblur.com/turntouch/blog/1*iXMqpq5IsfudAeT3GZlLFA.png" width="400"></a>
* <a href="http://ofbrooklyn.com/2019/01/3/building-turntouch/#cnc">Step three: CNC machining and fixturing to accurately cut wood <br /><img src="https://s3.amazonaws.com/static.newsblur.com/turntouch/blog/1*SYmywFE5tY3GBO9t9lcTiA.png" width="400"></a>
* <a href="http://ofbrooklyn.com/2019/01/3/building-turntouch/#laser">Step four: Inlaying the mother of pearl <br /><img src="https://s3.amazonaws.com/static.newsblur.com/turntouch/blog/1*kl8WDuTd0RpCkkipLeHx0g.png" width="400"></a>

## Author

Samuel Clay – [@samuelclay](https://twitter.com/samuelclay) – [samuelclay.com](http://samuelclay.com)

Distributed under the MIT license. See ``LICENSE`` for more information.

[https://github.com/samuelclay/turntouch-mac](https://github.com/samuelclay)

[swift-image]:https://img.shields.io/badge/swift-3.0-orange.svg
[swift-url]: https://swift.org/
[license-image]: https://img.shields.io/badge/License-MIT-blue.svg
[license-url]: LICENSE
[travis-image]: https://img.shields.io/travis/dbader/node-datadog-metrics/master.svg?style=flat-square
[travis-url]: https://travis-ci.org/dbader/node-datadog-metrics
[codebeat-image]: https://codebeat.co/badges/c19b47ea-2f9d-45df-8458-b2d952fe9dad
[codebeat-url]: https://codebeat.co/projects/github-com-vsouza-awesomeios-com
