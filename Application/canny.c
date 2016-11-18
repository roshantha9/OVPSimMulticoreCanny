#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
/*#include <string.h>*/
/*#include <stdbool.h>*/
/*#include <assert.h>*/

#include "canny.h"

#include "img_split_2x2rand.h"

#include "FreeRTOS.h"
#include "bsp.h"
#include "simulatorIntercepts.h"

/*
 * Loading part taken from
 * http://www.vbforums.com/showthread.php?t=261522
 * BMP info:
 * http://en.wikipedia.org/wiki/BMP_file_format
 *
 * Note: the magic number has been removed from the bmpfile_header_t
 * structure since it causes alignment problems
 *     bmpfile_magic_t should be written/read first
 * followed by the
 *     bmpfile_header_t
 * [this avoids compiler-specific alignment pragmas etc.]
 */

char buff[128];
/* common for all cores */
pixel_t *in_blk_data;
bitmap_info_header_t *local_bmp_ih;

volatile struct CoreDecodeFlags_t *CoreFlags = (volatile struct CoreDecodeFlags_t*) 0x41305002;
//volatile struct bitmap_info_header_t *Global_bmp_ih = (volatile struct bitmap_info_header_t*) 0x41305402;


pixel_t *load_bmp(const char *filename,
                  bitmap_info_header_t *bitmapInfoHeader)
{

	//printf("load_bmp::Enter");
	//prvWriteString(filename);

	FILE *filePtr = fopen(filename, "rb");

    if (filePtr == NULL) {
        perror("load_bmp::fopen()");
        prvWriteString("load_bmp::Error! could not open file");
        return NULL;
    }

	//prvWriteString("load_bmp::INFO: pass fopen");

    bmpfile_magic_t mag;
    if (fread(&mag, sizeof(bmpfile_magic_t), 1, filePtr) != 1) {
        prvWriteString("load_bmp::Error! could not read magic tag");
        fclose(filePtr);
        return NULL;
    }

	//printf("load_bmp::INFO: pass fread");

    // verify that this is a bmp file by check bitmap id
    // warning: dereferencing type-punned pointer will break
    // strict-aliasing rules [-Wstrict-aliasing]
    if (*((uint16_t*)mag.magic) != 0x4D42) {

		//sprintf(buff, "Not a BMP file: magic=%c%c", mag.magic[0], mag.magic[1]);
        //printf("Not a BMP file:");
        prvWriteString("load_bmp::Error! Not a BMP file");
        fclose(filePtr);
        return NULL;
    }

	//printf("load_bmp::INFO: pass verify bmp");
    bmpfile_header_t bitmapFileHeader; // our bitmap file header
    // read the bitmap file header
    if (fread(&bitmapFileHeader, sizeof(bmpfile_header_t),
              1, filePtr) != 1) {
        prvWriteString("load_bmp::Error! could not read file_header");
        fclose(filePtr);
        return NULL;
    }

    // read the bitmap info header
    if (fread(bitmapInfoHeader, sizeof(bitmap_info_header_t),
              1, filePtr) != 1) {
        prvWriteString("load_bmp::Error! could not read info_header");
        fclose(filePtr);
        return NULL;
    }

	//prvWriteString("load_bmp::INFO: pass bmp header read");

    if (bitmapInfoHeader->compress_type != 0){
        printf("load_bmp::Error! compression is not supported.");
        fclose(filePtr);
		return NULL;
	}

    // move file point to the beginning of bitmap data
    if (fseek(filePtr, bitmapFileHeader.bmp_offset, SEEK_SET)) {
        prvWriteString("load_bmp::Error! could not move to start of data");
        fclose(filePtr);
        return NULL;
    }

	sprintf(buff, "load_bmp:: bmp_bytesz = %d", bitmapInfoHeader->bmp_bytesz);
    prvWriteString(buff);

    // allocate enough memory for the bitmap image data
    pixel_t *bitmapImage = pvPortMalloc(bitmapInfoHeader->bmp_bytesz *
                                  sizeof(pixel_t));

	sprintf(buff, "load_bmp::pass malloc : %d bytes, pixsize =%d",bitmapInfoHeader->bmp_bytesz, sizeof(pixel_t));
	prvWriteString(buff);
	sprintf(buff, "load_bmp::pass malloc : remaining %d bytes",xPortGetFreeHeapSize());
	prvWriteString(buff);

    // verify memory allocation
    if (bitmapImage == NULL) {
        prvWriteString("load_bmp::Error! - bitmapImage is NULL");
        fclose(filePtr);
        return NULL;
    }

	//printf("load_bmp::INFO: pass fclose");

    // read in the bitmap image data
    size_t pad, count=0;
    unsigned char c;
    pad = 4*ceil(bitmapInfoHeader->bitspp*bitmapInfoHeader->width/32.) - bitmapInfoHeader->width;

	//printf("load_bmp::INFO: pass ceil");

    size_t i;
    size_t j;

	sprintf(buff, "load_bmp:: height =%d, width =%d", (int)bitmapInfoHeader->height, (int)bitmapInfoHeader->width);
    prvWriteString(buff);

    for(i=0; i<bitmapInfoHeader->height; i++){
	    for(j=0; j<bitmapInfoHeader->width; j++){
			if (fread(&c, sizeof(unsigned char), 1, filePtr) != 1) {
			    fclose(filePtr);
				prvWriteString("load_bmp::Error! fail fread");
			    return NULL;
		    }

		    bitmapImage[count++] = (pixel_t) c;

			//sprintf(buff, "load_bmp:: i = %d, j = %d",i, j);
			//prvWriteString("loading bmp...");
	    }
	    fseek(filePtr, pad, SEEK_CUR);
	    //prvWriteString("loading bmp...");

    }

	prvWriteString("load_bmp::INFO: done looping");

	//printf("load_bmp::INFO: pass looping");

    // If we were using unsigned char as pixel_t, then:
    // fread(bitmapImage, 1, bitmapInfoHeader->bmp_bytesz, filePtr);

	//printf("load_bmp::INFO: finished");

    // close file and return bitmap image data
    fclose(filePtr);

    prvWriteString("load_bmp::INFO: closed filePtr");

    return bitmapImage;
}

