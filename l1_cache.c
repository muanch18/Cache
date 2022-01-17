
/************************************************************

The L1 cache is a 64KB, 4-way set associative, write-back cache.  
As with the rest of the memory subsystem, a cache line is 8 words,
where each word is 32 bits (4 bytes).

The size of the L1 cache is 64KB of data
                          = 16K words  (since there are 4 bytes/word)
                          = 2K cache lines (since there are 8 words/cache line)
                          = 512 sets (since there are 4 cache lines/set)

Each cache entry has: valid bit, reference bit, dirty bit,
                     tag, and cache-line data.

An address is decomposed as follows (from lsb to msb):
2 bits are used for byte offset within a word (bits 0-1)
3 bits are used for word offset within a cache line (bits 2-4)
9 bits are used for the set index, since there are 512 = 2^9 sets per cache (bits 5-13).
18 bits remaining are used as the tag (bits 14-31)


           18              9           3        2
    ------------------------------------------------
   |      tag       |    set      | word   |  byte  |
   |                |   index     | offset | offset |
    ------------------------------------------------


Each cache entry is structured as follows:

    1 1 1    11      18 
    ------------------------------------------------
   |v|r|d|reserved|  tag  |  8-word cache line data |
    ------------------------------------------------

where:
  v is the valid bit
  r is the reference bit
  d is the dirty bit
and the 11 "reserved" bits are an artifact of using C. The
cache hardware would not have those.

**************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "l1_cache.h"

/***************************************************
This struct defines the structure of a single cache
entry in the L1 cache. It has the following fields:
  v_r_d_tag: 32-bit unsigned word containing the 
           valid (v) bit at bit 31 (leftmost bit),
           the reference (r) bit at bit 30,
           the dirty bit (d) at bit 29, and the tag 
           in bits 0 through 17 (the 18 rightmost bits)
  cache_line: an array of 8 words, constituting a single
              cache line.
****************************************************/

typedef struct
{
  uint32_t v_r_d_tag;
  uint32_t cache_line[WORDS_PER_CACHE_LINE];
} L1_CACHE_ENTRY;

//4-way set-associative cache, so there are
//4 cache lines per set.
#define L1_LINES_PER_SET 4

/***************************************************
  This structure defines an L1 cache set. Its only
  field, lines, is an array of four cache lines.
***************************************************/

typedef struct
{
  L1_CACHE_ENTRY lines[L1_LINES_PER_SET];
} L1_CACHE_SET;

//There are 512 sets in the L1 cache
#define L1_NUM_CACHE_SETS 512

//The L1 cache itself is just an array of 512 cache sets.
L1_CACHE_SET l1_cache[L1_NUM_CACHE_SETS];

//Mask for v bit: Bit 31 of v_r_d_tag
#define L1_VBIT_MASK (1 << 31)

//Mask for r bit: Bit 30 of v_r_d_tag
#define L1_RBIT_MASK (1 << 30)

//Mask for d bit: Bit 29 of v_r_d_tag
#define L1_DIRTYBIT_MASK (1 << 29)

//The tag is the low 18 bits of v_r_d_tag
//The mask is just 18 ones, which is 3FFFF hex
#define L1_ENTRY_TAG_MASK 0x3ffff

//Bits 2-4 of an address specifies the offset of the addressed
//word within the cache line
// Mask is 11100 in binary = 0x1C

#define WORD_OFFSET_MASK 0x1C

//After masking to extract the word offset, it needs
//to be shifted to the right by 2.
#define WORD_OFFSET_SHIFT 2

//The upper 18 bits (bits 14-31) of an address are used as the tag bits
//The mask is 18 ones (so 3FFFF hex) shifted left by 14 bits.
#define L1_ADDRESS_TAG_MASK (0x3FFFF << 14)

//After masking to extract the tag from the address, it needs to
//be shifted right by 14.
#define L1_ADDRESS_TAG_SHIFT 14

//Bits 5-13 are used to extract the set index from an address.
//The mask is 9 ones (so 1FF hex) shifted left by 5.
#define L1_SET_INDEX_MASK (0x1ff << 5)

//After masking to extract the set index from an address, it
//needs to be shifted to the right by 5
#define L1_SET_INDEX_SHIFT 5

//This can be used to set or clear the lowest bit of the status
//register to indicate a cache hit or miss.
#define L1_CACHE_HIT_MASK 0x1

