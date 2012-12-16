//exported functions
#include "agg_graphics.h"
#undef IS_ERROR
//extern "C" void Reb_Print(char *fmt, ...);//output just for testing
extern "C" void* Rich_Text;
extern "C" REBINT Draw_Gob(void *graphics, REBSER *block, REBSER *args);

namespace agg
{
	extern "C" RL_LIB *RL;

	extern "C" void agg_add_vertex (void* gr, REBXYF p)
	{
		((agg_graphics*)gr)->agg_add_vertex(p.x, p.y);
	}

	extern "C" void agg_anti_alias(void* gr, REBINT mode)
	{
		((agg_graphics*)gr)->agg_anti_alias(mode!=0);
	}

	extern "C" void agg_arc(void* gr, REBXYF c, REBXYF r, REBDEC ang1, REBDEC ang2, REBINT closed)
	{
		((agg_graphics*)gr)->agg_arc(c.x, c.y, r.x, r.y, ang1, ang2, closed);
	}

	extern "C" void agg_arrow(void* gr, REBXYF mode, REBYTE* col)
	{
		((agg_graphics*)gr)->agg_arrows(col, (REBINT)mode.x, (REBINT)mode.y);
	}

	extern "C" void agg_begin_poly (void* gr, REBXYF p)
	{
		((agg_graphics*)gr)->agg_begin_poly(p.x, p.y);
	}

	extern "C" void agg_box(void* gr, REBXYF p1, REBXYF p2, REBDEC r)
	{
		if (r) {
			((agg_graphics*)gr)->agg_rounded_rect(p1.x, p1.y, p2.x, p2.y, r);
		} else {
			((agg_graphics*)gr)->agg_box(p1.x, p1.y, p2.x, p2.y);
		}
	}

	extern "C" void agg_circle(void* gr, REBXYF p, REBXYF r)
	{
		((agg_graphics*)gr)->agg_ellipse(p.x, p.y, r.x, r.y);
	}

	extern "C" void agg_clip(void* gr, REBXYF p1, REBXYF p2)
	{
		((agg_graphics*)gr)->agg_set_clip(p1.x, p1.y, p2.x, p2.y);
	}

	extern "C" void agg_curve3(void* gr, REBXYF p1, REBXYF p2, REBXYF p3)
	{
		((agg_graphics*)gr)->agg_curve3(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	}

	extern "C" void agg_curve4(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBXYF p4)
	{
		((agg_graphics*)gr)->agg_curve4(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y);
	}

	extern "C" void agg_ellipse(void* gr, REBXYF p1, REBXYF p2)
	{
		REBDEC rx = p2.x / 2;
		REBDEC ry = p2.y / 2;
		((agg_graphics*)gr)->agg_ellipse(p1.x + rx, p1.y + ry, rx, ry);
	}

	extern "C" void agg_end_poly (void* gr)
	{
		((agg_graphics*)gr)->agg_end_poly();
	}

	extern "C" void agg_end_spline (void* gr, REBINT step, REBINT closed)
	{
		((agg_graphics*)gr)->agg_end_bspline(step, closed);
	}

	extern "C" void agg_fill_pen(void* gr, REBYTE* col)
	{
		if (col)
			((agg_graphics*)gr)->agg_fill_pen(col[0], col[1], col[2], 255 - col[3]);
		else
			((agg_graphics*)gr)->agg_fill_pen(0, 0, 0, 0);

	}

	extern "C" void agg_fill_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
	{
#ifndef AGG_OPENGL	
		((agg_graphics*)gr)->agg_fill_pen(0, 0, 0, 255, img, w, h);
#endif
	}

	extern "C" void agg_fill_rule(void* gr, REBINT mode)
	{
	    if (mode >= W_DRAW_EVEN_ODD && mode <= W_DRAW_NON_ZERO)
            ((agg_graphics*)gr)->agg_fill_rule((agg::filling_rule_e)mode);
	}

	extern "C" void agg_gamma(void* gr, REBDEC gamma)
	{
		((agg_graphics*)gr)->agg_set_gamma(gamma);
	}

    extern "C" void agg_gradient_pen(void* gr, REBINT gradtype, REBINT mode, REBXYF oft, REBXYF range, REBDEC angle, REBXYF scale, REBSER* colors){
#ifndef AGG_OPENGL
        unsigned char colorTuples[256*4+1] = {2, 0,0,0,0, 0,0,0,0, 255,255,255,0}; //max number of color tuples is 256 + one length information char
		REBDEC offsets[256] = {0.0 , 0.0, 1.0};

        //gradient fill
        RXIARG val;
        REBCNT type,i,j,k;
        REBDEC* matrix = new REBDEC[6];

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
                memcpy(&colorTuples[k], val.bytes + 1, 4);
                k+=4;
            }
        }

