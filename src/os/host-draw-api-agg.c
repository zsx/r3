
#include <stdlib.h>

#include "reb-host.h"
#include "host-renderer.h"
#include "host-draw-api.h"
#include "host-draw-api-agg.h"

struct REBRDR_DRW draw_agg = {
	.rebdrw_add_poly_vertex = aggdrw_add_poly_vertex,
	.rebdrw_add_spline_vertex = aggdrw_add_spline_vertex,
	.rebdrw_anti_alias = aggdrw_anti_alias,
	.rebdrw_arc = aggdrw_arc,
	.rebdrw_arrow = aggdrw_arrow,
	.rebdrw_begin_poly = aggdrw_begin_poly,
	.rebdrw_begin_spline = aggdrw_begin_spline,
	.rebdrw_box = aggdrw_box,
	.rebdrw_circle = aggdrw_circle,
	.rebdrw_clip = aggdrw_clip,
	.rebdrw_curve3 = aggdrw_curve3,
	.rebdrw_curve4 = aggdrw_curve4,
	.rebdrw_ellipse = aggdrw_ellipse,
	.rebdrw_end_poly = aggdrw_end_poly,
	.rebdrw_end_spline = aggdrw_end_spline,
	.rebdrw_fill_pen = aggdrw_fill_pen,
	.rebdrw_fill_pen_image = aggdrw_fill_pen_image,
	.rebdrw_fill_rule = aggdrw_fill_rule,
	.rebdrw_gamma = aggdrw_gamma,
	.rebdrw_gob_color = aggdrw_gob_color,
	.rebdrw_gob_draw = aggdrw_gob_draw,
	.rebdrw_gob_image = aggdrw_gob_image,
	.rebdrw_gradient_pen = aggdrw_gradient_pen,
	.rebdrw_invert_matrix = aggdrw_invert_matrix,
	.rebdrw_image = aggdrw_image,
	.rebdrw_image_filter = aggdrw_image_filter,
	.rebdrw_image_options = aggdrw_image_options,
	.rebdrw_image_scale = aggdrw_image_scale,
	.rebdrw_image_pattern = aggdrw_image_pattern,
	.rebdrw_line = aggdrw_line,
	.rebdrw_line_cap = aggdrw_line_cap,
	.rebdrw_line_join = aggdrw_line_join,
	.rebdrw_line_pattern = aggdrw_line_pattern,
	.rebdrw_line_width = aggdrw_line_width,
	.rebdrw_matrix = aggdrw_matrix,
	.rebdrw_pen = aggdrw_pen,
	.rebdrw_pen_image = aggdrw_pen_image,
	.rebdrw_pop_matrix = aggdrw_pop_matrix,
	.rebdrw_push_matrix = aggdrw_push_matrix,
	.rebdrw_reset_gradient_pen = aggdrw_reset_gradient_pen,
	.rebdrw_reset_matrix = aggdrw_reset_matrix,
	.rebdrw_rotate = aggdrw_rotate,
	.rebdrw_scale = aggdrw_scale,
	.rebdrw_to_image = aggdrw_to_image,
	.rebdrw_skew = aggdrw_skew,
	.rebdrw_text = aggdrw_text,
	.rebdrw_transform = aggdrw_transform,
	.rebdrw_translate = aggdrw_translate,
	.rebdrw_triangle = aggdrw_triangle,

	.rebshp_arc = aggshp_arc,
	.rebshp_close = aggshp_close,
	.rebshp_hline = aggshp_hline,
	.rebshp_line = aggshp_line,
	.rebshp_move = aggshp_move,
	.rebshp_begin = aggshp_begin,
	.rebshp_end = aggshp_end,
	.rebshp_vline = aggshp_vline,
	.rebshp_curv = aggshp_curv,
	.rebshp_curve = aggshp_curve,
	.rebshp_qcurv = aggshp_qcurv,
	.rebshp_qcurve = aggshp_qcurve
};