/************************************************
            l2_initialize()

This procedure initializes the L1 cache by clearing
the valid bit of each cache entry in each set in
the cache.
************************************************/

void l1_initialize()
{
  for (int i = 0; i < L1_NUM_CACHE_SETS; i++)
  {
    for (int j = 0; j < L1_LINES_PER_SET; j++)
    {
      l1_cache[i].lines[j].v_r_d_tag = 0;
    }
  }
}

/**********************************************************

             l1_cache_access()

This procedure implements the reading or writing of a single word
to the L1 cache. 

The parameters are:

address:  unsigned 32-bit address. This address can be anywhere within
          a cache line.

write_data: a 32-bit word. On a write operation, if there is a cache
          hit, write_data is copied to the appropriate word in the
          appropriate cache line.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: a 32-bit output parameter (thus, a pointer to it is passed).
         On a read operation, if there is a cache hit, the appropriate
         word of the appropriate cache line in the cache is copied
         to read_data.

status: this in an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether a cache hit occurred or not:
              cache hit: bit 0 of status = 1
              cache miss: bit 0 of status = 0

If the access results in a cache miss, then the only
effect is to set the lowest bit of the status byte to 0.

**********************************************************/

void l1_cache_access(uint32_t address, uint32_t write_data,
                     uint8_t control, uint32_t *read_data, uint8_t *status)
{

  //Extract from the address the index of the cache set in the cache.
  //Use the L1_SET_INDEX_MASK mask to mask out the appropriate
  //bits of the address and L1_SET_INDEX_SHIFT to shift the
  //bits the appropriate amount.

  u_int32_t index_set_bits = (address & L1_SET_INDEX_MASK) >> L1_SET_INDEX_SHIFT;

  //Extract from the address the tag bits.
  //Use the L1_ADDRESS_TAG_MASK mask to mask out the appropriate
  //bits of the address and L1_ADDRESS_TAG_SHIFT to shift the
  //bits the appropriate amount.

  u_int32_t tag_bits = (address & L1_ADDRESS_TAG_MASK) >> L1_ADDRESS_TAG_SHIFT;

  //Extract from the address the word offset within the cache line.
  //Use the WORD_OFFSET_MASK to mask out the appropriate bits of
  //the address and WORD_OFFSET_SHIFT to shift the bits the
  //appropriate amount.

  u_int32_t word_offset_bits = (address & WORD_OFFSET_MASK) >> WORD_OFFSET_SHIFT;

  //Within the set specified by the set index extracted from the address,
  //look through the cache entries for an entry that 1) has its valid
  //bit set AND 2) has a tag that matches the tag extracted from the address.

  //If no such entry exists in the set, then the result is a cache miss.
  //The low bit of the status output parameter should be set to 0. There
  //is nothing further to do in this case, the function can return.

  //Otherwise, if an entry is found with a set valid bit and a matching tag,
  //then it is a cache hit. The reference bit of the cache entry should be set
  //and the low bit of status output parameter should be set to 1.

  //If a read operation was specified, the appropriate word (as specified by
  //the word offset extracted from the address) of the entry's
  //cache line data should be written to read_data.

  //If a write operation was specified, the value of write_data should be
  //written to the appropriate word of the entry's cache line data and
  //the entry's dirty bit should be set.

  //CODE HERE
  for (int i = 0; i < L1_LINES_PER_SET; i++)
  {
    if ((l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_VBIT_MASK) && ((l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_ENTRY_TAG_MASK) == tag_bits))
    {
      l1_cache[index_set_bits].lines[i].v_r_d_tag = l1_cache[index_set_bits].lines[i].v_r_d_tag | L1_RBIT_MASK;
      if (control & READ_ENABLE_MASK)
      {
        *read_data = l1_cache[index_set_bits].lines[i].cache_line[word_offset_bits];
      }
      if (control & WRITE_ENABLE_MASK)
      {
        l1_cache[index_set_bits].lines[i].cache_line[word_offset_bits] = write_data; //this should be of some pointer type
        l1_cache[index_set_bits].lines[i].v_r_d_tag = l1_cache[index_set_bits].lines[i].v_r_d_tag | L1_DIRTYBIT_MASK;
      }
      *status = *status | (L1_CACHE_HIT_MASK);
      return;
    }
  }
  *status = *status & ~(L1_CACHE_HIT_MASK);
}

