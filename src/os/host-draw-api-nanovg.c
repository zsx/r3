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

#include <stdio.h>
#include "reb-host.h"
#include "reb-series.h"

#include "nanovg.h"

#include "host-view.h"
#include "host-draw-api.h"

#define NANOVG_GL3_IMPLEMENTATION   // Use GL3 implementation.
#define NANOVG_FBO_VALID
#define GL_GLEXT_PROTOTYPES

#include <GL/glew.h>

#include "nanovg_gl.h"

struct REBDRW_CTX {
	NVGcontext	*nvg;
	NVGlayer	*win_layer;
	NVGlayer	*gob_layer;
	NVGlayer	*tmp_layer;
	REBINT		ww;
	REBINT		wh;
	float		pixel_ratio;

	/* gob clip, in gob's local coordinates */
	float		clip_x; 
	float		clip_y;
	float		clip_w;
	float		clip_h;

	/* gob offset, in window coordinates*/
	float		offset_x;
	float		offset_y;

	/* for shapes */
	float 		last_x;
	float 		last_y;

	/* fill or stroke */
	int 		fill_image;
	int 		stroke_image;
	unsigned int fill: 1;
	unsigned int stroke: 1;
};

REBUPT RL_Series(REBSER *ser, REBCNT what);

void rebdrw_add_vertex (void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgLineTo(ctx->nvg, p.x, p.y);

	//printf("new polygen vertex at (%f, %f)\n", p.x, p.y);

	//((agg_graphics*)gr)->agg_add_vertex(p.x, p.y);
}

void rebdrw_anti_alias(void* gr, REBINT mode)
{
	//always on
}

#define BEGIN_NVG_PATH(ctx) 					\
	do { 									\
		if (! ((ctx)->fill || (ctx)->stroke)) {	\
			printf("early return from line %d because of no fill or stroke\n", __LINE__); \
			return; 						\
		} 									\
		nvgBeginPath((ctx)->nvg); 			\
	} while (0)

#define END_NVG_PATH(ctx) 				\
	do { 							\
		if ((ctx)->fill) {			\
			/* printf("Filling a path, %d\n", __LINE__); */ \
			nvgFill((ctx)->nvg); 		\
		} 							\
		if ((ctx)->stroke) { 			\
			/* printf("stroking a path, %d\n", __LINE__); */ \
			nvgStroke((ctx)->nvg);	\
		} 							\
	} while (0)

#define REBCNT_NVG_COLOR(c) \
	nvgRGBA(((unsigned char*)&(c))[C_R], \
			((unsigned char*)&(c))[C_G], \
			((unsigned char*)&(c))[C_B], \
			((unsigned char*)&(c))[C_A])

#define PAINT_LAYER(ctx, layer, paint_mode, alpha, clip_oft, clip_size) 	\
	do {												\
		REBXYF img_oft = {0, 0};						\
		REBXYF img_size = {ctx->ww, ctx->wh};			\
		paint_image(ctx, (layer)->image, paint_mode, alpha, img_oft, img_size, clip_oft, clip_size);\
	} while (0)

#define PAINT_LAYER_FULL(ctx, layer, paint_mode) 	\
	do {												\
		REBXYF clip_oft = {0, 0};						\
		REBXYF clip_size = {ctx->ww, ctx->wh};			\
		PAINT_LAYER(ctx, layer, paint_mode, 1.0f, clip_oft, clip_size);\
	} while (0)

void rebdrw_arc(void* gr, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGcontext *nvg = ctx->nvg;
	BEGIN_NVG_PATH(ctx);
	if (r.x == r.y) {
		if (closed) {
			float x0, y0;
			x0 = c.x + r.x * cos(nvgDegToRad(ang1));
			y0 = c.y + r.y * sin(nvgDegToRad(ang1));

			nvgMoveTo(nvg, c.x, c.y);
			nvgLineTo(nvg, x0, y0);
			nvgArc(nvg, c.x, c.y, r.x, nvgDegToRad(ang1), nvgDegToRad(ang2), NVG_CW);
			nvgClosePath(nvg);
		} else {
			nvgArc(nvg, c.x, c.y, r.x, nvgDegToRad(ang1), nvgDegToRad(ang2), NVG_CW);
		}
	} else {
		/* FIXME */
		printf("FIXME: %d\n", __LINE__);
	}
	END_NVG_PATH(ctx);
}

void rebdrw_arrow(void* gr, REBXYF mode, REBCNT col)
{
	//((agg_graphics*)gr)->agg_arrows((col) ? (REBYTE*)&col : NULL, (REBINT)mode.x, (REBINT)mode.y);
}

