
/***********************************************************
  This file contains the code for the L2 cache. It is a 
  1MB direct-mapped, write-back cache.

   Since a word is 32 bits (4 bytes) and a cache line is 8 words, 
   there are a total of 32 bytes (which is 2^5 bytes) in a cache
   line. Thus, the lowest 5 bits (i.e. log 32) of an address are used to 
   specify the byte offset within the cache line.

   Since the total amount of data in the L2 cache is 1MB 
   (i.e. 2^20 bytes) and there are 32 bytes (i.e. 2^5 bytes)
   per cache line, the number of cache lines is:

      (2^20 bytes)/(2^5 bytes/cache line) = 2^15 cache lines

   Therefore, the index within an address used to select the cache 
   line is 15 bits (i.e. log 2^15).
   
   Since the offset is the lowest 5 bits and the index is the next
   lowest 15 bits of a 32-bit address, the tag bits are the remaining 
   32-(15+5) = 12 bits.
   
   So, the 32-bit address is decomposed as follows:

              12                 15           5      
    ------------------------------------------------
   |         tag      |       index       | offset |
    ------------------------------------------------


   Each L2 cache entry is structured as follows:

    1 1    18       12            
   ------------------------------------------------
   |v|d|reserved|  tag  |  8-word cache line data  |
   ------------------------------------------------

  where "v" is the valid bit and "d" is the dirty bit.
  The 18-bit "reserved" field is an artifact of using
  C -- it wouldn't be in the actual cache hardware.

************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "l2_cache.h"

//number of cache entries (2^15)
#define L2_NUM_CACHE_ENTRIES (1 << 15)

/***************************************************
 This struct defines the structure of a single cache
 entry in the L2 cache. It has the following fields:
   v_d_tag: 32-bit unsigned word containing the 
            valid (v) bit at bit 31 (leftmost bit),
            the dirty bit (d) at bit 30, and the tag 
            in bits 0 through 15 (the 16 rightmost bits)
  cache_line: an array of 8 words, constituting a single
              cache line.
****************************************************/
typedef struct
{
  uint32_t v_d_tag;
  uint32_t cache_line[WORDS_PER_CACHE_LINE];
} L2_CACHE_ENTRY;

//The valid bit is bit 31 (leftmost bit) of v_d_tag word
//The mask is 1 shifted left by 31
#define L2_VBIT_MASK (0x1 << 31)

//The dirty bit is bit 30 (second to leftmost bit) of v_d_tag word
//The mask is 1 shifted left by 30
#define L2_DIRTYBIT_MASK (0x1 << 30)

//The tag is lowest 12 bits of v_d_tag word, so the mask is FFF hex
#define L2_ENTRY_TAG_MASK 0xfff

// The L2 is just an array off cache entries

L2_CACHE_ENTRY l2_cache[L2_NUM_CACHE_ENTRIES];

// Note that since l2_cache is an array of L2_CACHE_ENTRY structs,
// not an array of pointers, you would NOT use "->" to access the
// a field of an entry. For example, you could write
// l2_cache[i].v_d_tag, but NOT l2_cache[i]->v_d_tag.

/************************************************
            l2_initialize()

This procedure initializes the L2 cache by clearing
(i.e. setting to 0) the valid bit of each cache entry.
************************************************/

void l2_initialize()
{
  //Code HERE
  for (int i = 0; i < L2_NUM_CACHE_ENTRIES; i++)
  {
    l2_cache[i].v_d_tag = l2_cache[i].v_d_tag & ~(L2_VBIT_MASK); //consider just setting this equal to zero
  }
}

//The upper 12 bits (bits 20-31) of an address are used as the tag bits.
//So the mask is 12 ones (so FFF hex) shifted left by 20 bits.
#define L2_ADDRESS_TAG_MASK (0xfff << 20)
#define L2_ADDRESS_TAG_SHIFT 20

//Bits 5-19 (so 15 bits in total) of an address specifies the index of the
//cache line within the L2 cache.
//The value of the mask is 15 ones (so 7FFF hex) shifted left by 5 bits
#define L2_INDEX_MASK (0x7fff << 5)
#define L2_INDEX_SHIFT 5

