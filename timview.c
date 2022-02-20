#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "tim_defs.h"

#include <SDL.h>

typedef struct _R8G8B8A8
{
	uint32_t uRed: 8, uGreen: 8, uBlue: 8, uAlpha: 8;
} R8G8B8A8;


static R8G8B8A8 TIMPixToRGBA8(const TIM_PIX* psPix)
{
	R8G8B8A8 sPix = {
		.uRed = CONV_U5_TO_U8(psPix->r),
		.uGreen = CONV_U5_TO_U8(psPix->g),
		.uBlue = CONV_U5_TO_U8(psPix->b),
		.uAlpha = 0xFF
	};

	return sPix;
}

static int DecodeTIMPixelDataWithPalette(
	const TIM_FILE* psFile,
	const uint32_t ui32PaletteIndex,
	R8G8B8A8* pui32PixelData)
{
	assert(psFile != NULL);
	assert(ui32PaletteIndex < psFile->sCLUTHeader.ui16Height);
	assert(pui32PixelData != NULL);

	const uint32_t ui32NumPixels = (
		psFile->sPixelHeader.ui16Width * psFile->sPixelHeader.ui16Height
	);

	uint8_t ui8ColourIndex;
	const uint32_t ui32PaletteOffset = ui32PaletteIndex * psFile->sCLUTHeader.ui16Width;
	for (uint32_t i = 0; i < ui32NumPixels; ++i)
	{
		if (psFile->sFileHeader.sFlags.uMode == TIM_PIX_FMT_4BIT_CLUT)
		{
			ui8ColourIndex = psFile->pui8PixelData[i / 2];
			if (i % 2)
			{
				ui8ColourIndex = ((ui8ColourIndex & 0xF0) >> 4);

			}
			else
			{
				ui8ColourIndex = ui8ColourIndex & 0x0F;
			}
		}
		else
		{
			ui8ColourIndex = psFile->pui8PixelData[i];
		}

		pui32PixelData[i] = TIMPixToRGBA8(
			&psFile->psCLUTData[ui32PaletteOffset + ui8ColourIndex]
		);
	}

	return 0;
}

static int RenderTIM(const TIM_FILE* psFile)
{
	SDL_Window* pWindow = SDL_CreateWindow(
		"timview",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		psFile->sPixelHeader.ui16Width,
		psFile->sPixelHeader.ui16Height,
		SDL_WINDOW_SHOWN
	);

	if (pWindow == NULL)
	{
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Surface* pScreenSurface = SDL_GetWindowSurface(pWindow);
	if (pScreenSurface == NULL)
	{
		printf("screen surface could not be created! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_GetWindowSurface;
	}

	SDL_Surface* pTIMSurface = SDL_CreateRGBSurface(
		0,
		psFile->sPixelHeader.ui16Width,
		psFile->sPixelHeader.ui16Height,
		32,
		// component masks
		0x000000ff,
		0x0000ff00,
		0x00ff0000,
		0xff000000
	);

	if (pTIMSurface == NULL)
	{
		printf("surface could not be created! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_CreateTIMSurface;
	}

	R8G8B8A8* psPixels = (R8G8B8A8*)pTIMSurface->pixels;
	uint32_t ui32PaletteIndex = 0;
	if (DecodeTIMPixelDataWithPalette(psFile, ui32PaletteIndex, psPixels) != 0)
	{
		printf("Failed to get pixel data from TIM\n");
		goto FAILED_DecodePalette;
	}

	SDL_BlitSurface(pTIMSurface, NULL, pScreenSurface, NULL);
	SDL_UpdateWindowSurface(pWindow);

	{
		SDL_Event sEvent;
		bool bQuit = false;
		while (!bQuit)
		{
			while (SDL_PollEvent(&sEvent))
			{
				if (sEvent.type == SDL_QUIT)
				{
					bQuit = true;
				}

				if (sEvent.type == SDL_KEYDOWN)
				{
					ui32PaletteIndex = (
						(ui32PaletteIndex + 1) % psFile->sCLUTHeader.ui16Height
					);

					if (DecodeTIMPixelDataWithPalette(psFile, ui32PaletteIndex, psPixels) != 0)
					{
						printf("Failed to get pixel data from TIM\n");
						goto FAILED_DecodePalette;
					}

					SDL_BlitSurface(pTIMSurface, NULL, pScreenSurface, NULL);
					SDL_UpdateWindowSurface(pWindow);
				}
			}
		}
	}

	SDL_FreeSurface(pTIMSurface);
	SDL_FreeSurface(pScreenSurface);
	SDL_DestroyWindow(pWindow);

	return 0;

FAILED_DecodePalette:
	SDL_FreeSurface(pTIMSurface);
FAILED_CreateTIMSurface:
	SDL_FreeSurface(pScreenSurface);
FAILED_GetWindowSurface:
	SDL_DestroyWindow(pWindow);

	return 1;
}

int main (int argc, char * argv[])
{
	TIM_FILE sFile;

	if (argc != 2)
	{
		printf("just one arg pls!\n");
		return 1;
	}

	if (ReadTIM(argv[1], &sFile) != 0)
	{
		return 1;
	}

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_SDL_Init;
	}

	PrintTIM(argv[1], &sFile);

	if (RenderTIM(&sFile) != 0)
	{
		goto FAILED_RenderTIM;
	}

	SDL_Quit();
	DestroyTIM(&sFile);

	return 0;

FAILED_RenderTIM:
	SDL_Quit();
FAILED_SDL_Init:
	DestroyTIM(&sFile);

	return 1;
}
