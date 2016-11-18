#ifndef CANNY_H_
#define CANNY_H_


#define MAX_BRIGHTNESS 255

// C99 doesn't define M_PI (GNU-C99 does)
#define CANNY_M_PI 3.14159265358979323846264338327

#include <stdint.h>

#define CANNY_DEBUG 0


typedef struct {
    uint8_t magic[2];
} bmpfile_magic_t;

typedef struct {
    uint32_t filesz;
    uint16_t creator1;
    uint16_t creator2;
    uint32_t bmp_offset;
} bmpfile_header_t;

typedef struct {
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
} bitmap_info_header_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t nothing;
} rgb_t;

// Use short int instead `unsigned char' so that we can
// store negative values.
typedef short int pixel_t;


// function prototypes
pixel_t *load_bmp(const char *filename,
					bitmap_info_header_t *bitmapInfoHeader);

int save_bmp(const char *filename, const bitmap_info_header_t *bmp_ih,
					const pixel_t *data);

void convolution(const pixel_t *in, pixel_t *out, const float *kernel,
                 const int nx, const int ny, const int kn,
                 const int normalize);

void gaussian_filter(const pixel_t *in, pixel_t *out,
                     const int nx, const int ny, const float sigma);

pixel_t *canny_edge_detection(const pixel_t *in,
                              const bitmap_info_header_t *bmp_ih,
                              const int tmin, const int tmax,
                              const float sigma);
void InitialiseCanny(void);
int Canny_Start(void);

// test functions
int Canny_ImgSplitTest(void);
void Initialise_Global_DecoderFlags(void);
void Initialise_LocalCore_Structs(void);
void FreeLocalCoreStructs(void);
pixel_t * CommonPerformCanny(const char* fname, int id);

// helpers
void * vCannyCalloc(size_t nmemb, size_t lsize);
void * vCannyMemset(void *_p, unsigned v, unsigned count);


void _redrawBorder(pixel_t *image, int w, int h);
void _printImg(pixel_t* img, int w, int h);
int HaveAllCoresFinished(void);
int CheckArrayOfFlags_isFALSE(volatile int arr[], int arr_size);
void SetAllFlags_b(volatile int arr[], int arr_size, int b);
void NotifyEndofSession(void);
void SetCoresCannyFinished_Flag(int core_id, int b);


void _DEBUG_PrintBmpIH(bitmap_info_header_t *local_bmp_ih);

// macros
/*
#define CANNY_ASSERT(xy,MSG) do \
{ \
if(!(xy))) \
{ \
prvWriteString(MSG);\
} \
}while(0)
*/

#define FALSE 0
#define TRUE 1



#endif /* CANNY_H_ */
