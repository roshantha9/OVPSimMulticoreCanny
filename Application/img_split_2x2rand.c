#include <stdio.h>
#include <stdlib.h>
/*#include <float.h>*/
#include <math.h>
/*#include <string.h>*/
/*#include <stdbool.h>*/
/*#include <assert.h>*/


#include "img_split_2x2rand.h"

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

// clear all shared memory areas
void ImgSplit_ClearSharedMemMalloc(){
	xImgSplitNextFreeByte = ( size_t ) 0;
}


void ImgSplit_Initialise(){

	int i;

    prvWriteString("ImgSplit_Initialise::Enter");

	/* clear the allocated shared memory (we will overwrite with new values) */
	ImgSplit_ClearSharedMemMalloc();

	/* reset global structures */
	// Core-to-Block-Mapping info Global
	for(i=0;i<NUM_CORES;i++){
        C2BInfo->blk_mem_location[i] = NULL;
        C2BInfo->blk_corespecific_start_ix[i] = 0;
        C2BInfo->blk_corespecific_height[i] = 0;
	}
    C2BInfo->header_sz = 0;
    C2BInfo->width = 0;
    C2BInfo->height = 0;
    C2BInfo->nplanes = 0;
    C2BInfo->bitspp = 0;
    C2BInfo->compress_type = 0;
    C2BInfo->bmp_bytesz = 0;
    C2BInfo->hres = 0;
    C2BInfo->vres = 0;
    C2BInfo->ncolors = 0;
    C2BInfo->nimpcolors = 0;
    C2BInfo->canny_blk_output_full_img = NULL;

    prvWriteString("ImgSplit_Initialise::Exit");

}