void rebdrw_begin_poly (void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p.x, p.y);
	//printf("new polygen at: (%f, %f)\n", p.x, p.y);
}

void rebdrw_box(void* gr, REBXYF p1, REBXYF p2, REBDEC r)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	if (r) {
		nvgRoundedRect(ctx->nvg, p1.x, p1.y, p2.x - p1.x, p2.y - p1.y, r);
	} else {
		nvgRect(ctx->nvg, p1.x, p1.y, p2.x - p1.x, p2.y - p1.y);
	}
	END_NVG_PATH(ctx);
}

void rebdrw_circle(void* gr, REBXYF p, REBXYF r)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	if (r.x != r.y) {
		nvgEllipse(ctx->nvg, p.x, p.y, r.x, r.y);
	} else {
		nvgCircle(ctx->nvg, p.x, p.y, r.x);
	}
	END_NVG_PATH(ctx);
}

void rebdrw_clip(void* gr, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgScissor(ctx->nvg, ctx->clip_x, ctx->clip_y, ctx->clip_w, ctx->clip_h);
	nvgIntersectScissor(ctx->nvg, p1.x, p1.y, p2.x - p1.x, p2.y - p1.y);
}

void rebdrw_curve3(void* gr, REBXYF p1, REBXYF p2, REBXYF p3)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p1.x, p1.y);
	nvgQuadTo(ctx->nvg, p2.x, p2.y, p3.x, p3.y);
	END_NVG_PATH(ctx);
}

void rebdrw_curve4(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBXYF p4)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p1.x, p1.y);
	nvgBezierTo(ctx->nvg, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y);
	END_NVG_PATH(ctx);
}

REBINT rebdrw_effect(void* gr, REBXYF* p1, REBXYF* p2, REBSER* block)
{
	return 0;
}

void rebdrw_ellipse(void* gr, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgEllipse(ctx->nvg, (p1.x + p2.x)/2, (p1.y + p2.y)/2, (p2.x - p1.x)/2, (p2.y - p1.y)/2);
	END_NVG_PATH(ctx);
}

void rebdrw_end_poly (void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgClosePath(ctx->nvg);
	END_NVG_PATH(ctx);
	//printf("polygen done\n");
}

void rebdrw_end_spline (void* gr, REBINT step, REBINT closed)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgClosePath(ctx->nvg);
	BEGIN_NVG_PATH(ctx);
	printf("spline done, FIXME\n");
	//((agg_graphics*)gr)->agg_end_bspline(step, closed);
}

void rebdrw_fill_pen(void* gr, REBCNT col)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	if (col) {
		nvgFillColor(ctx->nvg, REBCNT_NVG_COLOR(col));
		ctx->fill = TRUE;
	} else {
		ctx->fill = FALSE;
	}
}

void rebdrw_fill_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGpaint paint;

	if (ctx->fill_image != 0) {
		nvgFlush(ctx->nvg);
		nvgDeleteImage(ctx->nvg, ctx->fill_image);
	}

	ctx->fill_image = nvgCreateImageRGBA(ctx->nvg, w, h, 0, img);
	paint = nvgImagePattern(ctx->nvg, 0, 0, w, h, 0, ctx->fill_image, 1);
	nvgFillPaint(ctx->nvg, paint);
}

void rebdrw_fill_rule(void* gr, REBINT mode)
{
	//   if (mode >= W_DRAW_EVEN_ODD && mode <= W_DRAW_NON_ZERO)
	//      ((agg_graphics*)gr)->agg_fill_rule((agg::filling_rule_e)mode);
}

void rebdrw_gamma(void* gr, REBDEC gamma)
{
	//((agg_graphics*)gr)->agg_set_gamma(gamma);
}

void rebdrw_gradient_pen(void* gr, REBINT gradtype, REBINT mode, REBXYF oft, REBXYF range, REBDEC angle, REBXYF scale, REBSER* colors)
{
#if 0
#ifndef AGG_OPENGL
	unsigned char colorTuples[256*4+1] = {2, 0,0,0,255, 0,0,0,255, 255,255,255,255}; //max number of color tuples is 256 + one length information char
	REBDEC offsets[256] = {0.0 , 0.0, 1.0};

	//gradient fill
	RXIARG val;
	REBCNT type,i,j,k;
	REBCNT *ptuples = (REBCNT*)&colorTuples[5];

	for (i = 0, j = 1, k = 5; type = RL_GET_VALUE(colors, i, &val); i++) {
		if (type == RXT_DECIMAL || type == RXT_INTEGER) {
			offsets[j] = (type == RXT_DECIMAL) ? val.dec64 : val.int64;

			//do some validation
			offsets[j] = MIN(MAX(offsets[j], 0.0), 1.0);
			if (j != 1 && offsets[j] < offsets[j-1])
				offsets[j] = offsets[j-1];
			if (j != 1 && offsets[j] == offsets[j-1])
				offsets[j-1]-= 0.0000000001;

			j++;
		} else if (type == RXT_TUPLE) {
			*ptuples++ = RXI_COLOR_TUPLE(val);
			k+=4;
		}
	}

	//sanity checks
	if (j == 1) offsets[0] = -1;
	colorTuples[0] = MAX(2, (k - 5) / 4);

	((agg_graphics*)gr)->agg_gradient_pen(gradtype, oft.x, oft.y, range.x, range.y, angle, scale.x, scale.y, colorTuples, offsets, mode);
#endif		
#endif
}

