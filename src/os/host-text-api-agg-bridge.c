#include <stdlib.h>

#include "reb-host.h"
#include "host-renderer.h"
#include "host-draw-api-agg.h"
#include "host-text-api.h"
#include "host-text-api-agg.h"

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
