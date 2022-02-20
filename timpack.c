#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <argp.h>

#include "tim_defs.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define ALIGN_UP(x, y) (((x % y) != 0) ? (x + y - (x % y)) : x)

static const uint16_t aui16PixFmtNumColours[TIM_PIX_FMT_COUNT] =
{
	16, // TIM_PIX_FMT_4BIT_CLUT
	256, // TIM_PIX_FMT_8BIT_CLUT
	0, // 	TIM_PIX_FMT_15BIT_DIRECT (unsupported)
	0 // TIM_PIX_FMT_24BIT_DIRECT (unsupported)
};

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
	assert(psCLUTHeader != NULL);
	assert(ppsCLUTData != NULL);

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
	if ((ui16FBCoordX + psCLUTHeader->ui16Width) > PSX_VRAM_WIDTH)
	{
		printf("Palette Width + Destination FB X coordinate overflows PSX VRAM\n");
		goto FAILED_LoadPalette;
	}

	if ((ui16FBCoordY + psCLUTHeader->ui16Height) > PSX_VRAM_HEIGHT)
	{
		printf("Palette Height + Destination FB Y coordinate overflows PSX VRAM\n");
		goto FAILED_LoadPalette;
	}

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
	assert(psPaletteColours != NULL);
	assert(psPixelHeader != NULL);
	assert(ppui8PixelData != NULL);

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
	if ((ui16FBCoordX + iWidth) > PSX_VRAM_WIDTH)
	{
		printf("Texture Width + Destination FB X coordinate overflows PSX VRAM\n");
		goto FAILED_LoadTexture;
	}

	if ((ui16FBCoordY + iHeight) > PSX_VRAM_HEIGHT)
	{
		printf("Texture Height + Destination FB Y coordinate overflows PSX VRAM\n");
		goto FAILED_LoadTexture;
	}

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
					}
					else
					{
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

typedef struct _TIM_ARGS
{
	TIM_PIX_FMT ePixFmt;

	char* pszTextureFileName;
	uint16_t ui16TextureCoordX;
	uint16_t ui16TextureCoordY;

	char* pszPaletteFileName;
	uint16_t ui16PaletteCoordX;
	uint16_t ui16PaletteCoordY;

	char* pszOutputFileName;
} TIM_ARGS;

int PackTIM(const TIM_ARGS* psTIMArgs)
{
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

	PrintTIM(psTIMArgs->pszOutputFileName, &sFile);

	if (WriteTIM(psTIMArgs->pszOutputFileName, &sFile) != 0)
	{
		printf("failed to write TIM\n");
		DestroyTIM(&sFile);
		return 1;
	}

	DestroyTIM(&sFile);

	return 0;
}

const char *argp_program_version = "timpack 1.0";
const char *argp_program_bug_address = "<jw0z96@github>";
static char szDoc[] = "timpack - pack texture + palette data into the Sony Playstation's TIM file format";
static char szArgDoc[] = "OUTPUT_FILE";

static struct argp_option sOptions[] = {
	{ "bpp",		'b',	"<bits>",			0,	"Bits per pixel (4 for 16 colour, 8 for 256 colour)" },
	{ "texture",	't',	"FILE",				0,	"Texture file" },
	{ "texture-x",	'x',	"<X coordinate>",	0,	"Texture destination X coordinate in VRAM" },
	{ "texture-y",	'y',	"<Y coordinate>",	0,	"Texture destination Y coordinate in VRAM" },
	{ "palette",	'p',	"FILE",				0,	"Palette file" },
	{ "palette-x",	'i',	"<X coordinate>",	0,	"Palette destination X coordinate in VRAM" },
	{ "palette-y",	'j',	"<Y coordinate>",	0,	"Palette destination Y coordinate in VRAM" },
	{ 0 }
};

static error_t ParseOpts(int key, char *arg, struct argp_state *state)
{
	TIM_ARGS *psArgs = state->input;

	switch (key)
	{
		case 'b':
		{
			if ((*arg != '4') && (*arg != '8'))
			{
				printf("expected -b/--bpp arg to be '4' or '8'\n");
				argp_usage(state);
			}

			psArgs->ePixFmt = (
				*arg == '4' ?
				TIM_PIX_FMT_4BIT_CLUT :
				TIM_PIX_FMT_8BIT_CLUT
			);
			break;
		}

		case 't': psArgs->pszTextureFileName = arg; break;
		case 'x': psArgs->ui16TextureCoordX = strtol(arg, NULL, 10); break;
		case 'y': psArgs->ui16TextureCoordY = strtol(arg, NULL, 10); break;
		case 'p': psArgs->pszPaletteFileName = arg; break;
		case 'i': psArgs->ui16PaletteCoordX = strtol(arg, NULL, 10); break;
		case 'j': psArgs->ui16PaletteCoordY = strtol(arg, NULL, 10); break;

		case ARGP_KEY_ARG:
		{
			if (state->arg_num >= 1) // Too many args
			{
				argp_usage(state);
			}

			psArgs->pszOutputFileName = arg;
			break;
		}

		case ARGP_KEY_END:
		{
			if (state->arg_num < 1) // Not enough args
			{
				argp_usage(state);
			}
			break;
		}

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp sArgp = { sOptions, ParseOpts, szArgDoc, szDoc };

int main (int argc, char * argv[])
{
	// Default args
	TIM_ARGS sArgs;
	sArgs.ePixFmt = TIM_PIX_FMT_4BIT_CLUT;
	sArgs.pszTextureFileName = NULL;
	sArgs.ui16TextureCoordX = 0;
	sArgs.ui16TextureCoordY = 0;
	sArgs.pszPaletteFileName = NULL;
	sArgs.ui16PaletteCoordX = 0;
	sArgs.ui16PaletteCoordY = 0;
	sArgs.pszOutputFileName = NULL;

	argp_parse(&sArgp, argc, argv, 0, 0, &sArgs);

	// TODO: implement direct colour formats
	if (sArgs.ePixFmt > TIM_PIX_FMT_8BIT_CLUT)
	{
		printf("15/24 bit direct colour formats unsupported\n");
		return 1;
	}

	// Check if any files are null
	if (sArgs.pszTextureFileName == NULL)
	{
		printf("Texture File Name Invalid\n");
		return 1;
	}

	if (sArgs.pszPaletteFileName == NULL)
	{
		printf("Palette File Name Invalid\n");
		return 1;
	}

#define RETURN_IF_INVALID_COORD(coord, dim, fmt) do { if (coord >= dim) { \
		printf("%s coordinate must be in the range [0, %u], (%hu provided)\n", fmt, (dim - 1), coord); \
		return 1; \
	} } while (0)

	RETURN_IF_INVALID_COORD(sArgs.ui16TextureCoordX, PSX_VRAM_WIDTH, "Texture X");
	RETURN_IF_INVALID_COORD(sArgs.ui16TextureCoordY, PSX_VRAM_HEIGHT, "Texture Y");
	RETURN_IF_INVALID_COORD(sArgs.ui16PaletteCoordX, PSX_VRAM_WIDTH, "Palette X");
	RETURN_IF_INVALID_COORD(sArgs.ui16PaletteCoordY, PSX_VRAM_HEIGHT, "Palette Y");

#undef RETURN_IF_INVALID_COORD

	if (sArgs.pszOutputFileName == NULL)
	{
		printf("Output File Name Invalid\n");
		return 1;
	}

	return PackTIM(&sArgs);
}