void rebdrw_invert_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	float xform[6], inv[6];

	nvgCurrentTransform(ctx->nvg, xform);
	nvgTransformInverse(inv, xform);
	nvgTransform(ctx->nvg, inv[0], inv[1], inv[2], inv[3], inv[4], inv[5]);
}

static void paint_image(REBDRW_CTX *ctx, int image, REBINT mode, float alpha,
						REBXYF image_oft, REBXYF image_size,
						REBXYF clip_oft, REBXYF clip_size)
{
	NVGpaint paint = nvgImagePattern(ctx->nvg, image_oft.x, image_oft.y,
									 image_size.x, image_size.y, 0, image, alpha);

	nvgBlendMode(ctx->nvg, mode);

	nvgBeginPath(ctx->nvg);

	nvgFillPaint(ctx->nvg, paint);
	nvgRect(ctx->nvg, clip_oft.x, clip_oft.y, clip_size.x, clip_size.y);
	nvgFill(ctx->nvg);
}


void rebdrw_image(void* gr, REBYTE* img, REBINT w, REBINT h,REBXYF offset)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	REBXYF image_size = {w, h};

	int image = nvgCreateImageRGBA(ctx->nvg, w, h, 0, img);
	nvgSave(ctx->nvg);

	paint_image(ctx, image, NVG_COPY, 1.0f, offset, image_size, offset, image_size);

	nvgFlush(ctx->nvg);

	nvgDeleteImage(ctx->nvg, image);
	nvgRestore(ctx->nvg);
}

void rebdrw_image_filter(void* gr, REBINT type, REBINT mode, REBDEC blur)
{
	//((agg_graphics*)gr)->agg_image_filter(type, mode, blur);
}

void rebdrw_image_options(void* gr, REBCNT keyCol, REBINT border)
{
#if 0
	if (keyCol)
		((agg_graphics*)gr)->agg_image_options(((REBYTE*)&keyCol)[0], ((REBYTE*)&keyCol)[1], ((REBYTE*)&keyCol)[2], ((REBYTE*)&keyCol)[3], border);
	else
		((agg_graphics*)gr)->agg_image_options(0,0,0,0, border);
#endif
}

void rebdrw_image_pattern(void* gr, REBINT mode, REBXYF offset, REBXYF size)
{
#if 0
	if (mode)
		((agg_graphics*)gr)->agg_image_pattern(mode,offset.x,offset.y,size.x,size.y);
	else
		((agg_graphics*)gr)->agg_image_pattern(0,0,0,0,0);
#endif
}

void rebdrw_image_scale(void* gr, REBYTE* img, REBINT w, REBINT h, REBSER* points)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	int image = -1;
	NVGpaint paint;

	RXIARG a;
	REBXYF p[4];
	REBCNT type;
	REBCNT n, len = 0;

	for (n = 0; (type = RL_GET_VALUE(points, n, &a)); n++) {
		if (type == RXT_PAIR){
			REBXYF tmp = RXI_LOG_PAIR(a);
			p[len] = tmp;
			if (++len == 4) break;
		}
	}

	if (!len) return;

	image = nvgCreateImageRGBA(ctx->nvg, w, h, 0, img);
	nvgSave(ctx->nvg);

	paint = nvgImagePattern(ctx->nvg, p[0].x, p[0].y, w, h, 0, image, 1.0f);
	nvgBlendMode(ctx->nvg, NVG_SOURCE_OVER);

	nvgBeginPath(ctx->nvg);

	nvgFillPaint(ctx->nvg, paint);
	nvgMoveTo(ctx->nvg, p[0].x, p[0].y);

	switch (len) {
		case 2:
			nvgLineTo(ctx->nvg, p[1].x, p[0].y);
			nvgLineTo(ctx->nvg, p[1].x, p[1].y);
			nvgLineTo(ctx->nvg, p[0].x, p[1].y);
			break;
		case 3:
			nvgLineTo(ctx->nvg, p[1].x, p[1].y);
			nvgLineTo(ctx->nvg, p[2].x, p[2].y);
			nvgLineTo(ctx->nvg, p[0].x, p[2].y);
			break;
		case 4:
			nvgLineTo(ctx->nvg, p[1].x, p[1].y);
			nvgLineTo(ctx->nvg, p[2].x, p[2].y);
			nvgLineTo(ctx->nvg, p[3].x, p[3].y);
			break;
	}

	nvgClosePath(ctx->nvg);

	nvgFill(ctx->nvg);
	nvgFlush(ctx->nvg);

	nvgDeleteImage(ctx->nvg, image);
	nvgRestore(ctx->nvg);
}

