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
**  Title: DRAW and SHAPE dialect command dispatcher
**  Author: Richard Smolak, Carl Sassenrath
**  Purpose: Evaluates DRAW commands; calls graphics functions.
**  Tools: make-host-ext.r
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

#include <stdlib.h>
#include "reb-host.h"

//#include "agg-draw.h"
#include "host-view.h"
#include "host-renderer.h"
#include "host-draw-api.h"

#define INCLUDE_EXT_DATA
#include "host-ext-draw.h"
#include "host-ext-shape.h"

//***** Externs *****

//***** Locals *****

static u32* draw_ext_words;
static u32* shape_ext_words;

/***********************************************************************
**
*/	RXIEXT int RXD_Shape(int cmd, RXIFRM *frm, REBCEC *ctx)
/*
**		DRAW command dispatcher.
**
***********************************************************************/
{
//    Reb_Print("SHAPE called\n");
    REBCNT rel = 0;

	if (ctx == NULL && cmd != CMD_SHAPE_INIT_WORDS) {
		/* this is not called from SHOW */
		return RXR_ERROR;
	}

	switch (cmd) {

    case CMD_SHAPE_INIT_WORDS:
        shape_ext_words = RL_MAP_WORDS(RXA_SERIES(frm,1));
        break;

    case CMD_SHAPE_ARC_LIT:
        rel = 1;
    case CMD_SHAPE_ARC:
		{
			REBXYF r = RXA_LOG_PAIR(frm, 1); 
			REBXYF ang = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebshp_arc(
				ctx->envr,
				rel,
				r,
				ang,
				(RXA_TYPE(frm, 3) == RXT_DECIMAL) ? RXA_DEC64(frm, 3) : RXA_INT64(frm, 3),
				RL_FIND_WORD(shape_ext_words , RXA_WORD(frm, 4)) - W_SHAPE_NEGATIVE,
				RL_FIND_WORD(shape_ext_words , RXA_WORD(frm, 5)) - W_SHAPE_SMALL
			);
		}
        break;

    case CMD_SHAPE_CLOSE:
        rebol_renderer->draw->rebshp_close(ctx->envr);
        break;

    case CMD_SHAPE_CURV_LIT:
        rel = 1;
    case CMD_SHAPE_CURV:
        {
			RXIARG val[2];
			REBCNT type;
			REBCNT n, m = 0;
			REBSER blk = RXA_SERIES(frm, 1);

			for (n = 0; (type = RL_GET_VALUE(blk, n, &val[m])); n++) {
			    if (type == RXT_PAIR && ++m == 2) {
//                    rebol_renderer->draw->rebshp_curv(ctx->envr, rel, val[0].pair, val[1].pair);
				    REBXYF p1 = RXI_LOG_PAIR(val[0]);
				    REBXYF p2 =  RXI_LOG_PAIR(val[1]);
				    rebol_renderer->draw->rebshp_curv(ctx->envr, rel, p1, p2);
				    m = 0;
			    }
			}
        }
        break;

    case CMD_SHAPE_CURVE_LIT:
        rel = 1;
    case CMD_SHAPE_CURVE:
        {
			RXIARG val[3];
			REBCNT type;
			REBCNT n, m = 0;
			REBSER blk = RXA_SERIES(frm, 1);

			for (n = 0; (type = RL_GET_VALUE(blk, n, &val[m])); n++) {
                if (type == RXT_PAIR && ++m == 3) {
//                    rebol_renderer->draw->rebshp_curve(ctx->envr, rel, val[0].pair, val[1].pair, val[2].pair);
					REBXYF p1 = RXI_LOG_PAIR(val[0]);
					REBXYF p2 = RXI_LOG_PAIR(val[1]);
					REBXYF p3 = RXI_LOG_PAIR(val[2]);

					rebol_renderer->draw->rebshp_curve(ctx->envr, rel, p1, p2, p3);
                    m = 0;
                }
			}
        }
        break;

    case CMD_SHAPE_HLINE_LIT:
        rel = 1;
    case CMD_SHAPE_HLINE:
        rebol_renderer->draw->rebshp_hline(ctx->envr, rel, LOG_COORD_X((RXA_TYPE(frm, 1) == RXT_DECIMAL) ? RXA_DEC64(frm, 1) : RXA_INT64(frm, 1)));
        break;

    case CMD_SHAPE_LINE_LIT:
        rel = 1;
    case CMD_SHAPE_LINE:
        if (RXA_TYPE(frm, 1) == RXT_PAIR)
		{
			REBXYF p = RXA_LOG_PAIR(frm, 1);
            rebol_renderer->draw->rebshp_line(ctx->envr, rel, p);
		} else {
			RXIARG val;
			REBCNT type;
			REBCNT n;
			REBSER blk = RXA_SERIES(frm, 1);

			for (n = 0; (type = RL_GET_VALUE(blk, n, &val)); n++) {
				if (type == RXT_PAIR) {
//                    rebol_renderer->draw->rebshp_line(ctx->envr, rel, val.pair);
					REBXYF p = RXI_LOG_PAIR(val);
					rebol_renderer->draw->rebshp_line(ctx->envr, rel, p);
				}
			}
        }
        break;

    case CMD_SHAPE_MOVE_LIT:
        rel = 1;
    case CMD_SHAPE_MOVE:
		{
			REBXYF p = RXA_LOG_PAIR(frm, 1);
			rebol_renderer->draw->rebshp_move(ctx->envr, rel, p);
		}
		break;

    case CMD_SHAPE_QCURV_LIT:
        rel = 1;
    case CMD_SHAPE_QCURV:
		{
			REBXYF p = RXA_LOG_PAIR(frm, 1);
			rebol_renderer->draw->rebshp_qcurv(ctx->envr, rel, p);
		}
		break;

    case CMD_SHAPE_QCURVE_LIT:
        rel = 1;
    case CMD_SHAPE_QCURVE:
        {
			RXIARG val[2];
			REBCNT type;
			REBCNT n, m = 0;
			REBSER blk = RXA_SERIES(frm, 1);

			for (n = 0; (type = RL_GET_VALUE(blk, n, &val[m])); n++) {
			    if (type == RXT_PAIR && ++m == 2) {
//                    rebol_renderer->draw->rebshp_qcurve(ctx->envr, rel, val[0].pair, val[1].pair);
					REBXYF p1 = RXI_LOG_PAIR(val[0]);
					REBXYF p2 = RXI_LOG_PAIR(val[1]);

					rebol_renderer->draw->rebshp_qcurve(ctx->envr, rel, p1, p2);
                    m = 0;
			    }
			}
        }
        break;

    case CMD_SHAPE_VLINE_LIT:
        rel = 1;
    case CMD_SHAPE_VLINE:
        rebol_renderer->draw->rebshp_vline(ctx->envr, rel, LOG_COORD_Y((RXA_TYPE(frm, 1) == RXT_DECIMAL) ? RXA_DEC64(frm, 1) : RXA_INT64(frm, 1)));
        break;

    default:
		return RXR_NO_COMMAND;
	}

    return RXR_UNSET;
}

