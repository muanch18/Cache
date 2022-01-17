
/*****************************************************************

    This is the interface between the CPU and the memory subsystem, 
    which includes L1 cache, L2 cache, and main memory.

    It supports reading from and writing to memory using 32-bit addresses.

*****************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "main_memory.h"
#include "l1_cache.h"
#include "l2_cache.h"
#include "memory_subsystem.h"

//These are defined below.
void memory_handle_l1_miss(uint32_t address);
void memory_handle_l2_miss(uint32_t address, uint8_t control);

//We are going to count how many L1 and L2 cache misses
//have occurred. These are the variables used to keep
//track of the misses.
uint32_t num_l1_misses;
uint32_t num_l2_misses;
#define L1_CACHE_HIT_MASK 0x1
/*******************************************************

        memory_subsystem_initialize()

This procedure is used to initialize the memory subsystem.
It takes a single 32-bit parameter, memory_size_in_bytes,
that determines large the main memory will be.

*******************************************************/
void memory_subsystem_initialize(uint32_t memory_size_in_bytes)
{

  //Call the intialization procedures for main memory,
  // L2 cache, and L1 cache.

  //Also initialize num_l1_misses and num_l2_misses to 0.

  //CODE HERE
  main_memory_initialize(memory_size_in_bytes);
  l1_initialize();
  l2_initialize();
  num_l1_misses = 0;
  num_l2_misses = 0;
}

/*****************************************************

              memory_access()

This is the procedure for reading and writing one word
of data from and to the memory subsystem. 

It takes the following parameters:

address:  32-bit address of the data being read or written.

write_data: In the case of a memory write, the 32-bit value
          being written.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: a 32-bit ouput parameter (thus, a pointer to it is passed).
         In the case of a read operation, the data being read will
         be written to read_data.

****************************************************/

void memory_access(uint32_t address, uint32_t write_data,
                   uint8_t control, uint32_t *read_data)
{

  uint8_t status;

  //call l1_cache_access to try to read or write the
  //data from or to the L1 cache.

  //CODE HERE
  l1_cache_access(address, write_data, control, read_data, &status);
  //If an L1 cache miss occurred, then:
  // -- increment num_l1_misses
  // -- call memory_handle_l1_miss(), below, specifying
  //    the requested address, to bring the needed
  //    cache line into L1.
  // -- call l1_cache_access again to read or
  //      write the data.

  //CODE HERE
  if (!(status & L1_CACHE_HIT_MASK))
  {
    num_l1_misses += 1;
    memory_handle_l1_miss(address);
    l1_cache_access(address, write_data, control, read_data, &status);
  }
}

//This procedure should be called when an L1 cache miss occurs.
//It doesn't matter if the miss occured on a read or a write
//operation. It takes as a parameter the address that resulted
//in the L1 cache miss.

