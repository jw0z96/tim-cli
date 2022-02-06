#include <stdio.h>
#include <stdlib.h>

#include "tim_defs.h"

int WriteTIM(
	const char* pszOutputFileName,
	const TIM_FILE* psFile)
{
	FILE *fFilePtr = fopen(pszOutputFileName, "wb");

	// TODO: check psFile

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
		printf("invalid file\n");
		return 1;
	}

	// TODO: check psFile

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
