#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include "tim_defs.h"
#include "timpack.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define ALIGN_UP(x, y) (((x % y) != 0) ? (x + y - (x % y)) : x)

// todo: in it's own header, share with sdl viewer app
static const uint16_t aui16PixFmtNumColours[TIM_PIX_FMT_COUNT] =
{
	16, // TIM_PIX_FMT_4BIT_CLUT
	256 // TIM_PIX_FMT_8BIT_CLUT
};

// todo: in it's own header, share with sdl viewer app
#define U5_MASK (0x1F) // The lower 5 bits
#define CONV_U8_TO_U5(x) (U5_MASK & (uint8_t)((float)x * (31.0f / 255.0f)))

static void RGB8ToTIMPix(
	TIM_PIX* psPixel,
	const uint8_t ui8Red,
	const uint8_t ui8Green,
	const uint8_t ui8Blue)
{
	psPixel->r = CONV_U8_TO_U5(ui8Red);
	psPixel->g = CONV_U8_TO_U5(ui8Green);
	psPixel->b = CONV_U8_TO_U5(ui8Blue);
	psPixel->stp = 0x1;
}

static int LoadPalette(
	const char* pszFileName,
	const TIM_PIX_FMT ePixFmt,
	const uint16_t ui16FBCoordX,
	const uint16_t ui16FBCoordY,
	TIM_BLOCK_HEADER* psCLUTHeader,
	TIM_PIX** ppsCLUTData)
{
	// Data from the loaded palette image
	int iWidth = 0;
	int iHeight = 0;
	int iNumChannels = 0;

	uint8_t* pui8Image = stbi_load(
		pszFileName,
		&iWidth,
		&iHeight,
		&iNumChannels,
		0
	);

	if (pui8Image == NULL)
	{
		printf("palette failed to load\n");
		return 1;
	}

	printf("loaded %i * %i palette with %i channels\n", iWidth, iHeight, iNumChannels);

	// NOTE: Assumes multi entry palette is stacked vertically
	psCLUTHeader->ui16Width = ALIGN_UP(iWidth, aui16PixFmtNumColours[ePixFmt]);
	psCLUTHeader->ui16Height = iHeight;

	if (iWidth < aui16PixFmtNumColours[ePixFmt])
	{
		printf(
			"palette has fewer than %u colours, resulting palette will be padded\n",
			aui16PixFmtNumColours[ePixFmt]
		);
	}
	else
	{
		if ((iWidth % aui16PixFmtNumColours[ePixFmt]) != 0)
		{
			printf("multi-entry palette is not correctly aligned\n");
			goto FAILED_LoadPalette;
		}
	}

	printf(
		"constructing %u palette(s), need to allocate %u pixels\n",
		psCLUTHeader->ui16Height,
		psCLUTHeader->ui16Width * psCLUTHeader->ui16Height
	);

	// Set the destination coordinates for the CLUT data within VRAM
	// TODO: Check this against the width/height to make sure we don't overflow
	psCLUTHeader->ui16FBCoordX = ui16FBCoordX;
	psCLUTHeader->ui16FBCoordY = ui16FBCoordY;

	// Copy the pixel data, whilst converting it to 15 bit colour
	{
		const uint8_t* pui8PixelData = pui8Image;

		uint32_t ui32NumColours = (
			psCLUTHeader->ui16Width * psCLUTHeader->ui16Height
		);

		TIM_PIX* psCLUTData = calloc(ui32NumColours, sizeof(TIM_PIX));
		if (psCLUTData == NULL)
		{
			printf("failed to allocate palette data\n");
			goto FAILED_LoadPalette;
		}

		printf(
			"allocated %lu bytes for CLUT data block\n",
			(
				psCLUTHeader->ui16Width *
				psCLUTHeader->ui16Height *
				sizeof(TIM_PIX)
			)
		);

		// Set the size of the CLUT data block, which includes the header
		psCLUTHeader->ui32SizeInBytes = (
			sizeof(TIM_BLOCK_HEADER) +
			(
				psCLUTHeader->ui16Width *
				psCLUTHeader->ui16Height *
				sizeof(TIM_PIX)
			)
		);

		for (uint32_t i = 0; i < ui32NumColours; ++i, pui8PixelData += iNumChannels)
		{
			RGB8ToTIMPix(
				&psCLUTData[i],
				pui8PixelData[0],
				pui8PixelData[1],
				pui8PixelData[2]
			);

			// printf(
			// 	"palette[%u]: (%u, %u, %u) -> (%u, %u, %u)\n",
			// 	i,
			// 	pui8PixelData[0],
			// 	pui8PixelData[1],
			// 	pui8PixelData[2],
			// 	psCLUTData[i].r,
			// 	psCLUTData[i].g,
			// 	psCLUTData[i].b
			// );
		}

		*ppsCLUTData = psCLUTData;
	}

	stbi_image_free(pui8Image);
	return 0;

FAILED_LoadPalette:
	stbi_image_free(pui8Image);
	return 1;
}

