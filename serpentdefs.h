

#define MAX_SOURCE_SIZE (0x100000)

typedef unsigned int    uint32_t;

typedef unsigned char   BYTE;

#define keyLen 128


#define BLOCK_SIZE_IN_BYTES 16
#define BLOCK_SIZE_IN_32BIT_WORDS 4

#define NUM_WORK_ITEMS 2048

#define NUM_ENCRYPT_BLOCKS_FOR_WORK_ITEM 10000

#define NUM_ENCRYPT_BLOCKS NUM_WORK_ITEMS * NUM_ENCRYPT_BLOCKS_FOR_WORK_ITEM

#define TOTAL_BLOCKS_SIZE BLOCK_SIZE_IN_32BIT_WORDS * NUM_ENCRYPT_BLOCKS   //4 32-bit words is the normal block. We want 10*32 blocks.
#define MEM_SIZE  sizeof(uint32_t) * TOTAL_BLOCKS_SIZE
#define MEM_SIZE_KEY 132*sizeof(uint32_t)
