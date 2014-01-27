#include <stdlib.h>
#include <string.h>
#include <fontconfig/fontconfig.h>

extern "C" void RL_Print(char *fmt, ...);//output just for testing

FcChar8 *find_font_path(
	const FcChar8* family,
	unsigned char bold,
	unsigned char italic,
	unsigned char size)
{
	FcPattern		*pat = NULL;
	FcResult		result;
	FcPattern   	*match = NULL;
	FcValue 		val;
	char 			*file = NULL;
	FcChar8 		*ret = NULL;

	pat = FcPatternCreate();
	if (pat == NULL) {
		return NULL;
	}

	FcConfigSubstitute(0, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);

	FcPatternAddString(pat, "family", family);

	if (italic) {
		FcPatternAddInteger(pat, "slant", FC_SLANT_ITALIC);
	}

	if (bold) {
		FcPatternAddInteger(pat, "weight", 700); //as FW_BOLD
	}

	if (bold) {
		FcPatternAddInteger(pat, "size", size);
	}

	match = FcFontMatch(0, pat, &result);
	if (match) {
		FcPatternGetString (match, FC_FILE, 0, (FcChar8 **) &file);
		ret = (FcChar8*)strdup(file);
		FcPatternDestroy(match);
	}
	FcPatternDestroy(pat);
	return ret;
}
