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

typedef struct REBDRW_CTX REBDRW_CTX;

extern void rebdrw_begin_frame(REBDRW_CTX *ctx);
extern void rebdrw_end_frame(REBDRW_CTX *ctx);
extern void rebdrw_blit_frame(REBDRW_CTX *ctx);
extern void rebdrw_resize_context(REBDRW_CTX *ctx, REBINT w, REBINT h);
extern REBDRW_CTX* rebdrw_create_context(REBINT w, REBINT h);
extern void rebdrw_destroy_context(REBDRW_CTX *ctx);


extern void rebdrw_add_poly_vertex (void* gr, REBXYF p);
extern void rebdrw_add_spline_vertex (void* gr, REBXYF p);
extern void rebdrw_anti_alias(void* gr, REBINT mode);
extern void rebdrw_arc(void* gr, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed);
extern void rebdrw_arrow(void* gr, REBXYF mode, REBCNT col);
extern void rebdrw_begin_poly (void* gr, REBXYF p);
extern void rebdrw_begin_spline (void* gr, REBXYF p);
extern void rebdrw_box(void* gr, REBXYF p1, REBXYF p2, REBDEC r);
extern void rebdrw_circle(void* gr, REBXYF p, REBXYF r);
extern void rebdrw_clip(void* gr, REBXYF p1, REBXYF p2);
extern void rebdrw_curve3(void* gr, REBXYF p1, REBXYF p2, REBXYF p3);
extern void rebdrw_curve4(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBXYF p4);
extern void rebdrw_ellipse(void* gr, REBXYF p1, REBXYF p2);
extern void rebdrw_end_poly (void* gr);
extern void rebdrw_end_spline (void* gr, REBINT step, REBINT closed);
extern void rebdrw_fill_pen(void* gr, REBCNT color);
extern void rebdrw_fill_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h);
extern void rebdrw_fill_rule(void* gr, REBINT mode);
extern void rebdrw_gamma(void* gr, REBDEC gamma);
extern void rebdrw_gob_color(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom);
extern void rebdrw_gob_draw(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom);
extern void rebdrw_gob_image(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom);
extern void rebdrw_gradient_pen(void* gr, REBINT gradtype, REBINT mode, REBXYF oft, REBXYF range, REBDEC angle, REBXYF scale, REBSER* colors);
extern void rebdrw_invert_matrix(void* gr);
extern void rebdrw_image(void* gr, REBYTE* img, REBINT w, REBINT h,REBXYF offset);
extern void rebdrw_image_filter(void* gr, REBINT type, REBINT mode, REBDEC blur);
extern void rebdrw_image_options(void* gr, REBOOL keyColEnabled, REBCNT keyCol, REBINT border);
extern void rebdrw_image_scale(void* gr, REBYTE* img, REBINT w, REBINT h, REBSER* points);
extern void rebdrw_image_pattern(void* gr, REBINT mode, REBXYF offset, REBXYF size);
extern void rebdrw_line(void* gr, REBXYF p1, REBXYF p2);
extern void rebdrw_line_cap(void* gr, REBINT mode);
extern void rebdrw_line_join(void* gr, REBINT mode);
extern void rebdrw_line_pattern(void* gr, REBCNT col, REBDEC* patterns);
extern void rebdrw_line_width(void* gr, REBDEC width, REBINT mode);
extern void rebdrw_matrix(void* gr, REBSER* mtx);
extern void rebdrw_pen(void* gr, REBCNT col);
extern void rebdrw_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h);
extern void rebdrw_pop_matrix(void* gr);
extern void rebdrw_push_matrix(void* gr);
extern void rebdrw_reset_gradient_pen(void* gr);
extern void rebdrw_reset_matrix(void* gr);
extern void rebdrw_rotate(void* gr, REBDEC ang);
extern void rebdrw_scale(void* gr, REBXYF sc);
extern void rebdrw_to_image(REBYTE *image, REBINT w, REBINT h, REBSER *block);
extern void rebdrw_skew(void* gr, REBXYF angle);
extern void rebdrw_text(void* gr, REBINT mode, REBXYF* p1, REBXYF* p2, REBSER* block);
extern void rebdrw_transform(void* gr, REBDEC ang, REBXYF ctr, REBXYF scm, REBXYF oft);
extern void rebdrw_translate(void* gr, REBXYF p);
extern void rebdrw_triangle(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBCNT c1, REBCNT c2, REBCNT c3, REBDEC dilation);

//extern REBINT rebdrw_effect(void* gr, REBPAR* p1, REBPAR* p2, REBSER* block);

//SHAPE functions
extern void rebshp_arc(void* gr, REBCNT rel, REBXYF p, REBXYF r, REBDEC ang, REBINT sweep, REBINT large);
extern void rebshp_close(void* gr);
extern void rebshp_hline(void* gr,REBCNT rel, REBDEC x);
extern void rebshp_line(void* gr, REBCNT rel, REBXYF p);
extern void rebshp_move(void* gr, REBCNT rel, REBXYF p);
extern void rebshp_open(void* gr);
extern void rebshp_end(void* gr);
extern void rebshp_vline(void* gr,REBCNT rel, REBDEC y);
extern void rebshp_curv(void* gr, REBCNT rel, REBXYF p1, REBXYF p2);
extern void rebshp_curve(void* gr, REBCNT rel, REBXYF p1, REBXYF p2, REBXYF p3);
extern void rebshp_qcurv(void* gr, REBCNT rel, REBXYF p);
extern void rebshp_qcurve(void* gr, REBCNT rel, REBXYF p1, REBXYF p2);