// Return: true on error.
int save_bmp(const char *filename, const bitmap_info_header_t *bmp_ih,
              const pixel_t *data)
{

	prvWriteString("save_bmp::Enter");

	FILE* filePtr = fopen(filename, "wb");
    if (filePtr == NULL)
        return TRUE;

    bmpfile_magic_t mag = {{0x42, 0x4d}};
    if (fwrite(&mag, sizeof(bmpfile_magic_t), 1, filePtr) != 1) {
        fclose(filePtr);
        return TRUE;
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
        return TRUE;
    }
    if (fwrite(bmp_ih, sizeof(bitmap_info_header_t), 1, filePtr) != 1) {
        fclose(filePtr);
        return TRUE;
    }

    // Palette
    size_t i;
    for (i = 0; i < (1U << bmp_ih->bitspp); i++) {
        const rgb_t color = {(uint8_t)i, (uint8_t)i, (uint8_t)i};
        if (fwrite(&color, sizeof(rgb_t), 1, filePtr) != 1) {
            fclose(filePtr);
            return TRUE;
        }
    }

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
			    return TRUE;
		    }
	    }
	    c = 0;
	    for(j=0; j<pad; j++)
		    if (fwrite(&c, sizeof(char), 1, filePtr) != 1) {
			    fclose(filePtr);
			    return TRUE;
		    }
    }

    fclose(filePtr);

    return FALSE;
}

