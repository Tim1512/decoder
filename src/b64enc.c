/*
 * vim: set tabstop=4 syntax=c :
 *
 * Copyright (C) 2014-2017, Peter Haemmerlein (peterpawn@yourfritz.de)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program, please look for the file LICENSE.
 */

#define B64ENC_C

#include "common.h"

static commandEntry_t 		__b64enc_command = { .name = "b64enc", .ep = &b64enc_entry, .usage = &b64enc_usage };
EXPORTED commandEntry_t *	b64enc_command = &__b64enc_command;

// display usage help

void 	b64enc_usage(bool help)
{
	errorMessage("help for b64enc\n");
	if (help)
		errorMessage("option --help used\n");
}

// 'b64enc' function - encode binary data from STDIN to Base64 encoded on STDOUT

int 	b64enc_entry(int argc, char** argv, int argo, commandEntry_t * entry)
{
	bool				hexInput = false;
	bool				padOutput = false;
	bool				wrapLines = false;
	uint32_t			lineSize = 76;
	uint32_t			charsOnLine = 0;
	char				buffer[120];
	size_t				read = 0;

	if (argc > argo + 1)
	{
		int				opt;
		int				optIndex = 0;

		static struct option options_long[] = {
			verbosity_options_long,
			{ "hex-input", no_argument, NULL, 'x' },
			{ "pad-output", no_argument, NULL, 'p' },
			{ "wrap-lines", optional_argument, NULL, 'w' },
		};
		char *			options_short = "xpw" verbosity_options_short;

		while ((opt = getopt_long(argc - argo, &argv[argo], options_short, options_long, &optIndex)) != -1)
		{
			switch (opt)
			{
				case 'x':
					hexInput = true;
					break;

				case 'p':
					padOutput = true;
					break;

				case 'w':
					{
						char *	endString = NULL;
						char *	startString;

						wrapLines = true;
						if (optarg && *optarg)
						{
							startString = optarg;	
						}
						else
						{
							if ((optind + argo) >= argc)
								break; /* last option, no number present */
							startString = argv[optind + argo];
							if (*startString == '-')
								break; /* next is an option */
						}
						lineSize = strtoul(startString, &endString, 10);
						if (*startString && strlen(endString))
						{
							errorMessage("Invalid line size '%s' specified for -w option.\a\n", startString);
							return(EXIT_FAILURE);
						}
						else
							optind++;
					}
					break;

				check_verbosity_options_short();
				help_option();
				getopt_message_displayed();
				invalid_option(opt);
			}
		} 
	}

	resetError();

	while ((read = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
	{
		if (hexInput)
		{
			char	withoutSpaces[sizeof(buffer)];
			size_t	used = 0;
			char *	in;
			char *	out;
			int		i;
			size_t	more = read;

			in = buffer;
			out = withoutSpaces;
			while (more > 0)
			{
				for (i = more; i > 0; i--, in++)
				{
					if (isspace(*(in)))
						continue;
					*(out++) = *in;
					used++;
				}
				if (used == sizeof(withoutSpaces))
					break;
				more = fread(buffer, 1, sizeof(withoutSpaces) - used, stdin);
				if (more == 0)
					break;
				in = buffer;
			}
			read = hexadecimalToBinary(withoutSpaces, used, buffer, sizeof(buffer));
			if (read == 0) 
				break;
		}
		
		char		base64[(sizeof(buffer) * 4 / 3) + 1]; /* one more byte for optional end of string */
		size_t		base64Size = binaryToBase64(buffer, read, base64, sizeof(base64) - 1, padOutput);
		
		if (base64Size == 0) break;
		
		uint32_t	toWrite = base64Size;
		char *		out = base64;

		out = wrapOutput(wrapLines, lineSize, &charsOnLine, &toWrite, out);
		if (isAnyError())
			break;

		if ((toWrite > 0) && fwrite(out, toWrite, 1, stdout) != 1)
		{
			setError(WRITE_FAILED);
			break;
		}
		if (wrapLines)
		{
			charsOnLine += toWrite;
		}
	}
	if (!isAnyError() && wrapLines && (fwrite("\n", 1, 1, stdout) != 1)) /* append newline */
		setError(WRITE_FAILED);
	
	if (isAnyError()) 
	{
		if (isError(WRITE_FAILED))
		{
			errorMessage("Write to STDOUT failed.\a\n");
		}
		else if (isError(INV_HEX_DATA))
		{
			errorMessage("Invalid hexadecimal data value encountered on STDIN.\a\n");
		}
		else if (isError(INV_HEX_SIZE))
		{
			errorMessage("Invalid hexadecimal data size encountered on STDIN.\a\n");
		}
		else if (isError(INV_B64_ENC_SIZE))
		{
			errorMessage("Invalid data size encountered on STDIN.\a\n");
		}
		else
		{
			errorMessage("Unexpected error %d (%s) encountered.\a\n", getError(), getErrorText(getError()));
		}
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