// splits into random blocks
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

    /* find the start and height of all the blocks */
    int n=0;
    int min = 10;
    int max = origbitmapInfoHeader->height;
    int start =0;
    int random_num=0;
    int prev_n=0;

    srand ( _getSeedFromFile() ); // intial seed
    for(i=0;i<NUM_CORES;i++){
        //random_num = rand();

        //sprintf(buff, "ImgSplit_SplitIntoBlocks:: Core-%d random_num = %d", i,random_num);
        //prvWriteString(buff);

        // need to check if it's the last core we are calculating for
        if(i == NUM_CORES-1){
            n = max;  // if the last one, then we give him the remaining sections of the image
        }else{
            n = _getRandomNumInRange(min,max);
        }
        C2BInfo->blk_corespecific_start_ix[i] = start;

        if(i==0){
            C2BInfo->blk_corespecific_height[i] = n;
        }else{
            //C2BInfo->blk_corespecific_height[i] = n-(C2BInfo->blk_corespecific_height[i-1]);
            C2BInfo->blk_corespecific_height[i] = n-prev_n;
        }

        sprintf(buff, "ImgSplit_SplitIntoBlocks:: Core-%d n = %d", i,n);
        prvWriteString(buff);

        // change start for next core
        // but check if n has reached limit or not first
        // additional check : also check if n < (min allowable height)
        if(n <= origbitmapInfoHeader->height){

            // if the height of this block is less than the min allow height
            // and
            // if there n value is low
            // then we give the rest to prev-core
            // SPECIAL_CONDITION_1
            if( (C2BInfo->blk_corespecific_height[i] < MIN_ALLOWABLE_BLOCK_HEIGHT) &&
                ((max - n) < MIN_ALLOWABLE_BLOCK_HEIGHT) )

            {

                prvWriteString("ImgSplit_SplitIntoBlocks:: SPECIAL_CONDITION_1");

                // give this remianing chunk to the previous core
                C2BInfo->blk_corespecific_height[i-1] = C2BInfo->blk_corespecific_height[i-1] +
                                                        C2BInfo->blk_corespecific_height[i];

                // then null this core and every other core, from here - they don't get any work this round
                for(j=i;j<NUM_CORES;j++){
                    C2BInfo->blk_corespecific_start_ix[j] = 0;
                    C2BInfo->blk_corespecific_height[j] = 0;

                    SetCoresCannyFinished_Flag(j, 1);   // signal that canny is finished in these cores

                    sprintf(buff, "ImgSplit_SplitIntoBlocks:: NULL-Core-%d start_ix = %d", j,C2BInfo->blk_corespecific_start_ix[j]);
                    prvWriteString(buff);
                    sprintf(buff, "ImgSplit_SplitIntoBlocks:: NULL-Core-%d n = %d", j,C2BInfo->blk_corespecific_height[j]);
                    prvWriteString(buff);
                }
                break;
            }else
            // SPECIAL_CONDITION_2
            if( (C2BInfo->blk_corespecific_height[i] < MIN_ALLOWABLE_BLOCK_HEIGHT) &&
                !((max - n) < MIN_ALLOWABLE_BLOCK_HEIGHT) )
            {

                prvWriteString("ImgSplit_SplitIntoBlocks:: SPECIAL_CONDITION_2");
                // we just increase the hieght allocated to this core to min_allow_height
                // adjust height as well
                int delta = MIN_ALLOWABLE_BLOCK_HEIGHT - C2BInfo->blk_corespecific_height[i];
                C2BInfo->blk_corespecific_height[i] = MIN_ALLOWABLE_BLOCK_HEIGHT;
                // adjust n as well (bump up)
                n = n + delta;
            }else
            // SPECIAL_CONDITION_3 - default
            {
                prvWriteString("ImgSplit_SplitIntoBlocks:: SPECIAL_CONDITION_3 - default");
                //start = (origbitmapInfoHeader->width)*n;
                start = start + (C2BInfo->blk_corespecific_height[i]*(origbitmapInfoHeader->width));
                min = n+1;
            }
        }else{
            // null all other cores - they don't get any work this round
            for(j=i+1;j<NUM_CORES;j++){
                C2BInfo->blk_corespecific_start_ix[j] = 0;
                C2BInfo->blk_corespecific_height[j] = 0;

                SetCoresCannyFinished_Flag(j, 1);   // signal that canny is finished in these cores

                sprintf(buff, "ImgSplit_SplitIntoBlocks:: NULL-Core-%d start_ix = %d", j,C2BInfo->blk_corespecific_start_ix[j]);
                prvWriteString(buff);
                sprintf(buff, "ImgSplit_SplitIntoBlocks:: NULL-Core-%d n = %d", j,C2BInfo->blk_corespecific_height[j]);
                prvWriteString(buff);
            }
            break;
        }

        prev_n =n;
    }

    _DEBUG_CoreBlockSpecificPointers();
    prvWriteString("ImgSplit_SplitIntoBlocks:: finished getting start,n for all cores");

    // allocate memory in shared memory for the image blocks
    // blk_size = (blk_corespecific_height*(origbitmapInfoHeader->width))
    pixel_t* CoreImgBlk[NUM_CORES];

    //initialise and allocate memory
    int temp_h=0;
    for(i=0;i<NUM_CORES;i++){
        // initialise
        CoreImgBlk[i]=NULL;

        // allocate space
        temp_h = C2BInfo->blk_corespecific_height[i];
        if(temp_h != 0){
            CoreImgBlk[i] = ImgSplit_SharedMemMalloc((temp_h*(origbitmapInfoHeader->width)) *
                              sizeof(pixel_t));
        }else{
            sprintf(buff, "ImgSplit_SplitIntoBlocks::Warning: block(%d) height is zero", i);
            prvWriteString(buff);
        }

        // verify
        sprintf(buff, "ImgSplit_SplitIntoBlocks:: Core-%d-ImgBlk = %p", i,CoreImgBlk[i]);
        prvWriteString(buff);

    }
    // allocate memory for final canny output storage image
    pixel_t *CannyOutputImage = ImgSplit_SharedMemMalloc(origbitmapInfoHeader->bmp_bytesz *
                                sizeof(pixel_t));

    prvWriteString("ImgSplit_SplitIntoBlocks::Finished Malloc, now copying");

    int32_t temp_core_start_ix=0;
    int32_t temp_core_height=0;

    /* loop through all the pixels and put into corret core depending on index */
    for(i=0;i<NUM_CORES;i++){
        if(CoreImgBlk[i]!=NULL){
            blk_index=0;
            j=0;

            temp_core_start_ix = C2BInfo->blk_corespecific_start_ix[i];
            temp_core_height = C2BInfo->blk_corespecific_height[i];

            sprintf(buff, "ImgSplit_SplitIntoBlocks:: Core-%d : startix=%d",i, temp_core_start_ix);
            prvWriteString(buff);
            sprintf(buff, "ImgSplit_SplitIntoBlocks:: Core-%d : height=%d",i, temp_core_height);
            prvWriteString(buff);


            for(j=temp_core_start_ix; j<temp_core_start_ix+(temp_core_height*origbitmapInfoHeader->width); j++){
                    CoreImgBlk[i][blk_index] = blkImage[j];
                    blk_index++;
            }

            if(blk_index == 0){
                prvWriteString("ImgSplit_SplitIntoBlocks:: ERROR !, didn't copy all blocks!!");
            }

            sprintf(buff, "ImgSplit_SplitIntoBlocks:: Copied-%d for Core-%d",blk_index, i);
            prvWriteString(buff);
        }else{
            sprintf(buff, "ImgSplit_SplitIntoBlocks:: Core-%d is NULL", i);
            prvWriteString(buff);
        }
    }

    prvWriteString("ImgSplit_SplitIntoBlocks::Populating blkInfo!!");

    /* fill in the block info */
    // block memory locations (in shared mem)
    for(i=0;i<NUM_CORES;i++){
        if(CoreImgBlk[i]!=NULL){
            C2BInfo->blk_mem_location[i] = CoreImgBlk[i];
        }
    }

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

    prvWriteString("ImgSplit_SplitIntoBlocks::DONE -- returning!!");

    return 1;

}