// if normalize is true, map pixels to range 0..MAX_BRIGHTNESS
void convolution(const pixel_t *in, pixel_t *out, const float *kernel,
                 const int nx, const int ny, const int kn,
                 const int normalize)
{

	prvWriteString("convolution::Enter");

	// perform checks
	if(!((kn % 2) == 1)){
		prvWriteString("convolution:ASSERT_FAIL: -1");
		return;
	}
	if(!((nx > kn) && (ny > kn))){
		prvWriteString("convolution:ASSERT_FAIL: -2");
		return;
	}

    int khalf = (kn / 2);
    float min = FLT_MAX, max = -FLT_MAX;

    int m,n,j,i;
	float pixel = 0.0;
    int c = 0;

	prvWriteString("convolution::Enter - after asserting");


    if (normalize){
        for (m = khalf; m < nx - khalf; m++){
            for (n = khalf; n < ny - khalf; n++) {
                pixel = 0.0;
                c = 0;
                for (j = -khalf; j <= khalf; j++){
                    for (i = -khalf; i <= khalf; i++) {
                        pixel += in[(n - j) * nx + m - i] * kernel[c];
                        c++;
						//prvWriteString("convolution:: looping (0)..");
                    }
					//prvWriteString("convolution:: looping (1)..");
				}
                if (pixel < min){
                    min = pixel;
				}
                if (pixel > max){
                    max = pixel;
                }

				//prvWriteString("convolution:: looping (2)..");
			}
		}
	}


	prvWriteString("convolution::Enter - after normalising");


    for (m = khalf; m < nx - khalf; m++){
        for (n = khalf; n < ny - khalf; n++) {
            float pixel = 0.0;
            size_t c = 0;
            for (j = -khalf; j <= khalf; j++)
                for (i = -khalf; i <= khalf; i++) {
                    pixel += in[(n - j) * nx + m - i] * kernel[c];
                    c++;
                }

            if (normalize)
                pixel = MAX_BRIGHTNESS * (pixel - min) / (max - min);
            out[n * nx + m] = (pixel_t)pixel;
        }
	}


}

/*
 * gaussianFilter:
 * http://www.songho.ca/dsp/cannyedge/cannyedge.html
 * determine size of kernel (odd #)
 * 0.0 <= sigma < 0.5 : 3
 * 0.5 <= sigma < 1.0 : 5
 * 1.0 <= sigma < 1.5 : 7
 * 1.5 <= sigma < 2.0 : 9
 * 2.0 <= sigma < 2.5 : 11
 * 2.5 <= sigma < 3.0 : 13 ...
 * kernelSize = 2 * int(2*sigma) + 3;
 */
void gaussian_filter(const pixel_t *in, pixel_t *out,
                     const int nx, const int ny, const float sigma)
{

	prvWriteString("gaussian_filter::Enter");


	const int n = 2 * (int)(2 * sigma) + 3;
	//int n=7;
    const float mean = (float)floor(n / 2.0);
    float kernel[n * n]; // variable length array

	int c = 0;
    int i,j;

	sprintf(buff, "gaussian_filter::INFO: got kernel and sigma n= %d",(int)n);
    prvWriteString(buff);


    for (i = 0; i < n; i++){
        for (j = 0; j < n; j++) {
            kernel[c] = exp(-0.5 * (pow((i - mean) / sigma, 2.0) +
                                    pow((j - mean) / sigma, 2.0)))
                        / (2 * M_PI * sigma * sigma);
            c++;

			//sprintf(buff, "gaussian_filter::looping c= %d",(int)n);
			//prvWriteString(buff);

        }
			//sprintf(buff, "gaussian_filter::looping c=%d",n);
			//prvWriteString(buff);
	}


    prvWriteString("gaussian_filter::INFO: finished looping");

    convolution(in, out, kernel, nx, ny, n, TRUE);
}

/*
 * Links:
 * http://en.wikipedia.org/wiki/Canny_edge_detector
 * http://www.tomgibara.com/computer-vision/CannyEdgeDetector.java
 * http://fourier.eng.hmc.edu/e161/lectures/canny/node1.html
 * http://www.songho.ca/dsp/cannyedge/cannyedge.html
 *
 * Note: T1 and T2 are lower and upper thresholds.
 */
