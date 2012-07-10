
#include "Stash.h"

// Map contains 256 bits; each bit represents a 64 byte block of memory
// for a total of 16384 bytes, or 16K
byte Stash::map[256/8];
// Two 64 byte blocks of data
Stash::Block Stash::bufs[2];

// Find a free block, allocate it, and return the index (0 to 255)
uint8_t Stash::allocBlock () {
  for (uint8_t i = 0; i < sizeof map; ++i)
    if (map[i] != 0)
      for (uint8_t j = 0; j < 8; ++j)
        if (bitRead(map[i], j)) {
          bitClear(map[i], j);
          return (i << 3) + j;
        }
  return 0;
}

// Release a block, flagging it again as free
void Stash::freeBlock (uint8_t block) {
  // block>>3 gives you the top 5 bytes addressing into the 32 byte array,
  // then we set the bit at block & 7 (which is the right-most three bits)
  // which basically means that "block" can be an integer from 0 o 255 and
  // this will "free" the matching bit. (flag it as free, set it to 1)
  bitSet(map[block>>3], block & 7);
}

// Fetches a byte from the specified block and specified offset;
// if the block in question isn't currently in local memory then
// fetch it from the ethernet chip
uint8_t Stash::fetchByte (uint8_t blk, uint8_t off) {
  return blk == bufs[0].bnum ? bufs[0].bytes[off] :
          blk == bufs[1].bnum ? bufs[1].bytes[off] :
           ether.peekin(blk, off);
}

// Clear all bits in the map
void Stash::initMap (uint8_t last) {
  while (--last > 0)
    freeBlock(last);
}

// Load a block of data from the ethernet chip; stash what is currently in memory to the chip
// so that we can load the block we want
void Stash::load (uint8_t idx, uint8_t blk) {
  if (blk != bufs[idx].bnum) {
    if (idx == 0) {
      ether.copyout(bufs[idx].bnum, bufs[idx].bytes);
      if (blk == bufs[1].bnum)
        bufs[1].bnum = 255; // forget read page if same
    } else if (blk == bufs[0].bnum) {
      // special case: read page is same as write buffer
      memcpy(&bufs[1], &bufs[0], sizeof bufs[0]);
      return;
    }
    bufs[idx].bnum = blk;
    ether.copyin(bufs[idx].bnum, bufs[idx].bytes);
  }
}

// Loop through the bitmap to see how many free blocks there are
uint8_t Stash::freeCount () {
  uint8_t count = 0;
  for (uint8_t i = 0; i < 256/8; ++i)
    for (uint8_t m = 0x80; m != 0; m >>= 1)
      if (map[i] & m)
        ++count;
  return count;
}

// Allocate a block then load that block into local memory buffer 0
uint8_t Stash::create () {
  uint8_t blk = allocBlock();
  load(0, blk);
  bufs[0].head.count = 0;
  bufs[0].head.first = bufs[0].head.last = blk;
  bufs[0].tail = sizeof (StashHeader);
  bufs[0].next = 0;
  return open(blk);
}

uint8_t Stash::open (uint8_t blk) {
  curr = blk;
  offs = sizeof (StashHeader);
  load(1, curr);
  memcpy((StashHeader*) this, bufs[1].bytes, sizeof (StashHeader));
  return curr;
}

void Stash::save () {
  load(0, first);
  memcpy(bufs[0].bytes, (StashHeader*) this, sizeof (StashHeader));
  if (bufs[1].bnum == first)
    load(1, 0); // invalidates original in case it was the same block
}

void Stash::release () {
  while (first > 0) {
    freeBlock(first);
    first = ether.peekin(first, 63);
  }
}

void Stash::put (char c) {
  load(0, last);
  uint8_t t = bufs[0].tail;
  bufs[0].bytes[t++] = c;
  if (t <= 62)
    bufs[0].tail = t;
  else {
    bufs[0].next = allocBlock();
    last = bufs[0].next;
    load(0, last);
    bufs[0].tail = bufs[0].next = 0;
    ++count;
  }
}

char Stash::get () {
  load(1, curr);
  if (curr == last && offs >= bufs[1].tail)
    return 0;
  uint8_t b = bufs[1].bytes[offs];
  if (++offs >= 63 && curr != last) {
    curr = bufs[1].next;
    offs = 0;
  }
  return b;
}

uint16_t Stash::size () {
  return 63 * count + fetchByte(last, 62) - sizeof (StashHeader);
}

void Stash::prepare (PGM_P fmt, ...) {
  Stash::load(0, 0);
  word* segs = Stash::bufs[0].words;
  *segs++ = strlen_P(fmt);
  *segs++ = (word) fmt;
  va_list ap;
  va_start(ap, fmt);
  for (;;) {
    char c = pgm_read_byte(fmt++);
    if (c == 0)
      break;
    if (c == '$') {
      word argval = va_arg(ap, word), arglen = 0;
      switch (pgm_read_byte(fmt++)) {
        case 'D': {
          char buf[7];
          wtoa(argval, buf);
          arglen = strlen(buf);
          break;
        }
        case 'S':
          arglen = strlen((const char*) argval);
          break;
        case 'F':
          arglen = strlen_P((PGM_P) argval);
          break;
        case 'E': {
          byte* s = (byte*) argval;
          char d;
          while ((d = eeprom_read_byte(s++)) != 0)
            ++arglen;
          break;
        }
        case 'H': {
          Stash stash (argval);
          arglen = stash.size();
          break;
        }
      }
      *segs++ = argval;
      Stash::bufs[0].words[0] += arglen - 2;
    }
  }
  va_end(ap);
}

word Stash::length () {
  Stash::load(0, 0);
  return Stash::bufs[0].words[0];
}

void Stash::extract (word offset, word count, void* buf) {
  Stash::load(0, 0);
  word* segs = Stash::bufs[0].words;
  PGM_P fmt = (PGM_P) *++segs;
  Stash stash;
  char mode = '@', tmp[7], *ptr = NULL, *out = (char*) buf;
  for (word i = 0; i < offset + count; ) {
    char c = 0;
    switch (mode) {
      case '@': {
        c = pgm_read_byte(fmt++);
        if (c == 0)
          return;
        if (c != '$')
          break;
        word arg = *++segs;
        mode = pgm_read_byte(fmt++);
        switch (mode) {
          case 'D':
            wtoa(arg, tmp);
            ptr = tmp;
            break;
          case 'S':
          case 'F':
          case 'E':
            ptr = (char*) arg;
            break;
          case 'H':
            stash.open(arg);
            ptr = (char*) &stash;
            break;
        }
        continue;
      }
      case 'D':
      case 'S':
        c = *ptr++;
        break;
      case 'F':
        c = pgm_read_byte(ptr++);
        break;
      case 'E':
        c = eeprom_read_byte((byte*) ptr++);
        break;
      case 'H':
        c = ((Stash*) ptr)->get();
        break;
    }
    if (c == 0) {
      mode = '@';
      continue;
    }
    if (i >= offset)
      *out++ = c;
    ++i;
  }
}

void Stash::cleanup () {
  Stash::load(0, 0);
  word* segs = Stash::bufs[0].words;
  PGM_P fmt = (PGM_P) *++segs;
  for (;;) {
    char c = pgm_read_byte(fmt++);
    if (c == 0)
      break;
    if (c == '$') {
      word arg = *++segs;
      if (pgm_read_byte(fmt++) == 'H') {
        Stash stash (arg);
        stash.release();
      }
    }
  }
}