/***********************************************************************
**
*/	RXIEXT int RXD_Draw(int cmd, RXIFRM *frm, REBCEC *ctx)
/*
**		DRAW command dispatcher.
**
***********************************************************************/
{
	if (ctx == NULL && cmd != CMD_DRAW_INIT_WORDS) {
		/* this is not called from SHOW */
		return RXR_ERROR;
	}
	switch (cmd) {

    case CMD_DRAW_INIT_WORDS:
        draw_ext_words = RL_MAP_WORDS(RXA_SERIES(frm,1));
        break;
    case CMD_DRAW_ANTI_ALIAS:
        rebol_renderer->draw->rebdrw_anti_alias(ctx->envr, RXA_LOGIC(frm, 1));
        break;

	case CMD_DRAW_ARC:
		{
			REBXYF c = RXA_LOG_PAIR(frm, 1);
			REBXYF r = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebdrw_arc(
				ctx->envr,
				c,
				r,
				(RXA_TYPE(frm, 3) == RXT_DECIMAL) ? RXA_DEC64(frm, 3) : RXA_INT64(frm, 3),
				(RXA_TYPE(frm, 4) == RXT_DECIMAL) ? RXA_DEC64(frm, 4) : RXA_INT64(frm, 4),
				RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 5)) - W_DRAW_OPENED
			);
		}
		break;

	case CMD_DRAW_ARROW:
		rebol_renderer->draw->rebdrw_arrow(ctx->envr, RXA_PAIR(frm, 1), (RXA_TYPE(frm, 2) == RXT_NONE) ? 0 : RXA_COLOR_TUPLE(frm, 2));
		break;

	case CMD_DRAW_BOX:
		{
			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebdrw_box(ctx->envr, p1, p2, LOG_COORD_X((RXA_TYPE(frm, 3) == RXT_DECIMAL) ? RXA_DEC64(frm, 3) : RXA_INT64(frm, 3)));
		}
		break;

	case CMD_DRAW_CIRCLE:
		{
			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebdrw_circle(ctx->envr, p1, p2);
		}
		break;

	case CMD_DRAW_CLIP:
		{
			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebdrw_clip(ctx->envr, p1, p2);
		}
		break;

	case CMD_DRAW_CURVE:
		{
  			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			REBXYF p3 = RXA_LOG_PAIR(frm, 3);
			REBXYF p4 = RXA_LOG_PAIR(frm, 4);
			if (RXA_TYPE(frm, 4) == RXT_NONE)
				rebol_renderer->draw->rebdrw_curve3(ctx->envr, p1, p2, p3);
			else
				rebol_renderer->draw->rebdrw_curve4(ctx->envr, p1, p2, p3, p4);
		}
		break;

	case CMD_DRAW_ELLIPSE:
		{
			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebdrw_ellipse(ctx->envr, p1, p2);
		}
		break;

	case CMD_DRAW_FILL_PEN:
		{
   			//REBYTE* val;
			//REBCNT type;
			//REBSER* img;

        if (RXA_TYPE(frm, 1) == RXT_TUPLE)
            rebol_renderer->draw->rebdrw_fill_pen(ctx->envr, RXA_COLOR_TUPLE(frm, 1));
        else if (RXA_TYPE(frm, 1) == RXT_LOGIC && !RXA_LOGIC(frm,1))
            rebol_renderer->draw->rebdrw_fill_pen(ctx->envr, 0);
        else {
            rebol_renderer->draw->rebdrw_fill_pen_image(ctx->envr, RXA_IMAGE_BITS(frm,1), RXA_IMAGE_WIDTH(frm,1), RXA_IMAGE_HEIGHT(frm,1));
            }
        }
		break;

    case CMD_DRAW_FILL_RULE:
        rebol_renderer->draw->rebdrw_fill_rule(ctx->envr, RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 1)));
        break;

    case CMD_DRAW_GAMMA:
        rebol_renderer->draw->rebdrw_gamma(ctx->envr, (RXA_TYPE(frm, 1) == RXT_DECIMAL) ? RXA_DEC64(frm, 1) : RXA_INT64(frm, 1));
        break;

	case CMD_DRAW_GRAD_PEN:
		{
			REBXYF p3 = RXA_LOG_PAIR(frm, 3);
			REBXYF p4 = RXA_LOG_PAIR(frm, 4);
			if (RXA_TYPE(frm, 7) == RXT_NONE)
				rebol_renderer->draw->rebdrw_reset_gradient_pen(ctx->envr);
			else
				rebol_renderer->draw->rebdrw_gradient_pen(
					ctx->envr,
					RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 1)), //type
					RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 2)), //mode
					p3, //offset
					p4, //range - (begin, end)
					(RXA_TYPE(frm, 5) == RXT_DECIMAL) ? RXA_DEC64(frm, 5) : RXA_INT64(frm, 5), // angle
					RXA_PAIR(frm, 6), // scale
					RXA_SERIES(frm, 7) // unsigned char *colors
				);
		}
		break;

    case CMD_DRAW_IMAGE:
		if (RXA_TYPE(frm, 2) == RXT_PAIR) {
        	REBXYF offset = RXA_LOG_PAIR(frm, 2);
		    rebol_renderer->draw->rebdrw_image(ctx->envr, RXA_IMAGE_BITS(frm,1), RXA_IMAGE_WIDTH(frm,1), RXA_IMAGE_HEIGHT(frm,1), offset);
		} else {
            rebol_renderer->draw->rebdrw_image_scale(ctx->envr, RXA_IMAGE_BITS(frm,1), RXA_IMAGE_WIDTH(frm,1), RXA_IMAGE_HEIGHT(frm,1), RXA_SERIES(frm, 2));
        }
        break;

    case CMD_DRAW_IMAGE_FILTER:
        rebol_renderer->draw->rebdrw_image_filter(
            ctx->envr,
            RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 1)) - W_DRAW_NEAREST,
            RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 2)) - W_DRAW_RESIZE,
            (RXA_TYPE(frm, 3) == RXT_NONE) ? 1.0 : (RXA_TYPE(frm, 3) == RXT_DECIMAL) ? RXA_DEC64(frm, 3) : RXA_INT64(frm, 3)
        );
        break;

    case CMD_DRAW_IMAGE_OPTIONS:
        rebol_renderer->draw->rebdrw_image_options(ctx->envr, (RXA_TYPE(frm, 1) == RXT_NONE) ? 0 : 1, RXA_COLOR_TUPLE(frm, 1), RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 2)) - W_DRAW_NO_BORDER);
        break;

    case CMD_DRAW_IMAGE_PATTERN:
        rebol_renderer->draw->rebdrw_image_pattern(ctx->envr, RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 1)) - W_DRAW_NORMAL, RXA_PAIR(frm, 2), RXA_PAIR(frm, 3));
        break;


	case CMD_DRAW_LINE:
		{
			RXIARG val[2];
			REBCNT type;
			REBCNT n, m = 0;
			REBSER blk = RXA_SERIES(frm, 1);

			for (n = 0; (type = RL_GET_VALUE(blk, n, &val[m])); n++) {
				if (type == RXT_PAIR) {
				    switch (++m){
                        case 1:
                            rebol_renderer->draw->rebshp_begin(ctx->envr);
                            break;
				        case 2:
							{
								REBXYF p1 = RXI_LOG_PAIR(val[0]);
								REBXYF p2 = RXI_LOG_PAIR(val[1]);
//								rebol_renderer->draw->rebdrw_line(ctx->envr, val[0].pair,val[1].pair);
								rebol_renderer->draw->rebdrw_line(ctx->envr, p1, p2);
								val[0] = val[1];
								m--;
							}
                            break;
				    }
				}
			}
		}
		break;

	case CMD_DRAW_LINE_CAP:
		rebol_renderer->draw->rebdrw_line_cap(ctx->envr, RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 1)) - W_DRAW_BUTT);
		break;

	case CMD_DRAW_LINE_JOIN:
		rebol_renderer->draw->rebdrw_line_join(ctx->envr, RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 1)) - W_DRAW_MITER);
		break;

	case CMD_DRAW_LINE_WIDTH:
		rebol_renderer->draw->rebdrw_line_width(ctx->envr, ((RXA_TYPE(frm, 1) == RXT_DECIMAL) ? RXA_DEC64(frm, 1) : RXA_INT64(frm, 1)), RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 2)) - W_DRAW_VARIABLE);
		break;

	case CMD_DRAW_LINE_PATTERN:
        if (RXA_TYPE(frm, 2) == RXT_NONE)
            rebol_renderer->draw->rebdrw_line_pattern(ctx->envr, 0, 0);
        else {
            REBSER patterns = RXA_SERIES(frm, 2);
            REBINT len = RL_SERIES(patterns, RXI_SER_TAIL);

            if (len > 1){

                RXIARG val;
                REBCNT type;
                REBCNT n;
                REBDEC* pattern = (REBDEC*) OS_ALLOC_ARRAY(REBDEC, len + 1) ;

                pattern[0] = len;

                for (n = 0; (type = RL_GET_VALUE(patterns, n, &val)); n++) {
                    if (type == RXT_DECIMAL)
                        pattern[n+1] = LOG_COORD_X(val.dec64);
                    else if (type == RXT_INTEGER)
                        pattern[n+1] = LOG_COORD_X(val.int64);
                    else
                        break;
                }
                rebol_renderer->draw->rebdrw_line_pattern(ctx->envr, RXA_COLOR_TUPLE(frm, 1), pattern);
            }

        }
		break;

	case CMD_DRAW_INVERT_MATRIX:
		rebol_renderer->draw->rebdrw_invert_matrix(ctx->envr);
		break;

	case CMD_DRAW_MATRIX:
        rebol_renderer->draw->rebdrw_matrix(ctx->envr, RXA_SERIES(frm, 1));
		break;

	case CMD_DRAW_PEN:
        if (RXA_TYPE(frm, 1) == RXT_TUPLE)
            rebol_renderer->draw->rebdrw_pen(ctx->envr, RXA_COLOR_TUPLE(frm, 1));
        else if (RXA_TYPE(frm, 1) == RXT_LOGIC && !RXA_LOGIC(frm,1))
            rebol_renderer->draw->rebdrw_pen(ctx->envr, 0);
        else
            rebol_renderer->draw->rebdrw_pen_image(ctx->envr, RXA_IMAGE_BITS(frm,1), RXA_IMAGE_WIDTH(frm,1), RXA_IMAGE_HEIGHT(frm,1));
		break;

	case CMD_DRAW_POLYGON:
		{
			RXIARG val;
			REBCNT type;
			REBCNT n;
			REBSER blk = RXA_SERIES(frm, 1);

			for (n = 0; (type = RL_GET_VALUE(blk, n, &val)); n++) {
				if (type == RXT_PAIR) {
					REBXYF p = RXI_LOG_PAIR(val);
					if (n > 0)
//						rebol_renderer->draw->rebdrw_add_vertex(ctx->envr, val.pair);
						rebol_renderer->draw->rebdrw_add_poly_vertex(ctx->envr, p);
					else
//						rebol_renderer->draw->rebdrw_begin_poly(ctx->envr, val.pair);
						rebol_renderer->draw->rebdrw_begin_poly(ctx->envr, p);
				}
			}
			rebol_renderer->draw->rebdrw_end_poly(ctx->envr);
		}
		break;

    case CMD_DRAW_PUSH:
        {
            REBCEC innerCtx;

            innerCtx.envr = ctx->envr;
            innerCtx.block = RXA_SERIES(frm, 1);
            innerCtx.index = 0;

            rebol_renderer->draw->rebdrw_push_matrix(ctx->envr);
            RL_Do_Commands(RXA_SERIES(frm, 1), 0, &innerCtx);
            rebol_renderer->draw->rebdrw_pop_matrix(ctx->envr);
        }
        break;

	case CMD_DRAW_RESET_MATRIX:
		rebol_renderer->draw->rebdrw_reset_matrix(ctx->envr);
		break;

    case CMD_DRAW_ROTATE:
        rebol_renderer->draw->rebdrw_rotate(ctx->envr, (RXA_TYPE(frm, 1) == RXT_DECIMAL) ? RXA_DEC64(frm, 1) : RXA_INT64(frm, 1));
        break;

    case CMD_DRAW_SCALE:
        rebol_renderer->draw->rebdrw_scale(ctx->envr, RXA_PAIR(frm, 1));
        break;

    case CMD_DRAW_SHAPE:
        {
            REBCEC innerCtx;

            innerCtx.envr = ctx->envr;
            innerCtx.block = RXA_SERIES(frm, 1);
            innerCtx.index = 0;

            rebol_renderer->draw->rebshp_begin(ctx->envr);
            RL_Do_Commands(RXA_SERIES(frm, 1), 0, &innerCtx);
            rebol_renderer->draw->rebshp_end(ctx->envr);
        }
        break;

    case CMD_DRAW_SKEW:
        rebol_renderer->draw->rebdrw_skew(ctx->envr, RXA_PAIR(frm, 1));
        break;

	case CMD_DRAW_SPLINE:
        {
            REBSER points = RXA_SERIES(frm, 1);
            REBINT len = RL_SERIES(points, RXI_SER_TAIL);

            if (len > 3){
                RXIARG val;
                REBCNT type;
                REBCNT n;

                for (n = 0; (type = RL_GET_VALUE(points, n, &val)); n++) {
                    if (type == RXT_PAIR) {
						REBXYF p = RXI_LOG_PAIR(val);
                        if (n > 0)
//                            rebol_renderer->draw->rebdrw_add_vertex(ctx->envr, val.pair);
							rebol_renderer->draw->rebdrw_add_spline_vertex(ctx->envr, p);
                        else
//                            rebol_renderer->draw->rebdrw_begin_poly(ctx->envr, val.pair);
							rebol_renderer->draw->rebdrw_begin_spline(ctx->envr, p);
                    }
                }
                rebol_renderer->draw->rebdrw_end_spline(ctx->envr, RXA_INT32(frm, 2), RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 3)) - W_DRAW_OPENED);
            }

		}
		break;

    case CMD_DRAW_TEXT:
