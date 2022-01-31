#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include "timpack.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// todo: in it's own header, share with sdl viewer app
typedef struct _TIM_PIX
{
	uint16_t r: 5, g: 5, b: 5, stp: 1;
} TIM_PIX;

// todo: in it's own header, share with sdl viewer app
typedef struct _TIM_PALETTE
{
	// one palette can hold multiple tables, for palette switching
	uint32_t ui32NumEntries;

	// The palettes are stored in the expected 16-bit color format (5:5:5)
	TIM_PIX* psColours;
} TIM_PALETTE;

// todo: in it's own header, share with sdl viewer app
typedef struct _TIM_TEXTURE
{
	uint16_t ui16Width;
	uint16_t ui16Height;
	uint8_t* pui8Indices;
} TIM_TEXTURE;

// todo: in it's own header, share with sdl viewer app
static const uint16_t aui16PixFmtNumColours[TIM_PIX_FMT_COUNT] =
{
	16, // TIM_PIX_FMT_4BPP
	256 // TIM_PIX_FMT_8BPP
};

static const uint32_t ui32TIMHeaderID = 0x10;

typedef struct _TIM_FLAG
{
	uint32_t mode: 3, clut: 1, reserved: 28;
} TIM_FLAG;

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
	TIM_PALETTE* psPalette)
{
	// Data from the loaded palette image
	uint8_t* pui8Image = NULL;
	int iWidth = 0;
	int iHeight = 0;
	int iNumChannels = 0;

	// The number of pixels present in the loaded image
	uint32_t ui32NumColours = 0;
	const uint8_t* pui8PixelData = NULL;

	pui8Image = stbi_load(
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

	ui32NumColours = iWidth * iHeight;

	if (ui32NumColours < aui16PixFmtNumColours[ePixFmt])
	{
		printf(
			"palette has fewer than %u colours, resulting palette will be padded\n",
			aui16PixFmtNumColours[ePixFmt]
		);
		psPalette->ui32NumEntries = 1;
	}
	else
	{
		if ((ui32NumColours % aui16PixFmtNumColours[ePixFmt]) != 0)
		{
			printf("multi-entry palette is not correctly aligned\n");
			goto FAILED_LoadPalette;
		}

		psPalette->ui32NumEntries = ui32NumColours / aui16PixFmtNumColours[ePixFmt];
	}

	printf(
		"constructing %u palette(s), need to allocate %u pixels\n",
		psPalette->ui32NumEntries,
		psPalette->ui32NumEntries * aui16PixFmtNumColours[ePixFmt]
	);

	psPalette->psColours = calloc(
		(psPalette->ui32NumEntries * aui16PixFmtNumColours[ePixFmt]),
		sizeof(TIM_PIX)
	);

	if (psPalette->psColours == NULL)
	{
		printf("failed to allocate palette data\n");
		goto FAILED_LoadPalette;
	}

	pui8PixelData = pui8Image;
	for (uint32_t i = 0; i < ui32NumColours; ++i, pui8PixelData += iNumChannels)
	{
		RGB8ToTIMPix(
			&psPalette->psColours[i],
			pui8PixelData[0],
			pui8PixelData[1],
			pui8PixelData[2]
		);

		printf(
			"palette[%u]: (%u, %u, %u) -> (%u, %u, %u)\n",
			i,
			pui8PixelData[0],
			pui8PixelData[1],
			pui8PixelData[2],
			psPalette->psColours[i].r,
			psPalette->psColours[i].g,
			psPalette->psColours[i].b
		);
	}

	stbi_image_free(pui8Image);
	return 0;

FAILED_LoadPalette:
	stbi_image_free(pui8Image);
	return 1;
}

static void DestroyPalette(TIM_PALETTE* psPalette)
{
	free(psPalette->psColours);
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
	const TIM_PIX* psPaletteColours,
	TIM_TEXTURE* psTexture)
{
	// Data from the loaded palette image
	uint8_t* pui8Image = NULL;
	int iWidth = 0;
	int iHeight = 0;
	int iNumChannels = 0;

	// The number of pixels present in the loaded image
	uint32_t ui32NumColours = 0;

	// Copy of the loaded pixel data pointer for when we're converting
	const uint8_t* pui8PixelData = NULL;

	// A temporary local colour value for comparing the loaded pixels to those
	// of the converted palette
	TIM_PIX sTempCol = {0};

	pui8Image = stbi_load(
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
	if (iWidth % ((ePixFmt == TIM_PIX_FMT_4BPP) ? 4 : 2) != 0)
	{
		printf(
			"Invalid image width, must be a multiple of %u (%u provided)\n",
			((ePixFmt == TIM_PIX_FMT_4BPP) ? 4 : 2),
			iWidth
		);
		goto FAILED_LoadTexture;
	}

	psTexture->ui16Width = iWidth;
	psTexture->ui16Height = iHeight;

	ui32NumColours = iWidth * iHeight;

	psTexture->pui8Indices = calloc(
		((ePixFmt == TIM_PIX_FMT_4BPP) ? (ui32NumColours / 2) : ui32NumColours),
		sizeof(uint8_t)
	);

	if (psTexture->pui8Indices == NULL)
	{
		printf("failed to allocate indices data\n");
		goto FAILED_LoadTexture;
	}

	pui8PixelData = pui8Image;
	for (uint32_t i = 0; i < ui32NumColours; ++i, pui8PixelData += iNumChannels)
	{
		bool bFoundColour = false;

		// Convert this colour to TIM format
		RGB8ToTIMPix(
			&sTempCol,
			pui8PixelData[0],
			pui8PixelData[1],
			pui8PixelData[2]
		);

		// Search for this colour in the palette
		for (uint16_t j = 0; j < aui16PixFmtNumColours[ePixFmt]; ++j)
		{
			if (CompareTIMPix(&sTempCol, &psPaletteColours[j]))
			{
				// If it's 4BPP, we need to pack two indices into one char
				if (ePixFmt == TIM_PIX_FMT_4BPP)
				{
					psTexture->pui8Indices[i / 2] |= (
						(i % 2) ?
						(j << 4) :
						j
					);

					printf(
						"pixel[%u] = %u (%02X)\n",
						(i / 2),
						psTexture->pui8Indices[i / 2],
						psTexture->pui8Indices[i / 2]
					);
				}
				else
				{
					printf("pixel[%u] = %u (%02X)\n", i, j, j);
					psTexture->pui8Indices[i] = j;
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

	stbi_image_free(pui8Image);
	return 0;

FAILED_LoadTexture:
	stbi_image_free(pui8Image);
	return 1;
}

static void DestroyTexture(TIM_TEXTURE* psTexture)
{
	free(psTexture->pui8Indices);
}

static int WriteTIM(
	const char* pszOutputFileName,
	const TIM_PIX_FMT ePixFmt,
	const TIM_PALETTE* psPalette,
	const TIM_TEXTURE* psTexture)
{
	FILE *fFilePtr = fopen(pszOutputFileName, "wb");

	fwrite(&ui32TIMHeaderID, sizeof(uint32_t), 1, fFilePtr);

	TIM_FLAG sTIMFlags = {0};
	sTIMFlags.mode = (ePixFmt == TIM_PIX_FMT_4BPP) ? 0x0 : 0x1;
	sTIMFlags.clut = 0x1;
	fwrite(&sTIMFlags, sizeof(uint32_t), 1, fFilePtr);

	// Write clut block
	uint32_t ui32CLUTBlockLength = 12u; // block length, coords, and dimensions

	// How many bytes of palette do we have?
	const uint32_t ui32PaletteSizeBytes = (
		psPalette->ui32NumEntries *
		aui16PixFmtNumColours[ePixFmt] *
		sizeof(TIM_PIX)
	);
	ui32CLUTBlockLength += ui32PaletteSizeBytes;
	fwrite(&ui32CLUTBlockLength, sizeof(uint32_t), 1, fFilePtr);

	// Pack two uint16_t's here for the targeted part of the framebuffer
	// uint32_t ui32CLUTFBCoords = 0x0; // ...but is this optional?
	uint32_t ui32CLUTFBCoords = (320); // ...but is this optional?
	fwrite(&ui32CLUTFBCoords, sizeof(uint32_t), 1, fFilePtr);

	// Dimensions of the clut block, stick to 16|256 x numPalettes, which may
	// not be optimal for 4BPP palettes
	const uint32_t ui32CLUTDimensions = (
		(psPalette->ui32NumEntries << 16) || aui16PixFmtNumColours[ePixFmt]
	);
	fwrite(&ui32CLUTDimensions, sizeof(uint32_t), 1, fFilePtr);

	fwrite(psPalette->psColours, ui32PaletteSizeBytes, 1, fFilePtr);

	if ((ui32PaletteSizeBytes % 4) != 0)
	{
		printf("need to align palette data!!!\n");
	}

	// TODO: Align the write handle to 4 bytes?

	// Write pixel block
	uint32_t ui32PixelBlockLength = 12u; // block length, coords, and dimensions

	// How many bytes of pixel data do we have?
	uint32_t ui32PixelDataInBytes = psTexture->ui16Width * psTexture->ui16Height;
	ui32PixelDataInBytes /= ((ePixFmt == TIM_PIX_FMT_4BPP) ? 2 : 1);
	ui32PixelBlockLength += ui32PixelDataInBytes;
	fwrite(&ui32PixelBlockLength, sizeof(uint32_t), 1, fFilePtr);

	// Pack two uint16_t's here for the targeted part of the framebuffer
	// uint32_t ui32PixelFBCoords = 0x0; // ...but is this optional?
	uint32_t ui32PixelFBCoords = ((240 << 16) | 320); // ...but is this optional?
	fwrite(&ui32PixelFBCoords, sizeof(uint32_t), 1, fFilePtr);

	// Dimensions of the Pixel block - from the spec:
	// "The W value (horizontal width) ... is in 16-pixel units, so in 4-bit or
	// 8-bit mode it will be, respectively, 1/4 or 1/2 of the actual image size"
	const uint32_t ui32PixelDimensions = (
		(psTexture->ui16Height << 16) |
		(
			(ePixFmt == TIM_PIX_FMT_4BPP) ?
			(psTexture->ui16Width / 4) :
			(psTexture->ui16Width / 2)
		)
	);
	fwrite(&ui32PixelDimensions, sizeof(uint32_t), 1, fFilePtr);

	fwrite(psTexture->pui8Indices, ui32PixelDataInBytes, 1, fFilePtr);

	fclose(fFilePtr);

	return 0;
}

int PackTIM(
	const TIM_PIX_FMT ePixFmt,
	const char* pszTextureFileName,
	const char* pszPaletteFileName,
	const char* pszOutputFileName)
{
	TIM_PALETTE sPalette = {0};
	TIM_TEXTURE sTexture = {0};

	printf(
		"bpp mode: %s\n"
		"texture: %s\n"
		"palette: %s\n"
		"output file: %s\n",
		ePixFmt == TIM_PIX_FMT_4BPP ? "4bpp" : "8bpp",
		pszTextureFileName,
		pszPaletteFileName,
		pszOutputFileName
	);

	if (LoadPalette(pszPaletteFileName, ePixFmt, &sPalette) != 0)
	{
		printf("failed to load palette\n");
		return 1;
	}

	if (LoadTexture(pszTextureFileName, ePixFmt, sPalette.psColours, &sTexture) != 0)
	{
		printf("failed to load Texture\n");
		DestroyPalette(&sPalette);
		return 1;
	}

	if (WriteTIM(pszOutputFileName, ePixFmt, &sPalette, &sTexture) != 0)
	{
		printf("failed to write TIM\n");
		DestroyPalette(&sPalette);
		DestroyTexture(&sTexture);
		return 1;
	}

	DestroyPalette(&sPalette);
	DestroyTexture(&sTexture);

	return 0;
}