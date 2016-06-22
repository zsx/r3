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

#include <stdio.h>
#include <math.h>
#include "reb-host.h"

#include "nanovg.h"

#include "host-view.h"
#include "host-renderer.h"
#include "host-text-api.h"
#include "host-draw-api.h"
#include "host-ext-draw.h"

#define NANOVG_GL3_IMPLEMENTATION   // Use GL3 implementation.
#define NANOVG_FBO_VALID
#define GL_GLEXT_PROTOTYPES

#include <GL/glew.h>

#include "nanovg_gl.h"
#include "host-draw-api-nanovg.h"
#include "host-text-api-nanovg.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef NDEBUG
#define NOT_IMPLEMENTED printf("FIXME: %s, %d\n", __FUNCTION__, __LINE__)
#else
#define NOT_IMPLEMENTED
#endif

REBUPT RL_Series(REBSER *ser, REBCNT what);
#define IMG_WIDE(s) ((s) & 0xffff)
#define IMG_HIGH(s) ((s) >> 16)

static void nvgdrw_add_poly_vertex (void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgLineTo(ctx->nvg, p.x, p.y);

	//printf("new polygen vertex at (%f, %f)\n", p.x, p.y);

	//((agg_graphics*)gr)->agg_add_vertex(p.x, p.y);
}

static void nvgdrw_add_spline_vertex (void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgLineTo(ctx->nvg, p.x, p.y);

	//printf("new spline vertex at (%f, %f)\n", p.x, p.y);
}

static void nvgdrw_anti_alias(void* gr, REBINT mode)
{
	//always on
}

#define BEGIN_NVG_PATH(ctx) 					\
	do { 									\
		if (! ((ctx)->fill || (ctx)->stroke)) {	\
			/* printf("early return from line %d because of no fill or stroke\n", __LINE__); */\
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

/* convert from centeral angle to parameter angle and normalize it to [0, PI / 2] */

static void elliptical_aux(REBXYF r, REBDEC ang, float *x, float *y)
{
	float t;

	while (ang > 360) ang -= 360;
	while (ang < 0) ang += 360;

	if (ang == 90) {
		*x = 0;
		*y = r.y;
	} else if (ang == 270) {
		*x = 0;
		*y = -r.y;
	} else if (ang > 90 && ang < 270) {
		t = tan(nvgDegToRad(ang));
		*x = -r.x / sqrt(r.x * r.x / (r.y * r.y) * t * t + 1);
		*y = *x * t;
	} else {
		t = tan(nvgDegToRad(ang));
		*x = r.x / sqrt(r.x * r.x / (r.y * r.y) * t * t + 1);
		*y = *x * t;
	}
}

/* draw an elliptical arc from ang1 to ang2, clockwise
	ang1 < ang2
*/
static void elliptical_arc(NVGcontext *nvg, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed)
{
	/* Approximate an ellipticial arc with line segments */
	float ra, da, ang;
	float matrix[6];
	float x, y;

	if (ang2 > ang1 + 360) ang2 = ang1 + 360;
	if (ang2 < ang1 - 360) ang2 = ang1 - 360;

	nvgCurrentTransform(nvg, matrix);
	ra = (r.x * matrix[0] + r.y * matrix[3]) / 2;
	da = acos(ra / (ra + 0.125)) * 180 / M_PI;

	if (ang2 < ang1) da = -da;

	//printf("da: %f\n", da);
	elliptical_aux(r, ang1, &x, &y);

	if (closed) {
		nvgLineTo(nvg, c.x + x, c.y + y);
	} else {
		nvgMoveTo(nvg, c.x + x, c.y + y);
	}

	for (ang = ang1 + da; (da > 0 && ang <= ang2) || ( da < 0 && ang >= ang2); ang += da) {
		elliptical_aux(r, ang, &x, &y);
		nvgLineTo(nvg, c.x + x, c.y + y);
	}

	if ((da > 0 && ang < ang2 + da)
		|| (da < 0 && ang > ang2 + da)){
		elliptical_aux(r, ang2, &x, &y);
		nvgLineTo(nvg, c.x + x, c.y + y);
	}
}

static void nvgdrw_arc(void* gr, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGcontext *nvg = ctx->nvg;
	float x0, y0;

	if (ang1 >= ang2) return;

	x0 = c.x + r.x * cos(nvgDegToRad(ang1));
	y0 = c.y + r.y * sin(nvgDegToRad(ang1));

	BEGIN_NVG_PATH(ctx);
	if (closed) {
		nvgMoveTo(nvg, c.x, c.y);
		elliptical_arc(nvg, c, r, ang1, ang2, closed);
		nvgClosePath(nvg);
	} else {
		elliptical_arc(nvg, c, r, ang1, ang2, closed);
	}
	END_NVG_PATH(ctx);
}

static void nvgdrw_arrow(void* gr, REBXYF mode, REBCNT col)
{
	//((agg_graphics*)gr)->agg_arrows((col) ? (REBYTE*)&col : NULL, (REBINT)mode.x, (REBINT)mode.y);
}

static void nvgdrw_begin_poly (void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p.x, p.y);
	//printf("new polygen at: (%f, %f)\n", p.x, p.y);
}

static void nvgdrw_begin_spline (void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p.x, p.y);
	//printf("new polygen at: (%f, %f)\n", p.x, p.y);
}

