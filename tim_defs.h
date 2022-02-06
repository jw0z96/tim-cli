#ifndef TIMDEFS_H
#define TIMDEFS_H

#include <stdint.h>

#define TIM_FILE_HEADER_ID (0x10)

typedef enum _TIM_PIX_FMT
{
	TIM_PIX_FMT_4BIT_CLUT = 0x0,
	TIM_PIX_FMT_8BIT_CLUT = 0x1,
	TIM_PIX_FMT_15BIT_DIRECT = 0x2,
	TIM_PIX_FMT_24BIT_DIRECT = 0x3,
	TIM_PIX_FMT_COUNT
} TIM_PIX_FMT;

#define TIM_PIX_FMT_HAS_CLUT(x) (x < TIM_PIX_FMT_15BIT_DIRECT)

typedef struct _TIM_FLAGS
{
	uint32_t uMode: 3, uClut: 1, reserved: 28;
} TIM_FLAGS;

typedef struct _TIM_FILE_HEADER
{
	uint32_t ui32ID;
	TIM_FLAGS sFlags;
} TIM_FILE_HEADER;

typedef struct _TIM_BLOCK_HEADER
{
	// The size of this block in bytes, which is the size of this struct plus
	// the data segment
	uint32_t ui32SizeInBytes;

	// The destination coordinates for this data block within VRAM
	uint16_t ui16FBCoordX;
	uint16_t ui16FBCoordY;

	// The dimensions of this block's data segment
	uint16_t ui16Width;
	uint16_t ui16Height;
} TIM_BLOCK_HEADER;

typedef struct _TIM_PIX
{
	uint16_t r: 5, g: 5, b: 5, stp: 1;
} TIM_PIX;

typedef struct _TIM_FILE
{
	TIM_FILE_HEADER sFileHeader;

	// CLUT Block
	TIM_BLOCK_HEADER sCLUTHeader;
	TIM_PIX* psCLUTData;

	// Pixel Block
	TIM_BLOCK_HEADER sPixelHeader;
	uint8_t* pui8PixelData;
} TIM_FILE;

int WriteTIM(const char* pszOutputFileName, const TIM_FILE* psFile);

int ReadTIM(const char* pszInputFileName, TIM_FILE* psFile);

void DestroyTIM(TIM_FILE* psFile);

// int DecodeTIMPixelData(const TIM_FILE* psFile, uint32_t** ppui32PixelData);

#endif // TIMDEFS_H