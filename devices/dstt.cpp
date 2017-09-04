/*

DSTT flashcart_core implementation by HandsomeMatt
This card is a real treat, it has the possibility of having any one of the
following 50 odd flashchips. Have a DSTT with it's own unique flashchip id?
You better find a datasheet for that shit then.

Actual implementation code starts past line 100.

Flash chips are all identified by the following command sequence:
    0x555:0xAA, 0x2AA:0x55, 0x555:0x90

Supported Chips using the same standard of command definitons (type A):
    Sector Block Addressing = 16KB,8KB,8KB,32KB,64KB,64KB,64KB... (unless otherwise specified)

    0x041F: https://media.digikey.com/pdf/Data%20Sheets/Atmel%20PDFs/AT49BV,LV001(N)(T).pdf
    0x051F: https://media.digikey.com/pdf/Data%20Sheets/Atmel%20PDFs/AT49BV,LV001(N)(T).pdf
    0x1A37: http://www.dataman.com/media/datasheet/AMIC/A29L800.pdf
    0x3437: http://www.dataman.com/media/datasheet/AMIC/A29L400.pdf
    0x49C2: http://pdf1.alldatasheet.com/datasheet-pdf/view/113400/MCNIX/MX29LV160BB.html
    0x5BC2: http://www.dataman.com/media/datasheet/Macronix/MX29LV800B-T.pdf
    0x80BF: http://www.metatech.com.hk/datasheet/sst/standard_mem_pdf/360-39LF-VFx00A-3-DS.pdf (29 blocks of 2048 bytes)
    0x9B37: AMIC A29L800U    
    0xA01F: http://pdf1.alldatasheet.com/datasheet-pdf/view/56175/ATMEL/AT49BV8192.html (16K bytes boot block) (Two 16K bytes param blocks) (976K bytes main memory)
    0xA31F: http://pdf1.alldatasheet.com/datasheet-pdf/view/56175/ATMEL/AT49BV8192.html (same as above pretty much)
    0xA7C2: http://nice.kaze.com/MX29LV320.pdf (8x4K-Word blocks)
    0xA8C2: http://nice.kaze.com/MX29LV320.pdf (8x4K-Word blocks)
    0xB537: AMIC A29L400U
    0xB91C: http://pdf1.alldatasheet.com/datasheet-pdf/view/113811/EON/EN29LV400AT-70BIP.html    
    0xBA01: http://pdf1.alldatasheet.com/datasheet-pdf/view/524736/SPANSION/AM29LV400BB-90EC.html
    0xBA04: http://pdf1.alldatasheet.com/datasheet-pdf/view/186858/SPANSION/MBM29LV400BC-55PBT.html
    0xBA1C: http://pdf1.alldatasheet.com/datasheet-pdf/view/113812/EON/EN29LV400AB-70BIP.html
    0xBA4A: http://pdf1.alldatasheet.com/datasheet-pdf/view/145388/EXCELSEMI/ES29LV400DB-70TGI.html
    0xBAC2: http://www.datasheet-pdf.com/datasheet/MacronixInternational/546750/MX29LV400B.pdf.html
    0xC11F: http://pdf.datasheetcatalog.com/datasheet/atmel/doc3405.pdf
    0xC298: http://pdf.datasheetcatalog.com/datasheets2/56/562126_1.pdf
    0xC31F: http://datasheetz.com/data/Integrated%20Circuits%20(ICs)/Memory/AT49BV802AT-70TI-datasheetz.html (8x4K-Word Blocks) (Writing slightly differs: see page 13 (uses AAA instead of 2AA?))
    0xC420: http://pdf.datasheetcatalog.com/datasheet/stmicroelectronics/6680.pdf
    0xC4C2: http://pdf1.alldatasheet.com/datasheet-pdf/view/113399/MCNIX/MX29LV160BT.html
    0xEF20: http://pdf1.alldatasheet.com/datasheet-pdf/view/23064/STMICROELECTRONICS/M29W400.html (16k bytes boot block)

// Atmel AT49BV1614T-90TC (8x4K-Word Blocks, 16,16,32*)

Different flash commands required (todo):
    0x912C: http://pdf.datasheetcatalog.com/datasheet/micron/MT28F160A3.pdf (8x4K-Word Blocks) (RA: X:0xFF) (PW: X:0x10/40 & WA:PD) (ER: X:0x20 & BA:0xD0)
    0x922C: http://www.dataman.com/media/datasheet/Micron/mt28f160c3_3.pdf (8x4K-Word Blocks) (similar/same to above)
    0x9320: http://pdf.datasheetcatalog.com/datasheet/stmicroelectronics/7585.pdf (8x4K-Word Blocks) (^)   

Unknown datasheets but *SHOULD* "supported" Flashchip IDs:
    0x49B0: SHARP LH28F160BGHB-BTL (can't find datasheet)
    0x9020: ST M28W160(B)T (can't find datasheet)
    0x9089: INTEL 28F160B3T (can't find specific datasheet)
    0x9120: ST M28W160(B)B (can't find datasheet)
    0x9189: INTEL 28F160B3B (can't find specific datasheet)
    0x9289: INTEL 28F800B3T (can't find specific datasheet)
    0x9389: INTEL 28F800B3B (can't find specific datasheet)
    0x9689: INTEL 28F320B3T (can't find specific datasheet)
    0x9789: INTEL 28F320B3B (can't find specific datasheet)

Known flashchips that are "unsupported":
    0x0B8A "DSTTi :P"
    0x23AD "BRIGHT MICRO. BM29LV400T"
    0x4398 "TOSHIBA TC58FVB160A"
    0x4920 "ST M29W160BB"
    0x5B20 "ST M29W800BT"
    0x68B0 "SHARP LH28F160BG(H)-TTL"
    0x69B0 "SHARP LH28F160BG(H)-BTL"
    0x81BF "SST SST39LF/VF800A"
    0x89BF "SST SST39LF/VF200A"
    0x902C "MICRON MT28F160A3-T"
    0x9220 "ST M28W800BT"
    0x932C "MICRON MT28F160C3(4)-B"
    0x9489 "INTEL 28F400B3T"
    0x9589 "INTEL 28F400B3B"
    0xABAD "BRIGHT MICRO. BM29LV400B"
    0xB901 "SPANSION Am29LV400BT"
    0xB904 "SPANSION MBM29LV400TC"
    0xB9C2 "MACRONIX MX29LV400T"
    0xB94A "ESI ES29LV400DT"
    0xB952 "ALIANCE SEMI. AS29LV400T"
    0xBA52 "ALIANCE SEMI. AS29LV400B"
    0xCE20 "ST M28W160E(C)T"
    0xCF20 "ST M28W160E(C)B"
    0xD089 "INTEL 28F016B3T"
    0xD189 "INTEL 28F016B3B"
    0xD289 "INTEL 28F008B3T"
    0xD389 "INTEL 28F008B3B"
    0xD489 "INTEL 28F004B3T"
    0xD520 "ST M29F400BT"
    0xD589 "INTEL 28F004B3B"
    0xD620 "ST M29F400BB"
    0xD689 "INTEL 28F032B3T"
    0xD720 "ST M29W800AT"
    0xD789 "INTEL 28F032B3B"
    0xDAC2 "MACRONIX MX29LV800T"
    0xEE20 "ST M29W400T"
*/

