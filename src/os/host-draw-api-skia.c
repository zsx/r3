/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Additional code modifications and improvements Copyright 2012 Saphirion AG
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Title: DRAW dialect API functions
**  Author: Richard Smolak
**
************************************************************************
**
**  NOTE to PROGRAMMERS:
**
**    1. Keep code clear and simple.
**    2. Document unusual code, reasoning, or gotchas.
**    3. Use same style for code, vars, indent(4), comments, etc.
**    4. Keep in mind Linux, OS X, BSD, big/little endian CPUs.
**    5. Test everything, then test it again.
**
***********************************************************************/

#include "math.h" // for infinity

#include "reb-host.h"
#include "reb-series.h"
#include "reb-skia.h"

extern void *Rich_Text;

#define NOT_IMPLEMENTED \
    printf("NOT IMPLEMENTED, %s, %s, %d\n", __FILE__, __func__, __LINE__)

void rebdrw_add_vertex (void* gr, REBXYF p)
{
	rs_draw_add_vertex(gr, p.x, p.y);
}

void rebdrw_anti_alias(void* gr, REBINT mode)
{
	rs_draw_anti_alias(gr, mode != 0);
}

void rebdrw_arc(void* gr, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed)
{
	rs_draw_arc(gr, c.x, c.y, r.x, r.y, ang1, ang2, closed != 0);
}

void rebdrw_arrow(void* gr, REBXYF mode, REBCNT col)
{
	rs_draw_arrow(gr, mode.x, mode.y, col);
}

void rebdrw_begin_poly (void* gr, REBXYF p)
{
	rs_draw_begin_poly(gr, p.x, p.y);
}

void rebdrw_box(void* gr, REBXYF p1, REBXYF p2, REBDEC r)
{
	rs_draw_box(gr, p1.x, p1.y, p2.x, p2.y, r);
}

void rebdrw_circle(void* gr, REBXYF p, REBXYF r)
{
	rs_draw_circle(gr, p.x, p.y, r.x);
}

void rebdrw_clip(void* gr, REBXYF p1, REBXYF p2)
{
	rs_draw_clip(gr, p1.x, p1.y, p2.x, p2.y);
}