pixel_t *canny_edge_detection(const pixel_t *in,
                              const bitmap_info_header_t *bmp_ih,
                              const int tmin, const int tmax,
                              const float sigma)
{

	prvWriteString("canny_edge_detection::Enter");

	const int nx = bmp_ih->width;
    const int ny = bmp_ih->height;

	/*
    pixel_t *G = vCannyCalloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *after_Gx = vCannyCalloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *after_Gy = vCannyCalloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *nms = vCannyCalloc(nx * ny * sizeof(pixel_t), 1);
	*/

	sprintf(buff, "canny_edge_detection::before malloc : remaining %d bytes",xPortGetFreeHeapSize());
	prvWriteString(buff);

	pixel_t *G = pvPortMalloc(nx * ny * sizeof(pixel_t));
    pixel_t *after_Gx = pvPortMalloc(nx * ny * sizeof(pixel_t));
    pixel_t *after_Gy = pvPortMalloc(nx * ny * sizeof(pixel_t));
    pixel_t *nms = pvPortMalloc(nx * ny * sizeof(pixel_t));
    pixel_t *out = pvPortMalloc(bmp_ih->bmp_bytesz * sizeof(pixel_t));

    sprintf(buff, "canny_edge_detection::after malloc : remaining %d bytes",xPortGetFreeHeapSize());
	prvWriteString(buff);

    if (G == NULL || after_Gx == NULL || after_Gy == NULL ||
        nms == NULL || out == NULL) {
        prvWriteString("canny_edge_detection:ERROR: Failed memory allocation(s)");
        /*exit(1);*/
		return NULL;
    }

    gaussian_filter(in, out, nx, ny, sigma);

    const float Gx[] = {-1, 0, 1,
                        -2, 0, 2,
                        -1, 0, 1};

    convolution(out, after_Gx, Gx, nx, ny, 3, FALSE);

    const float Gy[] = { 1, 2, 1,
                         0, 0, 0,
                        -1,-2,-1};

    convolution(out, after_Gy, Gy, nx, ny, 3, FALSE);

    int i,j;

    //ROSH_HACK :: added offset of +/-2 to i, j to stop border lines appearing
    for (i = 1; i < nx - 1; i++)
        for (j = 1; j < ny - 1; j++) {
            const int c = i + nx * j;
            // G[c] = abs(after_Gx[c]) + abs(after_Gy[c]);
            G[c] = (pixel_t)hypot(after_Gx[c], after_Gy[c]);
        }

    //ROSH_HACK :: added offset of +/-2 to i, j to stop border lines appearing
    // Non-maximum suppression, straightforward implementation.
    for (i = 1+2; i < nx - 1-2; i++)
        for (j = 1+2; j < ny - 1-2; j++) {
            const int c = i + nx * j;
            const int nn = c - nx;
            const int ss = c + nx;
            const int ww = c + 1;
            const int ee = c - 1;
            const int nw = nn + 1;
            const int ne = nn - 1;
            const int sw = ss + 1;
            const int se = ss - 1;

            const float dir = (float)(fmod(atan2(after_Gy[c],
                                                 after_Gx[c]) + CANNY_M_PI,
                                           CANNY_M_PI) / CANNY_M_PI) * 8;

            if (((dir <= 1 || dir > 7) && G[c] > G[ee] &&
                 G[c] > G[ww]) || // 0 deg
                ((dir > 1 && dir <= 3) && G[c] > G[nw] &&
                 G[c] > G[se]) || // 45 deg
                ((dir > 3 && dir <= 5) && G[c] > G[nn] &&
                 G[c] > G[ss]) || // 90 deg
                ((dir > 5 && dir <= 7) && G[c] > G[ne] &&
                 G[c] > G[sw]))   // 135 deg
                nms[c] = G[c];
            else
                nms[c] = 0;
        }

    // Reuse array
    // used as a stack. nx*ny/2 elements should be enough.
    int *edges = (int*) after_Gy;
    vCannyMemset(out, 0, sizeof(pixel_t) * nx * ny);
    vCannyMemset(edges, 0, sizeof(pixel_t) * nx * ny);

    // Tracing edges with hysteresis . Non-recursive implementation.
    size_t c = 1;
    for (j = 1; j < ny - 1; j++)
        for (i = 1; i < nx - 1; i++) {
            if (nms[c] >= tmax && out[c] == 0) { // trace edges
                out[c] = MAX_BRIGHTNESS;
                int nedges = 1;
                edges[0] = c;

                do {
                    nedges--;
                    const int t = edges[nedges];

                    int nbs[8]; // neighbours
                    nbs[0] = t - nx;     // nn
                    nbs[1] = t + nx;     // ss
                    nbs[2] = t + 1;      // ww
                    nbs[3] = t - 1;      // ee
                    nbs[4] = nbs[0] + 1; // nw
                    nbs[5] = nbs[0] - 1; // ne
                    nbs[6] = nbs[1] + 1; // sw
                    nbs[7] = nbs[1] - 1; // sein

                    int k;
                    for (k = 0; k < 8; k++)
                        if (nms[nbs[k]] >= tmin && out[nbs[k]] == 0) {
                            out[nbs[k]] = MAX_BRIGHTNESS;
                            edges[nedges] = nbs[k];
                            nedges++;
                        }
                } while (nedges > 0);
            }
            c++;
        }

    //_redrawBorder(out, nx,ny);

    vPortFree(after_Gx);
    vPortFree(after_Gy);
    vPortFree(G);
    vPortFree(nms);

    return out;
}


