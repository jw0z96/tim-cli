#include <stdio.h>
#include <stdlib.h>

#include <argp.h>

#include "timpack.h"

const char *argp_program_version = "timpack 1.0";
const char *argp_program_bug_address = "<jw0z96@github>";
static char szDoc[] = "timpack - pack texture + palette data into the Sony Playstation's TIM file format";
static char szArgDoc[] = "OUTPUT_FILE";

/* The options we understand. */
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

/* Parse a single option. */
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

	// ready to sail?
	return PackTIM(&sArgs);
}
