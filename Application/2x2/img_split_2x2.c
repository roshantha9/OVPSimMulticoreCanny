#include <stdio.h>
#include <stdlib.h>
/*#include <float.h>*/
#include <math.h>
/*#include <string.h>*/
/*#include <stdbool.h>*/
/*#include <assert.h>*/

#include "img_split_2x2.h"

#include "FreeRTOS.h"
#include "bsp.h"
#include "simulatorIntercepts.h"

static size_t xImgSplitNextFreeByte = ( size_t ) 0;

char buff[128];

// Information on blocks stored in shared memory
//volatile struct C2BMapping_t *C2BInfo = (volatile struct C2BMapping_t*) 0x41205002;
volatile struct C2BMapping_V2_t *C2BInfo = (volatile struct C2BMapping_V2_t*) 0x41205002;


/****************************************
 * HELPER FUNCTIONS
`****************************************/

// allocate memory in shared-memory
void *ImgSplit_SharedMemMalloc( size_t xWantedSize )
{
    void *pvReturn = NULL;

    /* Ensure that blocks are always aligned to the required number of bytes. */
	#if portBYTE_ALIGNMENT != 1
		if( xWantedSize & portBYTE_ALIGNMENT_MASK )
		{
			/* Byte alignment required. */
			xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
		}
	#endif

	vTaskSuspendAll();
	{
		/* Check there is enough room left for the allocation. */
		if( ( ( xImgSplitNextFreeByte + xWantedSize ) < MAX_SHARED_MEM_SIZE_FOR_IMG_BLKS ) &&
			( ( xImgSplitNextFreeByte + xWantedSize ) > xImgSplitNextFreeByte )	)/* Check for overflow. */
		{
			/* Return the next free byte then increment the index past this
			block. */
			pvReturn = &( xShMem.ucHeap[ xImgSplitNextFreeByte ] );
			xImgSplitNextFreeByte += xWantedSize;
		}
	}
	xTaskResumeAll();

	return pvReturn;
}

// splits into block of random sizes
// first trying to do random strips, so it's easier to
// calculate block sizes (i.e. m * n, broken into m *j)
int ImgSplit_SplitIntoRandomBlocks(){
}



