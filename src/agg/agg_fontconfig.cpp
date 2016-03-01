#ifdef AGG_FONTCONFIG

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <fontconfig/fontconfig.h>

	void RL_Print(char *fmt, ...);//output just for testing
}

extern "C" unsigned char *find_font_path(
	const unsigned char* family,
	unsigned char bold,
	unsigned char italic,
	unsigned char size);

typedef struct font_cache {
		char* 		family;
		char*		font_path;
		unsigned char 	bold;
		unsigned char 	italic;
		unsigned char 	size;
		unsigned char 	valid; /* valid = 1 */
		unsigned int 	hot;  /* how hot this entry is, the hottest one is the most recently used one */
		font_cache():family(NULL), font_path(NULL), valid(0) {
		}
		~font_cache() {
			if (valid) {
				if (family != NULL){
					free(family);
					family = NULL;
				}
				if (font_path != NULL){
					free(font_path);
					font_path = NULL;
				}
			}
		}
} font_cache_entry_t;

class font_cache_manager {
	public:
		font_cache_manager(int n)
		:last_hot_entry(0),
		num_of_entries(n),
		last_hotness(0),
		max_hotness(0xFFFFFFFF){
			font_cache_array = new font_cache_entry_t[n];
		}

		~font_cache_manager(){
			delete [] font_cache_array;
		}

		font_cache_entry_t* add_an_entry(const char* family,
										 unsigned char bold,
										 unsigned char italic,
										 unsigned char size,
										 const char* font_path)
		{
			unsigned int least_hot = max_hotness;
			unsigned int index = 0, i = 0;
			for(i = 0; i < num_of_entries; i ++) {
				if (font_cache_array[i].valid == 0) { /* found an empty entry */
					font_cache_array[i].valid = 1;
					index = i;
					break;
				}
				if (font_cache_array[i].hot < least_hot){
					least_hot = font_cache_array[i].hot;
					index = i;
				}
			}

			/* replace the least hot entry with the new one */

			if (font_cache_array[index].family == NULL) {
				font_cache_array[index].family = strdup(family);
			} else if (strncmp(family, font_cache_array[index].family, strlen(family) + 1)) {
				free(font_cache_array[index].family);
				font_cache_array[index].family = strdup(family);
			}

			if (font_cache_array[index].font_path == NULL){
				font_cache_array[index].font_path = strdup((char*)font_path);
			} else if (strncmp(font_path, font_cache_array[index].font_path, strlen(font_path) + 1)) {
				free(font_cache_array[index].font_path);
				font_cache_array[index].font_path = strdup((char*)font_path);
			}

			font_cache_array[index].bold = bold;
			font_cache_array[index].italic = italic;
			font_cache_array[index].size = size;
			update_hotness(index);
			return &font_cache_array[index];
		}

		void update_hotness(int i) {
			last_hot_entry = i;
			last_hotness ++;
			font_cache_array[i].hot = last_hotness;
			if (last_hotness == max_hotness) {
				rewind_hotness();
			}
		}

		font_cache_entry_t* find_an_entry (const char* family,
										   unsigned char bold,
										   unsigned char italic,
										   unsigned char size)
		{
			size_t famliy_len = strlen((char*)family);
			for(int i = 0; i < num_of_entries; i ++) {
				if (font_cache_array[i].valid == 0) {
					return NULL;
				}
				if (font_cache_array[i].bold == bold
					&& font_cache_array[i].italic == italic
					&& font_cache_array[i].size == size
					&& !strncasecmp(font_cache_array[i].family,
									family,
									famliy_len)){
					if (last_hot_entry != i) {
						update_hotness(i);
					}
					//RL_Print("found a font cache at %d with hotness %d\n", i, font_cache_array[i].hot);
					return &font_cache_array[i];
				}
			}

			return NULL;
		}

		void rewind_hotness()
		{
			for(int i = 0; i < num_of_entries; i ++) {
				if (font_cache_array[i].valid == 0) {
					return;
				}
				if (font_cache_array[i].hot < max_hotness / 2) {
					font_cache_array[i].hot = 0;
				} else {
					font_cache_array[i].hot -= max_hotness / 2;
				}
			}
			last_hotness = max_hotness / 2;
		}

	private:
		int last_hot_entry;
		unsigned int	last_hotness;
		unsigned int	num_of_entries;
		const unsigned int max_hotness;
		font_cache_entry_t *font_cache_array;

};

static font_cache_manager cache_manager(128);

/* the returned font_path must not be free'ed by the caller */
unsigned char *find_font_path(
	const unsigned char* family,
	unsigned char bold,
	unsigned char italic,
	unsigned char size)
{
	FcPattern		*pat = NULL;
	FcResult		result;
	FcPattern   	*match = NULL;
	FcValue 		val;
	FcChar8 		*file = NULL;
	FcChar8 		*ret = NULL;
    const FcChar8  * const fontformat = (FcChar8 *)"truetype";

	font_cache_entry_t *entry = NULL;
	entry = cache_manager.find_an_entry((char*)family, bold, italic, size);
	if (entry != NULL){
		return (FcChar8*)entry->font_path;
	}

	pat = FcPatternCreate();
	if (pat == NULL) {
		return NULL;
	}

	FcPatternAddString(pat, FC_FAMILY, family);

	if (italic) {
		FcPatternAddInteger(pat, FC_SLANT, FC_SLANT_ITALIC);
	}

	if (bold) {
		FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
	}

	FcPatternAddInteger(pat, FC_SIZE, size);
	FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
    FcPatternAddString(pat, FC_FONTFORMAT, fontformat);

	FcConfigSubstitute(0, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);

	match = FcFontMatch(0, pat, &result);
	if (match) {
		FcPatternGetString (match, FC_FILE, 0, &file);
		entry = cache_manager.add_an_entry((char*)family, bold, italic, size, (char*)file);
		FcPatternDestroy(match);
		ret = (FcChar8*)entry->font_path;
	}
	FcPatternDestroy(pat);
	return ret;
}

#endif //AGG_FONTCONFIG