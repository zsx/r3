/***********************************************************************
**
**  Copyright 2012 Saphirion AG
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
**  Module:  agg_graphics.cpp
**  Summary: REBOL EFFECTS dialect rendering functions
**  Author:  Richard Smolak
**
***********************************************************************/

#include "agg_effects.h"

namespace agg
{


	agg_effects::agg_effects() :
		m_rbas_dst(0),
		m_rbuf_dst(0)
	{
		m_gr = new agg_graphics(m_rbuf_dst, 0, 0, 0, 0);
	}

	agg_effects::~agg_effects(){
		delete m_gr;
	}

	void agg_effects::init(ren_base& rbas, rendering_buffer& rbuf, int w, int h, int x, int y, int alpha){
		m_rbas_dst = &rbas;
		m_rbuf_dst = &rbuf;
//Reb_Print("%dx%d", m_rbas_dst->xmin(),m_rbas_dst->ymin());
		m_src_w = w;
		m_src_h = h;
		m_src_x = x;
		m_src_y = y;

		m_clip = m_rbas_dst->clip_box();
		m_clip_w = (m_clip.x2 - m_clip.x1) + 1;
		m_clip_h = (m_clip.y2 - m_clip.y1) + 1;

		m_alpha = alpha;
	}

	void agg_effects::get_offset(REBPAR* p){
		p->x = m_src_x;
		p->y = m_src_y;
	}

	void agg_effects::get_size(REBPAR* p){
		p->x = m_src_w;
		p->y = m_src_h;
	}

	void agg_effects::copy_bg(rendering_buffer& dst){
		unsigned h = m_rbuf_dst->height();
		int ox = m_rbas_dst->xmin();
		unsigned oy = m_rbas_dst->ymin();

		if(dst.height() < h) h = dst.height();
        
		unsigned l = m_rbuf_dst->stride_abs();
		if(dst.stride_abs() < l) l = dst.stride_abs();

		unsigned y;
		for (y = 0; y < h; y++)
		{
			memcpy((rgba8*)dst.row(y), (rgba8*)m_rbuf_dst->row(y+oy)+ox, l);
		}
	}

	AGG_INLINE int8u agg_effects::blend(int8u s, int8u d, int8u alpha){
		s = alpha ? ((s - d) * (255 - alpha) + (d << 8)) >> 8 : s;
		return m_alpha ? ((s - d) * (255 - m_alpha) + (d << 8)) >> 8 : s;
	}

	/***********************************************************************
	**
	*/	REBOOL G_RGB_To_HSV( REBYTE* color, double *hp, double *sp, double *vp )
	/*
	**	Convert an RGB value to an HSV value.
	**
	***********************************************************************/
	{
		double m, r1, g1, b1;
		double h, s, v;
		double nr, ng, nb;		/* rgb values of 0.0 - 1.0	*/

		nb = (double)( color[2] / 255. );
		ng = (double)( color[1] / 255. );
		nr = (double)( color[0] / 255. );

		v = MAX(nr,MAX(ng,nb));
		m = MIN(nr,MIN(ng,nb));

		if(v != 0.) {
			s = (v - m) /v;
		} else {
			s = 0.;
		}

		*vp = v;
		if(s == 0.) return FALSE;	/* h is undefined	*/

		r1 = (v - nr) / (v - m);	/* distance of color from red	*/
		g1 = (v - ng) / (v - m);	/* distance of color from green	*/
		b1 = (v - nb) / (v - m);	/* distance of color from blue	*/

		if(v == nr) {
			if (m == ng) {
				h = ( double )( 5. + b1 );
			} else {
				h = ( double )( 1. - g1 );
			}
		} 

		if(v == ng) {
			if (m == nb) {
				h = ( double )( 1. + r1 );
			} else {
				h = ( double )( 3. - b1 );
			}
		}

		if(v == nb) {
			if(m == nr) {
				h = ( double )( 3. + g1 );
			} else {
				h = ( double )( 5. - r1 );
			}
		}
		h = ( double )( h * 60. );			/* convert to degrees		*/
		*hp = h;
		*sp = s;

		return TRUE;
	}

	/***********************************************************************
	**
	*/	REBINT G_HSV_To_RGB( double h, double s, double v )
	/*
	**	Convert an HSV value to an RGB value.
	**
	***********************************************************************/
	{
		double p1, p2, p3, i, f;
		double xh;
		double nr, ng, nb;		/* rgb values of 0.0 - 1.0	*/

		if(h == 360.) h = 0.;		/* (THIS LOOKS BACKWARDS)	*/

		xh = ( double )( h / 60. );			/* convert h to be in 0,6	*/
		i = (double)floor((double)xh);	/* i = greatest integer <= h	*/
		f = xh - i;			/* f = fractional part of h	*/
		p1 = v * (1 - s);
		p2 = v * (1 - (s * f));
		p3 = v * (1 - (s * (1 - f)));

		switch((int)i) {
			case 0: nr = v;  ng = p3; nb = p1; break;
			case 1: nr = p2; ng = v;  nb = p1; break;
			case 2: nr = p1; ng = v;  nb = p3; break;
			case 3: nr = p1; ng = p2; nb = v;  break;
			case 4: nr = p3; ng = p1; nb = v;  break;
			case 5: nr = v ; ng = p1; nb = p2; break;
		}

		return ( ( REBINT )( nr * 255. ) << 0) |
				( ( REBINT )( ng * 255. ) << 8 ) |
				( ( REBINT )( nb * 255. ) << 16 );
	}


	//effects--------------------------------------

