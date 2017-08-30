#include "device.h"

#include "ui.h" // platform specific (JUST FOR DEBUG)

const uint16_t supported_flashchips[] = {
    0x041F, 0x051F, 0x1A37, 0x3437, 0x49B0, 0x49C2, 0x5BC2, 0x80BF, 0x9020, 0x9089, 0x9120, 0x912C, 0x9189, 0x922C, 0x9289, 0x9320, 0x9389,
    0x9689, 0x9789, 0x9B37, 0xA01F, 0xA31F, 0xA7C2, 0xA8C2, 0xBA01, 0xBA04, 0xBA1C, 0xBA4A, 0xBAC2, 0xB537, 0xB91C, 0xC11F, 0xC298, 0xC31F,
    0xC420, 0xC4C2, 0xEF20
};

// todo: what makes these "unsupported"? or are they just fakes?
// could we still write to them?
const uint16_t unsupported_flashchips[] = {
    0x0B8A, 0x23AD, 0x4398, 0x4920, 0x5B20, 0x68B0, 0x69B0, 0x81BF, 0x89BF, 0x902C, 0x9220, 0x932C, 0x9489, 0x9589, 0xABAD, 0xB901, 0xB904,
    0xB9C2, 0xB94A, 0xB952, 0xBA52, 0xCE20, 0xCF20, 0xD089, 0xD189, 0xD289, 0xD389, 0xD489, 0xD520, 0xD589, 0xD620, 0xD689, 0xD720, 0xD789,
    0xDAC2, 0xEE20
};

// Header: TOP TF/SD DSTTDS
// Device ID: 0xFC2
// Sector Size: 0x2000
class DSTT : Flashcart {
private:
    uint32_t m_flashchip;

    uint32_t dstt_flash_command(uint8_t data0, uint32_t data1, uint32_t data2)
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

    uint32_t get_flashchip_id()
    {
        uint32_t flashchip;

        dstt_flash_command(0x86, 0, 0);        
        dstt_flash_command(0x87, 0x5555, 0xAA);
        dstt_flash_command(0x87, 0x2AAA, 0x55);
        dstt_flash_command(0x87, 0x5555, 0x90);
        flashchip = dstt_flash_command(0, 0, 0);
        dstt_flash_command(0x87, 0x5555, 0xF0);

        if ((flashchip & 0xFF00FFFF) != 0x7F003437 && (flashchip & 0xFF00FFFF) != 0x7F00B537
              && (uint16_t)flashchip != 0x41F && (uint16_t)flashchip != 0x51F)
        {
            dstt_flash_command(0x87, 0x5555, 0xAA);
            dstt_flash_command(0x87, 0x2AAA, 0x55);
            dstt_flash_command(0x87, 0x5555, 0x90);
            uint32_t device_id = dstt_flash_command(0, 0x100, 0);
            dstt_flash_command(0x87, 0x5555, 0xF0);
         
            if ((uint16_t)device_id == 0xBA1C || (uint16_t)device_id == 0xB91C)
                return device_id;

            //dstt_flash_command(0x87, 0x5555, 0xF0); // not sure if good to do ?
            //dstt_flash_command(0x87, 0x5555, 0xFF); // not sure if good to do ?
        }

        return flashchip;
    }

    bool flashchip_supported(uint32_t flashchip)
    {
        for (int i = 0; i < sizeof(supported_flashchips) / 2; i++)
            if (supported_flashchips[i] == (uint16_t)flashchip)
                return true;
        
        for (int i = 0; i < sizeof(unsupported_flashchips) / 2; i++)
           if (unsupported_flashchips[i] == (uint16_t)flashchip)
                return false;
    
        // unknown device (todo: show prompt)
        return false;
    }

    // Erases an 0x2000 sector ?
    void Sector_Erase_W(uint32_t offset, uint32_t length)
    {
        dstt_flash_command(0x86, 0, 0);
        dstt_flash_command(0x87, 0x5555, 0xAA);
        dstt_flash_command(0x87, 0x2AAA, 0x55);
        dstt_flash_command(0x87, 0x5555, 0x80);
        dstt_flash_command(0x87, 0x5555, 0xAA);
        dstt_flash_command(0x87, 0x2AAA, 0x55);

        dstt_flash_command(0x87, 0x5555, 0x10); // chip erase
        dstt_flash_command(0x87, offset, 0x30);

        uint32_t end_offset = offset + length;
        while (offset < end_offset)
        {
            while (dstt_flash_command(0, offset, 0) != 0xFFFFFFFF); // todo: error if this never happens
            offset += 4;
        }

        dstt_flash_command(0x87, 0x5555, 0xF0); // Reset Mode
    }

