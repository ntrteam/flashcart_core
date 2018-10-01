# flashcart_core
__A *hopefully* reusable component for dealing with flashcart specific behavior.__

## Usage
End users cannot use this directly, and should use one of the following applications:
 - [ntrboot_flasher](https://github.com/ntrteam/ntrboot_flasher) for use with 3DSes with [boot9strap](https://github.com/SciresM/boot9strap) or an equivalent FIRM loader installed.
 - [ds_ntrboot_flasher](https://github.com/ntrteam/ds_ntrboot_flasher) for use through DS flashcarts.

## Supported Carts
![From Left to Right: Acekard 2i HW81, Acekard 2i HW44, R4i Gold 3DS RTS, R4i Gold 3DS, R4i Ultra, R4 3D Revolution, DSTT, R4i-SDHC RTS Lite, R4i-SDHC Dual-Core, R4-SDHC Gold Pro, R4i 3DS RTS, Infinity 3 R4i, R4i Gold 3DS Deluxe Edition, R4i-B9S, R4iSDHC Dual-Core (r4isdhc.com.cn), Ace3DS Plus, Gateway Blue](https://i.lolis.stream/uploads/big/78f25c182b49dbbeeceab8e4ec83204a.png)

- Ace3DS Plus 
- Acekard 2i HW-44
- Acekard 2i HW-81
- DSTT (**some flash chips only!**)
- Gateway Blue (**not all variants are compatible!**)
- Infinity 3 R4i (r4infinity.com)
- R4 3D Revolution (r4idsn.com)
- R4i 3DS RTS (r4i-sdhc.com)
- R4i Gold 3DS (RTS, revisions A5/A6/A7) (r4ids.cn)
- R4i Gold 3DS Deluxe Edition (r4ids.cn) (**variants of this such as 3dslink, Orange 3DS, etc. may work as well, but have not been tested!**)
- R4i Ultra (r4ultra.com)
- R4i-B9S (r4i-sdhc.com)
- R4i-SDHC Dual-Core (r4isdhc.com)
- R4i-SDHC Gold Pro (r4isdhc.com)
- R4i-SDHC RTS Lite (r4isdhc.com)
- R4iSDHC Dual-Core (r4isdhc.com.cn)
- R4 SDHC Dual Core (revisions 5.06/7.00/7.07) (r4isdhc.hk) (**R4iGold.cc may work as well, but have not been tested!**)

**Note:** Flashcarts from r4isdhc.com tend to have yearly re-releases; most versions of these carts (2014-2018) should work but not all have been tested. Models from 2013 or earlier differ in hardware but could be similar to the current R4 Dual Core flashcarts from r4isdhc.hk. While the R4 Dual Core (r4isdhc.hk) works, r4isdhc.com 2013 and earlier flashcarts may not.

## Requesting support for a new cart
We get a lot of requests for new carts to be supported. Before requesting support, please read the existing open and closed issues to see if your cart has already been considered or not. If you'd like your cart to be supported please provide, at the very least, the following information:
 - Where your flashcart was purchased from and/or the website listed on the cart.
 - Any updaters, kernels or other misc software that may be useful.
 - Pictures of the inside of the cart. Please note we are not responsible for any damage to your cart.
 - A dump of the ROM using [GodMode9](https://github.com/d0k3/GodMode9).
 - Any other pertinent information, like any known commands for interacting with the cart.

## Developer Usage
If you want to use flashcart_core in your project, you will need [libncgc](https://github.com/angelsl/libncgc/) as well.

You will need to reopen the flashcart_core::platform namespace, and inside it define:
1. `showProgress()`, used to show current percentage of a given operation (e.g. drawing a green rectangle to represent percentage)
1. `logMessage()`, used to log things from the various flashcart classes to something that the user can read, e.g. a text file or printouts to the screen. You will need `va_list` for this.
1. `getBlowfishKey()`, used by the various flashcart classes to retrieve blowfish keys. You have to provide these blowfish keys yourself through e.g. a u8 array or using a .bin linker.

Then you can make an object from [one of the flashcart_core classes](https://github.com/ntrteam/flashcart_core/tree/master/devices), and then use the public functions inside that class.
For example:

```cpp
R4i_Gold_3DS R4iGold;
R4iGold.initialize();
R4iGold.injectNtrBoot(blowfish_key, firm, firm_size);
```

Your Makefile should create libncgc.a first, then compile your project normally using flashcart_core.

## Porting flashcart_core to a new flashcart
### Information needed for a new cart.
 - Initialization sequence.
 - Size and cluster size (for erasing) of flash.
 - Commands for reading flash, erasing flash, and writing flash.

## Licensing
This software is licensed under the terms of the GPLv3.
You can find a copy of the license in the LICENSE file.

## Credits (in alphabetical order)
- [@angelsl](https://github.com/angelsl) for [libncgc](https://github.com/angelsl/libncgc/) and many other contributions.
- [@EleventhSign](https://github.com/EleventhSign) for R4 Dual Core (r4isdhc.hk) testing.
- [@handsomematt](https://github.com/handsomematt) for DSTT support implementation.
- [@hedgeberg](https://twitter.com/hedgeberg) for testing and inspiration.  
- [@Hikari](https://twitter.com/yuukishiroko) for testing.
- [@Myria](https://twitter.com/Myriachan) for testing.  
- [@Normmatt](https://github.com/Normmatt) for bug squashing, expertise, etc.  
- [@SciresM](https://twitter.com/SciresM) for sighax/boot9strap.  
- [@stuckpixel](https://twitter.com/pixel_stuck) for testing.  
- [@zoogie](https://github.com/zoogie) for adding support for some R4i Gold 3DS revisions.