// splits into identical blocks
int ImgSplit_SplitIntoBlocks(pixel_t *blkImage,
                                       bitmap_info_header_t *origbitmapInfoHeader){

    //blk_header_t *blk_header_result;
    int32_t i,j;
    uint32_t blk_index=0;
    uint16_t offset=0; // main image offset
    int32_t blk_height =(int32_t)(origbitmapInfoHeader->height)/2;  // 2 because num_cores=4
    int32_t blk_width =(int32_t)(origbitmapInfoHeader->width)/2;

    int32_t blk_size = (int32_t) (blk_height*blk_width);


    prvWriteString("ImgSplit_SplitIntoBlocks::Enter");

    // allocate memory in shared memory for the image blocks
    pixel_t *Core0ImgBlk = ImgSplit_SharedMemMalloc(blk_size *
                          sizeof(pixel_t));

    pixel_t *Core1ImgBlk = ImgSplit_SharedMemMalloc(blk_size *
                          sizeof(pixel_t));

    pixel_t *Core2ImgBlk = ImgSplit_SharedMemMalloc(blk_size *
                          sizeof(pixel_t));

    pixel_t *Core3ImgBlk = ImgSplit_SharedMemMalloc(blk_size *
                          sizeof(pixel_t));

    pixel_t *CannyOutputImage = ImgSplit_SharedMemMalloc(origbitmapInfoHeader->bmp_bytesz *
                                sizeof(pixel_t));

    sprintf(buff, "ImgSplit_SplitIntoBlocks:: &Core0ImgBlk = %p", Core0ImgBlk);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SplitIntoBlocks:: &Core1ImgBlk = %p", Core1ImgBlk);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SplitIntoBlocks:: &Core2ImgBlk = %p", Core2ImgBlk);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SplitIntoBlocks:: &Core3ImgBlk = %p", Core3ImgBlk);
    prvWriteString(buff);


    prvWriteString("ImgSplit_SplitIntoBlocks::Finished Malloc");

    for(j=0; j<blk_height; j++){
	    for(i=0; i<blk_width; i++){

            // extract that block's data from the main image
            /* Core - 0 */
            offset =0;
            Core0ImgBlk[blk_index] = blkImage[ (i+offset)+((origbitmapInfoHeader->width)*j) ];

            /* Core - 1 */
            offset =blk_width;
            Core1ImgBlk[blk_index] = blkImage[ (i+offset)+((origbitmapInfoHeader->width)*j) ];

            /* Core - 2 */
            offset =(origbitmapInfoHeader->width)*blk_height;
            Core2ImgBlk[blk_index] = blkImage[ (i+offset)+((origbitmapInfoHeader->width)*j) ];

            /* Core - 3 */
            offset =((origbitmapInfoHeader->width)*blk_height)+blk_width;
            Core3ImgBlk[blk_index] = blkImage[ (i+offset)+((origbitmapInfoHeader->width)*j) ];

            blk_index++;

	    }
	    //sprintf(buff, "ImgSplit_SplitIntoBlocks:blk: (%d , %d)", i,j);
        //prvWriteString(buff);
    }

    prvWriteString("ImgSplit_SplitIntoBlocks::Populating blkInfo!!");

    /* fill in the block info */
    // block memory locations (in shared mem)
    C2BInfo->blk_mem_location[0] = Core0ImgBlk;
    C2BInfo->blk_mem_location[1] = Core1ImgBlk;
    C2BInfo->blk_mem_location[2] = Core2ImgBlk;
    C2BInfo->blk_mem_location[3] = Core3ImgBlk;

    // block specific
    C2BInfo->blk_width = blk_width;
    C2BInfo->blk_height = blk_height;
    C2BInfo->blk_bytesz = blk_index;

    // bitmap header info
    C2BInfo->header_sz = origbitmapInfoHeader->header_sz;
    C2BInfo->width = origbitmapInfoHeader->width;
    C2BInfo->height = origbitmapInfoHeader->height;
    C2BInfo->nplanes = origbitmapInfoHeader->nplanes;
    C2BInfo->bitspp = origbitmapInfoHeader->bitspp;
    C2BInfo->compress_type = origbitmapInfoHeader->compress_type;
    C2BInfo->bmp_bytesz = origbitmapInfoHeader->bmp_bytesz;
    C2BInfo->hres = origbitmapInfoHeader->hres;
    C2BInfo->vres = origbitmapInfoHeader->vres;
    C2BInfo->ncolors = origbitmapInfoHeader->ncolors;
    C2BInfo->nimpcolors = origbitmapInfoHeader->nimpcolors;

    // store the location of the final combined output image
    C2BInfo->canny_blk_output_full_img =  CannyOutputImage;

    sprintf(buff, "ImgSplit_SplitIntoBlocks:: blk_bytesz = %d", C2BInfo->blk_bytesz);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SplitIntoBlocks:: height = %d", C2BInfo->blk_height);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SplitIntoBlocks:: width = %d", C2BInfo->blk_width);
    prvWriteString(buff);


    prvWriteString("ImgSplit_SplitIntoBlocks::DONE -- returning!!");

    return 1;

}

pixel_t * ImgSplit_LoadBlockToLocalMem(int id){

    prvWriteString("ImgSplit_LoadBlockToLocalMem::Enter");

    // allocate enough memory for the block image data
    pixel_t *blkImage = pvPortMalloc((C2BInfo->blk_bytesz+32) *
                                  sizeof(pixel_t));

    prvWriteString("ImgSplit_LoadBlockToLocalMem::after malloc");

    int32_t i, j;
    uint32_t count=0;

    int32_t h = C2BInfo->blk_height;
    int32_t w = C2BInfo->blk_width;

    prvWriteString("ImgSplit_LoadBlockToLocalMem::going into loop");

    sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: h = %d", h);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: w = %d", w);
    prvWriteString(buff);

    sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: C2BInfo = %p", C2BInfo);
    prvWriteString(buff);


    _DEBUG_PrintGlobalC2BInfoStruct();

    for(j=0; j<h; j++){
	    for(i=0; i<w; i++){

            blkImage[count] = C2BInfo->blk_mem_location[id][count];
            count++;

        }
        //sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: count = %d", count);
        //prvWriteString(buff);
    }


    sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: Gheight = %d", C2BInfo->blk_height);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: Gwidth = %d", C2BInfo->blk_width);
    prvWriteString(buff);

    prvWriteString("ImgSplit_LoadBlockToLocalMem::Exit");

    return blkImage;

}

