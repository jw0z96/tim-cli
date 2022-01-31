#ifndef TIMPACK_H
#define TIMPACK_H

typedef enum _TIM_PIX_FMT
{
	TIM_PIX_FMT_4BPP,
	TIM_PIX_FMT_8BPP,
	TIM_PIX_FMT_COUNT
} TIM_PIX_FMT;

int PackTIM(
	const TIM_PIX_FMT ePixFmt,
	const char* pszTextureFileName,
	const char* pszPaletteFileName,
	const char* pszOutputFileName
);

#endif // TIMPACK_H