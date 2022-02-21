#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tim_defs.h"

void PrintTIM(const char* pszName, TIM_FILE* psFile)
{
	static const char* apszTIMFmtStr[TIM_PIX_FMT_COUNT] = {
		"4 BIT CLUT",
		"8 BIT CLUT",
		"15BIT DIRECT",
		"24BIT DIRECT",
	};

	assert(psFile != NULL);

	printf(
		"TIM File: %s\n"
		"\tPalette Format: %s\n"
		"\tPalette Dimensions: %hu, %hu\n"
		"\tPalette FB Coord: %hu, %hu\n"
		"\tTexture Dimensions: %hu, %hu\n"
		"\tTexture FB Coord: %hu, %hu\n",
		pszName,
		apszTIMFmtStr[psFile->sFileHeader.sFlags.uMode],
		psFile->sCLUTHeader.ui16Width,
		psFile->sCLUTHeader.ui16Height,
		psFile->sCLUTHeader.ui16FBCoordX,
		psFile->sCLUTHeader.ui16FBCoordY,
		psFile->sPixelHeader.ui16Width,
		psFile->sPixelHeader.ui16Height,
		psFile->sPixelHeader.ui16FBCoordX,
		psFile->sPixelHeader.ui16FBCoordY
	);
}

int WriteTIM(
	const char* pszOutputFileName,
	const TIM_FILE* psFile)
{
	FILE *fFilePtr = fopen(pszOutputFileName, "wb");
	if (fFilePtr == NULL)
	{
		printf("could not open %s for writing\n", pszOutputFileName);
		return 1;
	}

	assert(psFile != NULL);

	// Write header
	fwrite(&psFile->sFileHeader, sizeof(TIM_FILE_HEADER), 1, fFilePtr);

	// Write CLUT block
	fwrite(&psFile->sCLUTHeader, sizeof(TIM_BLOCK_HEADER), 1, fFilePtr);
	fwrite(
		psFile->psCLUTData,
		(psFile->sCLUTHeader.ui32SizeInBytes - sizeof(TIM_BLOCK_HEADER)),
		1,
		fFilePtr
	);

	// Write pixel block
	fwrite(&psFile->sPixelHeader, sizeof(TIM_BLOCK_HEADER), 1, fFilePtr);
	fwrite(
		psFile->pui8PixelData,
		(psFile->sPixelHeader.ui32SizeInBytes - sizeof(TIM_BLOCK_HEADER)),
		1,
		fFilePtr
	);

	fclose(fFilePtr);

	return 0;
}

int ReadTIM(
	const char* pszInputFileName,
	TIM_FILE* psFile)
{
	FILE *fFilePtr = fopen(pszInputFileName, "r");
	if (fFilePtr == NULL)
	{
		printf("could not open %s for reasing\n", pszInputFileName);
		return 1;
	}

	assert(psFile != NULL);

	printf("reading %s\n", pszInputFileName);

	// Read the file header, and check that it's what we expect
	fread(&psFile->sFileHeader, sizeof(TIM_FILE_HEADER), 1, fFilePtr);
	if (psFile->sFileHeader.ui32ID != TIM_FILE_HEADER_ID)
	{
		printf("File header does not match that of a TIM file\n");
		goto FAILED_ReadTIM;
	}

	// Read CLUT block
	{
		uint32_t ui32CLUTDataInBytes;
		fread(&psFile->sCLUTHeader, sizeof(TIM_BLOCK_HEADER), 1, fFilePtr);

		ui32CLUTDataInBytes = (
			psFile->sCLUTHeader.ui32SizeInBytes - sizeof(TIM_BLOCK_HEADER)
		);

		printf("allocating %u bytes for the CLUT data\n", ui32CLUTDataInBytes);

		psFile->psCLUTData = malloc(ui32CLUTDataInBytes);

		fread(psFile->psCLUTData, ui32CLUTDataInBytes, 1, fFilePtr);
	}

	// Read pixel block
	{
		uint32_t ui32PixelDataInBytes;
		fread(&psFile->sPixelHeader, sizeof(TIM_BLOCK_HEADER), 1, fFilePtr);

		ui32PixelDataInBytes = (
			psFile->sPixelHeader.ui32SizeInBytes - sizeof(TIM_BLOCK_HEADER)
		);

		printf("allocating %u bytes for the Pixel data\n", ui32PixelDataInBytes);

		psFile->pui8PixelData = malloc(ui32PixelDataInBytes);

		fread(psFile->pui8PixelData, ui32PixelDataInBytes, 1, fFilePtr);
	}

	fclose(fFilePtr);

	return 0;

FAILED_ReadTIM:
	fclose(fFilePtr);

	return 1;
}

void DestroyTIM(TIM_FILE* psFile)
{
	if (psFile->psCLUTData) { free(psFile->psCLUTData); }
	if (psFile->pui8PixelData) { free(psFile->pui8PixelData); }
}