static void nvgdrw_box(void* gr, REBXYF p1, REBXYF p2, REBDEC r)
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

static void nvgdrw_circle(void* gr, REBXYF p, REBXYF r)
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

static void nvgdrw_clip(void* gr, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgScissor(ctx->nvg, ctx->clip_x, ctx->clip_y, ctx->clip_w, ctx->clip_h);
	nvgIntersectScissor(ctx->nvg, p1.x, p1.y, p2.x - p1.x, p2.y - p1.y);
}

static void nvgdrw_curve3(void* gr, REBXYF p1, REBXYF p2, REBXYF p3)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p1.x, p1.y);
	nvgQuadTo(ctx->nvg, p2.x, p2.y, p3.x, p3.y);
	END_NVG_PATH(ctx);
}

static void nvgdrw_curve4(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBXYF p4)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgMoveTo(ctx->nvg, p1.x, p1.y);
	nvgBezierTo(ctx->nvg, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y);
	END_NVG_PATH(ctx);
}

static REBINT nvgdrw_effect(void* gr, REBXYF* p1, REBXYF* p2, REBSER* block)
{
	return 0;
}

static void nvgdrw_ellipse(void* gr, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	BEGIN_NVG_PATH(ctx);
	nvgEllipse(ctx->nvg, p1.x + p2.x/2, p1.y + p2.y/2, p2.x / 2, p2.y / 2);
	END_NVG_PATH(ctx);
}

static void nvgdrw_end_poly (void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgClosePath(ctx->nvg);
	END_NVG_PATH(ctx);
	//printf("polygen done\n");
}

static void nvgdrw_end_spline (void* gr, REBINT step, REBINT closed)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	//printf("spline step: %d\n", step);
	if (step == 0) {
		if (closed) {
			nvgClosePath(ctx->nvg);
		}
		END_NVG_PATH(ctx);
		return;
	}
	//printf("spline done, FIXME\n");
	printf("FIXME: %s, %d\n", __FUNCTION__, __LINE__);
}

static void nvgdrw_fill_pen(void* gr, REBCNT col)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	if (col) {
		nvgFillColor(ctx->nvg, REBCNT_NVG_COLOR(col));
		ctx->fill = TRUE;
	} else {
		ctx->fill = FALSE;
	}
	ctx->fill_color = col;
}

static void nvgdrw_fill_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGpaint paint;

	if (ctx->fill_image != 0) {
		nvgFlush(ctx->nvg);
		nvgDeleteImage(ctx->nvg, ctx->fill_image);
	}

	ctx->fill_image = nvgCreateImageRGBA(ctx->nvg, w, h, 0, NULL, img);
	paint = nvgImagePattern(ctx->nvg, 0, 0, w, h, 0, ctx->fill_image, 1);
	nvgFillPaint(ctx->nvg, paint);
}

