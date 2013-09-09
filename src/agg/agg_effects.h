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

#include "agg_color_rgba.h"
namespace agg
{


	class agg_effects
    {
		public:
#ifdef ENDIAN_BIG
		typedef pixfmt_argb32 pixfmt;
		typedef order_argb component_order;
#else
		typedef pixfmt_bgra32 pixfmt;
		typedef order_bgra component_order;
#endif
		typedef renderer_base<pixfmt> ren_base;

		agg_effects();
		~agg_effects();

		void init(ren_base& rbas, rendering_buffer& rbuf, int w, int h, int x = 0, int y = 0, int alpha = 0);
		void get_offset(REBPAR* p);
		void get_size(REBPAR* p);
		void copy_bg(rendering_buffer& dst);
		AGG_INLINE int8u blend(int8u s, int8u d, int8u alpha);

		void fx_add(unsigned char *img, int img_w, int img_h, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_alphamul(unsigned char *src, int src_w, int src_h, int alpha);
		void fx_blur(unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_colorify(REBYTE* color, REBINT depth, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_colorize(REBYTE* color, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_contrast(REBINT con, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_convolve(double* filter, double divisor, int offset, int gray, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_crop(REBPAR* p1, REBPAR* p2, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_difference(REBYTE* col, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_difference_image(unsigned char *img, int img_w, int img_h, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_extend(REBPAR* p1, REBPAR* p2, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_fit(REBINT filter, REBINT mode, REBDEC blur, unsigned char *src, int src_w, int src_h, REBINT aspect = 0);
		void fx_flip(REBPAR* p, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_gradcol(REBPAR* p, REBYTE* color1, REBYTE* color2, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_gradient(REBPAR* p, REBYTE* color1, REBYTE* color2,unsigned char *src = 0, int src_w = 0, int src_h = 0, unsigned char *dst = 0, int dst_w = 0, int dst_h = 0);
		void fx_gradmul(REBPAR* p, REBYTE* color1, REBYTE* color2, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_grayscale(unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_hsv(REBYTE* col, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_invert(unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_key(REBYTE* col, unsigned char *src, int src_w, int src_h);
		void fx_luma(REBINT brightness, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_mix(unsigned char *img, int img_w, int img_h, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_multiply(REBYTE* color, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_multiply_image(unsigned char *img, int img_w, int img_h, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_reflect(REBPAR* p, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_rotate(REBINT ang, unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_shadow(unsigned char *src, int src_w, int src_h, REBPAR* p1, REBPAR* p2, REBYTE* color, REBDEC blur, REBINT only);
		void fx_sharpen(unsigned char *src = 0, int src_w = 0, int src_h = 0);
		void fx_tile(unsigned char *src, int src_w, int src_h, REBINT x,REBINT y);
		void fx_tint(REBINT tint, unsigned char *src = 0, int src_w = 0, int src_h = 0);

		private:
		ren_base* m_rbas_dst;
		rendering_buffer* m_rbuf_dst;
		agg_graphics* m_gr;

		int m_src_x;
		int m_src_y;
		int m_src_w;
		int m_src_h;
		rect m_clip;
		int m_clip_w;
		int m_clip_h;
		int m_alpha;
	};
}