static bool CompareTIMPix(const TIM_PIX* psA, const TIM_PIX* psB)
{
	return (
		(psA->r == psB->r) &&
		(psA->g == psB->g) &&
		(psA->b == psB->b) &&
		(psA->stp == psB->stp)
	);
}

static int LoadTexture(
	const char* pszFileName,
	const TIM_PIX_FMT ePixFmt,
	const uint16_t ui16FBCoordX,
	const uint16_t ui16FBCoordY,
	const TIM_PIX* psPaletteColours,
	TIM_BLOCK_HEADER* psPixelHeader,
	uint8_t** ppui8PixelData)
{
	// The number of pixels present in the loaded image
	uint32_t ui32NumColours = 0;
	uint32_t ui32AllocationSize = 0;

	// Temp to copy the output
	uint8_t* pui8Indices = NULL;

	// Data from the loaded palette image
	int iWidth = 0;
	int iHeight = 0;
	int iNumChannels = 0;
	uint8_t* pui8Image = stbi_load(
		pszFileName,
		&iWidth,
		&iHeight,
		&iNumChannels,
		0
	);

	if (pui8Image == NULL)
	{
		printf("texture failed to load\n");
		return 1;
	}

	printf("loaded %i * %i texture with %i channels\n", iWidth, iHeight, iNumChannels);

	// Width of image must be a multiple for 4 for 4BPP, or 2 for 8BPP
	if (iWidth % ((ePixFmt == TIM_PIX_FMT_4BIT_CLUT) ? 4 : 2) != 0)
	{
		printf(
			"Invalid image width, must be a multiple of %u (%u provided)\n",
			((ePixFmt == TIM_PIX_FMT_4BIT_CLUT) ? 4 : 2),
			iWidth
		);
		goto FAILED_LoadTexture;
	}

	// Calculate the allocation size
	ui32NumColours = iWidth * iHeight;
	ui32AllocationSize = ALIGN_UP(
		((ePixFmt == TIM_PIX_FMT_4BIT_CLUT) ? (ui32NumColours / 2) : ui32NumColours),
		4 // Align the allocation to 4 bytes
	);

	pui8Indices = calloc(ui32AllocationSize, sizeof(uint8_t));

	if (pui8Indices == NULL)
	{
		printf("failed to allocate indices data\n");
		goto FAILED_LoadTexture;
	}

	printf("allocating %u bytes for the Pixel data\n", ui32AllocationSize);

	// Set the size of the pixel data block, which includes the header
	psPixelHeader->ui32SizeInBytes = (
		ui32AllocationSize + sizeof(TIM_BLOCK_HEADER)
	);

	// Set the destination coordinates for the pixel data within VRAM
	// TODO: Check this against the width/height to make sure we don't overflow
	psPixelHeader->ui16FBCoordX = ui16FBCoordX;
	psPixelHeader->ui16FBCoordY = ui16FBCoordY;

	psPixelHeader->ui16Width = iWidth;
	psPixelHeader->ui16Height = iHeight;

	// Convert each pixel to 15 bit, find it's index in the palette
	{
		TIM_PIX sTempCol = {0};
		const uint8_t* pui8PixelData = pui8Image;
		for (uint32_t i = 0; i < ui32NumColours; ++i, pui8PixelData += iNumChannels)
		{
			bool bFoundColour = false;

			RGB8ToTIMPix(
				&sTempCol,
				pui8PixelData[0],
				pui8PixelData[1],
				pui8PixelData[2]
			);

			for (uint16_t j = 0; j < aui16PixFmtNumColours[ePixFmt]; ++j)
			{
				if (CompareTIMPix(&sTempCol, &psPaletteColours[j]))
				{
					// If it's 4BPP, we need to pack two indices into one char
					if (ePixFmt == TIM_PIX_FMT_4BIT_CLUT)
					{
						pui8Indices[i / 2] |= (
							(i % 2) ?
							(j << 4) :
							j
						);

						// printf(
						// 	"pixel[%u] = %u (%02X)\n",
						// 	(i / 2),
						// 	pui8Indices[i / 2],
						// 	pui8Indices[i / 2]
						// );
					}
					else
					{
						// printf("pixel[%u] = %u (%02X)\n", i, j, j);
						pui8Indices[i] = j;
					}

					bFoundColour = true;
					break;
				}
			}

			if (!bFoundColour)
			{
				printf("failed to find colour in palette! aborting\n");
				goto FAILED_LoadTexture;
			}
		}
	}

	*ppui8PixelData = pui8Indices;

	stbi_image_free(pui8Image);
	return 0;

FAILED_LoadTexture:
	stbi_image_free(pui8Image);
	return 1;
}