static void nvgdrw_fill_rule(void* gr, REBINT mode)
{
	if (mode != W_DRAW_EVEN_ODD) {
		printf("FIXME: %s, %d\n", __FUNCTION__, __LINE__);
	}
	//   if (mode >= W_DRAW_EVEN_ODD && mode <= W_DRAW_NON_ZERO)
	//      ((agg_graphics*)gr)->agg_fill_rule((agg::filling_rule_e)mode);
}

static void nvgdrw_gamma(void* gr, REBDEC gamma)
{
	printf("FIXME: %s, %d\n", __FUNCTION__, __LINE__);
	//((agg_graphics*)gr)->agg_set_gamma(gamma);
}

static void nvgdrw_gradient_pen(void* gr, REBINT gradtype, REBINT mode, REBXYF oft, REBXYF range, REBDEC angle, REBXYF scale, REBSER* colors)
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
	printf("FIXME: %s, %d\n", __FUNCTION__, __LINE__);
}

static void nvgdrw_invert_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	float xform[6], inv[6];

	nvgCurrentTransform(ctx->nvg, xform);
	nvgTransformInverse(inv, xform);
	nvgTransform(ctx->nvg, inv[0], inv[1], inv[2], inv[3], inv[4], inv[5]);
}

void paint_image(REBDRW_CTX *ctx, int image, REBINT mode, float alpha,
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

static void nvgdrw_image(void* gr, REBYTE* img, REBINT w, REBINT h,REBXYF offset)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	REBXYF image_size = {w, h};

	int image;
   
	image = nvgCreateImageRGBA(ctx->nvg, w, h, ctx->key_color_enabled ? NVG_IMAGE_KEY_COLOR : 0, &ctx->key_color, img);
	nvgSave(ctx->nvg);

	//printf("size: (%d, %d) at (%f, %f)\n", w, h, offset.x, offset.y);
	paint_image(ctx, image, NVG_COPY, 1.0f, offset, image_size, offset, image_size);

	if (ctx->img_border) {
		nvgStroke(ctx->nvg);
	}

	nvgFlush(ctx->nvg);

	nvgDeleteImage(ctx->nvg, image);
	nvgRestore(ctx->nvg);
}

static void nvgdrw_image_filter(void* gr, REBINT type, REBINT mode, REBDEC blur)
{
	//((agg_graphics*)gr)->agg_image_filter(type, mode, blur);
}

static void nvgdrw_image_options(void* gr, REBOOL keyColEnabled, REBCNT keyCol, REBINT border)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	ctx->key_color_enabled = !!keyColEnabled;
	if (keyColEnabled) {
		ctx->key_color = REBCNT_NVG_COLOR(keyCol);
	}
	ctx->img_border = !!border;
}

static void nvgdrw_image_pattern(void* gr, REBINT mode, REBXYF offset, REBXYF size)
{
#if 0
	if (mode)
		((agg_graphics*)gr)->agg_image_pattern(mode,offset.x,offset.y,size.x,size.y);
	else
		((agg_graphics*)gr)->agg_image_pattern(0,0,0,0,0);
#endif
	printf("FIXME: %s, %d\n", __FUNCTION__, __LINE__);
}

static void nvgdrw_image_scale(void* gr, REBYTE* img, REBINT w, REBINT h, REBSER* points)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	int image = -1;
	NVGpaint paint;

	RXIARG a;
	REBXYF p[4];
	REBCNT type;
	REBCNT n, len = 0;

