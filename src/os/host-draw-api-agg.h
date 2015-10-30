/***********************************************************************
 **
 **  REBOL [R3] Language Interpreter and Run-time Environment
 **
 **  Copyright 2012 REBOL Technologies
 **  REBOL is a trademark of REBOL Technologies
 **
 **  Additional code modifications and improvements Copyright 2015 Atronix Engineering, Inc
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
 **  Author: Shixin Zeng
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

#include <SDL.h>

struct REBDRW_CTX {
	SDL_Window 		*win;
	SDL_Surface 	*surface;
	SDL_Renderer 	*renderer;
	SDL_Rect		clip;
};

extern void aggdrw_add_poly_vertex (void* gr, REBXYF p);
extern void aggdrw_add_spline_vertex(void* gr, REBXYF p);
extern void aggdrw_anti_alias(void* gr, REBINT mode);
extern void aggdrw_arc(void* gr, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed);
extern void aggdrw_arrow(void* gr, REBXYF mode, REBCNT col);
extern void aggdrw_begin_poly (void* gr, REBXYF p);
extern void aggdrw_begin_spline(void* gr, REBXYF p);
extern void aggdrw_box(void* gr, REBXYF p1, REBXYF p2, REBDEC r);
extern void aggdrw_circle(void* gr, REBXYF p, REBXYF r);
extern void aggdrw_clip(void* gr, REBXYF p1, REBXYF p2);
extern void aggdrw_curve3(void* gr, REBXYF p1, REBXYF p2, REBXYF p3);
extern void aggdrw_curve4(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBXYF p4);
extern void aggdrw_ellipse(void* gr, REBXYF p1, REBXYF p2);
extern void aggdrw_end_poly (void* gr);
extern void aggdrw_end_spline (void* gr, REBINT step, REBINT closed);
extern void aggdrw_fill_pen(void* gr, REBCNT color);
extern void aggdrw_fill_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h);
extern void aggdrw_fill_rule(void* gr, REBINT mode);
extern void aggdrw_gamma(void* gr, REBDEC gamma);
extern void aggdrw_gob_color(REBGOB *gob, REBDRW_CTX *draw_ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);
extern void aggdrw_gob_draw(REBGOB *gob, REBDRW_CTX *draw_ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);
extern void aggdrw_gob_image(REBGOB *gob, REBDRW_CTX *draw_ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);
extern void aggdrw_gradient_pen(void* gr, REBINT gradtype, REBINT mode, REBXYF oft, REBXYF range, REBDEC angle, REBXYF scale, REBSER* colors);
extern void aggdrw_invert_matrix(void* gr);
extern void aggdrw_image(void* gr, REBYTE* img, REBINT w, REBINT h,REBXYF offset);
extern void aggdrw_image_filter(void* gr, REBINT type, REBINT mode, REBDEC blur);
extern void aggdrw_image_options(void* gr, REBCNT keyCol, REBINT border);
extern void aggdrw_image_scale(void* gr, REBYTE* img, REBINT w, REBINT h, REBSER* points);
extern void aggdrw_image_pattern(void* gr, REBINT mode, REBXYF offset, REBXYF size);
extern void aggdrw_line(void* gr, REBXYF p1, REBXYF p2);
extern void aggdrw_line_cap(void* gr, REBINT mode);
extern void aggdrw_line_join(void* gr, REBINT mode);
extern void aggdrw_line_pattern(void* gr, REBCNT col, REBDEC* patterns);
extern void aggdrw_line_width(void* gr, REBDEC width, REBINT mode);
extern void aggdrw_matrix(void* gr, REBSER* mtx);
extern void aggdrw_pen(void* gr, REBCNT col);
extern void aggdrw_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h);
extern void aggdrw_pop_matrix(void* gr);
extern void aggdrw_push_matrix(void* gr);
extern void aggdrw_reset_gradient_pen(void* gr);
extern void aggdrw_reset_matrix(void* gr);
extern void aggdrw_rotate(void* gr, REBDEC ang);
extern void aggdrw_scale(void* gr, REBXYF sc);
extern void aggdrw_to_image(REBYTE *image, REBINT w, REBINT h, REBSER *block);
extern void aggdrw_skew(void* gr, REBXYF angle);
extern void aggdrw_text(void* gr, REBINT mode, REBXYF* p1, REBXYF* p2, REBSER* block);
extern void aggdrw_transform(void* gr, REBDEC ang, REBXYF ctr, REBXYF scm, REBXYF oft);
extern void aggdrw_translate(void* gr, REBXYF p);
extern void aggdrw_triangle(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBCNT c1, REBCNT c2, REBCNT c3, REBDEC dilation);

//extern REBINT aggdrw_effect(void* gr, REBPAR* p1, REBPAR* p2, REBSER* block);

//SHAPE functions
extern void aggshp_arc(void* gr, REBINT rel, REBXYF p, REBXYF r, REBDEC ang, REBINT sweep, REBINT large);
extern void aggshp_begin(void* gr);
extern void aggshp_close(void* gr);
extern void aggshp_end(void* gr);
extern void aggshp_hline(void* gr,REBINT rel, REBDEC x);
extern void aggshp_line(void* gr, REBINT rel, REBXYF p);
extern void aggshp_move(void* gr, REBINT rel, REBXYF p);
extern void aggshp_open(void* gr);
extern void aggshp_vline(void* gr,REBINT rel, REBDEC y);
extern void aggshp_curv(void* gr, REBINT rel, REBXYF p1, REBXYF p2);
extern void aggshp_curve(void* gr, REBINT rel, REBXYF p1, REBXYF p2, REBXYF p3);
extern void aggshp_qcurv(void* gr, REBINT rel, REBXYF p);
extern void aggshp_qcurve(void* gr, REBINT rel, REBXYF p1, REBXYF p2);
