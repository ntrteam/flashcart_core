# ntrboot_core
__A *hopefully* reusable component for dealing with flashcart specific behavior.__

## Usage
End users cannot use this directly, and should use one of the following applications:
 - [ntrboot_flasher](https://github.com/kitling/ntrboot_flasher) for the 3ds
 - [ak2i_ntrcardhax_flasher](https://github.com/d3m3vilurr/ak2i_ntrcardhax_flasher) for use through DS flashcarts.

### Supported Cards
 - Acekard 2i HW-44
 - Acekard 2i HW-81
 - R4i Gold 3DS RTS

### Incomplete Cards
 - R4 SDHC Dualcore

### Planned Cards
 - Supercard DS2

## Developer Usage
Define `Flashcart::platformInit`, `Flashcart::sendCommand`, and `Flashcart::showProgress`, and use `Flashcart::detectCart` to detect whatever you have. Then you can use the methods on the returned device to preform stuff in a (mostly) device-independent manner.

## Porting a flashcart_core to a new Flashcart
### Information needed for a new cart.
 - Initialization sequence.
 - Size and cluster size (for erasing) of flash.
 - Commands for reading flash, erasing flash, and writing flash.

TODO

## Credits
[@Normmatt](https://github.com/Normmatt) for bug squashing, expertiese... etc.  
[@SciresM](https://twitter.com/SciresM) for sighax/boot9strap  
[@hedgeberg](https://twitter.com/hedgeberg) for testing and inspiration.  
[@stuckpixel](https://twitter.com/pixel_stuck) for testing.  
[@Myria](https://twitter.com/Myriachan) for testing.  
[@Hikari](https://twitter.com/yuukishiroko) for testing.
