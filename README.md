# flashcart_core
__A *hopefully* reusable component for dealing with flashcart specific behavior.__

## Usage
End users cannot use this directly, and should use one of the following applications:
 - [ntrboot_flasher](https://github.com/kitling/ntrboot_flasher) for the 3ds
 - [ak2i_ntrcardhax_flasher](https://github.com/d3m3vilurr/ak2i_ntrcardhax_flasher) for use through DS flashcarts.

## Supported Carts
![From Left to Right: Acekard 2i HW81, Acekard 2i HW44, R4i Gold 3DS RTS, R4i Gold 3DS, R4i Ultra, R4 3D Revolution, DSTT, R4i-SDHC RTS Lite, R4i-SDHC Dual-Core, R4-SDHC Gold Pro, R4i 3DS RTS, Infinity 3 R4i, R4i Gold 3DS Deluxe Edition, R4i-B9S](https://i.hentai.design/uploads/big/7f6f676a2df3574c30ebc65b61c130df.png)
- Acekard 2i HW-44
- Acekard 2i HW-81
- DSTT (**some flash chips only!**)
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

**Note:** Flashcarts from r4isdhc.com tend to have yearly re-releases; all versions of these carts (2014-2017) should work but not all have been tested. 

### Planned Carts
 - Supercard DSTWO

## Requesting support for a new cart
We get a lot of requests for new carts to be supported. Before requesting support, please read other issues, both open and closed, to see if your cart has been considered or not. If you'd like your cart to be supported please provide, at the very least, the following information:
 - Where your flashcart was purchased from and/or the website listed on the cart.
 - Any updaters, kernels or other misc software that may be useful.
 - Pictures of the inside of the cart. Please note we are not responsible for any damage to your cart.
 - A dump of the ROM using [GodMode9](https://github.com/d0k3/GodMode9).
 - Any other pertinent information, like any known commands for interacting with the cart.

## Developer Usage
Define `Flashcart::platformInit`, `Flashcart::sendCommand`, and `Flashcart::showProgress`, and use `Flashcart::detectCart` to detect whatever you have. Then you can use the methods on the returned device to preform stuff in a (mostly) device-independent manner.

## Porting flashcart_core to a new flashcart
### Information needed for a new cart.
 - Initialization sequence.
 - Size and cluster size (for erasing) of flash.
 - Commands for reading flash, erasing flash, and writing flash.

## Licensing
This software is licensed under the terms of the GPLv3.
You can find a copy of the license in the LICENSE file.

## Credits
[@Normmatt](https://github.com/Normmatt) for bug squashing, expertiese... etc.  
[@SciresM](https://twitter.com/SciresM) for sighax/boot9strap  
[@hedgeberg](https://twitter.com/hedgeberg) for testing and inspiration.  
[@stuckpixel](https://twitter.com/pixel_stuck) for testing.  
[@Myria](https://twitter.com/Myriachan) for testing.  
[@Hikari](https://twitter.com/yuukishiroko) for testing.
