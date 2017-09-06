# flashcart_core
__A *hopefully* reusable component for dealing with flashcart specific behavior.__

## Usage
End users cannot use this directly, and should use one of the following applications:
 - [ntrboot_flasher](https://github.com/kitling/ntrboot_flasher) for the 3ds
 - [ak2i_ntrcardhax_flasher](https://github.com/d3m3vilurr/ak2i_ntrcardhax_flasher) for use through DS flashcarts.

## Supported Carts
![From Left to Right: Acekard 2i HW81, Acekard 2i HW44, R4i Gold 3DS RTS, R4i Gold 3DS, R4i Ultra, R4 3D Revolution, Infinity 3 R4i, R4i Gold 3DS Starter, DSTT](https://i.hentai.design/uploads/big/ea12cc28e688f1de427fd917f395f13b.png)
 - Acekard 2i HW-44
 - Acekard 2i HW-81
 - R4i Gold 3DS (RTS, revisions A5/A6/A7) (r4ids.cn)
 - R4i Ultra (r4ultra.com)
 - R4i Gold 3DS Starter (r4ids.cn)
 - R4 3D Revolution (r4idsn.com)
 - Infinity 3 R4i (r4infinity.com)
 - DSTT (**[some flash chips only!](https://gist.github.com/Hikari-chin/6b48f1bb8dd15136403c15c39fafdb42)**)

### Incomplete Carts
 - R4 SDHC Dualcore

### Planned Carts
 - Supercard DS2

## Requesting support for a new cart
We get a lot of requests for new carts to be supported. Before requesting support, please read other issues, both open and closed, to see if your cart has been considered or not. If you'd like your cart to be supported please provide, at the very least, the following information:
 - Where your flashcart was purchased from and/or the website listed on the cart.
 - Any updaters, kernels or other misc software that may be useful
 - Pictures of the inside of the cart. Please note we are not responsible for any damage to your cart.
 - a dump of the ROM using [GodMode9](https://github.com/d0k3/GodMode9)
 - any other pertinent information, like any known commands for interacting with the cart.

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