//	printf("scaling image size: (%d, %d)\n", w, h);
	for (n = 0; (type = RL_GET_VALUE(points, n, &a)); n++) {
		if (type == RXT_PAIR){
			REBXYF tmp = RXI_LOG_PAIR(a);
			p[len] = tmp;
			if (++len == 4) break;
		}
	}

	if (!len) return;

	image = nvgCreateImageRGBA(ctx->nvg, w, h, ctx->key_color_enabled ? NVG_IMAGE_KEY_COLOR : 0, &ctx->key_color, img);

	switch (len) {
	case 2:
		nvgPaintImage(ctx->nvg, image,
            p[0].x, p[1].y,
            p[1].x, p[1].y,
            p[0].x, p[0].y,
            p[1].x, p[0].y);
		break;
	case 3:
        nvgPaintImage(ctx->nvg, image,
            p[0].x, p[2].y,
            p[2].x, p[2].y,
            p[0].x, p[0].y,
            p[1].x, p[1].y);
		break;
	case 4:
        nvgPaintImage(ctx->nvg, image,
            p[3].x, p[3].y,
            p[2].x, p[2].y,
            p[0].x, p[0].y,
            p[1].x, p[1].y);
		break;
	}
	nvgFlush(ctx->nvg);

	nvgDeleteImage(ctx->nvg, image);
}

static void nvgdrw_line(void* gr, REBXYF* p, REBCNT n)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	REBCNT i;

	if (!ctx->stroke || p == NULL || n < 2) return;

	nvgBeginPath(ctx->nvg);
	nvgMoveTo(ctx->nvg, p[0].x, p[0].y);
	
	for (i = 1; i < n; i ++)
		nvgLineTo(ctx->nvg, p[i].x, p[i].y);

	nvgStroke((ctx)->nvg);
}

static void nvgdrw_line_cap(void* gr, REBINT mode)
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

static void nvgdrw_line_join(void* gr, REBINT mode)
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

static void nvgdrw_line_pattern(void* gr, REBCNT col, REBDEC* patterns)
{
	//((agg_graphics*)gr)->agg_line_pattern((col) ? (REBYTE*)&col : NULL, patterns);
	NOT_IMPLEMENTED;
}

static void nvgdrw_line_width(void* gr, REBDEC width, REBINT mode)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgStrokeWidth(ctx->nvg, width, mode == 0? NVG_LW_SCALED : NVG_LW_FIXED);
}

static void nvgdrw_matrix(void* gr, REBSER* mtx)
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

static void nvgdrw_pen(void* gr, REBCNT col)
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

static void nvgdrw_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	NVGpaint paint;

	if (ctx->stroke_image != 0) {
		nvgFlush(ctx->nvg);
		nvgDeleteImage(ctx->nvg, ctx->stroke_image);
	}

	ctx->stroke_image = nvgCreateImageRGBA(ctx->nvg, w, h, 0, NULL, img);
	paint = nvgImagePattern(ctx->nvg, 0, 0, w, h, 0, ctx->stroke_image, 1);
	nvgStrokePaint(ctx->nvg, paint);
}

static void nvgdrw_pop_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgRestore(ctx->nvg);
}

static void nvgdrw_push_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgSave(ctx->nvg);
}

static void nvgdrw_reset_gradient_pen(void* gr)
{
}

static void nvgdrw_reset_matrix(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgResetTransform(ctx->nvg);
	nvgTranslate(ctx->nvg, ctx->offset_x, ctx->offset_y);
}

static void nvgdrw_rotate(void* gr, REBDEC ang)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgRotate(ctx->nvg, nvgDegToRad(ang));
}

static void nvgdrw_scale(void* gr, REBXYF sc)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgScale(ctx->nvg, sc.x, sc.y);
}

static void nvgdrw_skew(void* gr, REBXYF angle)
{
	//REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	//nvgScale(ctx->nvg, sc.x, sc.y);
	NOT_IMPLEMENTED;
}
static void nvgdrw_text(void* gr, REBINT mode, REBXYF* p1, REBXYF* p2, REBSER* block)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
//	REBXYF oft = { p1->x + ctx->offset_x, p1->y + ctx->offset_y};
	nvg_text(ctx, mode, p1, p2, block);
}

static void nvgdrw_transform(void* gr, REBDEC ang, REBXYF ctr, REBXYF sc, REBXYF oft)
{
	//nvgTransform(ctx->nvg, sc.x, matrix[1], sc.y, matrix[3], oft.x, oft.y);
	//((agg_graphics*)gr)->agg_transform(ang, ctr.x, ctr.y, sc.x, sc.y, oft.x, oft.y);
	NOT_IMPLEMENTED;
}