/***********************
	HELPER functions
***********************/

void _redrawBorder(pixel_t *image, int w, int h){

    int i,j;
    int ix = 0;
    int ix_t = 0;

    // top border
    j=0;
    for (i = 0; i < w; i++) {
        ix = i+(j*w);
        ix_t = i+(3*w);
        image[ix] = image[ix_t];

        ix = i+((j+1)*w);
        image[ix] = image[ix_t];

    }

    // bottom border
    j=w*(h-1);
    for (i = 0; i < w; i++) {
        ix = i+(j*w);
        ix_t = i+(j-3);
        image[ix] = image[ix_t];

        ix = i+((j-1)*w);
        image[ix] = image[ix_t];

    }

    // left
    for (j = 0; j < h; j++) {
        ix = (j*w);
        ix_t = (j*w)+3;
        image[ix] = image[ix_t];

        ix = (j*(w+1));
        image[ix] = image[ix_t];
    }

    // right border
    for (j = 1; j < h; j++) {
        ix = (j*w)-1;
        ix_t = (j*w)-4;
        image[ix] = image[ix_t];

        ix = (j*w)-2;
        image[ix] = image[ix_t];

    }

}


void _printImg(pixel_t* img, int w, int h){
    int i;
    prvWriteString("_printImg::Enter");
    for(i=0;i<(w*h);i++){
        sprintf(buff, "%d",img[i]);
        prvWriteString(buff);
    }
}


void * vCannyCalloc(size_t nmemb, size_t lsize)
{

	//prvWriteString("vCannyCalloc::Enter");

	void *result;
	size_t size=lsize * nmemb;

	/* guard vs integer overflow, but allow nmemb
	 * to fall through and call pvPortMalloc(0) */
	if (nmemb && lsize != (size / nmemb)) {
		//__set_errno(ENOMEM);
		return NULL;
	}
	if ((result=pvPortMalloc(size)) != NULL) {
		vCannyMemset(result, 0, size);
	}
	return result;
}

/*-----------------------------------------------------------*/

void *vCannyMemset(void *_p, unsigned v, unsigned count)
{

	//prvWriteString("vCannyMemset::Enter");
	unsigned char *p = _p;
    while(count-- > 0) *p++ = 0;
    return _p;
}