#define L2_HIT_STATUS_MASK 0x1

/****************************************************

          l2_cache_access()
          
This procedure implements the reading and writing of cache lines from
and to the L2 cache. The parameters are:

address:  unsigned 32-bit address. This address can be anywhere within
          a cache line.

write_data:  an array of unsigned 32-bit words. On a write operation,
             if there is a cache hit, 8 32-bit words are copied from this
             array to the appropriate cache line in the L2 cache.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: an array of unsigned 32-bit integers. On a read operation,
           if there is a cache hit, 8 32-bit words are copied from the 
           appropriate cache line in the L2 cache to this array.

status: this in an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether a cache hit occurred or not:
              cache hit: bit 0 of status = 1
              cache miss: bit 0 of status = 0

If the access results in a cache miss, then the only
effect is to set the lowest bit of the status byte to 0.

**************************************************/

void l2_cache_access(uint32_t address, uint32_t write_data[],
                     uint8_t control, uint32_t read_data[], uint8_t *status)
{

  //Extract from the address the index of the cache entry in the cache.
  //Use the L2_INDEX_MASK mask to mask out the appropriate
  //bits of the address and L2_INDEX_SHIFT
  //to shift the appropriate amount.

  uint32_t index_bits = (address & L2_INDEX_MASK) >> L2_INDEX_SHIFT;

  //Extract from the address the tag bits.
  //Use the L2_ADDRESS_TAG_MASK mask to mask out the appropriate
  //bits of the address and L2_ADDRESS_TAG_SHIFT
  //to shift the appropriate amount.

  uint32_t tag_bits = (address & L2_ADDRESS_TAG_MASK) >> L2_ADDRESS_TAG_SHIFT;

  //if the selected cache entry has a zero valid bit or
  //if the entry's tag does not match the tag bits of
  //the address, then it is a cache miss: Set the
  //low bit of the status byte appropriately.

  if (!(l2_cache[index_bits].v_d_tag & L2_VBIT_MASK) || ((l2_cache[index_bits].v_d_tag & L2_ENTRY_TAG_MASK) != tag_bits))
  {
    *status &= ~(0x1);
    return; //consider commenting this out
  }

  //Otherwise, it's a cache hit:
  //If the read-enable bit of the control parameter is set (i.e. is 1)
  //then copy the cache line data in the cache entry into the read_data
  //array. If the write-enable bit of the control parameter is set, then
  //copy the data in the write_data array into the cache line starting at
  //the cache line index and set the dirty bit. Set the low bit
  //of the status byte appropriately.

  //CODE HERE //0x1 //0x2

  if (control & READ_ENABLE_MASK)
  {
    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
    {
      read_data[i] = l2_cache[index_bits].cache_line[i];
    }
  }

  if (control & WRITE_ENABLE_MASK)
  {
    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
    {
      l2_cache[index_bits].cache_line[i] = write_data[i]; //what do you increment the index_bits by, is that what it is supposed to be
    }
    l2_cache[index_bits].v_d_tag = l2_cache[index_bits].v_d_tag | L2_DIRTYBIT_MASK;
    // cache hit: bit 0 of status = 1
  }
  *status = *status | 0x1;
  return;
}

/********************************************************

             l2_insert_line()

This procedure inserts a new cache line into the L2 cache.

The parameters are:

address: 32-bit memory address for the new cache line.

write_data: an array of unsigned 32-bit words containing the 
            cache line data to be inserted into the cache.

evicted_writeback_address: a 32-bit output parameter (thus,
          a pointer to it is passed) that, if the cache line
          being evicted needs to be written back to memory,
          should be assigned the memory address for the evicted
          cache line.
          
evicted_writeback_data: an array of 32-bit words. If the cache 
          line being evicted needs to be written back to memory,
          the cache line data for the evicted cache line should be
          written to this array.  Since there are 8 words per cache line, 
          the actual parameter will be an array of at least 8 words.

status: this in an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether the evicted cache line needs to be
        written back to memory or not, as follows:
            0: no write-back required
            1: evicted cache line needs to be written back.

*********************************************************/