static void nvgdrw_translate(void* gr, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgTranslate(ctx->nvg, p.x, p.y);
}

static double square(double x)
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

static void nvgdrw_triangle(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBCNT c1, REBCNT c2, REBCNT c3, REBDEC dilation)
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
static void nvgshp_arc(void* gr, REBINT rel, REBXYF p, REBXYF r, REBDEC ang, REBINT positive, REBINT large)
{
	// See http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes


	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
    
	double x1 = ctx->last_x;
	double y1 = ctx->last_y;
    
	double x2, y2;
    
	double dx, dy;

	double x1_p, y1_p;

	double cx_p, cy_p;
	double cx, cy;

	double cos_a = cos(nvgDegToRad(ang));
	double sin_a = sin(nvgDegToRad(ang));

	double theta, delta;

	x2 = rel? ctx->last_x + p.x : p.x;
	y2 = rel? ctx->last_y + p.y : p.y;

	if (r.x == 0 || r.y == 0) {
		/* degenerated to a straight line */
		nvgLineTo(ctx->nvg, x2, y2);
		goto done;
	}

	if (x1 == x2 && y1 == y2) goto done;

	/* step 1 compute (x1_p, y1_p) */
	dx = (x1 - x2) / 2;
	dy = (y1 - y2) / 2;

	x1_p = (cos_a * dx + sin_a * dy);
	y1_p = (-sin_a * dx + cos_a * dy);

	/* check radii */
	{
		double radii_check;
		if (r.x < 0) r.x = -r.x;
		if (r.y < 0) r.y = -r.y;

		radii_check = x1_p * x1_p / (r.x * r.x) + y1_p * y1_p / (r.y * r.y);

		if (radii_check > 1) {
			double s = sqrt(radii_check);
			r.x *= s;
			r.y *= s;
		}
	}

	/* step 2: compute (cx_p, cy_p) */
	{
		double sq;
		double rxs = r.x * r.x;
		double rys = r.y * r.y;

		double tmp = rxs * y1_p * y1_p + rys * x1_p * x1_p;

		int sign = (!!positive == !!large) ? -1 : 1;;

		sq = (rxs * rys - tmp) / tmp;
		sq = (sq < 0) ? 0 : sq;
		tmp = sign * sqrt(sq);
		cx_p = tmp * r.x * y1_p / r.y;
		cy_p = -tmp * r.y * x1_p / r.x;
	}

	/* step 3: compute (cx, cy) from (cx_p, cy_p) */
	cx = (cos_a * cx_p - sin_a * cy_p) + (x1 + x2) / 2;
	cy = (sin_a * cx_p + cos_a * cy_p) + (y1 + y2) / 2;

	/* compute starting angle and the extent */
	{
		double ux, uy, vx, vy, p, n;
		int sign;

		ux = (x1_p - cx_p) / r.x;
		uy = (y1_p - cy_p) / r.y;
		vx = (-x1_p - cx_p) / r.x;
		vy = (-y1_p - cy_p) / r.y;
		n = sqrt(ux * ux + uy * uy);
		p = ux; // 1 * ux + 0 * vy;
		sign = (uy < 0) ? -1 : 1;
		theta = nvgRadToDeg(sign * acos(p / n));

		// angle extent
		n = sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
		p = ux * vx + uy * vy;

		sign = (ux * vy - uy * vx < 0) ? -1 : 1;
		delta = nvgRadToDeg(sign * acos(p / n));

		if (!positive && delta > 0)
			delta -= 360;
		else if (positive && delta < 0)
			delta += 360;
	}

	/* draw the arc */
	{
		REBXYF c = { cx, cy };
		elliptical_arc(ctx->nvg, c, r, theta, theta + delta, TRUE);
	}

done:
	ctx->last_x = x2;
	ctx->last_y = y2;

//	printf("current point after arc: (%f, %f)\n", x2, y2);

	ctx->last_shape_cmd = rel? 'a' : 'A';
}

static void nvgshp_close(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	nvgClosePath(ctx->nvg);
	ctx->last_shape_cmd = 'z';
	//printf("%s, %d\n", __func__, __LINE__);
}