pixel_t * ImgSplit_LoadBlockToLocalMem(int id){

    prvWriteString("ImgSplit_LoadBlockToLocalMem::Enter");

    // allocate enough memory for the block image data
    int blk_bytesz = C2BInfo->blk_corespecific_height[id]*C2BInfo->width;

    // if block size is zero then don't bother continuing - no work fot this core !
    if(blk_bytesz != 0){

        sprintf(buff, "ImgSplit_LoadBlockToLocalMem::before malloc : remaining %d bytes",xPortGetFreeHeapSize());
        prvWriteString(buff);
        pixel_t *blkImage = pvPortMalloc((blk_bytesz+32) *
                                      sizeof(pixel_t));

        sprintf(buff, "ImgSplit_LoadBlockToLocalMem::before malloc : remaining %d bytes",xPortGetFreeHeapSize());
        prvWriteString(buff);

        int32_t i, j;
        uint32_t count=0;

        int32_t h = C2BInfo->blk_corespecific_height[id];
        int32_t w = C2BInfo->width;

        prvWriteString("ImgSplit_LoadBlockToLocalMem::going into loop");

        sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: h = %d", h);
        prvWriteString(buff);
        sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: w = %d", w);
        prvWriteString(buff);
        sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: C2BInfo = %p", C2BInfo);
        prvWriteString(buff);


        _DEBUG_PrintGlobalC2BInfoStruct(id);

        for(j=0; j<h; j++){
            for(i=0; i<w; i++){
                blkImage[count] = C2BInfo->blk_mem_location[id][count];
                count++;
            }
            //sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: count = %d", count);
            //prvWriteString(buff);
        }

        sprintf(buff, "ImgSplit_LoadBlockToLocalMem:: count = %d", count);
        prvWriteString(buff);
        prvWriteString("ImgSplit_LoadBlockToLocalMem::Exit");

        return blkImage;
    }else{
        return NULL;
    }
}

// get blocks from different local processors and combine
// copy from locla mem to shared memory
// all cores will call this
pixel_t * ImgSplit_CombineBlocks(pixel_t *canny_output_data, int core_id){
    prvWriteString("ImgSplit_CombineBlocks::Enter");
    sprintf(buff, "ImgSplit_CombineBlocks:: canny_output_data = %p", canny_output_data);
    prvWriteString(buff);

    uint16_t offset = 0;
    int32_t i,j;
    uint32_t blk_index=0;

    int32_t blk_h = C2BInfo->blk_corespecific_height[core_id];
    int32_t blk_w = C2BInfo->width;
    int32_t bmp_h = C2BInfo->height;
    int32_t bmp_w = C2BInfo->width;

    /* loop through all local block specific and put into corret core depending on index */
    sprintf(buff, "ImgSplit_CombineBlocks:: C2BInfo->canny_blk_output_full_img = %p", C2BInfo->canny_blk_output_full_img);
    prvWriteString(buff);

    int32_t start_ix= C2BInfo->blk_corespecific_start_ix[core_id];
    int32_t blk_size = blk_h*blk_w;
    for(i=0;i<blk_size;i++){

        if(!((start_ix+i) % 1000 )){
            //sprintf(buff, "ImgSplit_CombineBlocks:: populating pixel: %d", start_ix+i);
            //prvWriteString(buff);
        }
        C2BInfo->canny_blk_output_full_img[start_ix+i] = canny_output_data[i];
        _wait(1000);
    }

    sprintf(buff, "ImgSplit_CombineBlocks:: i = %d bytes copied back to main img", i);
    prvWriteString(buff);

    return C2BInfo->canny_blk_output_full_img;
}