void l2_insert_line(uint32_t address, uint32_t write_data[],
                    uint32_t *evicted_writeback_address,
                    uint32_t evicted_writeback_data[],
                    uint8_t *status)
{

  //Extract from the address the index of the cache entry in the cache.
  //See l2_cache_access, above.

  //CODE HERE
  uint32_t evicted_index_bits = (address & L2_INDEX_MASK) >> L2_INDEX_SHIFT;

  //Extract from the address the tag bits.
  //See l2_cache_access, above.

  //CODE HERE
  uint32_t evicted_tag_bits = (address & L2_ADDRESS_TAG_MASK) >> L2_ADDRESS_TAG_SHIFT;
  //uint32_t evicted_tag_bits = l2_cache[evicted_index_bits].v_d_tag & L2_ENTRY_TAG_MASK;

  //If the cache entry has a zero valid bit or a zero dirty bit,
  //then the entry can simply be overwritten with the new line.
  //Copy the data from write_data to the cache line in the entry,
  //set the valid bit of the entry, clear the dirty bit of the
  //entry, and write the tag bits of the address into the tag of
  //the entry. Clear the low bit of the status byte
  //to indicate that no write-back is needed. Nothing further
  //needs to be done, the procedure can return.

  //CODE HERE
  if (!(l2_cache[evicted_index_bits].v_d_tag & L2_DIRTYBIT_MASK) || !(l2_cache[evicted_index_bits].v_d_tag & L2_VBIT_MASK))
  {
    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
    {

      l2_cache[evicted_index_bits].cache_line[i] = write_data[i];
      //write
    }
    l2_cache[evicted_index_bits].v_d_tag = L2_VBIT_MASK | evicted_tag_bits;
    //l2_cache[evicted_index_bits].v_d_tag = (l2_cache[evicted_index_bits].v_d_tag & (L2_ENTRY_TAG_MASK)) | evicted_tag_bits;

    *status = *status & (~0x1);
    return;
  }

  //Otherwise (i.e. both the valid and dirty bits are 1), the
  //current entry has to be written back before the
  //being overwritten by the new cache line:
  //The address to write the current entry back to is constructed from the
  //entry's tag and the index in the cache by:
  // (evicted_entry_tag << L2_ADDRESS_TAG_SHIFT) | (index << L2_INDEX_SHIFT)
  //This address should be written to the evicted_writeback_address output
  //parameter.
  //The cache line data in the current entry should be copied to the
  //evicted_writeback_data_array.
  //The low bit of the status byte should be set to 1 to indicate that
  //the write-back is needed.

  //CODE HERE
  //if ((l2_cache[evicted_index_bits].v_d_tag & L2_DIRTYBIT_MASK) && (l2_cache[evicted_index_bits].v_d_tag & L2_VBIT_MASK))
  //{
  //*(uint32_t *)evicted_writeback_address;
  *evicted_writeback_address = (((l2_cache[evicted_index_bits].v_d_tag & L2_ENTRY_TAG_MASK) << L2_ADDRESS_TAG_SHIFT) | (evicted_index_bits << L2_INDEX_SHIFT));

  for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
  {
    evicted_writeback_data[i] = l2_cache[evicted_index_bits].cache_line[i]; //use evicted_writeback_address
  }
  *status = *status | 0x1;
  //}

  //Then, copy the data from write_data to the cache line in the entry,
  //set the valid bit of the entry, clear the dirty bit of the
  //entry, and write the tag bits of the address into the tag of
  //the entry.

  for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
  {
    l2_cache[evicted_index_bits].cache_line[i] = write_data[i];
  }
  l2_cache[evicted_index_bits].v_d_tag = L2_VBIT_MASK | evicted_tag_bits;
  //l2_cache[evicted_index_bits].v_d_tag = (l2_cache[evicted_index_bits].v_d_tag & ~(L2_ENTRY_TAG_MASK)) | evicted_tag_bits

  return;
}