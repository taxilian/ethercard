// This code slightly follows the conventions of, but is not derived from:
//      EHTERSHIELD_H library for Arduino etherShield
//      Copyright (c) 2008 Xing Yu.  All right reserved. (this is LGPL v2.1)
// It is however derived from the enc28j60 and ip code (which is GPL v2)
//      Author: Pascal Stang
//      Modified by: Guido Socher
//      DHCP code: Andrew Lindsay
// Hence: GPL V2
//
// 2010-05-19 <jc@wippler.nl>

#include <EtherCard.h>
#include <stdarg.h>
#include <avr/eeprom.h>

void BufferFiller::emit_p(PGM_P fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (;;) {
        char c = pgm_read_byte(fmt++);
        if (c == 0)
            break;
        if (c != '$') {
            *ptr++ = c;
            continue;
        }
        c = pgm_read_byte(fmt++);
        switch (c) {
            case 'D':
                wtoa(va_arg(ap, word), (char*) ptr);
                break;
            case 'L':
                ltoa(va_arg(ap, long), (char*) ptr, 10);
                break;
            case 'S':
                strcpy((char*) ptr, va_arg(ap, const char*));
                break;
            case 'F': {
                PGM_P s = va_arg(ap, PGM_P);
                char d;
                while ((d = pgm_read_byte(s++)) != 0)
                    *ptr++ = d;
                continue;
            }
            case 'E': {
                byte* s = va_arg(ap, byte*);
                char d;
                while ((d = eeprom_read_byte(s++)) != 0)
                    *ptr++ = d;
                continue;
            }
            default:
                *ptr++ = c;
                continue;
        }
        ptr += strlen((char*) ptr);
    }
    va_end(ap);
}

EtherCard ether;

uint8_t EtherCard::mymac[6];  // my MAC address
uint8_t EtherCard::myip[4];   // my ip address
uint8_t EtherCard::mymask[4]; // my net mask
uint8_t EtherCard::gwip[4];   // gateway
uint8_t EtherCard::dhcpip[4]; // dhcp server
uint8_t EtherCard::dnsip[4];  // dns server
uint8_t EtherCard::hisip[4];  // dns result
uint16_t EtherCard::hisport = 80; // tcp port to browse to

uint8_t EtherCard::begin (const uint16_t size,
                           const uint8_t* macaddr,
                            uint8_t csPin) {
  Stash::initMap(56);
  copyMac(mymac, macaddr);
  return initialize(size, mymac, csPin);
}

bool EtherCard::staticSetup (const uint8_t* my_ip,
                              const uint8_t* gw_ip,
                               const uint8_t* dns_ip) {
  if (my_ip != 0)
    copyIp(myip, my_ip);
  if (gw_ip != 0)
    setGwIp(gw_ip);
  if (dns_ip != 0)
    copyIp(dnsip, dns_ip);
  return true;
}