#include "device.h"

#include "ui.h"

#include <stdlib.h>
#include <cstring>

const uint16_t supported_flashchips[] = {
    0x041F, 0x051F, 0x1A37, 0x3437, 0x49C2, 0x5BC2, 0x80BF, 0xA01F, 0xA31F, 0xA7C2, 0xA8C2, 0xBA01, 0xBA04,
    0xBA1C, 0xBA4A, 0xBAC2, 0xB91C, 0xC11F, 0xC298, 0xC31F, 0xC420, 0xC4C2, 0xEF20,
    0x9B37, 0xB537
};

// Header: TOP TF/SD DSTTDS
// Device ID: 0xFC2
// Sector Size: 0x2000
class DSTT : Flashcart {
private:
    uint32_t m_flashchip;

    uint32_t dstt_flash_command(uint8_t data0, uint32_t data1, uint16_t data2)
    {
        uint8_t cmd[8];
        cmd[0] = data0;
        cmd[1] = (uint8_t)((data1 >> 24)&0xFF);
        cmd[2] = (uint8_t)((data1 >> 16)&0xFF);
        cmd[3] = (uint8_t)((data1 >>  8)&0xFF);
        cmd[4] = (uint8_t)((data1 >>  0)&0xFF);
        cmd[5] = (uint8_t)((data2 >>  8)&0xFF);
        cmd[6] = (uint8_t)((data2 >>  0)&0xFF);
        cmd[7] = 0x00;

        uint32_t ret;

        sendCommand(cmd, 4, (uint8_t*)&ret, 0xa7180000);
        return ret;
    }

    void dstt_reset()
    {
        dstt_flash_command(0x87, 0, 0xF0);
    }

    uint32_t get_flashchip_id()
    {
        uint32_t flashchip;

        dstt_flash_command(0x87, 0x5555, 0xAA);
        dstt_flash_command(0x87, 0x2AAA, 0x55);
        dstt_flash_command(0x87, 0x5555, 0x90);
        flashchip = dstt_flash_command(0, 0, 0);
        dstt_reset();

        if ((flashchip & 0xFF00FFFF) != 0x7F003437 && (flashchip & 0xFF00FFFF) != 0x7F00B537
              && (uint16_t)flashchip != 0x41F && (uint16_t)flashchip != 0x51F)
        {
            dstt_flash_command(0x87, 0x5555, 0xAA);
            dstt_flash_command(0x87, 0x2AAA, 0x55);
            dstt_flash_command(0x87, 0x5555, 0x90);
            uint32_t device_id = dstt_flash_command(0, 0x100, 0);
            dstt_reset();
         
            if ((uint16_t)device_id == 0xBA1C || (uint16_t)device_id == 0xB91C)
                return device_id;
        }

        return flashchip;
    }

