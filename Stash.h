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

#ifndef Stash_h
#define Stash_h


#if ARDUINO >= 100
  #include <Arduino.h> // Arduino 1.0
  #define WRITE_RESULT size_t
  #define WRITE_RETURN return 1;
#else
  #include <WProgram.h> // Arduino 0022
  #define WRITE_RESULT void
  #define WRITE_RETURN
#endif

typedef struct {
  uint8_t count;     // number of allocated pages
  uint8_t first;     // first allocated page
  uint8_t last;      // last allocated page
} StashHeader;

class Stash : public /*Stream*/ Print, private StashHeader {
  uint8_t curr;      // current page
  uint8_t offs;      // current offset in page

  // This is a 64 byte block that can be accessed either as bytes or words;
  // it also contains a byte number
  typedef struct {
    union {
      uint8_t bytes[64];
      uint16_t words[32];
      struct {
        StashHeader head; // 3 bytes long
        uint8_t filler[59];
        uint8_t tail;
        uint8_t next;
      };
    };
    uint8_t bnum;
  } Block;

  static uint8_t allocBlock ();
  static void freeBlock (uint8_t block);
  static uint8_t fetchByte (uint8_t blk, uint8_t off);

  static Block bufs[2];
  static uint8_t map[256/8];

public:
  static void initMap (uint8_t last);
  static void load (uint8_t idx, uint8_t blk);
  static uint8_t freeCount ();

  Stash () : curr (0) { first = 0; }
  Stash (uint8_t fd) { open(fd); }

  uint8_t create ();
  uint8_t open (uint8_t blk);
  void save ();
  void release ();

  void put (char c);
  char get ();
  uint16_t size ();

  virtual WRITE_RESULT write(uint8_t b) { put(b); WRITE_RETURN }

  static void prepare (PGM_P fmt, ...);
  static uint16_t length ();
  static void extract (uint16_t offset, uint16_t count, void* buf);
  static void cleanup ();

  friend void dumpBlock (const char* msg, uint8_t idx); // optional
  friend void dumpStash (const char* msg, void* ptr);   // optional
};

#endif