// entry point
int Canny_Start()
{

    portENTER_CRITICAL();
    vTaskSuspendAll();

    prvWriteString("Canny_Start::Enter");

    int id;
    id = impProcessorId();

    if(id == 0){
        //vPortInitialiseBlocks();

        static bitmap_info_header_t ih;
        pixel_t *in_bitmap_data = load_bmp("image.bmp", &ih);
        if (in_bitmap_data == NULL) {
            prvWriteString("Canny_Start:ERROR: BMP image not loaded.");
            return 1;
        }

        sprintf(buff, "Canny_Start:Info: %d x %d x %d", (int)ih.width, (int)ih.height, (int)ih.bitspp);
        prvWriteString(buff);

        pixel_t *out_bitmap_data =
            canny_edge_detection(in_bitmap_data, &ih, 45, 50, 1.0f);
        if (out_bitmap_data == NULL) {
            prvWriteString("Canny_Start:ERROR: failed canny_edge_detection.");
            return 1;
        }

        if (save_bmp("out.bmp", &ih, out_bitmap_data)) {
            prvWriteString("Canny_Start:ERROR: BMP image not saved.");
            return 1;
        }
    }

    xTaskResumeAll();
    portEXIT_CRITICAL();

    //vPortFree((pixel_t*)in_bitmap_data);
    //vPortFree((pixel_t*)out_bitmap_data);
    return 0;
}



int Canny_ImgSplitTest(){

    //portENTER_CRITICAL();
    //vTaskSuspendAll();

    int id;
    pixel_t *out_bitmap_data = NULL;
    id = impProcessorId();

    prvWriteString("===================================================");
    sprintf(buff, ">>>>>>>>>>>  Canny_ImgSplitTest: ID!! - %d !", (int)id);
    prvWriteString(buff);

    /* Initialise globals */
    if(id==0){
        //InitialiseDecoderFlags();
        InitialiseCanny();
        CoreFlags->CannyDetect_Finished = 0;
    }

    switch(id){

			// Core - 0
			// ---------
			// load image, split image, perform canny on block-0
			// save result in shared mem
			// output full canny image to host
			case 0 :
			    prvWriteString("Canny_ImgSplitTest::Enter");

                static bitmap_info_header_t ih;
                pixel_t *in_bitmap_data = load_bmp("image.bmp", &ih);
                if (in_bitmap_data == NULL) {
                    prvWriteString("Canny_ImgSplitTest:ERROR: BMP image not loaded.");
                    for( ;; ) { }
                    //return 1;
                }

                prvWriteString("Canny_ImgSplitTest:: out of load_bmp");

                sprintf(buff, "Canny_ImgSplitTest:Info: %d x %d x %d", (int)ih.width, (int)ih.height, (int)ih.bitspp);
                prvWriteString(buff);

                if(ImgSplit_SplitIntoBlocks(in_bitmap_data, &ih) ==1){

                    // booleans to signal the other cores to start working
                    SetAllFlags_b(CoreFlags->CoreReady, NUM_CORES, 1);
                }
                prvWriteString("Canny_ImgSplitTest: Core 0 starting !");

                out_bitmap_data = CommonPerformCanny("temp_blk0.bmp",0);
                CoreFlags->CoresCannyFinished[0] = 1;

                if (out_bitmap_data == NULL) {
                    prvWriteString("Canny_ImgSplitTest:ERROR: failed canny_edge_detection.");
                    for( ;; ) { }
                }


                // wait until all cores have finished decoding and saved canny
                // block output to shared memory. once this is done, the final
                // image will be written out
                while(!HaveAllCoresFinished()){
                    //prvWriteString("Canny_ImgSplitTest: Core 0 waiting.. !");
                    asm volatile("mov r0, r0");
                }

                // proceed to save final image
                if (save_bmp("out.bmp", &ih, out_bitmap_data)) {
                    prvWriteString("Canny_ImgSplitTest:ERROR: BMP image not saved.");
                    return 1;
                }else{
                    prvWriteString("Canny_ImgSplitTest:: BMP Saved");
                }

                _wait(1000);    // wait for proper bmp write out - safety check

                sprintf(buff, "Canny_ImgSplitTest:: before freeing : remaining %d bytes",xPortGetFreeHeapSize());
                prvWriteString(buff);
                vPortFree((pixel_t*)in_bitmap_data);
                vPortFree((pixel_t*)out_bitmap_data);
                FreeLocalCoreStructs();

                InitialiseCanny(); // init again ?? bug ?
                CoreFlags->CannyDetect_Finished = 1;
                _wait(100); // sometimes shared volatile var update takes time ?
                sprintf(buff, "Canny_ImgSplitTest::after freeing : remaining %d bytes",xPortGetFreeHeapSize());
                prvWriteString(buff);

                prvWriteString("Canny_ImgSplitTest: Core 0 -- Canny Finished!!!");
                //for( ;; ) { }

                NotifyEndofSession();
                _wait(10000000); // wait for a bit for python to do it's thing


                break;

			// Cores : 1 - n
			// ---------
			// perform canny on block-1
			// save result in shared mem
			case 1 :
			case 2 :
            case 3 :
            case 4 :
            case 5 :
            case 6 :
            case 7 :
            case 8 :
            case 9 :
            case 10 :
            case 11 :
            case 12 :
            case 13 :
            case 14 :
            case 15 :
                CoreFlags->CoresCannyFinished[id] = 1;
                while(CoreFlags->CoreReady[id]!=1){
                    //prvWriteString("Canny_ImgSplitTest: waiting...");
                    asm volatile("mov r0, r0");
                }
                CoreFlags->CoresCannyFinished[id] = 0;
                prvWriteString("Canny_ImgSplitTest: Core starting !");

                out_bitmap_data = CommonPerformCanny("temp_blk.bmp",id);
                CoreFlags->CoresCannyFinished[id] = 1; // is this the best place to signal this ??
                if (out_bitmap_data == NULL) {
                    prvWriteString("Canny_ImgSplitTest:ERROR: failed canny_edge_detection.");
                    for( ;; ) { }
                    return 1;
                }

                prvWriteString("Canny_ImgSplitTest: Core finished .. !");

                //for( ;; ) { }
                // wait until canny detection is finished - wait for signal from core-0
                while((volatile int)CoreFlags->CannyDetect_Finished != 1){
                    asm volatile("mov r0, r0");
                }
                sprintf(buff, "Canny_ImgSplitTest:: before freeing : remaining %d bytes",xPortGetFreeHeapSize());
                prvWriteString(buff);

                vPortFree((pixel_t*)out_bitmap_data);
                FreeLocalCoreStructs();

                sprintf(buff, "Canny_ImgSplitTest:: after freeing : remaining %d bytes",xPortGetFreeHeapSize());
                prvWriteString(buff);

                prvWriteString("Canny_ImgSplitTest: Canny Finished!!!");

				break;

            default:
                sprintf(buff, "Canny_ImgSplitTest: wrong core ID!! - %d !", (int)id);
                prvWriteString(buff);
                break;

		}

    // initialise local core memory
    //vPortInitialiseBlocks();

    //xTaskResumeAll();
    //portEXIT_CRITICAL();
    prvWriteString("Canny_ImgSplitTest: Exit");

    return 1;

}