    bool flashchip_supported(uint32_t flashchip)
    {
        for (int i = 0; i < sizeof(supported_flashchips) / 2; i++)
            if (supported_flashchips[i] == (uint16_t)flashchip)
                return true;
        
        return false;
    }

    void Erase_Block(uint32_t offset, uint32_t length)
    {
        dstt_flash_command(0x87, 0x5555, 0xAA);
        dstt_flash_command(0x87, 0x2AAA, 0x55);
        dstt_flash_command(0x87, 0x5555, 0x80);
        dstt_flash_command(0x87, 0x5555, 0xAA);
        dstt_flash_command(0x87, 0x2AAA, 0x55);

        dstt_flash_command(0x87, offset, 0x30);

        uint32_t end_offset = offset + length;
        while (offset < end_offset)
        {
            while (dstt_flash_command(0, offset, 0) != 0xFFFFFFFF);
            offset += 4;
        }

        dstt_reset();
    }

    void Erase_Chip()
    {
        switch(m_flashchip)
        {
            case 0x41F:
            case 0xA01F:
            case 0xA31F:
            case 0xB91C:
                Erase_Block(0, 0x10000);
                break;

            case 0x51F:
                Erase_Block(0, 0x4000);
                Erase_Block(0x4000, 0x2000);
                Erase_Block(0x6000, 0x2000);
                Erase_Block(0x8000, 0x8000);
                break;

            case 0x80BF:
            case 0xC11F:
            case 0xC31F:
                for (int i = 0; i < 29; i++)
                    Erase_Block(i*0x800, 0x800);
                break;

            case 0x1A37:
            case 0x3437:
            case 0xA7C2:
            case 0xC298:
            case 0xC420:
            case 0xC4C2:
                Erase_Block(0, 0x8000);
                Erase_Block(0x8000, 0x2000);
                Erase_Block(0xA000, 0x2000);
                Erase_Block(0xC000, 0x4000);
                break;

            case 0x49C2:
            case 0x5BC2:
            case 0x9B37: // no datasheet
            case 0xA8C2:
            case 0xB537: // no datasheet
            case 0xBA01:            
            case 0xBA04:
            case 0xBA1C:
            case 0xBA4A:            
            case 0xBAC2:
            case 0xEF20:
            default:
                Erase_Block(0x0000, 0x2000);
                Erase_Block(0x2000, 0x1000);
                Erase_Block(0x3000, 0x1000);
                Erase_Block(0x4000, 0x4000);
                Erase_Block(0x8000, 0x8000);
                break;
        }
    }

    void Program_Byte(uint32_t offset, uint8_t data)
    {
        // todo: I think this is actually word write (which is 16 bits on these flash chips)
        dstt_flash_command(0x87, 0x5555, 0xAA); 
        dstt_flash_command(0x87, 0x2AAA, 0x55);
        dstt_flash_command(0x87, 0x5555, 0xA0);        
        dstt_flash_command(0x87, offset, data);

        while ((uint8_t)dstt_flash_command(0, offset, 0) != data);

        dstt_reset();
    }

public:
    DSTT() : Flashcart("DSTT", 0x10000) { }

    const char *getAuthor() { return "handsomematt"; }
    const char *getDescription() { return "This will run on the official DSTT as\nwell as a lot of clones.\n\nIf yours doesn't work post the\nflashchip ID on GitHub issues."; }

    bool initialize()
    {
        dstt_flash_command(0x86, 0, 0);

        m_flashchip = get_flashchip_id();
        if (!flashchip_supported(m_flashchip))
            return false;

        return true;
    }

    void shutdown() {
        dstt_flash_command(0x88, 0, 0);
    }
    
    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
        uint32_t i = 0;
        uint32_t end_address = address + length;

        while (address < end_address)
        {
            uint32_t data = dstt_flash_command(0, address, 0);
            showProgress(address, end_address);
            
            buffer[i++] = (uint8_t)((data >> 0) & 0xFF);
            buffer[i++] = (uint8_t)((data >> 8) & 0xFF);
            buffer[i++] = (uint8_t)((data >> 16) & 0xFF);
            buffer[i++] = (uint8_t)((data >> 24) & 0xFF);

            address += 4;
        }

        return true;
    }

    // todo: we're just assuming this is block (0x2000) aligned
    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer)
    {    
        // really fucking temporary, writeFlash can only do full length writes
        // todo: read and erase properly
        Erase_Chip();

        for(int i = 0; i < length; i++)
            Program_Byte(address++, buffer[i]);

        return true;
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) {
        // todo: we just read and write the entire flash chip because we don't align blocks
        // properly, when writeFlash works, don't use memcpy

        uint8_t* buffer = (uint8_t*)malloc(m_max_length);
        readFlash(0, m_max_length, buffer);

        memcpy(buffer + 0x1000, blowfish_key, 0x48);
        memcpy(buffer + 0x2000, blowfish_key + 0x48, 0x1000);
        memcpy(buffer + 0x7E00, firm, firm_size);
        
        writeFlash(0, m_max_length, buffer);

        return true;
    }
};

DSTT dstt;