void rebdrw_line(void* gr, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p1.x, p1.y);
	nvgLineTo(ctx->nvg, p2.x, p2.y);
	END_NVG_PATH(ctx);
}

void rebdrw_line_cap(void* gr, REBINT mode)
{
	/*
	   butt_cap,
	   square_cap,
	   round_cap
	   */
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	switch (mode) {
		case 0:
			nvgLineCap(ctx->nvg, NVG_BUTT);
			break;
		case 1:
			nvgLineCap(ctx->nvg, NVG_SQUARE);
			break;
		case 2:
			nvgLineCap(ctx->nvg, NVG_ROUND);
			break;
	}
}

void rebdrw_line_join(void* gr, REBINT mode)
{
	/*
	   miter_join         = 0,
	   miter_join_revert  = 1,
	   round_join         = 2,
	   bevel_join         = 3
	   miter_join_round   = 4,
	   */

	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	switch (mode) {
		case 0:
		case 1:
		case 4:
			nvgLineJoin(ctx->nvg, NVG_MITER);
			break;
		case 2:
			nvgLineJoin(ctx->nvg, NVG_ROUND);
			break;
		case 3:
			nvgLineJoin(ctx->nvg, NVG_BEVEL);
			break;
		default:
			nvgLineJoin(ctx->nvg, NVG_MITER);
			break;
	}
}

void rebdrw_line_pattern(void* gr, REBCNT col, REBDEC* patterns)
{
	//((agg_graphics*)gr)->agg_line_pattern((col) ? (REBYTE*)&col : NULL, patterns);
}

void rebdrw_line_width(void* gr, REBDEC width, REBINT mode)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	if (mode) {/* fixed */
		nvgStrokeWidth(ctx->nvg, width);
	} else { /* variable, scaled by the matrix */
		float xform[6];
		nvgCurrentTransform(ctx->nvg, xform);
		nvgStrokeWidth(ctx->nvg, width * (xform[0] + xform[3]) / 2);
	}
}

void rebdrw_matrix(void* gr, REBSER* mtx)
{
	RXIARG val;
	REBCNT type;
	REBCNT n;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	float matrix[6];

	for (n = 0; type = RL_GET_VALUE(mtx, n, &val),n < 6; n++) {
		if (type == RXT_DECIMAL)
			matrix[n] = val.dec64;
		else if (type == RXT_INTEGER)
			matrix[n] = val.int64;
		else {
			return;
		}
	}

	if (n != 6) return;

	nvgTransform(ctx->nvg, matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]);
}

void rebdrw_pen(void* gr, REBCNT col)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	if (col){
		ctx->stroke = TRUE;
		nvgStrokeColor(ctx->nvg, REBCNT_NVG_COLOR(col));
	} else {
		ctx->stroke = FALSE;
		nvgStrokeColor(ctx->nvg, nvgRGBA(255, 255, 255, 0));
	}
}

void rebdrw_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGpaint paint;

	if (ctx->stroke_image != 0) {
		nvgFlush(ctx->nvg);
		nvgDeleteImage(ctx->nvg, ctx->stroke_image);
	}

	ctx->stroke_image = nvgCreateImageRGBA(ctx->nvg, w, h, 0, img);
	paint = nvgImagePattern(ctx->nvg, 0, 0, w, h, 0, ctx->stroke_image, 1);
	nvgStrokePaint(ctx->nvg, paint);
}

void rebdrw_pop_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgRestore(ctx->nvg);
}

void rebdrw_push_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgSave(ctx->nvg);
}

void rebdrw_reset_gradient_pen(void* gr)
{
}

void rebdrw_reset_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgResetTransform(ctx->nvg);
	nvgTranslate(ctx->nvg, ctx->offset_x, ctx->offset_y);
}

void rebdrw_rotate(void* gr, REBDEC ang)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgRotate(ctx->nvg, nvgDegToRad(ang));
}