static void nvgshp_end(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	END_NVG_PATH(ctx);
	//printf("%s, %d\n", __func__, __LINE__);
}

static void nvgshp_curve(void* gr, REBINT rel, REBXYF p1, REBXYF p2, REBXYF p3)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	if (rel) {
		p1.x += ctx->last_x;
		p1.y += ctx->last_y;

		p2.x += ctx->last_x;
		p2.y += ctx->last_y;

		p3.x += ctx->last_x;
		p3.y += ctx->last_y;
	}

	nvgBezierTo(ctx->nvg, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);

	ctx->last_x = p3.x;
	ctx->last_y = p3.y;

	ctx->last_control_x = p2.x;
	ctx->last_control_y = p2.y;

	ctx->last_shape_cmd = rel? 'c' : 'C';

}

static void nvgshp_curv(void* gr, REBINT rel, REBXYF p2, REBXYF p3)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	REBXYF p1;
   
	if (ctx->last_shape_cmd != 's'
		&& ctx->last_shape_cmd != 'S'
		&& ctx->last_shape_cmd != 'c'
		&& ctx->last_shape_cmd != 'C') {
		p1.x = ctx->last_x;
		p1.y = ctx->last_y;
	} else { /* reflection of last control point to the current point */
		p1.x = 2 * ctx->last_x - ctx->last_control_x;
		p1.y = 2 * ctx->last_y - ctx->last_control_y;
	}

	if (rel) {
		p2.x += ctx->last_x;
		p2.y += ctx->last_y;

		p3.x += ctx->last_x;
		p3.y += ctx->last_y;
	}

	nvgshp_curve(gr, 0, p1, p2, p3);

	ctx->last_control_x = p2.x;
	ctx->last_control_y = p2.y;

	ctx->last_shape_cmd = rel? 's' : 'S';
}

static void nvgshp_hline(void* gr, REBINT rel, REBDEC x)
{
	float y;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + x : x;
	y = ctx->last_y;

	ctx->last_x = x;
	//ctx->last_y = y;

	nvgLineTo(ctx->nvg, x, y);
	ctx->last_shape_cmd = rel? 'h' : 'H';

//	printf("point after hline: (%f, %f)\n", ctx->last_x, ctx->last_y);
}

static void nvgshp_line(void* gr, REBINT rel, REBXYF p)
{
	float x, y;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + p.x : p.x;
	y = rel? ctx->last_y + p.y : p.y;

	ctx->last_x = x;
	ctx->last_y = y;

	nvgLineTo(ctx->nvg, x, y);
	ctx->last_shape_cmd = rel? 'l' : 'L';
	//printf("%s, %d: %fx%f\n", __func__, __LINE__, x, y);
}

static void nvgshp_move(void* gr, REBINT rel, REBXYF p)
{
	float x, y;
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;

	x = rel? ctx->last_x + p.x : p.x;
	y = rel? ctx->last_y + p.y : p.y;

	ctx->last_x = x;
	ctx->last_y = y;

	nvgMoveTo(ctx->nvg, x, y);
	ctx->last_shape_cmd = rel? 'm' : 'M';
//	printf("%s, %d: %fx%f\n", __FUNCTION__, __LINE__, x, y);
}

static void nvgshp_begin(void* gr)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
//	printf("%s, %d\n", __FUNCTION__, __LINE__);
	ctx->last_x = 0;
	ctx->last_y = 0;
	nvgBeginPath(ctx->nvg);
}

static void nvgshp_vline(void* gr, REBINT rel, REBDEC y)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	float x;

	x = ctx->last_x;
	y = rel? ctx->last_y + y : y;

	//ctx->last_x = x;
	ctx->last_y = y;

	nvgLineTo(ctx->nvg, x, y);
	ctx->last_shape_cmd = rel? 'v' : 'V';
}