/************************************************************

                 l1_insert_line()

This procedure inserts a new cache line into the L1 cache.

The parameters are:

address: 32-bit address of the new cache line.

write_data: an array of unsigned 32-bit words containing the 
            cache line data to be inserted into the cache.

evicted_writeback_address: a 32-bit output parameter (thus,
          a pointer to it is passed) that, if the cache line
          being evicted needs to be written back to memory,
          should be assigned the memory address for the evicted
          cache line.
          
evicted_writeback_data: an array of 32-bit words. If the cache 
          line being evicted needs to be written back to memory,
          the cache line data for the evicted cache line should
          be copied to this array. Since there are 8 words per 
          cache line, the actual parameter will be an array of 
          at least 8 words.

status: this in an 8-bit output parameter (thus, a pointer to it is 
        passed).  The lowest bit of this byte should be set to 
        indicate whether the evicted cache line needs to be
        written back to memory or not, as follows:
            0: no write-back required
            1: evicted cache line needs to be written back.


 The cache replacement algorithm uses a simple NRU
 algorithm. A cache entry (among the cache entries in the set) is 
 chosen to be written to in the following order of preference:
    - valid bit = 0
    - reference bit = 0 and dirty bit = 0
    - reference bit = 0 and dirty bit = 1
    - reference bit = 1 and dirty bit = 0
    - reference bit = 1 and dirty bit = 1
*********************************************************/