void rebdrw_scale(void* gr, REBXYF sc)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgScale(ctx->nvg, sc.x, sc.y);
}

void rebdrw_skew(void* gr, REBXYF angle)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	//nvgScale(ctx->nvg, sc.x, sc.y);
}
void rebdrw_text(void* gr, REBINT mode, REBXYF* p1, REBXYF* p2, REBSER* block)
{
}
void rebdrw_transform(void* gr, REBDEC ang, REBXYF ctr, REBXYF sc, REBXYF oft)
{
	//nvgTransform(ctx->nvg, sc.x, matrix[1], sc.y, matrix[3], oft.x, oft.y);
	//((agg_graphics*)gr)->agg_transform(ang, ctr.x, ctr.y, sc.x, sc.y, oft.x, oft.y);
}

void rebdrw_translate(void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgTranslate(ctx->nvg, p.x, p.y);
}

static inline double square(double x)
{
	return x * x;
}

/* get nearest point on the line (p1, p2) to p3, and save the result in p
*/
static void nearest_point(REBXYF *p1, REBXYF *p2, REBXYF *p3, REBXYF *p)
{
	double d = square(p1->x - p2->x) + square(p1->y - p2->y);
	p->y = ((p3->x - p1->x) * (p2->x - p1->x) * (p2->y - p1->y)
			+ square(p2->x - p1->x) * p1->y
			+ square(p1->y - p2->y) * p3->y) / d;
	p->x = ((p3->y - p1->y) * (p2->y - p1->y) * (p2->x - p1->x)
			+ square(p2->y - p1->y) * p1->x
			+ square(p1->x - p2->x) * p3->x) / d;
}

void rebdrw_triangle(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBCNT c1, REBCNT c2, REBCNT c3, REBDEC dilation)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGcolor cr1, cr2, cr3;

	if (c1 == 0) {// Gouraud shading is off
		BEGIN_NVG_PATH(ctx);
		nvgMoveTo(ctx->nvg, p1.x, p1.y);
		nvgLineTo(ctx->nvg, p2.x, p2.y);
		nvgLineTo(ctx->nvg, p3.x, p3.y);
		nvgClosePath(ctx->nvg);
		END_NVG_PATH(ctx);
	} else {
		cr1 = REBCNT_NVG_COLOR(c1);
		cr2 = REBCNT_NVG_COLOR(c2);
		cr3 = REBCNT_NVG_COLOR(c3);

		if (c1 == c2 == c3) { /* all vertices have the same color */
			nvgSave(ctx->nvg);

			nvgFillColor(ctx->nvg, cr1);

			BEGIN_NVG_PATH(ctx);
			nvgMoveTo(ctx->nvg, p1.x, p1.y);
			nvgLineTo(ctx->nvg, p2.x, p2.y);
			nvgLineTo(ctx->nvg, p3.x, p3.y);
			nvgClosePath(ctx->nvg);
			END_NVG_PATH(ctx);

			nvgRestore(ctx->nvg);
		} else if (c1 == c2 || c2 == c3 || c1 == c3) { /* two of them have the same color */
			NVGpaint paint;
			REBXYF p;
			if (c1 == c2) {
				nearest_point(&p1, &p2, &p3, &p);
				paint = nvgLinearGradient(ctx->nvg, p.x, p.y, p3.x, p3.y, cr1, cr3);
			} else if (c2 == c3) {
				nearest_point(&p2, &p3, &p1, &p);
				paint = nvgLinearGradient(ctx->nvg, p.x, p.y, p1.x, p1.y, cr2, cr1);
			} else { /* c1 == c3 */
				nearest_point(&p1, &p3, &p2, &p);
				paint = nvgLinearGradient(ctx->nvg, p.x, p.y, p2.x, p2.y, cr1, cr2);
			}

			nvgSave(ctx->nvg);

			nvgFillPaint(ctx->nvg, paint);

			BEGIN_NVG_PATH(ctx);
			nvgMoveTo(ctx->nvg, p1.x, p1.y);
			nvgLineTo(ctx->nvg, p2.x, p2.y);
			nvgLineTo(ctx->nvg, p3.x, p3.y);
			nvgClosePath(ctx->nvg);
			END_NVG_PATH(ctx);

			nvgRestore(ctx->nvg);
		} else { /* every vertex has a different color */
			NVGpaint paint;
			REBXYF p;
			if (ctx->tmp_layer == NULL) {
				ctx->tmp_layer = nvgCreateLayer(ctx->nvg, ctx->ww, ctx->wh, 0);
			}

			// blend mode could be changed
			nvgSave(ctx->nvg);

			//printf("begin a temporary win_layer: %d\n", __LINE__);
			nvgBeginLayer(ctx->nvg, ctx->tmp_layer);
			nvgBlendMode(ctx->nvg, NVG_LIGHTER);

			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

			nvgBeginPath(ctx->nvg);
			nvgMoveTo(ctx->nvg, p1.x, p1.y);
			nvgLineTo(ctx->nvg, p2.x, p2.y);
			nvgLineTo(ctx->nvg, p3.x, p3.y);
			nvgClosePath(ctx->nvg);

			nearest_point(&p1, &p2, &p3, &p);
			paint = nvgLinearGradient(ctx->nvg, p.x, p.y, p3.x, p3.y, nvgTransRGBAf(nvgRGB(0, 0, 0), cr3.a), cr3);
			nvgFillPaint(ctx->nvg, paint);
			nvgFill(ctx->nvg);

			nearest_point(&p2, &p3, &p1, &p);
			paint = nvgLinearGradient(ctx->nvg, p.x, p.y, p1.x, p1.y, nvgTransRGBAf(nvgRGB(0, 0, 0), cr1.a), cr1);
			nvgFillPaint(ctx->nvg, paint);
			nvgFill(ctx->nvg);

			nearest_point(&p1, &p3, &p2, &p);
			paint = nvgLinearGradient(ctx->nvg, p.x, p.y, p2.x, p2.y, nvgTransRGBAf(nvgRGB(0, 0, 0), cr2.a), cr2);
			nvgFillPaint(ctx->nvg, paint);
			nvgFill(ctx->nvg);

			if (ctx->stroke) {
				nvgStroke(ctx->nvg);
			}
			//printf("end of temporary win_layer: %d\n", __LINE__);
			nvgEndLayer(ctx->nvg, ctx->tmp_layer);

			PAINT_LAYER_FULL(ctx, ctx->tmp_layer, NVG_SOURCE_OVER);

			nvgFlush(ctx->nvg);

			nvgRestore(ctx->nvg);
		}
	}
}