int PackTIM(const TIM_ARGS* psTIMArgs)
{
	// TODO: SetHeader?
	TIM_FILE sFile = {
		.sFileHeader = {
			.ui32ID = TIM_FILE_HEADER_ID,
			.sFlags =
			{
				.uMode = psTIMArgs->ePixFmt,
				.uClut = TIM_PIX_FMT_HAS_CLUT(psTIMArgs->ePixFmt)
			}
		}
	};

	printf(
		"bpp mode: %s\n"
		"texture: %s\n"
		"palette: %s\n"
		"output file: %s\n",
		// TODO: static array of string
		psTIMArgs->ePixFmt == TIM_PIX_FMT_4BIT_CLUT ? "4bpp" : "8bpp",
		psTIMArgs->pszTextureFileName,
		psTIMArgs->pszPaletteFileName,
		psTIMArgs->pszOutputFileName
	);

	if (LoadPalette(
			psTIMArgs->pszPaletteFileName,
			psTIMArgs->ePixFmt,
			psTIMArgs->ui16PaletteCoordX,
			psTIMArgs->ui16PaletteCoordY,
			&sFile.sCLUTHeader,
			&sFile.psCLUTData
		) != 0)
	{
		printf("failed to load palette\n");
		return 1;
	}

	if (LoadTexture(
			psTIMArgs->pszTextureFileName,
			psTIMArgs->ePixFmt,
			psTIMArgs->ui16TextureCoordX,
			psTIMArgs->ui16TextureCoordY,
			sFile.psCLUTData,
			&sFile.sPixelHeader,
			&sFile.pui8PixelData
		) != 0)
	{
		printf("failed to load Texture\n");
		DestroyTIM(&sFile);
		return 1;
	}

	if (WriteTIM(psTIMArgs->pszOutputFileName, &sFile) != 0)
	{
		printf("failed to write TIM\n");
		DestroyTIM(&sFile);
		return 1;
	}

	DestroyTIM(&sFile);

	return 0;
}