// get blocks from different local processors and combine
// copy from locla mem to shared memory
pixel_t * ImgSplit_CombineBlocks(pixel_t *canny_output_data, int core_id){
    prvWriteString("ImgSplit_CombineBlocks::Enter");

    uint16_t offset = 0;
    int32_t i,j;
    uint32_t blk_index=0;

    int32_t blk_h = C2BInfo->blk_height;
    int32_t blk_w = C2BInfo->blk_width;
    int32_t bmp_h = C2BInfo->height;
    int32_t bmp_w = C2BInfo->width;

    switch(core_id){

        /* Core - 0 */
        case 0:
            offset =0;
            break;

        /* Core - 1 */
        case 1:
            offset =blk_w;
            break;

        /* Core - 2 */
        case 2:
            offset =(bmp_w)*blk_h;
            break;

        /* Core - 3 */
        case 3:
            offset =(bmp_w*blk_h)+blk_w;
            break;

        default:
            prvWriteString("ImgSplit_CombineBlocks::Unknown core_id");
            break;
    }


    for(j=0; j<blk_h; j++){
        for(i=0; i<blk_w; i++){

            // copy block data to the appropriate place in the main image
            C2BInfo->canny_blk_output_full_img[ (i+offset)+((bmp_w)*j) ] = canny_output_data[blk_index];

            _wait(1000);

            blk_index++;

        }
    }

    return C2BInfo->canny_blk_output_full_img;

}





int ImgSplit_SaveBlockBMP(const char *filename, pixel_t *data)
{

	prvWriteString("ImgSplit_SaveBlockBMP::Enter");


	sprintf(buff, "ImgSplit_SaveBlockBMP:: Gheight = %d", C2BInfo->blk_height);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: Gwidth = %d", C2BInfo->blk_width);
    prvWriteString(buff);


	/* modify the original bmp_header */

	bitmap_info_header_t * bmp_ih = pvPortMalloc(sizeof(bitmap_info_header_t));

	bmp_ih->header_sz = C2BInfo->header_sz;
	bmp_ih->nplanes = C2BInfo->nplanes;
	bmp_ih->bitspp = C2BInfo->bitspp;
	bmp_ih->compress_type = C2BInfo->compress_type;
	bmp_ih->hres = C2BInfo->hres;
	bmp_ih->vres = C2BInfo->vres;
	bmp_ih->ncolors = C2BInfo->ncolors;
	bmp_ih->nimpcolors = C2BInfo->nimpcolors;
	bmp_ih->bmp_bytesz = C2BInfo->blk_bytesz;
	bmp_ih->height = C2BInfo->blk_height;
	bmp_ih->width = C2BInfo->blk_width;


	sprintf(buff, "ImgSplit_SaveBlockBMP:: header_sz = %d", bmp_ih->header_sz);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: nplanes = %d", bmp_ih->nplanes);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: bitspp = %d", bmp_ih->bitspp);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: compress_type = %d", bmp_ih->compress_type);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: hres = %d", bmp_ih->hres);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: vres = %d", bmp_ih->vres);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: ncolors = %d", bmp_ih->ncolors);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: nimpcolors = %d", bmp_ih->nimpcolors);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: bmp_bytesz = %d", bmp_ih->bmp_bytesz);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: height = %d", bmp_ih->height);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: width = %d", bmp_ih->width);
    prvWriteString(buff);

    sprintf(buff, "ImgSplit_SaveBlockBMP:: Gheight = %d", C2BInfo->blk_height);
    prvWriteString(buff);
    sprintf(buff, "ImgSplit_SaveBlockBMP:: Gwidth = %d", C2BInfo->blk_width);
    prvWriteString(buff);


	FILE* filePtr = fopen(filename, "wb");
    if (filePtr == NULL){
        prvWriteString("ImgSplit_SaveBlockBMP:: Error - fname");
        return 1;
    }

    bmpfile_magic_t mag = {{0x42, 0x4d}};
    if (fwrite(&mag, sizeof(bmpfile_magic_t), 1, filePtr) != 1) {
        fclose(filePtr);
        prvWriteString("ImgSplit_SaveBlockBMP:: Error - magic_tag");
        return 1;
    }

    const uint32_t offset = sizeof(bmpfile_magic_t) +
                            sizeof(bmpfile_header_t) +
                            sizeof(bitmap_info_header_t) +
                            ((1U << bmp_ih->bitspp) * 4);

    const bmpfile_header_t bmp_fh = {
        .filesz = offset + bmp_ih->bmp_bytesz,
        .creator1 = 0,
        .creator2 = 0,
        .bmp_offset = offset
    };

    if (fwrite(&bmp_fh, sizeof(bmpfile_header_t), 1, filePtr) != 1) {
        fclose(filePtr);
        prvWriteString("ImgSplit_SaveBlockBMP:: Error - bmpfile_header_t");
        return TRUE;
    }
    if (fwrite(bmp_ih, sizeof(bitmap_info_header_t), 1, filePtr) != 1) {
        fclose(filePtr);
        prvWriteString("ImgSplit_SaveBlockBMP:: Error - bitmap_info_header_t");
        return TRUE;
    }

    // Palette
    size_t i;
    for (i = 0; i < (1U << bmp_ih->bitspp); i++) {
        const rgb_t color = {(uint8_t)i, (uint8_t)i, (uint8_t)i};
        if (fwrite(&color, sizeof(rgb_t), 1, filePtr) != 1) {
            fclose(filePtr);
            prvWriteString("ImgSplit_SaveBlockBMP:: Error - palette");
            return 1;
        }
    }

    prvWriteString("ImgSplit_SaveBlockBMP::going to write data..");

    // We use int instead of uchar, so we can't write img
    // in 1 call any more.
    // fwrite(data, 1, bmp_ih->bmp_bytesz, filePtr);

    // Padding: http://en.wikipedia.org/wiki/BMP_file_format#Pixel_storage
    size_t pad = 4*ceil(bmp_ih->bitspp*bmp_ih->width/32.) - bmp_ih->width;
    unsigned char c;
    size_t j;
    for(i=0; i < bmp_ih->height; i++) {
	    for(j=0; j < bmp_ih->width; j++) {
		    c = (unsigned char) data[j + bmp_ih->width*i];
		    if (fwrite(&c, sizeof(char), 1, filePtr) != 1) {
			    fclose(filePtr);
                prvWriteString("ImgSplit_SaveBlockBMP:: Error - write error");
			    return 1;
		    }
	    }
	    c = 0;
	    for(j=0; j<pad; j++)
		    if (fwrite(&c, sizeof(char), 1, filePtr) != 1) {
			    fclose(filePtr);
			    prvWriteString("ImgSplit_SaveBlockBMP:: Error - write padding");
			    return TRUE;
		    }
    }

    fclose(filePtr);

    return 0;
}