static void nvgshp_qcurv(void* gr, REBINT rel, REBXYF p)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	REBXYF p1;

	if (ctx->last_shape_cmd != 'q'
		&& ctx->last_shape_cmd != 'Q'
		&& ctx->last_shape_cmd != 't'
		&& ctx->last_shape_cmd != 'T') {
		p1.x = ctx->last_x;
		p1.y = ctx->last_y;
	} else {
		p1.x = 2 * ctx->last_x - ctx->last_control_x;
		p1.y = 2 * ctx->last_y - ctx->last_control_y;
	}

	if (rel) {
		p.x += ctx->last_x;
		p.y += ctx->last_y;
	}

	nvgQuadTo(ctx->nvg, p1.x, p1.y, p.x, p.y);
	ctx->last_x = p.x;
	ctx->last_y = p.y;
	ctx->last_shape_cmd = rel? 't' : 'T';
	ctx->last_control_x = p1.x;
	ctx->last_control_y = p1.y;
}

static void nvgshp_qcurve(void* gr, REBINT rel, REBXYF p1, REBXYF p2)
{
	REBDRW_CTX* ctx = (REBDRW_CTX *)gr;
	if (rel) {
		p1.x += ctx->last_x;
		p1.y += ctx->last_y;
		p2.x += ctx->last_x;
		p2.y += ctx->last_y;
	}

	nvgQuadTo(ctx->nvg, p1.x, p1.y, p2.x, p2.y);
	ctx->last_x = p2.x;
	ctx->last_y = p2.y;

	ctx->last_shape_cmd = rel? 'q' : 'Q';
	ctx->last_control_x = p1.x;
	ctx->last_control_y = p1.y;
}


static void nvgdrw_to_image(REBYTE *image, REBINT w, REBINT h, REBSER *block)
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
	NOT_IMPLEMENTED;
}