	void agg_effects::fx_add(unsigned char *img, int img_w, int img_h, unsigned char *src, int src_w, int src_h){

		rendering_buffer rbuf_img(img,img_w,img_h,img_w * 4);

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* im = 0;

					if (y < img_h)
						im = rbuf_img.row(y) + x1;

					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						if (im && (w - len) < (img_w - m_clip.x1)){
							int a = im[component_order::A];
							int a2 = pix[component_order::A];
							int r = im[component_order::R] + pix[component_order::R];
							if (r > 255) r = 255;
							int g = im[component_order::G] + pix[component_order::G];
							if (g > 255) g = 255;
							int b = im[component_order::B] + pix[component_order::B];
							if (b > 255) b = 255;

							d[component_order::R] = blend(r, blend(pix[component_order::R], d[component_order::R], a2), a);
							d[component_order::G] = blend(g, blend(pix[component_order::G], d[component_order::G], a2), a);
							d[component_order::B] = blend(b, blend(pix[component_order::B], d[component_order::B], a2), a);
							im+=4;
						} else {
							int a = pix[component_order::A];
							d[component_order::R] = blend(pix[component_order::R], d[component_order::R], a);
							d[component_order::G] = blend(pix[component_order::G], d[component_order::G], a);
							d[component_order::B] = blend(pix[component_order::B], d[component_order::B], a);
						}
						pix+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			int y2 = MIN(m_clip.y2+1, img_h);
			for (int y = m_clip.y1; y < y2; y++){
				int len = MIN(m_clip_w,img_w-m_clip.x1);
				if (len < 1) break;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				int8u* im = rbuf_img.row(y) + (m_clip.x1 << 2);
				do {
					int a = im[component_order::A];
					int r = pix[component_order::R] + im[component_order::R];
					if (r > 255) r = 255;
					int g = pix[component_order::G] + im[component_order::G];
					if (g > 255) g = 255;
					int b = pix[component_order::B] + im[component_order::B];
					if (b > 255) b = 255;

					pix[component_order::R] = blend(r, pix[component_order::R], a);
					pix[component_order::G] = blend(g, pix[component_order::G], a);
					pix[component_order::B] = blend(b, pix[component_order::B], a);
					pix+=4;
					im+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_alphamul(unsigned char *src, int src_w, int src_h, int alpha){
			rendering_buffer rbuf_src(src,src_w,src_h,src_w * 4);
			agg_graphics::pixfmt pixf_src(rbuf_src);
			m_rbas_dst->blend_from(pixf_src,0,0,0, 255 - alpha);
	}

	void agg_effects::fx_blur(unsigned char *src, int src_w, int src_h){
		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* r1 = rbuf_src.row(y-1) + x1;
					int8u* r2 = rbuf_src.row(y) + x1;
					int8u* r3 = rbuf_src.row(y+1) + x1; 
					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						int a = r2[component_order::A];
						if (y == m_clip.y1 || y == y2-1 || len == w || len == 0) {
							d[component_order::R] = blend(r2[component_order::R], d[component_order::R],a);
							d[component_order::G] = blend(r2[component_order::G], d[component_order::G],a);
							d[component_order::B] = blend(r2[component_order::B], d[component_order::B],a);
						} else {
							d[component_order::R] = blend ((
								r2[component_order::R] + r1[component_order::R] + r3[component_order::R] +
								(r2-4)[component_order::R] + (r2+4)[component_order::R]
							) / 5, d[component_order::R], a);
							d[component_order::G] = blend ((
								r2[component_order::G] + r1[component_order::G] + r3[component_order::G] +
								(r2-4)[component_order::G] + (r2+4)[component_order::G]
							) / 5, d[component_order::G], a);
							d[component_order::B] = blend ((
								r2[component_order::B] + r1[component_order::B] + r3[component_order::B] +
								(r2-4)[component_order::B] + (r2+4)[component_order::B]
							) / 5, d[component_order::B], a);
						}
						r1+=4;
						r2+=4;
						r3+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			int x = (m_clip.x1+1) << 2;
			for (int y = m_clip.y1+1; y < m_clip.y2; y++){
				int len = m_clip_w-2;
				int8u* r1 = m_rbuf_dst->row(y-1) + x;
				int8u* r2 = m_rbuf_dst->row(y) + x;
				int8u* r3 = m_rbuf_dst->row(y+1) + x; 
				do {
					int a = r2[component_order::A];
					r2[component_order::R] = blend ((
						r2[component_order::R] + r1[component_order::R] + r3[component_order::R] +
						(r2-4)[component_order::R] + (r2+4)[component_order::R]
					) / 5, r2[component_order::R], a);
					r2[component_order::G] = blend ((
						r2[component_order::G] + r1[component_order::G] + r3[component_order::G] +
						(r2-4)[component_order::G] + (r2+4)[component_order::G]
					) / 5, r2[component_order::G], a);
					r2[component_order::B] = blend ((
						r2[component_order::B] + r1[component_order::B] + r3[component_order::B] +
						(r2-4)[component_order::B] + (r2+4)[component_order::B]
					) / 5, r2[component_order::B], a);

					r1+=4;
					r2+=4;
					r3+=4;
				} while (--len > 0);
			}
		}
	}

	void agg_effects::fx_colorify(REBYTE* color, REBINT depth, unsigned char *src, int src_w, int src_h){
		double	hue, sat, value;
		REBINT clr;
		REBINT	colors[ 256 ];

		if ( G_RGB_To_HSV( color, &hue, &sat, &value ) ) {
			for (int val=0; val<256; val++ ) {
				double v = (val * value) / 255;
				double s = ((sat * depth) + ((1.0 - v) * (255 - depth))) / 255;
				colors[ val ] = G_HSV_To_RGB( hue, s, v);
			}
		} else {
			for (int val=0; val<256; val++ ) {
				if ( val )
					clr = (color[0] * val) / 255;
				else
					clr = 0;
				colors[ val ] = clr | clr << 8 | clr << 16;
			}
		}

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;
					do {
						int a = pix[component_order::A];
						REBYTE* t=(REBYTE*)&colors[MAX(pix[component_order::R],MAX(pix[component_order::G],pix[component_order::B]))];
						d[component_order::R] = blend(t[0], d[component_order::R], a);
						d[component_order::G] = blend(t[1], d[component_order::G], a);
						d[component_order::B] = blend(t[2], d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}
			}
		} else {

			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					REBYTE* t=(REBYTE*)&colors[MAX(pix[component_order::R],MAX(pix[component_order::G],pix[component_order::B]))];
					pix[component_order::R] = blend(t[0], pix[component_order::R], a);
					pix[component_order::G] = blend(t[1], pix[component_order::G], a);
					pix[component_order::B] = blend(t[2], pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_colorize(REBYTE* color, unsigned char *src, int src_w, int src_h){
		REBINT	colors[ 256 ];

		REBINT pos = (int)(((color[0]*30)+(color[1]*59)+(color[2]*11)) / 100);
		REBINT r = color[0];
		REBINT g = color[1];
		REBINT b = color[2];
		double step_r;
		double step_g;
		double step_b;

		colors[pos] = r | g << 8 | b << 16;
		
		if (pos){
			step_r = (double)r / pos;
			step_g = (double)g / pos;
			step_b = (double)b / pos;

			for (int i=pos; i>=0; i--)
				colors[pos-i] = r - (int)(step_r * i) | (g - (int)(step_g * i)) << 8 | (b - (int)(step_b * i)) << 16;
		}

		REBINT to_white = 255 - pos;

		if (to_white > 0){
			step_r = (255.0-r) / to_white;
			step_g = (255.0-g) / to_white;
			step_b = (255.0-b) / to_white;
		} else {
			step_r = step_g = step_b = 0;
		}

		for (int i=pos+1; i<256; i++) {
			int j = i - pos;
			colors[i] = r + (int)(step_r * j) | (g + (int)(step_g * j)) << 8 | (b + (int)(step_b * j)) << 16;
		}

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;
					do {
						int a = pix[component_order::A];
						REBYTE* t=(REBYTE*)&colors[((pix[component_order::R]*30) + (pix[component_order::G]*59) + (pix[component_order::B]*11)) / 100];
						d[component_order::R] = blend(t[0], d[component_order::R], a);
						d[component_order::G] = blend(t[1], d[component_order::G], a);
						d[component_order::B] = blend(t[2], d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}
			}
		} else {

			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					REBYTE* t=(REBYTE*)&colors[((pix[component_order::R]*30) + (pix[component_order::G]*59) + (pix[component_order::B]*11)) / 100];
					pix[component_order::R] = blend(t[0], pix[component_order::R], a);
					pix[component_order::G] = blend(t[1], pix[component_order::G], a);
					pix[component_order::B] = blend(t[2], pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}

	}

	void agg_effects::fx_contrast(REBINT con, unsigned char *src, int src_w, int src_h){
		int mag = MIN(128, abs(con));
		int rs =0, gs = 0, bs = 0;

		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = 0; y < src_h; y++){
					int len = src_w;
					int8u* pix = rbuf_src.row(y) + x1;
					do {
						rs += pix[component_order::R];
						gs += pix[component_order::G];
						bs += pix[component_order::B];
						pix+=4;
					} while (--len);
				}

				int t = src_w *  src_h;
				int ra = rs / t;
				int ga = gs / t;
				int ba = bs / t;

				if (con < 0) {
					for (int y = m_clip.y1; y < y2; y++){
						int len = w;
						int8u* pix = rbuf_src.row(y) + x1;
						int8u* d = m_rbuf_dst->row(y) + x1;
						do {
							int a = pix[component_order::A];
							d[component_order::R] = blend(MAX(0, MIN ( ((int) ((18 * (pix[component_order::R] - ra)) / (mag + 25))) + ra, 255) ), d[component_order::R], a);
							d[component_order::G] = blend(MAX(0, MIN ( ((int) ((18 * (pix[component_order::G] - ga)) / (mag + 25))) + ga, 255) ), d[component_order::G], a);
							d[component_order::B] = blend(MAX(0, MIN ( ((int) ((18 * (pix[component_order::B] - ba)) / (mag + 25))) + ba, 255) ), d[component_order::B], a);
							pix+=4;
							d+=4;
						} while (--len);
					}
				} else {
					for (int y = m_clip.y1; y < y2; y++){
						int len = w;
						int8u* pix = rbuf_src.row(y) + x1;
						int8u* d = m_rbuf_dst->row(y) + x1;
						do {
							int a = pix[component_order::A];
							d[component_order::R] = blend(MAX(0, MIN ( ((int) ((mag + 18) * (int) (pix[component_order::R] - ra)) / 18) + ra, 255) ), d[component_order::R], a);
							d[component_order::G] = blend(MAX(0, MIN ( ((int) ((mag + 18) * (int) (pix[component_order::G] - ga)) / 18) + ga, 255) ), d[component_order::G], a);
							d[component_order::B] = blend(MAX(0, MIN ( ((int) ((mag + 18) * (int) (pix[component_order::B] - ba)) / 18) + ba, 255) ), d[component_order::B], a);
							pix+=4;
							d+=4;
						} while (--len);
					}
				}
			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					rs += pix[component_order::R];
					gs += pix[component_order::G];
					bs += pix[component_order::B];
					pix+=4;
				} while (--len);
			}
			int t = m_clip_w *  m_clip_h;
			int ra = rs / t;
			int ga = gs / t;
			int ba = bs / t;

			if (con < 0) {
				for (int y = m_clip.y1; y < m_clip.y2+1; y++){
					int len = m_clip_w;
					int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
					do {
						int a = pix[component_order::A];
						pix[component_order::R] = blend(MAX(0, MIN ( ((int) ((18 * (pix[component_order::R] - ra)) / (mag + 25))) + ra, 255) ), pix[component_order::R], a);
						pix[component_order::G] = blend(MAX(0, MIN ( ((int) ((18 * (pix[component_order::G] - ga)) / (mag + 25))) + ga, 255) ), pix[component_order::G], a);
						pix[component_order::B] = blend(MAX(0, MIN ( ((int) ((18 * (pix[component_order::B] - ba)) / (mag + 25))) + ba, 255) ), pix[component_order::B], a);
						pix+=4;
					} while (--len);
				}
			} else {
				for (int y = m_clip.y1; y < m_clip.y2+1; y++){
					int len = m_clip_w;
					int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
					do {
						int a = pix[component_order::A];
						pix[component_order::R] = blend(MAX(0, MIN ( ((int) ((mag + 18) * (int) (pix[component_order::R] - ra)) / 18) + ra, 255) ), pix[component_order::R], a);
						pix[component_order::G] = blend(MAX(0, MIN ( ((int) ((mag + 18) * (int) (pix[component_order::G] - ga)) / 18) + ga, 255) ), pix[component_order::G], a);
						pix[component_order::B] = blend(MAX(0, MIN ( ((int) ((mag + 18) * (int) (pix[component_order::B] - ba)) / 18) + ba, 255) ), pix[component_order::B], a);
						pix+=4;
					} while (--len);
				}
			}
		}
	}