void l1_insert_line(uint32_t address, uint32_t write_data[],
                    uint32_t *evicted_writeback_address,
                    uint32_t evicted_writeback_data[],
                    uint8_t *status)
{

  //Extract from the address the index of the set in the cache.
  //see l1_cache_access above

  u_int32_t index_set_bits = (address & L1_SET_INDEX_MASK) >> L1_SET_INDEX_SHIFT;
  //CODE HERE

  //Extract from the address the tag bits.
  //see l1_cache_access above.

  u_int32_t tag_bits = (address & L1_ADDRESS_TAG_MASK) >> L1_ADDRESS_TAG_SHIFT;
  //CODE HERE

  // The cache replacement algorithm uses a simple NRU
  // algorithm. A cache entry (among the cache entries in the set) is
  // chosen to be written to in the following order of preference:
  //    - valid bit = 0
  //    - reference bit = 0 and dirty bit = 0
  //    - reference bit = 0 and dirty bit = 1
  //    - reference bit = 1 and dirty bit = 0
  //    - reference bit = 1 and dirty bit = 1
  //  The search loops through the entries in the set. If it
  //  finds a valid bit = 0, then that entry can be overwritten and
  //  we can exit the loop.
  //  Otherwise, we remember the *first* line we encounter which has r=0 and d=0,
  //  the *first* line that has r=0 and d=1, etc. When we're done looping,
  //  we choose the entry with the highest preference on the above list to evict.

  //This variable is used to store the index within the set
  //of a cache entry that has its r bit = 0 and its dirty bit = 0.
  //Initialize it to ~0x0 (i.e. all 1's) to indicate an invalid value.
  uint32_t r0_d0_index = ~0x0;

  //This variable is used to store the index within the set
  //of a cache entry that has its r bit = 0 and its dirty bit = 1.
  uint32_t r0_d1_index = ~0x0;

  //This variable isused to store the index within the set
  //of a cache entry that has its r bit = 1 and its dirty bit = 0.
  uint32_t r1_d0_index = ~0x0;

  //In a loop, iterate though each entry in the set.

  //LOOP STARTS HERE
  for (int i = 0; i < L1_LINES_PER_SET; i++)
  {

    // if the current entry has a zero v bit, then overwrite
    // the cache line in the entry with the data in write_data,
    // set the v bit of the entry, clear the dirty and reference bits,
    // and write the new tag to the entry. Set the low bit of the status
    // output parameter to 0 to indicate the evicted line does not need
    // to be written back. There is nothing further to do, the procedure
    // can return

    //CODE HERE
    if (!(l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_VBIT_MASK))
    {
      for (int j = 0; j < WORDS_PER_CACHE_LINE; j++)
      {
        l1_cache[index_set_bits].lines[i].cache_line[j] = write_data[j];
      }
      l1_cache[index_set_bits].lines[i].v_r_d_tag = l1_cache[index_set_bits].lines[i].v_r_d_tag | L1_VBIT_MASK;
      l1_cache[index_set_bits].lines[i].v_r_d_tag = l1_cache[index_set_bits].lines[i].v_r_d_tag & ~(L1_DIRTYBIT_MASK);
      l1_cache[index_set_bits].lines[i].v_r_d_tag = l1_cache[index_set_bits].lines[i].v_r_d_tag & ~(L1_RBIT_MASK);
      l1_cache[index_set_bits].lines[i].v_r_d_tag = (l1_cache[index_set_bits].lines[i].v_r_d_tag & ~(L1_ENTRY_TAG_MASK)) | tag_bits;
      *status &= ~(L1_CACHE_HIT_MASK);
      return;
    }
    //  Otherwise, we remember the first entry we encounter which has r=0 and d=0,
    //  the first entry that has r=0 and d=1, etc.

    //what does it mean by remember the first entry, like set it to a variable?
    else
    {
      if (!(l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_RBIT_MASK) && !(l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_DIRTYBIT_MASK))
      {
        if (r0_d0_index == (~0x0))
        {
          r0_d0_index = i;
        }
      }
      else if (!(l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_RBIT_MASK) && (l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_DIRTYBIT_MASK))
      {
        if (r0_d1_index == (~0x0))
        {
          r0_d1_index = i;
        }
      }
      else if ((l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_RBIT_MASK) && !(l1_cache[index_set_bits].lines[i].v_r_d_tag & L1_DIRTYBIT_MASK))
      {
        if (r1_d0_index == (~0x0))
        {
          r1_d0_index = i;
        }
      }
    }
  }
  //LOOP ENDS HERE

  //When we're done looping, we choose the entry with the highest preference
  //on the above list to evict.

  //CODE HERE
  u_int32_t curr_entry;
  if (r0_d0_index != (~0x0))
  {
    curr_entry = r0_d0_index;
  }

  else if (r0_d1_index != (~0x0))
  {
    curr_entry = r0_d1_index;
  }

  else if (r1_d0_index != (~0x0))
  {
    curr_entry = r1_d0_index;
  }
  else
  {
    curr_entry = 0;
  }

  //if the dirty bit of the cache entry to be evicted is set, then the data in the
  //cache line needs to be written back. The address to write the current entry
  //back to is constructed from the entry's tag and the set index by:
  // (evicted_entry_tag << L1_ADDRESS_TAG_SHIFT) | (set_index << L1_SET_INDEX_SHIFT)
  //This address should be written to the evicted_writeback_address output
  //parameter. The cache line data in the evicted entry should be copied to the
  //evicted_writeback_data_array.

  //Also, if the dirty bit of the chosen entry is been set, the low bit of the status byte
  //should be set to 1 to indicate that the write-back is needed. Otherwise,
  //the low bit of the status byte should be set to 0.

  //CODE HERE
  if (l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag & L1_DIRTYBIT_MASK)
  {
    *evicted_writeback_address = ((l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag & L1_ENTRY_TAG_MASK) << L1_ADDRESS_TAG_SHIFT) | (index_set_bits << L1_SET_INDEX_SHIFT);
    for (int i = 0; i < 8; i++)
    {
      evicted_writeback_data[i] = l1_cache[index_set_bits].lines[curr_entry].cache_line[i];
    }
    *status = *status | (L1_CACHE_HIT_MASK);
  }
  else
  {
    *status = *status & ~(L1_CACHE_HIT_MASK);
  }

  //Then, copy the data from write_data to the cache line in the entry,
  //set the valid bit of the entry, clear the dirty bit of the
  //entry, and write the tag bits of the address into the tag of
  //the entry.

  //CODE HERE
  for (int i = 0; i < 8; i++)
  {
    l1_cache[index_set_bits].lines[curr_entry].cache_line[i] = write_data[i];
  }
  l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag = l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag | L1_VBIT_MASK;
  l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag = l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag & ~(L1_DIRTYBIT_MASK);
  l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag = (l1_cache[index_set_bits].lines[curr_entry].v_r_d_tag & ~(L1_ENTRY_TAG_MASK)) | tag_bits;
}

/************************************************

       l1_clear_r_bits()

This procedure clears the r bit of each entry in each set of the L1
cache. It is called periodically to support the NRU algorithm.

***********************************************/

void l1_clear_r_bits()
{
  //CODE HERE
  for (int i = 0; i < L1_NUM_CACHE_SETS; i++)
  {
    for (int j = 0; j < L1_LINES_PER_SET; j++)
    {
      l1_cache[i].lines[j].v_r_d_tag = l1_cache[i].lines[j].v_r_d_tag & ~(L1_RBIT_MASK);
    }
  }
}
