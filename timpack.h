#ifndef TIMPACK_H
#define TIMPACK_H

#include "timdefs.h"

int PackTIM(
	const TIM_PIX_FMT ePixFmt,
	const char* pszTextureFileName,
	const char* pszPaletteFileName,
	const char* pszOutputFileName
);

#endif // TIMPACK_H