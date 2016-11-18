#ifndef IMG_SPLIT_2x2_H_
#define IMG_SPLIT_2x2_H_


#include "FreeRTOS.h"

#include "canny.h"

/** defines **/
#define MAX_SHARED_MEM_SIZE_FOR_IMG_BLKS    16777216    // 16MB
#define NUM_CORES   4


typedef struct {
    int32_t blk_width;
    int32_t blk_height;
    uint32_t blk_bytesz;
    bitmap_info_header_t *bitmapInfoHeader;
} blk_header_t;


/* Allocate the memory for the heap.  The struct is used to force byte
alignment without using any non-portable code. */
static union xIMG_SPLIT_SHDMEM
{
	#if portBYTE_ALIGNMENT == 8
		volatile portDOUBLE dDummy;
	#else
		volatile unsigned long ulDummy;
	#endif
	unsigned char ucHeap[ MAX_SHARED_MEM_SIZE_FOR_IMG_BLKS ];
} xShMem __attribute__((section(".ImgSplitShdMem")));

//static size_t xImgSplitNextFreeByte = ( size_t ) 0;
int ImgSplit_SplitIntoRandomBlocks(void);

int ImgSplit_SplitIntoBlocks(pixel_t *blkImage,
                                       bitmap_info_header_t *origbitmapInfoHeader);

pixel_t * ImgSplit_LoadBlockToLocalMem(int id);
int ImgSplit_SaveBlockBMP(const char *filename, pixel_t *data);

pixel_t * ImgSplit_CombineBlocks(pixel_t *canny_output_data, int core_id);

void _DEBUG_PrintGlobalC2BInfoStruct(void);

bitmap_info_header_t * ImgSplit_GetLocalBmpIH(void);

void _wait(int delay);

// stack implemented using buffer (LIFO)
struct C2BMapping_t{
    pixel_t *blk_mem_location[NUM_CORES];   // location in memory where each block's data is stored
    blk_header_t *blks_info;	// size changes as you pop/push items from stack
};


struct C2BMapping_V2_t{
    pixel_t *blk_mem_location[NUM_CORES];

    // block specific
    int32_t blk_width;
    int32_t blk_height;
    uint32_t blk_bytesz;

    // bitmap header info
    uint32_t header_sz;
    int32_t  width;
    int32_t  height;
    uint16_t nplanes;
    uint16_t bitspp;
    uint32_t compress_type;
    uint32_t bmp_bytesz;
    int32_t  hres;
    int32_t  vres;
    uint32_t ncolors;
    uint32_t nimpcolors;

    // where to store the canny output block after processing
    // combination of all the cannied' blocks
    pixel_t *canny_blk_output_full_img;


};

struct CoreDecodeFlags_t{
    int CoreReady[NUM_CORES];   // should cores start canny process ? 1= true, 0 false
    int CoresCannyFinished[NUM_CORES]; // have cores finished canny process ?
};



#endif /* IMG_SPLIT_2x2_H_ */
