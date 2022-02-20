#ifndef TIMPACK_H
#define TIMPACK_H

#include "tim_defs.h"

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

int PackTIM(const TIM_ARGS* psTIMArgs);

#endif // TIMPACK_H