//SHAPE functions
void rebshp_arc(void* gr, REBCNT rel, REBXYF p, REBXYF r, REBDEC ang, REBINT positive, REBINT large)
{
	float x, y;
	REBXYF c;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + p.x : p.x;
	y = ctx->last_y;

	if (r.x == r.y) {
		//nvgArc(ctx->nvg, c.x, c.y, r.x, ang, ang, positive? NVG_CCW : NVG_CW);
	} else {
		// elliptical arc
		// FIXME
	}
	printf("FIXME: %d\n", __LINE__);
	ctx->last_x = x;
	ctx->last_y = y;
}

void rebshp_close(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgClosePath(ctx->nvg);
}

void rebshp_curv(void* gr, REBCNT rel, REBXYF p2, REBXYF p3)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	REBXYF p1 = {ctx->last_x, ctx->last_y};

	rebshp_curve(gr, 0, p1, p2, p3);
}

void rebshp_curve(void* gr, REBCNT rel, REBXYF p1,REBXYF p2, REBXYF p3)
{
	float x1, y1;
	float x2, y2;
	float x3, y3;

	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	if (rel) {
		nvgBezierTo(ctx->nvg, ctx->last_x + p1.x, ctx->last_y + p1.y,
					ctx->last_x + p2.x, ctx->last_y + p2.y,
					ctx->last_x + p3.x, ctx->last_y + p3.y);
		ctx->last_x += p3.x;
		ctx->last_y += p3.y;
	} else {
		nvgBezierTo(ctx->nvg, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
		ctx->last_x = p3.x;
		ctx->last_y = p3.y;
	}
}

void rebshp_hline(void* gr, REBCNT rel, REBDEC x)
{
	float y;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + x : x;
	y = ctx->last_y;

	ctx->last_x = x;
	ctx->last_y = y;

	nvgLineTo(ctx->nvg, x, y);
}

void rebshp_line(void* gr, REBCNT rel, REBXYF p)
{
	float x, y;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + p.x : p.x;
	y = rel? ctx->last_y + p.y : p.y;

	ctx->last_x = x;
	ctx->last_y = y;

	nvgLineTo(ctx->nvg, x, y);
}

void rebshp_move(void* gr, REBCNT rel, REBXYF p)
{
	float x, y;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + p.x : p.x;
	y = rel? ctx->last_y + p.y : p.y;

	ctx->last_x = x;
	ctx->last_y = y;

	nvgMoveTo(ctx->nvg, x, y);
}

void rebshp_open(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgBeginPath(ctx->nvg);
}

void rebshp_vline(void* gr, REBCNT rel, REBDEC y)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	float x;

	x = ctx->last_x;
	y = rel? ctx->last_y + y : y;

	ctx->last_x = x;
	ctx->last_y = y;

	nvgLineTo(ctx->nvg, x, y);
}