	void agg_effects::fx_convolve(double* filter, double divisor, int offset, int gray, unsigned char *src, int src_w, int src_h)
	{
		rendering_buffer rbuf_src;
		int k,l,idx;
		double div,f,h;
		double color[3];

		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h1 = MIN(src_h - m_clip.y1,m_clip_h);
				rbuf_src.attach(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h1;
				int x = m_clip.x1 << 2;
				int y1 = m_clip.y1;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* dst = m_rbuf_dst->row(y) + x;
					int x1 = x-4;
					do {
						int a = (rbuf_src.row(y1) + x1)[component_order::A];
						idx = 0;
						div = 0;
						color[0] = 0;
						color[1] = 0;
						color[2] = 0;
						for(k=0; k<3; k++){
							for(l=0; l<3; l++){
								f = filter[idx++];
								if (f){
									int rx = MAX(0, MIN(((m_clip.x1+w)-1) << 2, x1+(l << 2)));
									int ry = MIN(y2 - 1, MAX(m_clip.y1, (y1-1)+k));
									
									int8u* pix = rbuf_src.row(ry) + rx;
									
									div += f;
									
									int r = pix[component_order::R];
									int g = pix[component_order::G];
									int b = pix[component_order::B];
									
									if (gray){
										h = (r+g+b) / 3;
										
										if(h>255) h=255;
										if(h<0) h=0;
										h= h * f;
										
										color[0] += h;
										color[1] += h;
										color[2] += h;
									} else {
										color[0] += r * f;
										color[1] += g * f;
										color[2] += b * f;
									}
								}
							}
						}
						
						if (divisor == 0) divisor = div;
						if (divisor <= 0) divisor = 1;
						
						for (k=0; k<3; k++){
							color[k] = (color[k] / divisor) + offset;
							if (color[k] > 255) color[k]=255;
							if (color[k] < 0) color[k]=0;
						}
						
						dst[component_order::R] = blend((unsigned char)color[0], dst[component_order::R], a);
						dst[component_order::G] = blend((unsigned char)color[1], dst[component_order::G], a);
						dst[component_order::B] = blend((unsigned char)color[2], dst[component_order::B], a);
						dst+=4;
						x1+=4;
					} while (--len);
					y1++;
				}
			}
		} else {
			src = new unsigned char[m_clip_w * m_clip_h * 4];
			rbuf_src.attach(src, m_clip_w, m_clip_h, m_clip_w * 4);
			copy_bg(rbuf_src);

			int x = m_clip.x1 << 2;
			int y1 = 0;
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;

				int8u* dst = m_rbuf_dst->row(y) + x;

				int x1 = -4;
				do {
					int a = (rbuf_src.row(y1) + x1)[component_order::A];
					idx = 0;
					div = 0;
					color[0] = 0;
					color[1] = 0;
					color[2] = 0;
					for(k=0; k<3; k++){
						for(l=0; l<3; l++){
							f = filter[idx++];
							if (f){
								int rx = MAX(0, MIN((m_clip_w-1) << 2, x1+(l << 2)));
								int ry = MIN(m_clip_h - 1, MAX(1, (y1-1)+k));

								int8u* pix = rbuf_src.row(ry) + rx;

								div += f;

								int r = pix[component_order::R];
								int g = pix[component_order::G];
								int b = pix[component_order::B];

								if (gray){
									h = (r+g+b) / 3;

									if(h>255) h=255;
									if(h<0) h=0;
									h= h * f;

									color[0] += h;
									color[1] += h;
									color[2] += h;
								} else {
									color[0] += r * f;
									color[1] += g * f;
									color[2] += b * f;
								}
							}
						}
					}
					
					if (divisor == 0) divisor = div;
					if (divisor <= 0) divisor = 1;

					for (k=0; k<3; k++){
						color[k] = (color[k] / divisor) + offset;
						if (color[k] > 255) color[k]=255;
						if (color[k] < 0) color[k]=0;
					}
					
					dst[component_order::R] = blend((unsigned char)color[0], dst[component_order::R], a);
					dst[component_order::G] = blend((unsigned char)color[1], dst[component_order::G], a);
					dst[component_order::B] = blend((unsigned char)color[2], dst[component_order::B], a);
					dst+=4;
					x1+=4;
				} while (--len > 0);
				y1++;
			}
			delete[] src;
		}
	}

	void agg_effects::fx_crop(REBPAR* p1, REBPAR* p2, unsigned char *src, int src_w, int src_h){
		agg_graphics::pixfmt pixf_src;
		rendering_buffer rbuf_src;

		REBINT x, y, x2, y2;
		
		if (p1) {
			x = p1->x;
			y = p1->y;
		} else
			x = y = 0;

		if (p2) {
			x2 = p2->x;
			y2 = p2->y;
		} else {
			x2 = src_w;
			y2 = src_h;
		}
		rect r(x,y,x+x2-1,y+y2-1);

		if (src) {
			rbuf_src.attach(src,src_w,src_h,src_w * 4);
			pixf_src.attach(rbuf_src);
		} else {
			pixf_src.attach(*m_rbuf_dst);
		}
		m_rbas_dst->blend_from(pixf_src,&r,-x,-y, 255 - m_alpha);
	}

	void agg_effects::fx_difference(REBYTE* col, unsigned char *src, int src_w, int src_h){
		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;
					do {
						int a = pix[component_order::A];
						d[component_order::R] = blend(abs(pix[component_order::R] - (col[0] & 0xff)), d[component_order::R], a);
						d[component_order::G] = blend(abs(pix[component_order::G] - (col[1] & 0xff)), d[component_order::G], a);
						d[component_order::B] = blend(abs(pix[component_order::B] - (col[2] & 0xff)), d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}
			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					pix[component_order::R] = blend(abs(pix[component_order::R] - (col[0] & 0xff)), pix[component_order::R], a);
					pix[component_order::G] = blend(abs(pix[component_order::G] - (col[1] & 0xff)), pix[component_order::G], a);
					pix[component_order::B] = blend(abs(pix[component_order::B] - (col[2] & 0xff)), pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_difference_image(unsigned char *img, int img_w, int img_h, unsigned char *src, int src_w, int src_h){

		rendering_buffer rbuf_img(img,img_w,img_h,img_w * 4);

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* im = 0;

					if (y < img_h)
						im = rbuf_img.row(y) + x1;

					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						if (im && (w - len) < (img_w - m_clip.x1)){
							int a = im[component_order::A];
							int a2 = pix[component_order::A];
							d[component_order::R] = blend(abs(pix[component_order::R] - im[component_order::R]), blend(pix[component_order::R], d[component_order::R], a2), a);
							d[component_order::G] = blend(abs(pix[component_order::G] - im[component_order::G]), blend(pix[component_order::G], d[component_order::G], a2), a);
							d[component_order::B] = blend(abs(pix[component_order::B] - im[component_order::B]), blend(pix[component_order::B], d[component_order::B], a2), a);
							im+=4;
						} else {
							int a = pix[component_order::A];
							d[component_order::R] = blend(pix[component_order::R], d[component_order::R], a);
							d[component_order::G] = blend(pix[component_order::G], d[component_order::G], a);
							d[component_order::B] = blend(pix[component_order::B], d[component_order::B], a);
						}
						pix+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			int y2 = MIN(m_clip.y2+1, img_h);
			for (int y = m_clip.y1; y < y2; y++){
				int len = MIN(m_clip_w,img_w-m_clip.x1);
				if (len < 1) break;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				int8u* im = rbuf_img.row(y) + (m_clip.x1 << 2);
				do {
					int a = im[component_order::A];
					pix[component_order::R] = blend(abs(pix[component_order::R] - im[component_order::R]), pix[component_order::R], a);
					pix[component_order::G] = blend(abs(pix[component_order::G] - im[component_order::G]), pix[component_order::G], a);
					pix[component_order::B] = blend(abs(pix[component_order::B] - im[component_order::B]), pix[component_order::B], a);
					pix+=4;
					im+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_extend(REBPAR* p1, REBPAR* p2, unsigned char *src, int src_w, int src_h){
		agg_graphics::pixfmt pixf_src;
		rendering_buffer rbuf_src;
		REBINT y1,x1;
		if (src) {
			rbuf_src.attach(src,src_w,src_h,src_w * 4);
			pixf_src.attach(rbuf_src);
			x1 = y1 = 0;
		} else {
			pixf_src.attach(*m_rbuf_dst);
			src_w = m_src_w;
			src_h = m_src_h;
			x1 = m_clip.x1;
			y1 = m_clip.y1;
		}

		REBINT px, py, px2, py2;
		
		if (p1) {
			px = p1->x;
			py = p1->y;
		} else
			px = py = 0;

		if (p2) {
			px2 = p2->x;
			py2 = p2->y;
		} else {
			px2 = py2 = 0;
		}

		REBINT xp = MIN(src_w, px+x1);
		REBINT yp = MIN(src_h, py+y1);
		REBINT xd = MIN(MAX(0, px2), m_src_w);
		REBINT yd = MIN(MAX(0, py2), m_src_h);

		if((xp<0)||(xp>=m_src_w)||(yp<0)||(yp>=m_src_h)||((!xd)&&(!yd))){
			m_rbas_dst->blend_from(pixf_src,0,0,0, 255 - m_alpha);
			return;
		}

		src_w+=xd;
		src_h+=yd;

		unsigned char* tmp = new unsigned char[src_w * src_h * 4];
		rendering_buffer rbuf_tmp(tmp, src_w, src_h, src_w * 4);
		agg_graphics::pixfmt pix(rbuf_tmp);
		ren_base rbas_tmp(pix);


		rect r1(x1,y1,xp,yp);
		rect r2(xp,y1, m_src_w,yp);
		rect r3(x1,yp,xp,m_src_h);
		rect r4(xp,yp,m_src_w,m_src_h);

		rbas_tmp.blend_from(pixf_src,&r1,0,0, 255 - m_alpha);
		rbas_tmp.blend_from(pixf_src,&r2,xd,0, 255 - m_alpha);
		rbas_tmp.blend_from(pixf_src,&r3,0,yd, 255 - m_alpha);
		rbas_tmp.blend_from(pixf_src,&r4,xd,yd, 255 - m_alpha);


		for (int y = 0; y < src_h; y++){
			rgba8* s = (rgba8*)rbuf_tmp.row(y) + xp;
			rgba8* d = s;
			for(int x = 0; x < xd;x++){
				memmove(d++,s, sizeof(rgba8));
			}
		}
		
		rgba8* s = (rgba8*)rbuf_tmp.row(yp);
		for (y = yp+1; y < yp+yd; y++){
			rgba8* d = (rgba8*)rbuf_tmp.row(y);
			memmove(
				d,
				s, 
				sizeof(rgba8) * src_w
			);
		}


		m_rbas_dst->blend_from(pix,0,0,0, 255 - m_alpha);
		delete[] tmp;
	}

	void agg_effects::fx_fit(REBINT filter, REBINT mode, REBDEC blur, unsigned char *src, int src_w, int src_h, REBINT aspect){
		REBINT w,h;
		m_gr->agg_reset();
		m_gr->agg_set_buffer(m_rbuf_dst, m_src_w, m_src_h, m_clip.x1, m_clip.y1);

		if (aspect) {
			w = (src_w * m_src_h) / src_h;
			h = (src_h * m_src_w) / src_w;
			if (w <= m_src_w)
				h = m_src_h;
			else
				w = m_src_w;
		} else {
			w = m_src_w;
			h = m_src_h;
		}

		m_gr->agg_image_filter(filter, mode, blur);

		m_gr->agg_begin_poly(0, 0);
		m_gr->agg_add_vertex(w, 0);
		m_gr->agg_add_vertex(w, h);
		m_gr->agg_add_vertex(0, h);
		m_gr->agg_end_poly_img(src, src_w, src_h);
		m_gr->agg_render(*m_rbas_dst);
	}

	void agg_effects::fx_flip(REBPAR* p, unsigned char *src, int src_w, int src_h){
		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;

				if (p->y){
					int y1 = src_h - m_clip.y1;

					if (p->x){
						for (int y = m_clip.y1; y < y2; y++){
							rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
							rgba8* s = (rgba8*)rbuf_src.row(--y1) + (src_w - 1) - m_clip.x1;
							for(int x = 0; x < w;x++){
								memmove(d++,s--, sizeof(rgba8));
							}
						}
						return;
					} else {
						for (int y = m_clip.y1; y < y2; y++){
							rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
							memmove(
								d,
								(rgba8*)rbuf_src.row(--y1) + m_clip.x1, 
								sizeof(rgba8) * w
							);
						}
						return;
					}
				}

				if (p->x){
					for (int y = m_clip.y1; y < y2; y++){
						rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
						rgba8* s = (rgba8*)rbuf_src.row(y) + (src_w - 1) - m_clip.x1;
						for(int x = 0; x < w;x++){
							memmove(d++,s--, sizeof(rgba8));
						}
					}
					return;
				}

				for (int y = m_clip.y1; y < y2; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
					memmove(
						d,
						(rgba8*)rbuf_src.row(y) + m_clip.x1, 
						sizeof(rgba8) * w
					);
				}

			}
		} else {

			if (p->x){
				for (int y = m_clip.y1; y < m_clip.y2+1; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
					rgba8* d2 = d + (m_clip_w - 1);
					for(int x = 0; x < m_clip_w / 2;x++){
						rgba8 tmp =  *d2;
						memmove(d2--,d, sizeof(rgba8));
						memmove(d++,&tmp, sizeof(rgba8));
					}
				}
			}

			if (p->y){
				src = new unsigned char[m_clip_w * 4];

				rendering_buffer rbuf_src;
				rbuf_src.attach(src,m_clip_w,1,m_clip_w * 4);

				for (int y = 0; y < m_clip_h / 2; y++){
					rgba8* s = (rgba8*)m_rbuf_dst->row(y + m_clip.y1) + m_clip.x1;
					rgba8* s2 = (rgba8*)m_rbuf_dst->row(m_clip.y2 - y) + m_clip.x1;
					memmove(
						(rgba8*)rbuf_src.row(0), 
						s, 
						sizeof(rgba8) * m_clip_w
					);
					memmove(
						s, 
						s2, 
						sizeof(rgba8) * m_clip_w
					);
					memmove(
						s2, 
						(rgba8*)rbuf_src.row(0), 
						sizeof(rgba8) * m_clip_w
					);
				}
				delete[] src;
			}
		}
	}

	void agg_effects::fx_gradcol(REBPAR* p, REBYTE* color1, REBYTE* color2, unsigned char *src, int src_w, int src_h){
		if (src) {
			unsigned char* srcg = new unsigned char[src_w * src_h * 4];
			fx_gradient(p,color1,color2, 0,0,0, srcg, src_w, src_h);
			rendering_buffer rbuf_img(srcg,src_w,src_h,src_w * 4);

			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* im = 0;

					if (y < src_h)
						im = rbuf_img.row(y) + x1;

					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						int a = pix[component_order::A];
						d[component_order::R] =  blend(MIN(255, (im[component_order::R]  * pix[component_order::R]) >> 7), d[component_order::R], a);
						d[component_order::G] =  blend(MIN(255, (im[component_order::G]  * pix[component_order::G]) >> 7), d[component_order::G], a);
						d[component_order::B] =  blend(MIN(255, (im[component_order::B]  * pix[component_order::B]) >> 7), d[component_order::B], a);
						im+=4;
						pix+=4;
						d+=4;
					} while (--len);
				}

			}

			delete srcg;

		} else {
			src = new unsigned char[m_src_w * m_src_h * 4];
			fx_gradient(p,color1,color2, 0,0,0,src, m_src_w, m_src_h);

			rendering_buffer rbuf_img(src,m_src_w,m_src_h,m_src_w * 4);

			int y2 = MIN(m_clip.y2+1, m_src_h);
			for (int y = m_clip.y1; y < y2; y++){
				int len = MIN(m_clip_w,m_src_w-m_clip.x1);
				if (len < 1) break;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				int8u* im = rbuf_img.row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					pix[component_order::R] =  blend(MIN(255, (im[component_order::R]  * pix[component_order::R]) >> 7), pix[component_order::R], a);
					pix[component_order::G] =  blend(MIN(255, (im[component_order::G]  * pix[component_order::G]) >> 7), pix[component_order::G], a);
					pix[component_order::B] =  blend(MIN(255, (im[component_order::B]  * pix[component_order::B]) >> 7), pix[component_order::B], a);
					pix+=4;
					im+=4;
				} while (--len);
			}

			delete src;
		}
	}

	void agg_effects::fx_gradient(REBPAR* p, REBYTE* color1, REBYTE* color2, unsigned char *src, int src_w, int src_h, unsigned char *dst, int dst_w, int dst_h){
		if (src){
			rendering_buffer rbuf_src(src,src_w,src_h,src_w * 4);
			agg_graphics::pixfmt pixf_src(rbuf_src);
			m_rbas_dst->blend_from(pixf_src,0,0,0, 255 - m_alpha);
		}

		rendering_buffer* rbuf_dst;
		agg_graphics::pixfmt pix;
		ren_base* rbas_dst;
		int dst_x, dst_y;

		if (dst){
			rbuf_dst = new rendering_buffer(dst, dst_w, dst_h, dst_w * 4);
			pix.attach(*rbuf_dst);
			rbas_dst = new ren_base(pix);
			dst_x = 0;
			dst_y = 0;
		} else {
			rbuf_dst = m_rbuf_dst;
			rbas_dst = m_rbas_dst;

			dst_w = (src) ? MIN(src_w,m_src_w) : m_src_w;
			dst_h = (src) ? MIN(src_h,m_src_h) : m_src_h;
			dst_x = m_clip.x1;
			dst_y = m_clip.y1;
		}

		if (m_alpha) {
			color1[3] = color2[3] = m_alpha;
		}

		unsigned char colors[13] = {2, 0,0,0,0 };
		REBDEC rot = 0;
		REBDEC stop = dst_w;
		REBINT type = 3;
		REBINT x = 0;
		REBINT y = 0;
		REBDEC offsets[1] = {-1};
		if (p->y == 0){
			if (p->x > 0){
				memcpy(&colors[5], color1, 4);
				memcpy(&colors[9], color2, 4);
			}else {
				memcpy(&colors[5], color2, 4);
				memcpy(&colors[9], color1, 4);
			}
		} else if (p->x == 0){
			rot = 90;
			stop = dst_h;
			if (p->y > 0){
				memcpy(&colors[5], color1, 4);
				memcpy(&colors[9], color2, 4);
			}else {
				memcpy(&colors[5], color2, 4);
				memcpy(&colors[9], color1, 4);
			}
		} else if (p->x == 1){
			type = 0;
			stop = sqrt((dst_w * dst_w) + (dst_h * dst_h));
			memcpy(&colors[5], color1, 4);
			memcpy(&colors[9], color2, 4);

			if (p->y < 0){
				y = dst_h;
			}
		} else if (p->x == -1){
			type = 0;
			stop = sqrt((dst_w * dst_w) + (dst_h * dst_h));
			memcpy(&colors[5], color1, 4);
			memcpy(&colors[9], color2, 4);

			if (p->y < 0){
				x = dst_w;
				y = dst_h;
			} else {
				x = dst_w;
			}
		}
		
		m_gr->agg_reset();
		m_gr->agg_set_buffer(rbuf_dst, dst_w, dst_h, dst_x, dst_y);
		m_gr->agg_pen(0,0,0,0);
		m_gr->agg_gradient_pen(type, x, y, 0, stop, rot, 1, 1, colors, offsets, 0);
		m_gr->agg_box(0, 0, dst_w, dst_h);
		m_gr->agg_render(*rbas_dst);
		if (dst) {
			delete rbuf_dst;
			delete rbas_dst;
		}
	}

	void agg_effects::fx_gradmul(REBPAR* p, REBYTE* color1, REBYTE* color2, unsigned char *src, int src_w, int src_h){
		if (src) {
			unsigned char* srcg = new unsigned char[src_w * src_h * 4];
			fx_gradient(p,color1,color2, 0 ,0 ,0 , srcg, src_w, src_h);
			fx_multiply_image(srcg, src_w, src_h, src, src_w, src_h);
			delete srcg;
		} else {
			src = new unsigned char[m_src_w * m_src_h * 4];
			fx_gradient(p,color1,color2, 0, 0 ,0 , src, m_src_w, m_src_h);
			fx_multiply_image(src, m_src_w, m_src_h);
			delete src;
		}
	}

	void agg_effects::fx_grayscale(unsigned char *src, int src_w, int src_h){
		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						int a = pix[component_order::A];
						int8u c = (pix[component_order::R] + pix[component_order::G] + pix[component_order::B]) / 3;
						d[component_order::R] = blend(c, d[component_order::R], a);
						d[component_order::G] = blend(c, d[component_order::G], a);
						d[component_order::B] = blend(c, d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					int8u c = (pix[component_order::R] + pix[component_order::G] + pix[component_order::B]) / 3;
					pix[component_order::R] = blend(c, pix[component_order::R], a);
					pix[component_order::G] = blend(c, pix[component_order::G], a);
					pix[component_order::B] = blend(c, pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_hsv(REBYTE* col, unsigned char *src, int src_w, int src_h){
		double	hu, sa, va;
		double	nh = 128, ns = 128, nv = 128;

		nh = col[0];
		ns = col[1];
		nv = col[2];

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;
					do {
						REBYTE i[] = {pix[component_order::R] , pix[component_order::G] , pix[component_order::B]};
						int a = pix[component_order::A];
						if (G_RGB_To_HSV(i,	&hu, &sa, &va)){
							hu += 180 * (nh - 127) / 128;
							if (hu < 0.0) hu = 0.0;
							if (hu > 360.0) hu = 360.0;
							sa += (ns - 128) / 128;
							if (sa < 0.0) sa = 0.0;
							if (sa > 1.0) sa = 1.0;
						} else sa = 0.0;

						va += (nv - 128) / 128;

						if (va < 0.0) va = 0.0;
						if (va > 1.0) va = 1.0;

						REBINT r = G_HSV_To_RGB(hu, sa, va);
						REBYTE* p = (REBYTE*)&r;

						d[component_order::R] = blend(p[0], d[component_order::R],a);
						d[component_order::G] = blend(p[1], d[component_order::G],a);
						d[component_order::B] = blend(p[2], d[component_order::B],a);
						pix+=4;
						d+=4;
					} while (--len);
				}
			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					REBYTE i[] = {pix[component_order::R] , pix[component_order::G] , pix[component_order::B]};
					int a = pix[component_order::A];
					if (G_RGB_To_HSV(i,	&hu, &sa, &va)){
						hu += 180 * (nh - 127) / 128;
						if (hu < 0.0) hu = 0.0;
						if (hu > 360.0) hu = 360.0;
						sa += (ns - 128) / 128;
						if (sa < 0.0) sa = 0.0;
						if (sa > 1.0) sa = 1.0;
					} else sa = 0.0;

					va += (nv - 128) / 128;

					if (va < 0.0) va = 0.0;
					if (va > 1.0) va = 1.0;

					REBINT r = G_HSV_To_RGB(hu, sa, va);
					REBYTE* p = (REBYTE*)&r;

					pix[component_order::R] = blend(p[0], pix[component_order::R], a);
					pix[component_order::G] = blend(p[1], pix[component_order::G], a);
					pix[component_order::B] = blend(p[2], pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_invert(unsigned char *src, int src_w, int src_h){
		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						int a = pix[component_order::A];
						d[component_order::R] = blend(pix[component_order::R] ^ 0xffffff, d[component_order::R], a);
						d[component_order::G] = blend(pix[component_order::G] ^ 0xffffff, d[component_order::G], a);
						d[component_order::B] = blend(pix[component_order::B] ^ 0xffffff, d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					pix[component_order::R] = blend(pix[component_order::R] ^ 0xffffff, pix[component_order::R], a);
					pix[component_order::G] = blend(pix[component_order::G] ^ 0xffffff, pix[component_order::G], a);
					pix[component_order::B] = blend(pix[component_order::B] ^ 0xffffff, pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_key(REBYTE* col, unsigned char *src, int src_w, int src_h){
		int w = MIN(src_w - m_clip.x1,m_clip_w);
		if (w > 0) {
			int h = MIN(src_h - m_clip.y1,m_clip_h);
			rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

			int y2 = m_clip.y1 + h;
			int x1 = m_clip.x1 * 4;

			for (int y = m_clip.y1; y < y2; y++){
				int len = w;
				int8u* pix = rbuf_src.row(y) + x1;
				int8u* d = m_rbuf_dst->row(y) + x1;

				do {
					if (
						!(
							(pix[component_order::R] == col[0]) &&
							(pix[component_order::G] == col[1]) &&
							(pix[component_order::B] == col[2])
						)
					){
						int a = pix[component_order::A];
						d[component_order::R] = blend(pix[component_order::R], d[component_order::R], a);
						d[component_order::G] = blend(pix[component_order::G], d[component_order::G], a);
						d[component_order::B] = blend(pix[component_order::B], d[component_order::B], a);
					}
					pix+=4;
					d+=4;
				} while (--len);
			}

		}
	}

	void agg_effects::fx_luma(int brightness, unsigned char *src, int src_w, int src_h){
		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						int a = pix[component_order::A];
						d[component_order::R] = blend(MAX(0, MIN (pix[component_order::R]+brightness,255)), d[component_order::R], a);
						d[component_order::G] = blend(MAX(0, MIN (pix[component_order::G]+brightness,255)), d[component_order::G], a);
						d[component_order::B] = blend(MAX(0, MIN (pix[component_order::B]+brightness,255)), d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
//			m_rbas_dst->for_each_pixel(fx_luma(brightness), *m_rbuf_dst);
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					int a = pix[component_order::A];
					pix[component_order::R] = blend(MAX(0, MIN (pix[component_order::R]+brightness,255)), pix[component_order::R], a);
					pix[component_order::G] = blend(MAX(0, MIN (pix[component_order::G]+brightness,255)), pix[component_order::G], a);
					pix[component_order::B] = blend(MAX(0, MIN (pix[component_order::B]+brightness,255)), pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_mix(unsigned char *img, int img_w, int img_h, unsigned char *src, int src_w, int src_h){
		rendering_buffer rbuf_img;
		agg_graphics::pixfmt pixf_img(rbuf_img);
		if (src){
			rbuf_img.attach(src,src_w,src_h,src_w * 4);
			
			m_rbas_dst->blend_from(pixf_img,0,0,0, 255 - m_alpha);
		}
		rbuf_img.attach(img,img_w,img_h,img_w * 4);
		m_rbas_dst->blend_from(pixf_img,0,0,0, 255 - m_alpha);
	}

	void agg_effects::fx_multiply(REBYTE* color, unsigned char *src, int src_w, int src_h){
		REBINT cr = color[0];
		REBINT cg = color[1];
		REBINT cb = color[2];

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;
					do {
						REBINT r = (pix[component_order::R] * cr) >> 7;
						REBINT g = (pix[component_order::G] * cg) >> 7;
						REBINT b = (pix[component_order::B] * cb) >> 7;
						REBINT a = pix[component_order::A];
						if ( r > 0xff )
							r = 0xff;
						if ( g > 0xff )
							g = 0xff;
						if ( b > 0xff )
							b = 0xff;
						
						d[component_order::R] = blend(r, d[component_order::R], a);
						d[component_order::G] = blend(g, d[component_order::G], a);
						d[component_order::B] = blend(b, d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}
			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					REBINT r = (pix[component_order::R] * cr) >> 7;
					REBINT g = (pix[component_order::G] * cg) >> 7;
					REBINT b = (pix[component_order::B] * cb) >> 7;
					REBINT a = pix[component_order::A];
					if ( r > 0xff )
						r = 0xff;
					if ( g > 0xff )
						g = 0xff;
					if ( b > 0xff )
						b = 0xff;
					
					pix[component_order::R] = blend(r, pix[component_order::R], a);
					pix[component_order::G] = blend(g, pix[component_order::G], a);
					pix[component_order::B] = blend(b, pix[component_order::B], a);
					pix+=4;
				} while (--len);
			}

		}
		
	}

	void agg_effects::fx_multiply_image(unsigned char *img, int img_w, int img_h, unsigned char *src, int src_w, int src_h){

		rendering_buffer rbuf_img(img,img_w,img_h,img_w * 4);

		REBINT strength = 255;
		
		#define CALCIT(c,b) ((unsigned char) MAX(0, MIN(255, (REBINT) ((c) + (b)))))	

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* im = 0;

					if (y < img_h)
						im = rbuf_img.row(y) + x1;

					int8u* d = m_rbuf_dst->row(y) + x1;
					
					do {
						if (im && (w - len) < (img_w - m_clip.x1)){
							REBINT a = im[component_order::A];
							REBINT a2 = pix[component_order::A];

							REBINT b = ((strength * (REBINT) im[component_order::R]) / 128) - strength;
							d[component_order::R] =  blend(CALCIT(pix[component_order::R],b), blend(pix[component_order::R], d[component_order::R], a2), a);

							b = ((strength * (REBINT) im[component_order::G]) / 128) - strength;
							d[component_order::G] =  blend(CALCIT(pix[component_order::G],b), blend(pix[component_order::G], d[component_order::G], a2), a);

							b = ((strength * (REBINT) im[component_order::B]) / 128) - strength;
							d[component_order::B] =  blend(CALCIT(pix[component_order::B],b), blend(pix[component_order::B], d[component_order::B], a2), a);

							im+=4;
						} else {
							REBINT a = pix[component_order::A];
							d[component_order::R] = blend(pix[component_order::R], d[component_order::R], a);
							d[component_order::G] = blend(pix[component_order::G], d[component_order::G], a);
							d[component_order::B] = blend(pix[component_order::B], d[component_order::B], a);
						}
						pix+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			int y2 = MIN(m_clip.y2+1, img_h);
			for (int y = m_clip.y1; y < y2; y++){
				int len = MIN(m_clip_w,img_w-m_clip.x1);
				if (len < 1) break;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				int8u* im = rbuf_img.row(y) + (m_clip.x1 << 2);
				do {
					REBINT a = im[component_order::A];

					REBINT b = ((strength * (REBINT) im[component_order::R]) / 128) - strength;
					pix[component_order::R] =  blend(CALCIT(pix[component_order::R],b), pix[component_order::R], a);

					b = ((strength * (REBINT) im[component_order::G]) / 128) - strength;
					pix[component_order::G] =  blend(CALCIT(pix[component_order::G],b), pix[component_order::G], a);

					b = ((strength * (REBINT) im[component_order::B]) / 128) - strength;
					pix[component_order::B] =  blend(CALCIT(pix[component_order::B],b), pix[component_order::B], a);
					pix+=4;
					im+=4;
				} while (--len);
			}
		}
	}

	void agg_effects::fx_reflect(REBPAR* p, unsigned char *src, int src_w, int src_h){

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				unsigned char* tmp = new unsigned char[src_w * src_h * 4];
				rendering_buffer rbuf_tmp(tmp,src_w,src_h,src_w * 4);
				
				int h2 = (src_h + 1) / 2;
				int w2 = (src_w + 1) / 2;

				if (p->y > 0){
					for (int y = 0; y < h2; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y);
						memmove(
							d,
							(rgba8*)rbuf_src.row(y), 
							sizeof(rgba8) * src_w
						);
					}

					for (y = h2; y < src_h; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y);
						memmove(
							d,
							(rgba8*)rbuf_src.row(src_h-y), 
							sizeof(rgba8) * src_w
						);
					}
				} else if (p->y < 0){
					for (int y = 0; y < h2; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y);
						memmove(
							d,
							(rgba8*)rbuf_src.row(src_h - y - 1), 
							sizeof(rgba8) * src_w
						);
					}

					for (y = h2; y < src_h; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y);
						memmove(
							d,
							(rgba8*)rbuf_src.row(y), 
							sizeof(rgba8) * src_w
						);
					}
				} else {
					for (int y = 0; y < src_h; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y);
						memmove(
							d,
							(rgba8*)rbuf_src.row(y), 
							sizeof(rgba8) * src_w
						);
					}
				}

				if (p->x > 0){
					for (int y = 0; y < src_h; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y) + src_w;
						for (int x = 0; x < w2; x++){
							memmove(
								--d,
								(rgba8*)rbuf_tmp.row(y) + x, 
								sizeof(rgba8)
							);
						}
					}

				}

				if (p->x < 0){
					for (int y = 0; y < src_h; y++){
						rgba8* d = (rgba8*)rbuf_tmp.row(y) + w2;
						for (int x = w2; x < src_w; x++){
							memmove(
								--d,
								(rgba8*)rbuf_tmp.row(y) + x, 
								sizeof(rgba8)
							);
						}
					}

				}

				int y2 = MIN(src_h, m_clip.y2+1);
				
				for (int y = m_clip.y1; y < y2; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
					memmove(
						d,
						(rgba8*)rbuf_tmp.row(y) + m_clip.x1, 
						sizeof(rgba8) * w
						);
				}

				delete[] tmp;
			}
		} else {
			if (p->y > 0){
				int h2 = (m_clip_h + 1) / 2;
				int y2 = m_clip.y1 + m_clip_h;
				for (int y = m_clip.y1 + h2; y < y2; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
					memmove(
						d,
						(rgba8*)m_rbuf_dst->row(m_clip.y1+(y2-y)) + m_clip.x1, 
						sizeof(rgba8) * m_clip_w
					);
				}
			} else if (p->y < 0){
				int h2 = (m_clip_h + 1) / 2;
				int y2 = m_clip.y1 + h2;
				for (int y = m_clip.y1; y < y2; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
					memmove(
						d,
						(rgba8*)m_rbuf_dst->row(m_clip.y1 + m_clip.y2 - y) + m_clip.x1, 
						sizeof(rgba8) * m_clip_w
					);
				}
			}

			if (p->x > 0){
				int w2 = (m_clip_w + 1) / 2;
				int y2 = m_clip.y1 + m_clip_h;
				for (int y = m_clip.y1; y < y2; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1 + w2-1;
					for (int x = 0; x < w2; x++){
						memmove(
							d,
							(rgba8*)m_rbuf_dst->row(y) + m_clip.x1 + (w2-x-1), 
							sizeof(rgba8)
						);
						d++;
					}
				}
			} else if (p->x < 0){
				int w2 = (m_clip_w + 1) / 2;
				int y2 = m_clip.y1 + m_clip_h;
				for (int y = m_clip.y1; y < y2; y++){
					rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
					for (int x = 0; x < w2; x++){
						memmove(
							d,
							(rgba8*)m_rbuf_dst->row(y) + m_clip.x1 + (m_clip_w-x-1), 
							sizeof(rgba8)
						);
						d++;
					}
				}
			}
		}
	}

	void agg_effects::fx_rotate(REBINT ang, unsigned char *src, int src_w, int src_h){
		rendering_buffer rbuf_src;

		int x = m_clip.x1;
		int y = m_clip.y1;

		int w = m_clip_w;
		int h = m_clip_h;

		if (src){
			w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				
				rbuf_src.attach(src,w,src_h,src_w * 4);

				if (ang == 90){
					int m = MIN(m_clip_h + m_clip.y1,src_w);
					int k = MIN(w + m_clip.x1,src_h);
					
					for (int i = m_clip.y1; i < m; i++){
						rgba8* d = (rgba8*)m_rbuf_dst->row(i) + m_clip.x1;

						for(int j = m_clip.x1; j < k;j++){
							rgba8* s = (rgba8*)rbuf_src.row(src_h-1-j) + i;
							memmove(d,s, sizeof(rgba8));
							d++;
						}
					}
				} else if (ang == 180){
					h = MIN(src_h - m_clip.y1,m_clip_h);
					int y1 = src_h - m_clip.y1;
					int y2 = m_clip.y1 + h;

					for (int y = m_clip.y1; y < y2; y++){
						rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
						rgba8* s = (rgba8*)rbuf_src.row(--y1) + (src_w - 1) - m_clip.x1;
						for(int x = 0; x < w;x++){
							memmove(d++,s--, sizeof(rgba8));
						}
					}
				} else if (ang == 270){
					int m = MIN(m_clip_h + m_clip.y1,src_w);
					int k = MIN(w + m_clip.x1,src_h);
					
					for (int i = m_clip.y1; i < m; i++){
						rgba8* d = (rgba8*)m_rbuf_dst->row(i) + m_clip.x1;

						for(int j = m_clip.x1; j < k;j++){
							rgba8* s = (rgba8*)rbuf_src.row(j) + (src_w - 1 - i);
							memmove(d,s, sizeof(rgba8));
							d++;
						}
					}
				} else {
					int y2 = m_clip.y1 + h;
					for (int y = m_clip.y1; y < y2; y++){
						rgba8* d = (rgba8*)m_rbuf_dst->row(y) + m_clip.x1;
						memmove(
							d,
							(rgba8*)rbuf_src.row(y) + m_clip.x1, 
							sizeof(rgba8) * w
						);
					}
				}

			}
			return;

		}

		src = new unsigned char[w * h * 4];
		rbuf_src.attach(src, w, h, w * 4);
		copy_bg(rbuf_src);
		
		if (ang == 90) {
			int k = MIN(w,h);
			for (int i = y; i < y+k; i++){
				rgba8* d = (rgba8*)m_rbuf_dst->row(i) + (x + w - 1);
				for(int j = 0; j < k;j++){
					rgba8* s = (rgba8*)rbuf_src.row(j) + (i-y);
					memmove(d,s, sizeof(rgba8));
					d--;
				}
			}
		} else if (ang == 180) {
			int gw= (x) ? 0 : m_src_w-w;
			int gh= (y) ? y : m_src_h-h;
			for (int i = gh; i < h; i++){
				rgba8* d = (rgba8*)m_rbuf_dst->row(gh+(h-i-1)) + (w - 1);
				for(int j = gw; j < w-x;j++){
					rgba8* s = (rgba8*)rbuf_src.row(i-y) + j;
					memmove(d,s, sizeof(rgba8));
					d--;
				}
			}
		} else if (ang == 270) {
			int k = MIN(w,h);
			for (int i = y; i < y+k; i++){
				rgba8* d = (rgba8*)m_rbuf_dst->row(y+((h-i)+(y-1))) + x;
				for(int j = 0; j < k;j++){
					rgba8* s = (rgba8*)rbuf_src.row(j) + (i-y);
					memmove(d,s, sizeof(rgba8));
					d++;
				}
			}
		}


		delete[] src;
	}

	void agg_effects::fx_shadow(unsigned char *src, int src_w, int src_h, REBPAR* p1,REBPAR* p2, REBYTE* color, REBDEC blur, REBINT only){

		int w = MIN(src_w,m_src_w);
		if (w > 0) {
			int y2 = MIN(src_h,m_src_h);
			int br, bg, bb, ba, px, py, sx, sy;
			
			rendering_buffer rbuf_src(src,w,src_h,src_w * 4);
			agg_graphics::pixfmt pixf_src(rbuf_src);

			unsigned bl = unsigned(blur + 0.5);
			int bld = bl*2;
			src_w+=bld;
			src_h+=bld;

			unsigned char* tmp = new unsigned char[src_w * src_h * 4];
			rendering_buffer rbuf_tmp(tmp, src_w, src_h, src_w * 4);
			agg_graphics::pixfmt pixf_tmp(rbuf_tmp);
			ren_base rbas_tmp(pixf_tmp);

			if (color){
				br = color[0];
				bg = color[1];
				bb = color[2];
				ba = color[3];
			} else {
				br = bg = bb = ba = 96;
			}

			rbas_tmp.clear(agg::rgba8(br,bg,bb,255));
			for (int y = m_clip.y1; y < y2; y++){
				int len = w;
				int8u* pix = rbuf_src.row(y) ;
				int8u* d = rbuf_tmp.row(y+bl) + (bl*4);

				do {
					REBINT a = pix[component_order::A];
					if (a != 255){
						d[component_order::A] = ba;
					}
					pix+=4;
					d+=4;
				} while (--len);
			}
			if (bl)
				stack_blur_rgba32(pixf_tmp, bl, bl);

			if (p1) {
				px = p1->x;
				py = p1->y;
			} else 
				px = py = 2;

			if (p2) {
				sx = p2->x;
				sy = p2->y;
			} else 
				sx = sy = 0;

			m_rbas_dst->blend_from(pixf_tmp,0,px-bl,py-bl, 255 - m_alpha);
			if (!only)
				m_rbas_dst->blend_from(pixf_src,0,sx,sy, 255 - m_alpha);
			delete[] tmp;
		}
	}

	void agg_effects::fx_sharpen(unsigned char *src, int src_w, int src_h){
		if (src) {
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* r1 = rbuf_src.row(y-1) + x1;
					int8u* r2 = rbuf_src.row(y) + x1;
					int8u* r3 = rbuf_src.row(y+1) + x1; 
					int8u* d = m_rbuf_dst->row(y) + x1;

					do {
						REBINT a = r2[component_order::A];
						if (y == m_clip.y1 || y == y2-1 || len == w || len == 0) {
							d[component_order::R] = blend(r2[component_order::R], d[component_order::R],a);
							d[component_order::G] = blend(r2[component_order::G], d[component_order::G],a);
							d[component_order::B] = blend(r2[component_order::B], d[component_order::B],a);
						} else {
							d[component_order::R] = blend (MAX (0, MIN ((
								((((8L * r2[component_order::R]) - r1[component_order::R]) - r3[component_order::R]) -
								(r2-4)[component_order::R]) - (r2+4)[component_order::R]
								) / 4 , 255)), d[component_order::R], a);
							d[component_order::G] = blend (MAX (0L, MIN ((
								((((8L * r2[component_order::G]) - r1[component_order::G]) - r3[component_order::G]) -
								(r2-4)[component_order::G]) - (r2+4)[component_order::G]
								) / 4 , 255)), d[component_order::G], a);
							d[component_order::B] = blend (MAX (0, MIN ((
								((((8L * r2[component_order::B]) - r1[component_order::B]) - r3[component_order::B]) -
								(r2-4)[component_order::B]) - (r2+4)[component_order::B]
								) / 4 , 255)), d[component_order::B], a);
						}
						r1+=4;
						r2+=4;
						r3+=4;
						d+=4;
					} while (--len);
				}

			}
		} else {
			int x = (m_clip.x1+1) << 2;
			for (int y = m_clip.y1+1; y < m_clip.y2; y++){
				int len = m_clip_w-2;
				int8u* r1 = m_rbuf_dst->row(y-1) + x;
				int8u* r2 = m_rbuf_dst->row(y) + x;
				int8u* r3 = m_rbuf_dst->row(y+1) + x; 
				do {
					int a = r2[component_order::A];
					r2[component_order::R] = blend ((unsigned)MAX (0, MIN ((
						((((8L * r2[component_order::R]) - r1[component_order::R]) - r3[component_order::R]) -
						(r2-4)[component_order::R]) - (r2+4)[component_order::R]
						) / 4 , 255)), r2[component_order::R], a);
					r2[component_order::G] = blend ((unsigned)MAX (0L, MIN ((
						((((8L * r2[component_order::G]) - r1[component_order::G]) - r3[component_order::G]) -
						(r2-4)[component_order::G]) - (r2+4)[component_order::G]
						) / 4 , 255)), r2[component_order::G], a);
					r2[component_order::B] = blend ((unsigned)MAX (0, MIN ((
						((((8L * r2[component_order::B]) - r1[component_order::B]) - r3[component_order::B]) -
						(r2-4)[component_order::B]) - (r2+4)[component_order::B]
						) / 4 , 255)), r2[component_order::B], a);

					r1+=4;
					r2+=4;
					r3+=4;
				} while (--len > 0);
			}
		}
	}

	void agg_effects::fx_tile(unsigned char *src, int src_w, int src_h, REBINT ox,REBINT oy){
		rendering_buffer rbuf_src(src,src_w,src_h,src_w * 4);
		agg_graphics::pixfmt pixf_src(rbuf_src);

		int w = MIN(m_src_w - m_clip.x1,m_clip_w);
		int idx = 0;
		if (w > 0) {
			int a = 255 - m_alpha;
			int h = (m_clip.y1) ? m_src_h : MIN(m_src_h - m_clip.y1,m_clip_h);

			w = (m_clip.x1) ? m_src_w : w;

			for (int y = (-oy > 0) ? -oy : -oy % src_h; y < h; y+=src_h)
				for (int x = (-ox > 0) ? -ox : -ox % src_w; x < w; x+=src_w)
					m_rbas_dst->blend_from(pixf_src,0,x,y, a);
		}
	}

	void agg_effects::fx_tint(REBINT tint, unsigned char *src, int src_w, int src_h){
	
		if (tint < -180) tint = -180;
		if (tint > 180)  tint =  180;

		REBDEC theta = (3.14159 * (double) tint) / 180.0;
		REBINT c = (REBINT)(256. * cos (theta));
		REBINT s = (REBINT)(256. * sin (theta));

		if (src){
			int w = MIN(src_w - m_clip.x1,m_clip_w);
			if (w > 0) {
				int h = MIN(src_h - m_clip.y1,m_clip_h);
				rendering_buffer rbuf_src(src,w,src_h,src_w * 4);

				int y2 = m_clip.y1 + h;
				int x1 = m_clip.x1 * 4;

				for (int y = m_clip.y1; y < y2; y++){
					int len = w;
					int8u* pix = rbuf_src.row(y) + x1;
					int8u* d = m_rbuf_dst->row(y) + x1;
					do {
						REBINT a = pix[component_order::A];
						REBINT r = ((REBINT) (70  * (REBINT) pix[component_order::R] - 59*(int)pix[component_order::G] - 11*(int)pix[component_order::B]) / 100);
						REBINT b = ((REBINT) (-30 * (REBINT) pix[component_order::R] - 59*(int)pix[component_order::G] + 89*(int)pix[component_order::B]) / 100);
						REBINT y = ((REBINT) (30  * (REBINT) pix[component_order::R] + 59*(int)pix[component_order::G] + 11*(int)pix[component_order::B]) / 100);
						
						REBINT by = (c*b - s*r)/256;
						REBINT ry = (s*b + c*r)/256;
						REBINT gy = ((int)(-51*r - 19*by)/100);
						
						d[component_order::R] = blend(MAX( 0, MIN(ry + y, 255)), d[component_order::R], a);
						d[component_order::G] = blend(MAX( 0, MIN(gy + y, 255)), d[component_order::G], a);
						d[component_order::B] = blend(MAX( 0, MIN(by + y, 255)), d[component_order::B], a);
						pix+=4;
						d+=4;
					} while (--len);
				}
			}
		} else {
			for (int y = m_clip.y1; y < m_clip.y2+1; y++){
				int len = m_clip_w;
				int8u* pix = m_rbuf_dst->row(y) + (m_clip.x1 << 2);
				do {
					REBINT a = pix[component_order::A];
					REBINT r = ((REBINT) (70  * (REBINT) pix[component_order::R] - 59*(int)pix[component_order::G] - 11*(int)pix[component_order::B]) / 100);
					REBINT b = ((REBINT) (-30 * (REBINT) pix[component_order::R] - 59*(int)pix[component_order::G] + 89*(int)pix[component_order::B]) / 100);
					REBINT y = ((REBINT) (30  * (REBINT) pix[component_order::R] + 59*(int)pix[component_order::G] + 11*(int)pix[component_order::B]) / 100);
					
					REBINT by = (c*b - s*r)/256;
					REBINT ry = (s*b + c*r)/256;
					REBINT gy = ((int)(-51*r - 19*by)/100);
					
					pix[component_order::R] = blend(MAX( 0, MIN(ry + y, 255)), pix[component_order::R], a);
					pix[component_order::G] = blend(MAX( 0, MIN(gy + y, 255)), pix[component_order::R], a);
					pix[component_order::B] = blend(MAX( 0, MIN(by + y, 255)), pix[component_order::R], a);
					pix+=4;
				} while (--len);
			}

		}
	}
}
