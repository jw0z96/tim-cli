#include <stdio.h>

#include <argp.h>

#include "timpack.h"

const char *argp_program_version = "timpack 1.0";
const char *argp_program_bug_address = "<jw0z96@github>";
static char szDoc[] = "timpack - pack texture + palette data into the Sony Playstation's TIM file format";
static char szArgDoc[] = "OUTPUT_FILE";

/* The options we understand. */
static struct argp_option sOptions[] = {
  { "bpp",		'b',	"<bits>",	0,	"Bits per pixel (4 for 16 colour, 8 for 256 colour)" },
  { "texture",	't',	"FILE",		0,	"Texture file" },
  { "palette",	'p',	"FILE",		0,	"Palette file" },
  { 0 }
};

typedef struct _TIM_ARGS
{
	TIM_PIX_FMT ePixFmt;
	char* pszTextureFileName;
	char* pszPaletteFileName;
	char* pszOutputFileName;
} TIM_ARGS;

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
				TIM_PIX_FMT_4BPP :
				TIM_PIX_FMT_8BPP
			);
			break;
		}

		case 't': psArgs->pszTextureFileName = arg; break;

		case 'p': psArgs->pszPaletteFileName = arg; break;

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
	sArgs.ePixFmt = TIM_PIX_FMT_4BPP;
	sArgs.pszTextureFileName = NULL;
	sArgs.pszPaletteFileName = NULL;
	sArgs.pszOutputFileName = NULL;

	argp_parse(&sArgp, argc, argv, 0, 0, &sArgs);

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

	if (sArgs.pszOutputFileName == NULL)
	{
		printf("Output File Name Invalid\n");
		return 1;
	}

	// ready to sail?
	return PackTIM(
		sArgs.ePixFmt,
		sArgs.pszTextureFileName,
		sArgs.pszPaletteFileName,
		sArgs.pszOutputFileName
	);
}