bitmap_info_header_t * ImgSplit_GetLocalBmpIH(){
    bitmap_info_header_t * bmp_ih = pvPortMalloc(sizeof(bitmap_info_header_t));

    bmp_ih->header_sz = C2BInfo->header_sz;
	bmp_ih->nplanes = C2BInfo->nplanes;
	bmp_ih->bitspp = C2BInfo->bitspp;
	bmp_ih->compress_type = C2BInfo->compress_type;
	bmp_ih->hres = C2BInfo->hres;
	bmp_ih->vres = C2BInfo->vres;
	bmp_ih->ncolors = C2BInfo->ncolors;
	bmp_ih->nimpcolors = C2BInfo->nimpcolors;
	bmp_ih->bmp_bytesz = C2BInfo->blk_bytesz;
	bmp_ih->height = C2BInfo->blk_height;
	bmp_ih->width = C2BInfo->blk_width;

    return bmp_ih;
}


void _DEBUG_PrintGlobalC2BInfoStruct(){

    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: header_sz = %d", C2BInfo->header_sz);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: nplanes = %d", C2BInfo->nplanes);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: bitspp = %d", C2BInfo->bitspp);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: compress_type = %d", C2BInfo->compress_type);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: hres = %d", C2BInfo->hres);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: vres = %d", C2BInfo->vres);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: ncolors = %d", C2BInfo->ncolors);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: nimpcolors = %d", C2BInfo->nimpcolors);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: bmp_bytesz = %d", C2BInfo->bmp_bytesz);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: height = %d", C2BInfo->height);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobaImgSplit_lC2BInfoStruct:: width = %d", C2BInfo->width);
    prvWriteString(buff);

    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: blk_bytesz = %d", C2BInfo->blk_bytesz);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: blk_height = %d", C2BInfo->blk_height);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: blk_width = %d", C2BInfo->blk_width);
    prvWriteString(buff);

}

void _wait(int delay){

	int i;
	for(i=0;i<delay;i++){
		asm volatile("mov r0, r0");
	}
}
