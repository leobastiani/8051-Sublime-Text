#include <c8051f120.h>           // Get SFR declarations
#include "types.h"
#include "spi.h"
#include "lcd.h"
#include "sd.h"
#include "dac.h"
#include "readdir.h"
#include "utils.h"
#include "wav.h"

//#pragma code                        // Include ASM in .LST file
void kbinit ( void );
uint8_t kbcheck ( void );
void dacinit ( void );
void itoa8(uint8_t i, char *a);
void itoa16(uint16_t i, char *a);
void itoa32(uint32_t i, char *a);

extern uint8_t  xdata numSongs;      // Number of songs found
extern uint32_t xdata songSector[32];// Starting sector of each file.

int main(void){
char idata foo2;
    wav_header xdata *header_ptr;
    uint32_t xdata current_sector = songSector[0];
    int i = 0, ping, pong;
    uint32_t bytestoplay, bytesread;
    uint8_t xdata buffer[2][512];
    char itoa_buf[4] = "\0\0\0\0";
    char itoa_16buf[6] = "\0\0\0\0\0\0";
    char itoa_32buf[11] = "\0\0\0\0\0\0\0\0\0\0\0";
    uint32_t increment = 1;
	uint16_t tmp;
    uint8_t displayToggle = 0;      // Because too lazy to make a bit a redo library
    //init procedures
    SFRPAGE = 0x0F;
    WDTCN = 0xDE;                   // Disable watchdog timer
    WDTCN = 0xAD;
    OSCICN = 0xC2;
    XBR2 |= 0x40;                   // Enable crossbar and weak pull-up
    EA = 1;                         // Enable all interrupts
    spi_set_divisor(0);
    spiinit();
    lcdinit();
    kbinit();
    dacinit();
    dac2init();
    SFRPAGE = 0;
	DACnCN = 0x9A;
    SFRPAGE = 1;
    DACnCN = 0x9A;
    dacrate(11025);
    dacstereo(0);                   // Set number of channels to mono
    dacvolume(1);
/*
    while(1) {
        uint8_t temp[1];
        PCON |= 1;
        temp[0] = kbcheck();
        if (temp[0] == 13) {
            lcdclear();
            lcdpos(0,0);
        }
        else if (temp[0] == 8) {
            _lcdcmd(0x10);
            lcdwrite(" ");
            _lcdcmd(0x10);
        }
        else if (temp[0] != 255)
            lcdwrite(temp);
    }
*/

    while(1){
        if(!spicardpresent()){
            lcdpos(0,0);
            lcdwrite("Insert microSD");
            lcdpos(1,0);
            lcdwrite("card...");    // Only write once.
        }
        while(!spicardpresent());   // Wait until card is detected
        if(!microSDinit()){         // If initialization fails, print error.
            lcdclear();
            lcdpos(0,0);
            lcdwrite("ERROR: microSD");
            lcdpos(1,0);
            lcdwrite("failure.");
        } else {
            spi_set_divisor(1);     // Set to max speed after initialisation
            readdir();              // Fill in numSongs and songSector[32]
            while(spicardpresent()){
                PCON |= 1;          // Power management setting
                for(i; i<numSongs; i++){
                    current_sector = songSector[i];
                    microSDread(current_sector, (uint8_t *) header_ptr);
                    switch(ntohs(header_ptr->numChannels)){
                        case 2:
                            dacstereo(1); // Stereo
                            break;
                        case 1:
                        default:
                            dacstereo(0); // Mono
                            break;
                    }
					tmp = ntohl(header_ptr->sampleRate);
					dacrate(tmp);
                    //dacrate(ntohs(header_ptr->sampleRate));
                    bytestoplay = ntohl(header_ptr->subchunk2Size);
                    lcdclear();
                    lcdpos(0,0);
                    itoa8(i, itoa_buf);
                    lcdwrite(itoa_buf);
                    lcdwrite(":");
                    lcdwrite(header_ptr->artist);
                    lcdpos(1,0);
                    lcdwrite(header_ptr->title);
                    if(!dacbusy()){
                        dacplay(404, (uint8_t xdata *)(header_ptr + 108));
                        //RIFF header + artist&title strings out of 512 B block
                    }
                    bytesread = 404;
                    bytestoplay -= 404;
                    ping = 0;
                    microSDread(current_sector + bytesread, buffer[pong]);
                    increment = 1;
                    // Well, we stop playing when bytestoplay = 0. If not, read
                    while(bytestoplay > 0){
                        if(!dacbusy()){
                            if(bytestoplay >= 512){
                                dacplay(512, buffer[pong]);
                                ping = pong;
                                pong = 1-ping;  // To keep ahead of the DAC
                                bytestoplay -= 512;
                                bytesread += 512;
                            }else{
                                dacplay(bytestoplay, buffer[pong]);
                                bytestoplay = 0; // Done playing song.
                            }
                            microSDread(current_sector + increment, buffer[ping]);
                            ++increment;
                            switch(kbcheck()){
                                case 'd':
                                case 'D':
                                    displayToggle = displayToggle ? 0 : 1;
                                    lcdclear()
                                    if(displayToggle){
                                        lcdwrite(header_ptr->artist);
                                        lcdpos(1,0);
                                        lcdwrite(header_ptr->title);
                                    }else{
                                        _lcddata
                                    }
                                    break;
                                case '+':
                                case '=':
                                    dacvolume(1);
                                    break;
                                case '-':
                                case '_':
                                    dacvolume(0);
                                    break;
                                case '>':
                                case '.':
                                    dacbalance(0);
                                    break;
                                case '<':
                                case ',':
                                    dacbalance(1);
                                    break;
                                case 'n':
                                case 'N':
                                    ++i;
                                    i %= numSongs-1; //next song with wrap around
                                    break;
                                case 'l':
                                case 'L':
                                    --i;
                                    i %= numSongs-1; //previous song with wrap
                                    break;
                                case '0':
                                case ')':
                                    i = 0;
                                    break;
                                case '1':
                                case '!':
                                    i = 1;
                                    break;
                                case '2':
                                case '@':
                                    i = 2;
                                    break;
                                case '3':
                                case '#':
                                    i = 3;
                                    break;
                                case '4':
                                case '$':
                                    i = 4;
                                    break;
                                case '5':
                                case '%':
                                    i = 5;
                                    break;
                                case '6':
                                case '^':
                                    i = 6;
                                    break;
                                case '7':
                                case '&':
                                    i = 7;
                                    break;
                                case '8':
                                case '*':
                                    i = 8;
                                    break;
                                case '9':
                                case '(':
                                    i = 9;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

void itoa(uint8_t i, char *a) {
    uint8_t p = 0;
    // char* a = "\0\0\0\0";

    if (i > 99) {
        a[p++] = i / 100 + 0x30;
        i %= 100;
    }
    if (i > 9) {
        a[p++] = i /= 10 + 0x30;
        i %= 10;
    }
    a[p] = i + 0x30;
}

void itoa16(uint16_t i, char *a) {
    uint8_t p = 0;
    // max is 65,535

    if (i > 9999) {
        a[p++] = i / 10000 + 0x30;
        i %= 10000;
    }
    if (i > 999) {
        a[p++] = i /= 1000 + 0x30;
        i %= 1000;
    }
    if (i > 99) {
        a[p++] = i / 100 + 0x30;
        i %= 100;
    }
    if (i > 9) {
        a[p++] = i /= 10 + 0x30;
        i %= 10;
    }
    a[p] = i + 0x30;
}

void itoa32(uint32_t i, char *a) {
    uint32_t p = 0;
    // max is 4,294,967,295
    if (i > 999999) {
        a[p++] = i / 1000000 + 0x30;
        i %= 1000000;
    }
    if (i > 99999) {
        a[p++] = i / 100000 + 0x30;
        i %= 100000;
    }
    if (i > 9999) {
        a[p++] = i / 10000 + 0x30;
        i %= 10000;
    }
    if (i > 999) {
        a[p++] = i /= 1000 + 0x30;
        i %= 1000;
    }
    if (i > 99) {
        a[p++] = i / 100 + 0x30;
        i %= 100;
    }
    if (i > 9) {
        a[p++] = i /= 10 + 0x30;
        i %= 10;
    }
    a[p] = i + 0x30;
}