pixel_t * CommonPerformCanny(const char* fname, int id){

    prvWriteString("CommonPerformCanny:ENTER");

    in_blk_data = ImgSplit_LoadBlockToLocalMem(id);

    if(in_blk_data != NULL){

        local_bmp_ih = ImgSplit_GetLocalBmpIH(id);
        _DEBUG_PrintBmpIH(local_bmp_ih);

        //_printImg(in_blk_data,local_bmp_ih->width, local_bmp_ih->height);
        //sprintf(buff, "temp_out_%d.bmp", id);
        //ImgSplit_SaveBlockBMP(fname, in_blk_data);

        pixel_t *out_bitmap_data_common =
        canny_edge_detection(in_blk_data, local_bmp_ih, 45, 50, 0.5f);

        //_printImg(out_bitmap_data,local_bmp_ih->width, local_bmp_ih->height);

        if (out_bitmap_data_common == NULL) {
            prvWriteString("CommonPerformCanny:ERROR: failed canny_edge_detection.");
            return NULL;
        }

        pixel_t *output_canny_blocks = ImgSplit_CombineBlocks(out_bitmap_data_common, id); // combine the result to the

        return output_canny_blocks;
    }else{
        return NULL;
    }
}

void InitialiseCanny(){

    // initialise structures
    //Initialise_LocalCore_Structs();
    Initialise_Global_DecoderFlags();

    ImgSplit_Initialise();

}