#if defined(AGG_WIN32_FONTS) || defined(AGG_FREETYPE)	
		{
			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			rebol_renderer->draw->rebdrw_text(
				ctx->envr,
				(RL_FIND_WORD(draw_ext_words , RXA_WORD(frm, 3)) == W_DRAW_VECTORIAL) ? 1 : 0,
				 &p1,
				 (RXA_TYPE(frm, 2) == RXT_PAIR) ? &p2 : NULL,
				 RXA_SERIES(frm, 4)
			);
		}
#endif		
        break;

	case CMD_DRAW_TRANSFORM:
		{
			REBXYF center = RXA_LOG_PAIR(frm, 2);
			REBXYF offset = RXA_LOG_PAIR(frm, 4);
			rebol_renderer->draw->rebdrw_transform(
				ctx->envr,
				(RXA_TYPE(frm, 1) == RXT_DECIMAL) ? RXA_DEC64(frm, 1) : RXA_INT64(frm, 1), // angle
				center,
				RXA_PAIR(frm, 3), // scale
				offset
			);
		}
		break;

    case CMD_DRAW_TRANSLATE:
		{
			REBXYF p = RXA_LOG_PAIR(frm, 1);
			rebol_renderer->draw->rebdrw_translate(ctx->envr, p);
		}
		break;

	case CMD_DRAW_TRIANGLE:
        {
            REBCNT b = 0xff000000;
			REBXYF p1 = RXA_LOG_PAIR(frm, 1);
			REBXYF p2 = RXA_LOG_PAIR(frm, 2);
			REBXYF p3 = RXA_LOG_PAIR(frm, 3);

            rebol_renderer->draw->rebdrw_triangle(
                ctx->envr,
                p1, // vertex-1
                p2, // vertex-2
                p3, // vertex-3
                (RXA_TYPE(frm, 4) == RXT_NONE) ? 0 : RXA_COLOR_TUPLE(frm, 4), // color-1
                (RXA_TYPE(frm, 5) == RXT_NONE) ? b : RXA_COLOR_TUPLE(frm, 5), // color-2
                (RXA_TYPE(frm, 6) == RXT_NONE) ? b : RXA_COLOR_TUPLE(frm, 6), // color-3
                (RXA_TYPE(frm, 7) == RXT_DECIMAL) ? RXA_DEC64(frm, 7) : RXA_INT64(frm, 7) // dilation
            );
        }
		break;

	default:
		return RXR_NO_COMMAND;
	}

    return RXR_UNSET;
}
