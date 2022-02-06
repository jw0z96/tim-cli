#include <stdio.h>

#include "tim_defs.h"

#include <SDL2/SDL.h>

#include <stdbool.h>

typedef struct _R8G8B8A8
{
	uint32_t uRed: 8, uGreen: 8, uBlue: 8, uAlpha: 8;
} R8G8B8A8;

#define U5TOU8(x) ((uint8_t)(x * (255.0f / 32.0f)))

static R8G8B8A8 TIMPixToRGBA8(const TIM_PIX* psPix)
{
	R8G8B8A8 sPix = {
		.uRed = U5TOU8(psPix->r),
		.uGreen = U5TOU8(psPix->g),
		.uBlue = U5TOU8(psPix->b),
		.uAlpha = 0xFF
	};

	return sPix;
}

static int DecodeTIMPixelData(
	const TIM_FILE* psFile,
	uint32_t** ppui32PixelData)
{
	const uint32_t ui32NumPixels = (
		psFile->sPixelHeader.ui16Width * psFile->sPixelHeader.ui16Height
	);

	R8G8B8A8* pui32PixelData = calloc(ui32NumPixels, sizeof(R8G8B8A8));
	uint8_t ui8PaletteIndex;
	for (uint32_t i = 0; i < ui32NumPixels; ++i)
	{
		if (psFile->sFileHeader.sFlags.uMode == TIM_PIX_FMT_4BIT_CLUT)
		{
			ui8PaletteIndex = psFile->pui8PixelData[i / 2];
			ui8PaletteIndex >>= ((i % 2) ? 4 : 0);
		}
		else
		{
			ui8PaletteIndex = psFile->pui8PixelData[i];
		}

		pui32PixelData[i] = TIMPixToRGBA8(&psFile->psCLUTData[ui8PaletteIndex]);
	}

	*ppui32PixelData = (uint32_t*)pui32PixelData;

	return 0;
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
		goto FAILED_Render;
	}

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_Render;
	}

	printf(
		"creating %u x %u window\n",
		sFile.sPixelHeader.ui16Width,
		sFile.sPixelHeader.ui16Height
	);

	SDL_Window* pWindow = SDL_CreateWindow(
		"timview",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		sFile.sPixelHeader.ui16Width,
		sFile.sPixelHeader.ui16Height,
		SDL_WINDOW_SHOWN
	);

	if (pWindow == NULL)
	{
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_Render;
	}

	SDL_Surface* pScreenSurface = SDL_GetWindowSurface(pWindow);
	if (pScreenSurface == NULL)
	{
		printf("screen surface could not be created! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_Render;
	}

	uint32_t* pui32PixelData = NULL;
	if (DecodeTIMPixelData(&sFile, &pui32PixelData) != 0)
	{
		printf("Failed to get pixel data from TIM\n");
		goto FAILED_Render;
	}

	SDL_Surface* pTIMSurface = SDL_CreateRGBSurfaceFrom(
		pui32PixelData,
		sFile.sPixelHeader.ui16Width,
		sFile.sPixelHeader.ui16Height,
		32,
		4 * sFile.sPixelHeader.ui16Width, // pitch
		// component masks
		0x000000ff,
		0x0000ff00,
		0x00ff0000,
		0xff000000
	);

	if (pTIMSurface == NULL)
	{
		printf("surface could not be created! SDL_Error: %s\n", SDL_GetError());
		goto FAILED_Render;
	}

	SDL_BlitSurface(pTIMSurface, NULL, pScreenSurface, NULL);

	SDL_UpdateWindowSurface(pWindow);

	SDL_Event e;
	bool bQuit = false;
	while (!bQuit)
	{
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT){
			bQuit = true;
			}
			if (e.type == SDL_KEYDOWN){
			bQuit = true;
			}
			if (e.type == SDL_MOUSEBUTTONDOWN){
			bQuit = true;
			}
		}
	}

	SDL_FreeSurface(pScreenSurface);

	free(pui32PixelData);

	SDL_FreeSurface(pTIMSurface);

	SDL_DestroyWindow(pWindow);

	SDL_Quit();

	DestroyTIM(&sFile);

	return 0;

FAILED_Render:
	DestroyTIM(&sFile);

	return 1;
}
