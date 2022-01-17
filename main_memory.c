#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_subsystem_constants.h"
#include "main_memory.h"

//main memory is just a (dynamically allocated) array
//of unsigned 32-bit words.

uint32_t *main_memory;

uint32_t main_memory_size_in_bytes;

/************************************************************************
                 main_memory_initialize
This procedure allocates main memory, according to the size specified in bytes.
The procedure should check to make sure that the size is a multiple of 32 (since
there are 4 bytes per word and 8 words per cache line).
*************************************************************************/

void main_memory_initialize(uint32_t size_in_bytes)
{

  //Check if size in bytes is divisible by 32.
  if (size_in_bytes & 0x1F)
  { //lowest 5 bits should be 00000
    printf("Error: Memory size (in bytes) must be a multiple of 8-word cache lines (32 bytes)\n");
    exit(1);
  }

  //Allocate the main memory, using malloc

  main_memory = (uint32_t *)malloc(size_in_bytes);
  main_memory_size_in_bytes = size_in_bytes;

  //Write a 0 to each word in main memory. Note that the
  //size_in_bytes parameter specifies the size of main memory
  //in bytes, but, since main_memory is declared as an
  //array of 32-bit words, it is written to a word at a time
  //(not a byte at a time). Obviously, the size of main memory
  //in words is 1/4 of the size of main memory in bytes.

  for (int i = 0; i < main_memory_size_in_bytes / 4; i++)
  {
    main_memory[i] = 0;
  }
}

//zeroing out the lowest five bits of an address indicates
//the address of the start of the corresponding cache line
//in memory. Use this mask to zero out the lowest 5 bits,
//since 1F hex = 000...0011111 binary, and using ~ to flip the bits
//gives 111...1100000 in binary.
#define CACHE_LINE_ADDRESS_MASK ~0x1f

/********************************************************************
               main_memory_access

This procedure implements the reading and writing of cache lines from
and to main memory. The parameters are:

address:  unsigned 32-bit address. This address can be anywhere within
          a cache line.

write_data:  an array of unsigned 32-bit words. On a write operation,
             8 words are copied from write_data to the appropriate cache
             line in memory.

control:  an unsigned byte (8 bits), of which only the two lowest bits
          are meaningful, as follows:
          -- bit 0:  read enable (1 means read, 0 means don't read)
          -- bit 1:  write enable (1 means write, 0 means don't write)

read_data: an array of unsigned 32-bit integers. On a write operation,
           8 32-bit words are copied from the appropriate cache line in 
           memory to read_data.

*********************************************************/

void main_memory_access(uint32_t address, uint32_t write_data[],
                        uint8_t control, uint32_t read_data[])

{

  //Need to check that the specified address is within the
  //size of the memory. If not, print an error message and
  //exit from the program.

  if (address >= main_memory_size_in_bytes)
  {
    printf("Error: Address must be within the size of memory\n");
    exit(1);
  }

  //Determine the address of the start of the desired cache line.
  //Use CACHE_LINE_ADDRESS_MASK to mask out the appropriate
  //number of low bits of the address.

  uint32_t cache_line_address = address & CACHE_LINE_ADDRESS_MASK;
  cache_line_address = cache_line_address >> 2;

  //If the read-enable bit of the control parameter is set (i.e. is 1),
  //then copy the cache line starting at cache_line_address into read_data.
  //See memory_subsystem_constants.h for masks that are convenient for
  //testing the bits of the control parameter.

  uint32_t a = cache_line_address;

  if (control & READ_ENABLE_MASK)
  {
    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
    {
      read_data[i] = main_memory[a + i];
    }
  }

  //If the write-enable bit of the control parameter is set then copy
  //write_data into the cache line starting at cache_line_address.

  a = cache_line_address;
  if (control & WRITE_ENABLE_MASK)
  {
    for (int i = 0; i < WORDS_PER_CACHE_LINE; i++)
    {
      main_memory[a + i] = write_data[i];
    }
  }
}