void Initialise_LocalCore_Structs(){

    in_blk_data = NULL;
    local_bmp_ih = NULL;

}

void FreeLocalCoreStructs(){

    sprintf(buff, "FreeLocalCoreStructs:: before freeing : remaining %d bytes",xPortGetFreeHeapSize());
    prvWriteString(buff);
    vPortFree((pixel_t *)in_blk_data);
    vPortFree((bitmap_info_header_t *)local_bmp_ih);
    sprintf(buff, "FreeLocalCoreStructs:: after freeing : remaining %d bytes",xPortGetFreeHeapSize());
    prvWriteString(buff);

}

void Initialise_Global_DecoderFlags(){
    int i =0;

    prvWriteString("Initialise_Global_DecoderFlags:Enter");

    for(i=0;i<NUM_CORES;i++){
        CoreFlags->CoreReady[i] = 0;
        CoreFlags->CoresCannyFinished[i] = 0;
    }

    prvWriteString("Initialise_Global_DecoderFlags:Exit");

}


void SetCoresCannyFinished_Flag(int core_id, int b){
    CoreFlags->CoresCannyFinished[core_id] = b;
}

// params :
// arr - array_of_flags
// arr_size - size of array
int HaveAllCoresFinished(){
    int i;
    int flag = 1;

    // if any of them equal b, then break and reset result
    for(i=0;i<NUM_CORES;i++){
        if((volatile int)CoreFlags->CoresCannyFinished[i]==0){
            //sprintf(buff, "HaveAllCoresFinished:: Core %d is still busy",i);
            //prvWriteString(buff);

            flag = 0;
            break;
        }
    }
    return flag;
}
int CheckArrayOfFlags_isFALSE(volatile int arr[], int arr_size){
    int i;
    int flag = 1;

    // if any of them equal b, then break and reset result
    for(i=0;i<arr_size;i++){
        if(arr[i]==1){
            flag = 0;
            break;
        }
    }

    return flag;
}
void SetAllFlags_b(volatile int arr[], int arr_size, int b){
    int i;
    // if any of them equal b, then break and reset result
    for(i=0;i<arr_size;i++){
       if(ImgSplit_DoesCoreHaveValidBlock(i) == 1){
        arr[i] = b;
       }
    }
}

// writes out to a file to let external simulator framework
// know that one run is finished
void NotifyEndofSession(){
    static int counter = 1;

    FILE *f = fopen("CannyCounter.txt", "w");
    if (f == NULL){
       prvWriteString("NotifyEndofSession::FileIO Error");
       return;
    }

    /* write out the counter value */
    fprintf(f, "%d", counter);
    fclose(f);

    counter++;

    return;
}


void _DEBUG_PrintBmpIH(bitmap_info_header_t *local_bmp_ih){

   #if (CANNY_DEBUG)

    prvWriteString("********************");
    sprintf(buff, "_DEBUG_PrintBmpIH:: local_bmp_ih_struct_adr = %p", local_bmp_ih);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: header_sz = %d", local_bmp_ih->header_sz);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: nplanes = %d", local_bmp_ih->nplanes);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: bitspp = %d", local_bmp_ih->bitspp);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: compress_type = %d", local_bmp_ih->compress_type);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: hres = %d", local_bmp_ih->hres);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: vres = %d", local_bmp_ih->vres);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: ncolors = %d", local_bmp_ih->ncolors);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: nimpcolors = %d", local_bmp_ih->nimpcolors);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: bmp_bytesz = %d", local_bmp_ih->bmp_bytesz);
    prvWriteString(buff);
    sprintf(buff, "_DEBUG_PrintBmpIH:: height = %d", local_bmp_ih->height);
    prvWriteString(buff);Some text: %s\n
    sprintf(buff, "_DEBUG_PrintBmpIH:: width = %d", local_bmp_ih->width);
    prvWriteString(buff);
    prvWriteString("********************");

    #endif

}