        //sanity checks
        if (j == 1) offsets[0] = -1;
        colorTuples[0] = MAX(2, (k - 5) / 4);

	    ((agg_graphics*)gr)->agg_gradient_pen(gradtype, oft.x, oft.y, range.x, range.y, angle, scale.x, scale.y, colorTuples, offsets, mode);
#endif		
	}

	extern "C" void agg_invert_matrix(void* gr)
	{
		((agg_graphics*)gr)->agg_invert_mtx();
	}

	extern "C" void agg_image(void* gr, REBYTE* img, REBINT w, REBINT h,REBXYF offset)
	{
#ifndef AGG_OPENGL	
		((agg_graphics*)gr)->agg_image(img, offset.x, offset.y, w, h);
#endif
	}

	extern "C" void agg_image_filter(void* gr, REBINT type, REBINT mode, REBDEC blur)
	{
		((agg_graphics*)gr)->agg_image_filter(type, mode, blur);
	}

	extern "C" void agg_image_options(void* gr, REBYTE* keyCol, REBINT border)
	{
	    if (keyCol)
            ((agg_graphics*)gr)->agg_image_options(keyCol[0], keyCol[1], keyCol[2], 255 - keyCol[3], border);
        else
            ((agg_graphics*)gr)->agg_image_options(0,0,0,0, border);
	}

    extern "C" void agg_image_pattern(void* gr, REBINT mode, REBXYF offset, REBXYF size){
        if (mode)
            ((agg_graphics*)gr)->agg_image_pattern(mode,offset.x,offset.y,size.x,size.y);
        else
            ((agg_graphics*)gr)->agg_image_pattern(0,0,0,0,0);
    }

	extern "C" void agg_image_scale(void* gr, REBYTE* img, REBINT w, REBINT h, REBSER* points)
	{
#ifndef AGG_OPENGL	
			RXIARG p[4];
			REBCNT type;
			REBCNT n, len = 0;

			for (n = 0; type = RL_GET_VALUE(points, n, &p[len]); n++) {
				if (type == RXT_PAIR)
                    if (++len == 4) break;
			}

            if (!len) return;
            if (len == 1) ((agg_graphics*)gr)->agg_image(img, p[0].pair.x, p[0].pair.y, w, h);

            ((agg_graphics*)gr)->agg_begin_poly(p[0].pair.x, p[0].pair.y);

            switch (len) {
                case 2:
       				((agg_graphics*)gr)->agg_add_vertex(p[1].pair.x, p[0].pair.y);
                    ((agg_graphics*)gr)->agg_add_vertex(p[1].pair.x, p[1].pair.y);
                    ((agg_graphics*)gr)->agg_add_vertex(p[0].pair.x, p[1].pair.y);
                    break;
                case 3:
       				((agg_graphics*)gr)->agg_add_vertex(p[1].pair.x, p[1].pair.y);
                    ((agg_graphics*)gr)->agg_add_vertex(p[2].pair.x, p[2].pair.y);
                    ((agg_graphics*)gr)->agg_add_vertex(p[0].pair.x, p[2].pair.y);
                    break;
                case 4:
       				((agg_graphics*)gr)->agg_add_vertex(p[1].pair.x, p[1].pair.y);
                    ((agg_graphics*)gr)->agg_add_vertex(p[2].pair.x, p[2].pair.y);
                    ((agg_graphics*)gr)->agg_add_vertex(p[3].pair.x, p[3].pair.y);
                    break;
            }

            ((agg_graphics*)gr)->agg_end_poly_img(img, w, h);
#endif
	}

	extern "C" void agg_line(void* gr, REBXYF p1, REBXYF p2)
	{
		((agg_graphics*)gr)->agg_line(p1.x, p1.y, p2.x, p2.y);
	}

	extern "C" void agg_line_cap(void* gr, REBINT mode)
	{
		((agg_graphics*)gr)->agg_stroke_cap((line_cap_e)mode);
	}

	extern "C" void agg_line_join(void* gr, REBINT mode)
	{
		((agg_graphics*)gr)->agg_stroke_join((line_join_e)mode);
		((agg_graphics*)gr)->agg_dash_join((line_join_e)mode);
	}

	extern "C" void agg_line_pattern(void* gr, REBYTE* col, REBDEC* patterns)
	{
        ((agg_graphics*)gr)->agg_line_pattern(col, patterns);
	}

	extern "C" void agg_line_width(void* gr, REBDEC width, REBINT mode)
	{
		((agg_graphics*)gr)->agg_line_width(width, mode);
	}

	extern "C" void agg_matrix(void* gr, REBSER* mtx)
	{
			RXIARG val;
			REBCNT type;
			REBCNT n;
			REBDEC* matrix = new REBDEC[6];

			for (n = 0; type = RL_GET_VALUE(mtx, n, &val),n < 6; n++) {
				if (type == RXT_DECIMAL)
				    matrix[n] = val.dec64;
				else if (type == RXT_INTEGER)
				    matrix[n] = val.int64;
				else
                    return;
			}

            if (n != 6) return;

            ((agg_graphics*)gr)->agg_set_mtx(matrix);

            delete[] matrix;
	}

	extern "C" void agg_pen(void* gr, REBYTE* col)
	{
		if (col)
			((agg_graphics*)gr)->agg_pen(col[0], col[1], col[2], 255 - col[3]);
		else
			((agg_graphics*)gr)->agg_pen(0,0,0,0);

	}

	extern "C" void agg_pen_image(void* gr, REBYTE* img, REBINT w, REBINT h)
	{
#ifndef AGG_OPENGL	
		((agg_graphics*)gr)->agg_pen(0, 0, 0, 255, img, w, h);
#endif
	}

	extern "C" void agg_pop_matrix(void* gr)
	{
		((agg_graphics*)gr)->agg_pop_mtx();
	}

	extern "C" void agg_push_matrix(void* gr)
	{
		((agg_graphics*)gr)->agg_push_mtx();
	}

	extern "C" void agg_reset_gradient_pen(void* gr)
	{
		((agg_graphics*)gr)->agg_reset_gradient_pen();
	}

	extern "C" void agg_reset_matrix(void* gr)
	{
		((agg_graphics*)gr)->agg_reset_mtx();
	}

	extern "C" void agg_rotate(void* gr, REBDEC ang)
	{
		((agg_graphics*)gr)->agg_rotate(ang);
	}

	extern "C" void agg_scale(void* gr, REBXYF sc)
	{
		((agg_graphics*)gr)->agg_scale(sc.x, sc.y);
	}

	extern "C" void agg_skew(void* gr, REBXYF angle)
	{
		((agg_graphics*)gr)->agg_skew(angle.x, angle.y);
	}

	extern "C" REBINT agg_text(void* gr, REBINT mode, REBXYF* p1, REBXYF* p2, REBSER* block)
	{
#ifndef AGG_OPENGL	
		return ((agg_graphics*)gr)->agg_text(mode, p1, p2, block);
#else
		return 0;
#endif
	}

	extern "C" void agg_transform(void* gr, REBDEC ang, REBXYF ctr, REBXYF sc, REBXYF oft)
	{
		((agg_graphics*)gr)->agg_transform(ang, ctr.x, ctr.y, sc.x, sc.y, oft.x, oft.y);
	}

	extern "C" void agg_translate(void* gr, REBXYF p)
	{
		((agg_graphics*)gr)->agg_translate(p.x, p.y);
	}

	extern "C" void agg_triangle(void* gr, REBXYF p1, REBXYF p2, REBXYF p3, REBYTE* c1, REBYTE* c2, REBYTE* c3, REBDEC dilation)
	{
#ifndef AGG_OPENGL	
		((agg_graphics*)gr)->agg_gtriangle(p1, p2, p3, c1, c2, c3, dilation);
#endif
	}


	//SHAPE functions
	extern "C" void agg_path_arc(void* gr, REBINT rel, REBXYF p, REBXYF r, REBDEC ang, REBINT sweep, REBINT large)
	{
		((agg_graphics*)gr)->agg_path_arc(rel, r.x, r.y, ang, large, sweep, p.x, p.y);
	}

	extern "C" void agg_path_close(void* gr)
	{
		((agg_graphics*)gr)->agg_path_close();
	}

	extern "C" void agg_path_curv(void* gr, REBINT rel, REBXYF p1, REBXYF p2)
	{
		((agg_graphics*)gr)->agg_path_cubic_curve_to(rel, p1.x, p1.y, p2.x, p2.y);
	}

	extern "C" void agg_path_curve(void* gr, REBINT rel, REBXYF p1,REBXYF p2, REBXYF p3)
	{
		((agg_graphics*)gr)->agg_path_cubic_curve(rel, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	}

	extern "C" void agg_path_hline(void* gr, REBCNT rel, REBDEC x)
	{
		((agg_graphics*)gr)->agg_path_hline(rel, x);
	}

	extern "C" void agg_path_line(void* gr, REBCNT rel, REBXYF p)
	{
		((agg_graphics*)gr)->agg_path_line(rel, p.x, p.y);
	}

	extern "C" void agg_path_move(void* gr, REBCNT rel, REBXYF p)
	{
		((agg_graphics*)gr)->agg_path_move(rel, p.x, p.y);
	}

	extern "C" void agg_path_open(void* gr)
	{
		((agg_graphics*)gr)->agg_begin_path();
	}

	extern "C" void agg_path_vline(void* gr, REBCNT rel, REBDEC y)
	{
		((agg_graphics*)gr)->agg_path_vline(rel, y);
	}

	extern "C" void agg_path_qcurv(void* gr, REBINT rel, REBXYF p)
	{
		((agg_graphics*)gr)->agg_path_quadratic_curve_to(rel, p.x, p.y);
	}

	extern "C" void agg_path_qcurve(void* gr, REBINT rel, REBXYF p1, REBXYF p2)
	{
		((agg_graphics*)gr)->agg_path_quadratic_curve(rel, p1.x, p1.y, p2.x, p2.y);
	}


	extern "C" REBINT Draw_Image(REBYTE *image, REBINT w, REBINT h, REBSER *block)
	{
		agg_graphics::ren_buf renbuf(image, w, h, w * 4);
		agg_graphics::pixfmt pixf(renbuf);
		agg_graphics::ren_base rb_win(pixf);

		agg_graphics* graphics = new agg_graphics(&renbuf, w, h, 0, 0);

		REBSER *args = 0;

		REBINT result = Draw_Gob(graphics, block, args);

		if (result < 0) goto do_cleanup;

		result = graphics->agg_render(rb_win);

        do_cleanup:
            delete graphics;

            return result;
	}

#ifdef ndef

	extern "C" void agg_get_size(void* gr, REBPAR* p)
	{
		((agg_graphics*)gr)->agg_size(p);
	}

	extern "C" void agg_effect(void* gr, REBPAR* p1, REBPAR* p2, REBSER* block)
	{
		((agg_graphics*)gr)->agg_effect(p1, p2, block);
	}


#endif
}