void memory_handle_l1_miss(uint32_t address)
{
  uint32_t write_data[WORDS_PER_CACHE_LINE];
  uint32_t read_data[WORDS_PER_CACHE_LINE];
  uint8_t status;
  uint32_t evicted_writeback_address;
  uint32_t evicted_writeback_data[WORDS_PER_CACHE_LINE];
  //call l2_cache_access to read the cache line containing
  //the specified address from the L2 cache. This is necessary
  //regardless if the operation that caused the L1 cache miss
  //was a read or a write.
  l2_cache_access(address, NULL, READ_ENABLE_MASK, read_data, &status);
  //CODE HERE

  //if the result was an L2 cache miss, then:
  //   -- increment num_l2_misses
  //   -- call memory_handle_l2_miss (below), specifying the address that
  //      caused the L2 miss (which is the same as the address that
  //      caused the L1 miss), and specifying that the L2 miss
  //      occurred when attempting to read (not write) from L2 the cache.
  //  --  call l2_cache_access again to read the needed cache line
  //      from the l2 cache.

  //CODE HERE
  if (!(status & L1_CACHE_HIT_MASK))
  {
    num_l2_misses += 1;
    memory_handle_l2_miss(address, READ_ENABLE_MASK);
    l2_cache_access(address, NULL, READ_ENABLE_MASK, read_data, &status);
  }

  //Now that the needed cache line has been retrieved from the
  //L2 cache (whether an L2 cache miss occurred or not),
  //insert the cache line into the L1 cache by calling l1_insert_line.

  //CODE HERE
  uint32_t temp[WORDS_PER_CACHE_LINE];
  l1_insert_line(address, read_data, &evicted_writeback_address, evicted_writeback_data, &status);

  //if the cache line that was evicted from L1 has to be written back,
  //then l2_cache_access must be called to write the evicted cache line
  //to L2. If a cache miss occurs when writing the evicted cache line
  //to L2, then:
  //   -- memory_handle_l2_miss (below) should be called, specifying the
  //      address (of the evicted line) that caused the L2 cache miss,
  //      and specifying that the operation that caused the L2 miss
  //      was a write (not a read).
  //   -- l2_cache_access should be called again to write the cache line
  //      evicted from L1 into L2.
  //
  //control &= ~(WRITE_ENABLE_MASK);
  if (status & L1_CACHE_HIT_MASK){
    l2_cache_access(evicted_writeback_address, evicted_writeback_data, WRITE_ENABLE_MASK, temp, &status);
    if (!(status & L1_CACHE_HIT_MASK))
    {
      memory_handle_l2_miss(evicted_writeback_address, WRITE_ENABLE_MASK);
      l2_cache_access(evicted_writeback_address, read_data, WRITE_ENABLE_MASK, temp, &status); //or is it evicted writeback data
    }
  }
  //CODE HERE
}

//This procedure handles an L2 cache miss, whether the miss occurred
//due to a read or a write. It takes the following parameters:
// -- address: the address that caused the L2 cache miss
// -- control:  an unsigned byte (8 bits) that indicates whether the
//          L2 miss occurred on a read or a write operation. Only the
//          two lowest bits are meaningful, as follows:
//          -- bit 0 = 1 means a read operation caused the miss
//          -- bit 1 = 1 means a write operation caused the miss
//           (naturally, the two bits should not both be set to 1)

void memory_handle_l2_miss(uint32_t address, uint8_t control)
{
  uint32_t cache_line[WORDS_PER_CACHE_LINE];
  uint32_t write_data[WORDS_PER_CACHE_LINE];
  uint32_t read_data;
  uint8_t status;
  uint32_t evicted_writeback_address;
  uint32_t evicted_writeback_data[WORDS_PER_CACHE_LINE];
  uint32_t temp[WORDS_PER_CACHE_LINE];
  //if the L2 miss was on a read operation, then main_memory_access
  //must be called to fetch the needed cache line from main_memory.
  //The fetched cache line should be written to cache_line (see above).
  //However, if the L2 miss was on a write operation (with an evicted line from L1),
  //there's no need to read the cache line from main memory, since
  //that line will be overwritten.

  //CODE HERE
  if (control & READ_ENABLE_MASK) //is this supposed to be an if statement
    main_memory_access(address, write_data, READ_ENABLE_MASK, cache_line);

  //Now call l2_insert_line to insert the cache line data in cache_line,
  //above, into L2. In the case of a read, this is the cache line data
  //that has been read from main memory. In the case of a write, then
  //it's just meaningless data being written to L2 (i.e. whatever happened
  //to be in cache_line), since that line in L2 will be overwritten subsequently.

  //CODE HERE
  l2_insert_line(address, cache_line, &evicted_writeback_address, evicted_writeback_data, &status);

  //If the call to l2_insert_line resulted in an evicted cache line
  //that has to be written back to main memory, call main_memory_access
  //to write the evicted cache line to main memory.
  if (status & L1_CACHE_HIT_MASK)
    main_memory_access(evicted_writeback_address, evicted_writeback_data, WRITE_ENABLE_MASK, temp);
  //CODE HERE
}

//This procedure will be called periodically (e.g. when a clock
//interrupt occurs) in order to cause the r bits in the
//L1 cache to be cleared in support of the NRU replacement
//algorithm.
void memory_handle_clock_interrupt()
{
  //call the function which clears the r bits in the L1 cache

  //CODE HERE
  l1_clear_r_bits();
}