void rebshp_qcurv(void* gr, REBCNT rel, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	REBXYF p1 = {ctx->last_x, ctx->last_y};

	nvgQuadTo(ctx->nvg, p1.x, p1.y, p.x, p.y);
	ctx->last_x = p.x;
	ctx->last_y = p.y;
}

void rebshp_qcurve(void* gr, REBCNT rel, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	if (rel) {
		nvgQuadTo(ctx->nvg, ctx->last_x + p1.x, ctx->last_y + p1.y,
				  ctx->last_x + p2.x, ctx->last_y + p2.y);
		ctx->last_x += p2.x;
		ctx->last_y += p2.y;
	} else {
		nvgQuadTo(ctx->nvg, p1.x, p1.y, p2.x, p2.y);
		ctx->last_x = p2.x;
		ctx->last_y = p2.y;
	}
}


void rebdrw_to_image(REBYTE *image, REBINT w, REBINT h, REBSER *block)
{
#if 0
	REBCEC ctx;
	REBSER *args = 0;

	agg_graphics::ren_buf rbuf_win(image, w, h, w * 4);
	agg_graphics::pixfmt pixf_win(rbuf_win);
	agg_graphics::ren_base rb_win(pixf_win);

	agg_graphics* graphics = new agg_graphics(&rbuf_win, w, h, 0, 0);


	ctx.envr = graphics;
	ctx.block = block;
	ctx.index = 0;

	RL_DO_COMMANDS(block, 0, &ctx);

	graphics->agg_render(rb_win);

	delete graphics;
#endif
}

