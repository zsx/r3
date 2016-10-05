#include <stdlib.h>
#include <string.h>

#include "reb-host.h"
#include "host-renderer.h"
#include "host-draw-api.h"
#include "host-draw-api-agg.h"

#ifdef __cplusplus

const struct REBRDR_DRW init_agg_draw()
{
    struct REBRDR_DRW rdr;

    memset(&rdr, 0, sizeof(rdr));

	rdr.rebdrw_add_poly_vertex = aggdrw_add_poly_vertex;
	rdr.rebdrw_add_spline_vertex = aggdrw_add_spline_vertex;
	rdr.rebdrw_anti_alias = aggdrw_anti_alias;
	rdr.rebdrw_arc = aggdrw_arc;
	rdr.rebdrw_arrow = aggdrw_arrow;
	rdr.rebdrw_begin_poly = aggdrw_begin_poly;
	rdr.rebdrw_begin_spline = aggdrw_begin_spline;
	rdr.rebdrw_box = aggdrw_box;
	rdr.rebdrw_circle = aggdrw_circle;
	rdr.rebdrw_clip = aggdrw_clip;
	rdr.rebdrw_curve3 = aggdrw_curve3;
	rdr.rebdrw_curve4 = aggdrw_curve4;
	rdr.rebdrw_ellipse = aggdrw_ellipse;
	rdr.rebdrw_end_poly = aggdrw_end_poly;
	rdr.rebdrw_end_spline = aggdrw_end_spline;
	rdr.rebdrw_fill_pen = aggdrw_fill_pen;
	rdr.rebdrw_fill_pen_image = aggdrw_fill_pen_image;
	rdr.rebdrw_fill_rule = aggdrw_fill_rule;
	rdr.rebdrw_gamma = aggdrw_gamma;
	rdr.rebdrw_gob_color = aggdrw_gob_color;
	rdr.rebdrw_gob_draw = aggdrw_gob_draw;
	rdr.rebdrw_gob_image = aggdrw_gob_image;
	rdr.rebdrw_gradient_pen = aggdrw_gradient_pen;
	rdr.rebdrw_invert_matrix = aggdrw_invert_matrix;
	rdr.rebdrw_image = aggdrw_image;
	rdr.rebdrw_image_filter = aggdrw_image_filter;
	rdr.rebdrw_image_options = aggdrw_image_options;
	rdr.rebdrw_image_scale = aggdrw_image_scale;
	rdr.rebdrw_image_pattern = aggdrw_image_pattern;
	rdr.rebdrw_line = aggdrw_line;
	rdr.rebdrw_line_cap = aggdrw_line_cap;
	rdr.rebdrw_line_join = aggdrw_line_join;
	rdr.rebdrw_line_pattern = aggdrw_line_pattern;
	rdr.rebdrw_line_width = aggdrw_line_width;
	rdr.rebdrw_matrix = aggdrw_matrix;
	rdr.rebdrw_pen = aggdrw_pen;
	rdr.rebdrw_pen_image = aggdrw_pen_image;
	rdr.rebdrw_pop_matrix = aggdrw_pop_matrix;
	rdr.rebdrw_push_matrix = aggdrw_push_matrix;
	rdr.rebdrw_reset_gradient_pen = aggdrw_reset_gradient_pen;
	rdr.rebdrw_reset_matrix = aggdrw_reset_matrix;
	rdr.rebdrw_rotate = aggdrw_rotate;
	rdr.rebdrw_scale = aggdrw_scale;
	rdr.rebdrw_to_image = aggdrw_to_image;
	rdr.rebdrw_skew = aggdrw_skew;
	rdr.rebdrw_text = aggdrw_text;
	rdr.rebdrw_transform = aggdrw_transform;
	rdr.rebdrw_translate = aggdrw_translate;
	rdr.rebdrw_triangle = aggdrw_triangle;

	rdr.rebshp_arc = aggshp_arc;
	rdr.rebshp_close = aggshp_close;
	rdr.rebshp_hline = aggshp_hline;
	rdr.rebshp_line = aggshp_line;
	rdr.rebshp_move = aggshp_move;
	rdr.rebshp_begin = aggshp_begin;
	rdr.rebshp_end = aggshp_end;
	rdr.rebshp_vline = aggshp_vline;
	rdr.rebshp_curv = aggshp_curv;
	rdr.rebshp_curve = aggshp_curve;
	rdr.rebshp_qcurv = aggshp_qcurv;
    rdr.rebshp_qcurve = aggshp_qcurve;

    return rdr;
}

struct REBRDR_DRW draw_agg = init_agg_draw();
#else
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
#endif