static void nvgdrw_gob_color(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{
	REBYTE* color = (REBYTE*)&GOB_CONTENT(gob);
	if (ctx == NULL) return;

	nvgSave(ctx->nvg);
	nvgReset(ctx->nvg);
	nvgBeginPath(ctx->nvg);
	nvgRect(ctx->nvg, clip_top.x, clip_top.y, clip_bottom.x - clip_top.x, clip_bottom.y - clip_top.y);
	nvgFillColor(ctx->nvg, nvgRGBA(color[C_R], color[C_G], color[C_B], GOB_ALPHA(gob) * color[C_A] / 255));
	nvgFill(ctx->nvg);
	nvgRestore(ctx->nvg);
}

static void nvgdrw_gob_image(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{
	struct rebol_series* img = (struct rebol_series*)GOB_CONTENT(gob);
    REBUPT size = RL_SERIES(img, RXI_SER_SIZE);
    int w = IMG_WIDE(size);
	int h = IMG_HIGH(size);
	NVGcontext *nvg = NULL;

	REBINT paint_mode = (GOB_ALPHA(gob) == 255) ? NVG_COPY : NVG_SOURCE_OVER;
	REBXYF image_size = {w, h};
	REBXYF clip_oft = {clip_top.x, clip_top.y};
	REBXYF clip_size = {clip_bottom.x - clip_top.x, clip_bottom.y - clip_bottom.y};

	if (ctx == NULL) return;
	nvg = ctx->nvg;

	int image = nvgCreateImageRGBA(nvg, w, h, 0, NULL, GOB_BITMAP(gob));

	nvgSave(nvg);
	nvgReset(nvg);

	paint_image(ctx, image, paint_mode, GOB_ALPHA(gob) / 255.0f,
				clip_oft, image_size,
				clip_oft, clip_size);

	nvgFlush(nvg);
	nvgDeleteImage(nvg, image);

	nvgRestore(nvg);
}


static void nvgdrw_gob_draw(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{
	REBCEC cec_ctx;
	REBSER *block = (REBSER *)GOB_CONTENT(gob);

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
	//printf("gob %p abs offset: (%d, %d)\n", gob, abs_oft.x, abs_oft.y);
	//printf("scissor for gob %p is: (%f, %f) & (%f, %f)\n", gob,
	//	   clip_oft.x, clip_oft.y, clip_size.x, clip_size.y);
	nvgTranslate(ctx->nvg, abs_oft.x, abs_oft.y);
	nvgStrokeColor(ctx->nvg, nvgRGB(255, 255, 255)); //default stroke color
	RL_DO_COMMANDS(block, 0, &cec_ctx);

	nvgRestore(ctx->nvg);
}

REBRDR_DRW draw_nanovg = {
	.rebdrw_add_poly_vertex = nvgdrw_add_poly_vertex,
	.rebdrw_add_spline_vertex = nvgdrw_add_spline_vertex,
	.rebdrw_anti_alias = nvgdrw_anti_alias,
	.rebdrw_arc = nvgdrw_arc,
	.rebdrw_arrow = nvgdrw_arrow,
	.rebdrw_begin_poly = nvgdrw_begin_poly,
	.rebdrw_begin_spline = nvgdrw_begin_spline,
	.rebdrw_box = nvgdrw_box,
	.rebdrw_circle = nvgdrw_circle,
	.rebdrw_clip = nvgdrw_clip,
	.rebdrw_curve3 = nvgdrw_curve3,
	.rebdrw_curve4 = nvgdrw_curve4,
	.rebdrw_ellipse = nvgdrw_ellipse,
	.rebdrw_end_poly = nvgdrw_end_poly,
	.rebdrw_end_spline = nvgdrw_end_spline,
	.rebdrw_fill_pen = nvgdrw_fill_pen,
	.rebdrw_fill_pen_image = nvgdrw_fill_pen_image,
	.rebdrw_fill_rule = nvgdrw_fill_rule,
	.rebdrw_gamma = nvgdrw_gamma,
	.rebdrw_gob_color = nvgdrw_gob_color,
	.rebdrw_gob_draw = nvgdrw_gob_draw,
	.rebdrw_gob_image = nvgdrw_gob_image,
	.rebdrw_gradient_pen = nvgdrw_gradient_pen,
	.rebdrw_invert_matrix = nvgdrw_invert_matrix,
	.rebdrw_image = nvgdrw_image,
	.rebdrw_image_filter = nvgdrw_image_filter,
	.rebdrw_image_options = nvgdrw_image_options,
	.rebdrw_image_scale = nvgdrw_image_scale,
	.rebdrw_image_pattern = nvgdrw_image_pattern,
	.rebdrw_line = nvgdrw_line,
	.rebdrw_line_cap = nvgdrw_line_cap,
	.rebdrw_line_join = nvgdrw_line_join,
	.rebdrw_line_pattern = nvgdrw_line_pattern,
	.rebdrw_line_width = nvgdrw_line_width,
	.rebdrw_matrix = nvgdrw_matrix,
	.rebdrw_pen = nvgdrw_pen,
	.rebdrw_pen_image = nvgdrw_pen_image,
	.rebdrw_pop_matrix = nvgdrw_pop_matrix,
	.rebdrw_push_matrix = nvgdrw_push_matrix,
	.rebdrw_reset_gradient_pen = nvgdrw_reset_gradient_pen,
	.rebdrw_reset_matrix = nvgdrw_reset_matrix,
	.rebdrw_rotate = nvgdrw_rotate,
	.rebdrw_scale = nvgdrw_scale,
	.rebdrw_to_image = nvgdrw_to_image,
	.rebdrw_skew = nvgdrw_skew,
	.rebdrw_text = nvgdrw_text,
	.rebdrw_transform = nvgdrw_transform,
	.rebdrw_translate = nvgdrw_translate,
	.rebdrw_triangle = nvgdrw_triangle,

	.rebshp_arc = nvgshp_arc,
	.rebshp_close = nvgshp_close,
	.rebshp_hline = nvgshp_hline,
	.rebshp_line = nvgshp_line,
	.rebshp_move = nvgshp_move,
	.rebshp_begin = nvgshp_begin,
	.rebshp_end = nvgshp_end,
	.rebshp_vline = nvgshp_vline,
	.rebshp_curv = nvgshp_curv,
	.rebshp_curve = nvgshp_curve,
	.rebshp_qcurv = nvgshp_qcurv,
	.rebshp_qcurve = nvgshp_qcurve
};