void rebdrw_gob_color(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{
	REBYTE* color = (REBYTE*)&GOB_CONTENT(gob);
	if (ctx == NULL) return;

	nvgSave(ctx->nvg);
	nvgRect(ctx->nvg, clip_top.x, clip_top.y, clip_bottom.x - clip_top.x, clip_bottom.y - clip_top.y);
	nvgFillColor(ctx->nvg, nvgRGBA(color[C_R], color[C_G], color[C_B], color[C_A]));
	nvgFill(ctx->nvg);
	nvgRestore(ctx->nvg);
}

void rebdrw_gob_image(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{
	struct rebol_series* img = (struct rebol_series*)GOB_CONTENT(gob);
	int w = IMG_WIDE(img);
	int h = IMG_HIGH(img);
	NVGcontext *nvg = NULL;

	REBINT paint_mode = (GOB_ALPHA(gob) == 255) ? NVG_COPY : NVG_SOURCE_OVER;
	REBXYF image_size = {w, h};
	REBXYF clip_oft = {clip_top.x, clip_top.y};
	REBXYF clip_size = {clip_bottom.x - clip_top.x, clip_bottom.y - clip_bottom.y};

	if (ctx == NULL) return;
	nvg = ctx->nvg;

	int image = nvgCreateImageRGBA(nvg, w, h, 0, GOB_BITMAP(gob));

	nvgSave(nvg);

	paint_image(ctx, image, paint_mode, GOB_ALPHA(gob) / 255.0f,
				clip_oft, image_size,
				clip_oft, clip_size);

	nvgFlush(nvg);
	nvgDeleteImage(nvg, image);

	nvgRestore(nvg);
}


void rebdrw_gob_draw(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{
	REBINT result;
	REBCEC cec_ctx;
	REBSER *block = (REBSER *)GOB_CONTENT(gob);
	NVGpaint paint;
	NVGlayer *layer;

	REBXYF clip_oft = {clip_top.x, clip_top.y};
	REBXYF clip_size = {
		clip_bottom.x - clip_top.x,
		clip_bottom.y - clip_top.y
	};

	if (ctx == NULL) return;

	ctx->offset_x = abs_oft.x;
	ctx->offset_y = abs_oft.y;

	ctx->clip_x = clip_oft.x;
	ctx->clip_y = clip_oft.y;
	ctx->clip_w = clip_size.x;
	ctx->clip_h = clip_size.y;

	cec_ctx.envr = ctx;
	cec_ctx.block = block;
	cec_ctx.index = 0;

	nvgSave(ctx->nvg);

	nvgReset(ctx->nvg);
	nvgScissor(ctx->nvg, clip_oft.x, clip_oft.y, clip_size.x, clip_size.y);
	printf("gob %p abs offset: (%d, %d)\n", gob, abs_oft.x, abs_oft.y);
	printf("scissor for gob %p is: (%f, %f) & (%f, %f)\n", gob,
		   clip_oft.x, clip_oft.y, clip_size.x, clip_size.y);
	nvgTranslate(ctx->nvg, abs_oft.x, abs_oft.y);
	nvgStrokeColor(ctx->nvg, nvgRGB(255, 255, 255)); //default stroke color
	RL_DO_COMMANDS(block, 0, &cec_ctx);

	nvgRestore(ctx->nvg);
}

static int create_layers(REBDRW_CTX *ctx, REBINT w, REBINT h)
{
	if (ctx == NULL) return -1;
	ctx->win_layer = nvgCreateLayer(ctx->nvg, w, h, 0);
	if (ctx->win_layer == NULL) return -1;
	ctx->gob_layer = nvgCreateLayer(ctx->nvg, w, h, 0);
	if (ctx->gob_layer == NULL) {
		nvgDeleteLayer(ctx->nvg, ctx->win_layer);
		return -1;
	}
	ctx->tmp_layer = NULL;

	//printf("Created a NVG context at: %p, w: %d, h: %d\n", ctx->nvg, w, h);
	ctx->pixel_ratio = 1.0;	/* FIXME */

	/* initialize the GL context */
	glViewport(0, 0, w, h);

	//printf("begin win_layer: %d\n", __LINE__);
	nvgBeginLayer(ctx->nvg, ctx->win_layer);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	printf("end of win_layer: %d\n", __LINE__);
	nvgEndLayer(ctx->nvg, ctx->win_layer);
}

static void delete_layers(REBDRW_CTX *ctx)
{
	nvgDeleteLayer(ctx->nvg, ctx->win_layer);
	ctx->win_layer = NULL;
	nvgDeleteLayer(ctx->nvg, ctx->gob_layer);
	ctx->gob_layer = NULL;
	if (ctx->tmp_layer) {
		nvgDeleteLayer(ctx->nvg, ctx->tmp_layer);
		ctx->tmp_layer = NULL;
	}
}

REBDRW_CTX* rebdrw_create_context(REBINT w, REBINT h)
{
	REBDRW_CTX *ctx = (REBDRW_CTX*)malloc(sizeof(REBDRW_CTX));
	if (ctx == NULL) return NULL;
	ctx->ww = w;
	ctx->wh = h;
	ctx->nvg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
	ctx->fill_image = 0;
	ctx->stroke_image = 0;
	ctx->fill = FALSE;
	ctx->stroke = TRUE;
	ctx->last_x = 0;
	ctx->last_y = 0;

	if (ctx->nvg == NULL) {
		free(ctx);
		return NULL;
	}
	if (create_layers(ctx, w, h) < 0){
		free(ctx);
		nvgDeleteGL3(ctx->nvg);
		return NULL;
	}

	return ctx;
}

void rebdrw_resize_context(REBDRW_CTX *ctx, REBINT w, REBINT h)
{
	if (ctx == NULL) return;
	ctx->ww = w;
	ctx->wh = h;
	delete_layers(ctx);
	create_layers(ctx, w, h);
}

void rebdrw_destroy_context(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;

	delete_layers(ctx);

	if (ctx->fill_image != 0) {
		nvgDeleteImage(ctx->nvg, ctx->fill_image);
	}
	if (ctx->stroke_image != 0) {
		nvgDeleteImage(ctx->nvg, ctx->stroke_image);
	}

	nvgDeleteGL3(ctx->nvg);
	ctx->nvg = NULL;

	//printf("Destroyed a NVG context at: %p\n", ctx->nvg);

	free(ctx);
}

void rebdrw_begin_frame(REBDRW_CTX *ctx)
{
	//printf("begin frame: %d\n", __LINE__);
	if (ctx == NULL) return;
	nvgBeginFrame(ctx->nvg, ctx->ww, ctx->wh, ctx->pixel_ratio);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	//printf("begin win_layer: %d\n", __LINE__);
	nvgBeginLayer(ctx->nvg, ctx->win_layer);
	/* Do NOT clear the win_layer, whose content will be reused, as it might only update part of the screen */
}

void rebdrw_end_frame(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;
	nvgEndLayer(ctx->nvg, ctx->win_layer);
	//printf("End frame: %d\n", __LINE__);
	nvgEndFrame(ctx->nvg);
}

void rebdrw_blit_frame(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;
	nvgBeginFrame(ctx->nvg, ctx->ww, ctx->wh, ctx->pixel_ratio);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	PAINT_LAYER_FULL(ctx, ctx->win_layer, NVG_SOURCE_OVER);
	//printf("End frame: %d\n", __LINE__);
	nvgEndFrame(ctx->nvg);
}
