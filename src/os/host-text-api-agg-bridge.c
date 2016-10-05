#include <stdlib.h>
#include <string.h>

#ifndef R3_CPP
#define AGGAPI AGGAPI
#else
#define AGGAPI
#endif

#include "reb-host.h"
#include "host-renderer.h"
#include "host-draw-api-agg.h"
#include "host-text-api.h"
#include "host-text-api-agg.h"

#ifdef __cplusplus
static const REBRDR_TXT init_agg_text()
{
    REBRDR_TXT rdr;

    memset(&rdr, 0, sizeof(rdr));

	rdr.init = agg_rt_init;
	rdr.fini = agg_rt_fini;
	rdr.create_rich_text = agg_create_rich_text;
	rdr.destroy_rich_text = agg_destroy_rich_text;
	rdr.rt_anti_alias = agg_rt_anti_alias;
	rdr.rt_bold = agg_rt_bold;
	rdr.rt_caret = agg_rt_caret;
	rdr.rt_center = agg_rt_center;
	rdr.rt_color = agg_rt_color;
	rdr.rt_drop = agg_rt_drop;
	rdr.rt_font = agg_rt_font;
	rdr.rt_font_size = agg_rt_font_size;
	rdr.rt_get_font = agg_rt_get_font;
	rdr.rt_get_para = agg_rt_get_para;
	rdr.rt_italic = agg_rt_italic;
	rdr.rt_left = agg_rt_left;
	rdr.rt_newline = agg_rt_newline;
	rdr.rt_para = agg_rt_para;
	rdr.rt_right = agg_rt_right;
	rdr.rt_scroll = agg_rt_scroll;
	rdr.rt_shadow = agg_rt_shadow;
	rdr.rt_set_font_styles = agg_rt_set_font_styles;
	rdr.rt_size_text = agg_rt_size_text;
	rdr.rt_text = agg_rt_text;
	rdr.rt_underline = agg_rt_underline;
	rdr.rt_offset_to_caret = agg_rt_offset_to_caret;
	rdr.rt_caret_to_offset = agg_rt_caret_to_offset;
	rdr.rt_gob_text = agg_rt_gob_text;
    rdr.rt_block_text = agg_rt_block_text;

    return rdr;
}
struct REBRDR_TXT text_agg = init_agg_text();
#else
struct REBRDR_TXT text_agg = {
	.init = agg_rt_init,
	.fini = agg_rt_fini,
	.create_rich_text = agg_create_rich_text,
	.destroy_rich_text = agg_destroy_rich_text,
	.rt_anti_alias = agg_rt_anti_alias,
	.rt_bold = agg_rt_bold,
	.rt_caret = agg_rt_caret,
	.rt_center = agg_rt_center,
	.rt_color = agg_rt_color,
	.rt_drop = agg_rt_drop,
	.rt_font = agg_rt_font,
	.rt_font_size = agg_rt_font_size,
	.rt_get_font = agg_rt_get_font,
	.rt_get_para = agg_rt_get_para,
	.rt_italic = agg_rt_italic,
	.rt_left = agg_rt_left,
	.rt_newline = agg_rt_newline,
	.rt_para = agg_rt_para,
	.rt_right = agg_rt_right,
	.rt_scroll = agg_rt_scroll,
	.rt_shadow = agg_rt_shadow,
	.rt_set_font_styles = agg_rt_set_font_styles,
	.rt_size_text = agg_rt_size_text,
	.rt_text = agg_rt_text,
	.rt_underline = agg_rt_underline,
	.rt_offset_to_caret = agg_rt_offset_to_caret,
	.rt_caret_to_offset = agg_rt_caret_to_offset,
	.rt_gob_text = agg_rt_gob_text,
	.rt_block_text = agg_rt_block_text
};
#endif