int ImgSplit_SaveBlockBMP(const char *filename, pixel_t *data)
{

	prvWriteString("ImgSplit_SaveBlockBMP::Enter");

#if 0
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
        prvWriteString("ImgSplit_Sa#if 0veBlockBMP:: Error - bmpfile_header_t");
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
    #endif
}





bitmap_info_header_t * ImgSplit_GetLocalBmpIH(int coreid){

    sprintf(buff, "ImgSplit_GetLocalBmpIH::before malloc : remaining %d bytes",xPortGetFreeHeapSize());
    prvWriteString(buff);
    bitmap_info_header_t * bmp_ih = pvPortMalloc(sizeof(bitmap_info_header_t));
    sprintf(buff, "ImgSplit_GetLocalBmpIH::after malloc : remaining %d bytes",xPortGetFreeHeapSize());
    prvWriteString(buff);

    bmp_ih->header_sz = C2BInfo->header_sz;
	bmp_ih->nplanes = C2BInfo->nplanes;
	bmp_ih->bitspp = C2BInfo->bitspp;
	bmp_ih->compress_type = C2BInfo->compress_type;
	bmp_ih->hres = C2BInfo->hres;
	bmp_ih->vres = C2BInfo->vres;
	bmp_ih->ncolors = C2BInfo->ncolors;
	bmp_ih->nimpcolors = C2BInfo->nimpcolors;
	bmp_ih->bmp_bytesz = C2BInfo->blk_corespecific_height[coreid] * C2BInfo->width;
	bmp_ih->height = C2BInfo->blk_corespecific_height[coreid];
	bmp_ih->width = C2BInfo->width;

    return bmp_ih;
}


void _DEBUG_PrintGlobalC2BInfoStruct(int coreid){

   #if (IMG_SPLIT_DEBUG)
    prvWriteString("********************");
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: C2BInfo_struct_adr = %p", C2BInfo);
    prvWriteString(buff);
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

    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: blk_bytesz = %d", C2BInfo->blk_corespecific_height[coreid]*C2BInfo->width);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: blk_height = %d", C2BInfo->blk_corespecific_height[coreid]);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintGlobalC2BInfoStruct:: blk_width = %d", C2BInfo->width);
    prvWriteString(buff);
    prvWriteString("********************");
    #endif

}

void _DEBUG_CoreBlockSpecificPointers(){
    int i;
    for(i=0;i<NUM_CORES;i++){
        sprintf(buff, "_DEBUG_CoreBlockSpecificPointers:: Core-%d blk_corespecific_start_ix = %d", i,C2BInfo->blk_corespecific_start_ix[i]);
        prvWriteString(buff);
        sprintf(buff, "_DEBUG_CoreBlockSpecificPointers:: Core-%d blk_corespecific_height = %d", i,C2BInfo->blk_corespecific_height[i]);
        prvWriteString(buff);
    }
}


// return true if a core has a valid allocated block to decode of height > 0
int ImgSplit_DoesCoreHaveValidBlock(int cid){

    if(C2BInfo->blk_corespecific_height[cid]==0){
        return 0;
    }else{
        return 1;
    }
}


//
// HELPER FUNCTIONS
//

void _wait(long delay){

	long i;
	for(i=0;i<delay;i++){
		asm volatile("mov r0, r0");
	}
}

int _getRandomNumInRange(int Min, int Max)
{
    int diff = Max-Min;
    return (int) (((double)(diff+1)/RAND_MAX) * rand() + Min);
}


int _getSeedFromFile(){

  FILE *fp;
  int seed = 123;
  char buf[128];

  fp = fopen("seed.txt", "r");
  if(fp == NULL){
    prvWriteString("_getSeedFromFile::ERROR - can't get seed from file!");
    return seed;
  }

  fgets(buf, sizeof(buf), fp);
  sscanf(buf, "%d", &seed);
  fclose(fp);

  return seed;
}