    // For certain chip types
    void Write_Type_1(uint32_t offset, uint32_t length, const uint32_t* data)
    {
        dstt_flash_command(0x86, 0, 0); // unsure if required

        for (int i = 0; i < length; i+=4)
        {
            dstt_flash_command(0x87, offset, 0x50);
            dstt_flash_command(0x87, offset, 0x40);
            dstt_flash_command(0x87, offset, *data);
    
            while (!(dstt_flash_command(0, offset, 0) & 0x80));
    
            dstt_flash_command(0x87, offset, 0x50);
            dstt_flash_command(0x87, offset, 0xFF);         

            offset += 4;
            data += 1;
        }
    }

    void Program_Word(uint32_t offset, uint32_t data)
    {
        // Word:
        dstt_flash_command(0x87, 0x5555, 0xAA);        
        dstt_flash_command(0x87, 0x2AAA, 0x55);
        dstt_flash_command(0x87, 0x5555, 0xA0);        
        dstt_flash_command(0x87, offset, data);

        // Byte: ???
        // dstt_flash_command(0x87, 0xAAAA, 0xAA);        
        // dstt_flash_command(0x87, 0x5555, 0x55);
        // dstt_flash_command(0x87, 0xAAAA, 0xA0);        
        // dstt_flash_command(0x87, offset, data);

        // todo: add timeout
        // also lots of weird shit to do if it's not aligned to 4 bytes
        // while (dstt_flash_command(0, offset, 0) != data);

        dstt_flash_command(0x87, 0x5555, 0xF0);
    }

public:
    DSTT() : Flashcart("DSTT", 0x10000) { }

    bool initialize()
    {
        return true;

        m_flashchip = get_flashchip_id();
        ShowPrompt(BOTTOM_SCREEN, false, "Flashchip: %08x", m_flashchip);
        if (!flashchip_supported(m_flashchip))
            return false;

        // debug atm
        //if (m_flashchip != 0xBAC2)
        //  return false;

        return true;
    }

    void shutdown() {
        dstt_flash_command(0x88, 0, 0);
    }
    
    void readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
        uint32_t i = 0;
        uint32_t end_address = address + length;

        dstt_flash_command(0x86, 0, 0);        

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
    }

    void writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
        // also todo.. read the flash before we overwrite
        // todo: I know this works on the BAC2 chip, but we need a list
        //Sector_Erase_W(address, length);
        //Write_Type_1(address, length, (uint32_t*)buffer);
    }

    void injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) {
        // 0x0000:0x2000 = 0xFF
        // 8192

        // Erase(0, 0x2000);
        // Erase(0x2000, 0x1000);
        // Erase(0x3000, 0x1000);
        // Erase(0x4000, 0x4000);
        // Erase(0x8000, 0x8000);

        Sector_Erase_W(0, 1);

        Program_Word(0,  0x10);
        Program_Word(1,  0x11);
        Program_Word(2,  0x12);
        Program_Word(3,  0x12);
        Program_Word(4,  0x20);
        Program_Word(5,  0x21);
        Program_Word(6,  0x22);
        Program_Word(7,  0x23);
        Program_Word(8,  0x30);
        Program_Word(9,  0x31);
        Program_Word(10,  0x32);
        Program_Word(11,  0x33);
        Program_Word(12, 0x40);
        Program_Word(13, 0x41);
        Program_Word(14, 0x42);
        Program_Word(15, 0x43);

        return; // don't fuckin run this shit yet        

        // DSTT Mirrors the flash, but only the second is used?
        // Flash them both anyway
        writeFlash(0x00000 + 0x1000, 0x48, blowfish_key);
        writeFlash(0x00000 + 0x2000, 0x1000, blowfish_key + 0x48);
        writeFlash(0x10000 + 0x1000, 0x48, blowfish_key);
        writeFlash(0x10000 + 0x2000, 0x1000, blowfish_key + 0x48);

        writeFlash(0x00000 + 0x7E00, firm_size, firm);
        writeFlash(0x10000 + 0x7E00, firm_size, firm);
    }

};

DSTT dstt;