void rebdrw_curve3(void* gr, REBXYF p1, REBXYF p2, REBXYF p3)
{
	rs_draw_curve3(gr, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

void rebdrw_curve4(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBXYF p4)
{
	rs_draw_curve4(gr, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y);
}
								
REBINT rebdrw_effect(void* gr, REBPAR* p1, REBPAR* p2, REBSER* block)
{
    NOT_IMPLEMENTED;
	return 0;
}

void rebdrw_ellipse(void* gr, REBXYF p1, REBXYF p2)
{
	rs_draw_ellipse(gr, p1.x, p1.y, p2.x, p2.y);
}

void rebdrw_end_poly (void* gr)
{
	rs_draw_end_poly(gr);
}

void rebdrw_end_spline (void* gr, REBINT step, REBINT closed)
{
    rs_draw_end_spline(gr, step, closed);
}

void rebdrw_fill_pen(void* gr, REBCNT col)
{
	rs_draw_fill_pen(gr, col);
}

void rebdrw_fill_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
{
	rs_draw_fill_pen_image(gr, img, w, h);
}

void rebdrw_fill_rule(void* gr, REBINT mode)
{
	rs_draw_fill_rule(gr,
		mode == 1? RS_FILL_RULE_EVEN_ODD :
		mode == 2? RS_FILL_RULE_NON_ZERO :
		RS_FILL_RULE_EVEN_ODD //default
	);
}

void rebdrw_gamma(void* gr, REBDEC gamma)
{
	rs_draw_set_gamma(gr, gamma);
}

void rebdrw_gradient_pen(void* gr, REBINT gradtype, REBINT mode, REBXYF oft, REBXYF range, REBDEC angle, REBXYF scale, REBSER* colors)
{
    NOT_IMPLEMENTED;
    // TODO
    //rs_draw_gradient_pen(gr, gradtype, mode, oft.x, range.x, angle, scale.x, scale.y, colors);
}

void rebdrw_invert_matrix(void* gr)
{
	rs_draw_invert_matrix(gr);
}

void rebdrw_image(void* gr, REBYTE* img, REBINT w, REBINT h,REBXYF offset)
{
	rs_draw_image(gr, img, w, h, offset.x, offset.y);
}

void rebdrw_image_filter(void* gr, REBINT type, REBINT mode, REBDEC blur)
{
    rs_draw_image_filter(gr, type, mode, blur);
}

void rebdrw_image_options(void* gr, REBCNT keyCol, REBINT border)
{
    rs_draw_image_options(gr, keyCol, border);
}

void rebdrw_image_pattern(void* gr, REBINT mode, REBXYF offset, REBXYF size)
{
    rs_draw_image_pattern(gr, mode, offset.x, offset.y, size.x, size.y);
}

void rebdrw_image_scale(void* gr, REBYTE* img, REBINT w, REBINT h, REBSER* points)
{
    RXIARG a;
    REBXYF p[4];
    REBCNT type;
    REBCNT n, len = 0;

    for (n = 0; type = RL_GET_VALUE(points, n, &a); n++) {
        if (type == RXT_PAIR) {
            p[len] = (REBXYF) RXI_LOG_PAIR(a);
            if (++len == 4) break;
        }
    }

    if (!len) return;
    if (len == 1 && log_size.x == 1 && log_size.y == 1) {
        rs_draw_image(gr, img, w, h, p[0].x, p[0].y);
        return;
    }

    switch (len) {
    case 2:
        rs_draw_image_scale(gr, img, w, h, p[0].x, p[0].y, p[1].x, p[1].y);
        break;
    case 3:
    case 4:
        NOT_IMPLEMENTED;
    }
}

void rebdrw_line(void* gr, REBXYF p1, REBXYF p2)
{
	rs_draw_line(gr, p1.x, p1.y, p2.x, p2.y);
}

void rebdrw_line_cap(void* gr, REBINT mode)
{
    rs_draw_line_cap(gr,
        mode == 0 ? RS_LINE_CAP_BUTT :
        mode == 1 ? RS_LINE_CAP_SQUARE :
        mode == 2 ? RS_LINE_CAP_ROUND :
        mode);
}

void rebdrw_line_join(void* gr, REBINT mode)
{
    rs_draw_line_join(gr,
        mode == 0 ? RS_LINE_JOIN_MITER :
        mode == 1 ? RS_LINE_JOIN_MITER : // FIXME: Rebol expects miter-bevel
        mode == 2 ? RS_LINE_JOIN_ROUND :
        mode == 3 ? RS_LINE_JOIN_BEVEL :
        mode);
}

void rebdrw_line_pattern(void* gr, REBCNT col, REBDEC* patterns)
{
    rs_draw_line_pattern(gr, col, patterns);
}

void rebdrw_line_width(void* gr, REBDEC width, REBINT mode)
{
	rs_draw_line_width(gr, width, mode);
}

void rebdrw_matrix(void* gr, REBSER* mtx)
{
    RXIARG val;
    REBCNT type;
    REBCNT n;
    float m[6];

    for (n = 0; type = RL_GET_VALUE(mtx, n, &val), n < 6; n++) {
        if (type == RXT_DECIMAL)
            m[n] = val.dec64;
        else if (type == RXT_INTEGER)
            m[n] = val.int64;
        else {
            return;
        }
    }

    if (n != 6) return;

    rs_draw_matrix(gr, m[0], m[1], m[2], m[3], m[4], m[5]);
}

void rebdrw_pen(void* gr, REBCNT col)
{
	rs_draw_pen(gr, col);
}

void rebdrw_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
{
    rs_draw_pen_image(gr, img, w, h);
}

void rebdrw_pop_matrix(void* gr)
{
	rs_draw_pop_matrix(gr);
}

void rebdrw_push_matrix(void* gr)
{
	rs_draw_push_matrix(gr);
}

void rebdrw_reset_gradient_pen(void* gr)
{
    NOT_IMPLEMENTED;
    //rs_draw_reset_gradient_pen(gr);
}

void rebdrw_reset_matrix(void* gr)
{
	rs_draw_reset_matrix(gr);
}

void rebdrw_rotate(void* gr, REBDEC ang)
{
	rs_draw_rotate(gr, ang);
}

void rebdrw_scale(void* gr, REBXYF sc)
{
	rs_draw_scale(gr, sc.x, sc.y);
}

void rebdrw_skew(void* gr, REBXYF angle)
{
	rs_draw_skew(gr, angle.x, angle.y);
}

void rebdrw_text(void* gr, REBINT mode, REBXYF* p1, REBXYF* p2, REBSER* block)
{
	rs_rich_text_t *rt = (rs_rich_text_t *)Rich_Text;
    rs_rt_reset(rt);
    rs_draw_text_pre_setup(gr, rt);
    rt_block_text(rt, block);
	rs_draw_text(gr, p1->x, p1->y, p2? p2->x : INFINITY, p2 ? p2->y : INFINITY, rt);
}

void rebdrw_transform(void* gr, REBDEC ang, REBXYF ctr, REBXYF sc, REBXYF oft)
{
	rs_draw_transform(gr, ang, ctr.x, ctr.y, sc.x, sc.y, oft.x, oft.y);
}

void rebdrw_translate(void* gr, REBXYF p)
{
	rs_draw_translate(gr, p.x, p.y);
}

void rebdrw_triangle(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBCNT c1, REBCNT c2, REBCNT c3, REBDEC dilation)
{
	rs_draw_triangle(gr, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, c1, c2, c3, 0);
}

//SHAPE functions
void rebshp_arc(void* gr, REBINT rel, REBXYF p, REBXYF r, REBDEC ang, REBINT sweep, REBINT large)
{
	rs_shape_arc(gr, rel, p.x, p.y, r.x, r.y, ang, sweep, large);
}

void rebshp_close(void* gr)
{
	rs_shape_close(gr);
}

void rebshp_curv(void* gr, REBINT rel, REBXYF p1, REBXYF p2)
{
	rs_shape_curv(gr, rel, p1.x, p1.y, p2.x, p2.y);
}

void rebshp_curve(void* gr, REBINT rel, REBXYF p1,REBXYF p2, REBXYF p3)
{
	rs_shape_curve(gr, rel, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

void rebshp_hline(void* gr, REBCNT rel, REBDEC x)
{
	rs_shape_hline(gr, rel, x);
}

void rebshp_line(void* gr, REBCNT rel, REBXYF p)
{
	rs_shape_line(gr, rel, p.x, p.y);
}

void rebshp_move(void* gr, REBCNT rel, REBXYF p)
{
	rs_shape_move(gr, rel, p.x, p.y);
}

void rebshp_open(void* gr)
{
	rs_shape_open(gr);
}

void rebshp_vline(void* gr, REBCNT rel, REBDEC y)
{
	rs_shape_vline(gr, rel, y);
}

void rebshp_qcurv(void* gr, REBINT rel, REBXYF p)
{
	rs_shape_qcurv(gr, rel, p.x, p.y);
}

void rebshp_qcurve(void* gr, REBINT rel, REBXYF p1, REBXYF p2)
{
	rs_shape_qcurve(gr, rel, p1.x, p1.y, p2.x, p2.y);
}


void rebdrw_to_image(REBYTE *image, REBINT w, REBINT h, REBSER *block)
{
    rs_draw_context_t *ctx = rs_draw_create_context_with_dimension(w, h);
    rs_draw_begin_frame(ctx);

	REBCEC cec;
	cec.envr = ctx;
	cec.block = block;
	cec.index = 0;

	RL_DO_COMMANDS(block, 0, &cec);

    rs_draw_end_frame(ctx);

    rs_draw_read_pixel(ctx, image);
    rs_draw_free_context(ctx);
}

void rebdrw_gob_color(REBGOB *gob, rs_draw_context_t *ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz)
{
    rs_draw_reset_painters(ctx);
	rs_argb_t color = *(rs_argb_t*)&GOB_CONTENT(gob);
    if (GOB_ALPHA(gob) == 255) {
        rs_draw_box_color(ctx, clip_oft.x, clip_oft.y, clip_siz.x, clip_siz.y, 0, color);
    } else {
        // FIXME: replacing the alpha component in color with alpha in gob
        // might not be the same as old r3-alpha
        color &= 0x00FFFFFF;
        color |= GOB_ALPHA(gob) << 24;

        rs_draw_box_color(ctx, clip_oft.x, clip_oft.y, clip_siz.x, clip_siz.y, 0, color);
    }
}

void rebdrw_gob_image(REBGOB *gob, rs_draw_context_t *ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz)
{

    struct rebol_series* img = (struct rebol_series*)GOB_CONTENT(gob);
    int w = IMG_WIDE(img);
    int h = IMG_HIGH(img);

    rs_draw_reset_painters(ctx);

    // TODO: set clip
    rs_draw_image(ctx, IMG_DATA(img), w, h, abs_oft.x, abs_oft.y);
}

void rebdrw_gob_draw(REBGOB *gob, rs_draw_context_t *ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz)
{
	REBCEC cec;
	REBSER *block = (REBSER *)GOB_CONTENT(gob);

	rs_draw_push_local(ctx, abs_oft.x, abs_oft.y,
		clip_oft.x, clip_oft.y, clip_siz.x + clip_oft.x, clip_siz.y + clip_oft.y);
    rs_draw_reset_painters(ctx);

	cec.envr = ctx;
	cec.block = block;
	cec.index = 0;

	RL_DO_COMMANDS(block, 0, &cec);

	rs_draw_pop_local(ctx);
}