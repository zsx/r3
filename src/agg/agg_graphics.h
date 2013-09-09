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
**  Summary: REBOL DRAW dialect graphics renderer
**  Author:  Richard Smolak
**
***********************************************************************/

#include "reb-host.h"

//basic api
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"
#include "agg_renderer_scanline.h"
#include "agg_pixfmt_rgba_rebol.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_color_rgba.h"

//boolean algebra
#include "agg_scanline_boolean_algebra.h"

//gradients
#include "agg_span_gradient.h"

//affine transformations
#include "agg_trans_affine.h"
#include "agg_conv_transform.h"
#include "agg_trans_bilinear.h"
#include "agg_trans_perspective.h"

//curves
#include "agg_conv_curve.h"

//gouraud support
#include "agg_span_allocator.h"
#include "agg_span_gouraud_rgba.h"
#include "agg_span_solid.h"

//arc & ellipse support
#include "agg_bezier_arc.h"

//rounded rectangle
#include "agg_rounded_rect.h"

//path storage
#include "agg_path_storage.h"

//bspline
#include "agg_conv_bspline.h"

//dash and stroke & marker
#include "agg_conv_stroke.h"
#include "agg_conv_dash.h"

#include "agg_conv_marker.h"
#include "agg_arrowhead.h"
#include "agg_vcgen_markers_term.h"
//
#include "agg_rasterizer_outline_aa.h"
#include "agg_pattern_filters_rgba_rebol.h"
#include "agg_renderer_outline_image_rebol.h"

//image filtering
#include "agg_span_image_filter_rgba_rebol.h"
#include "agg_span_pattern_filter_rgba_rebol.h"
#include "agg_span_pattern_resample_rgba_rebol.h"
#include "agg_span_image_resample_rgba_rebol.h"
#include "agg_span_interpolator_linear.h"
#include "agg_span_interpolator_persp.h"
#include "agg_span_interpolator_trans.h"
#include "agg_span_subdiv_adaptor.h"

#include "agg_blur.h"

//REBOL stuff
// #include "reb-config.h"
// #include "reb-c.h"
// #include "reb-defs.h"
// #include "reb-gob.h"
// #undef IS_ERROR

namespace agg
{
  #include "host-ext-draw.h"

	template<class VertexSource>
	double* getVertices(VertexSource& pth, unsigned p, double c = 0){
		path_storage tmp;
		tmp.add_path(pth,p);

		double* vertices = new double[tmp.total_vertices() * 2];
		double x;
		double y;

		tmp.rewind(0);
		int idx = 0;
		while(tmp.vertex(&x, &y) != 0){
			vertices[idx++] = x + c;
			vertices[idx++] = y + c;
		}

		return vertices;
	}

	//gradient support

	class gradient_polymorphic_wrapper_base
	{
	public:
		virtual int calculate(int x, int y, int) const = 0;
		virtual void mode(int mode) = 0;
	};

	template<class GradientF>
	class gradient_polymorphic_wrapper : public gradient_polymorphic_wrapper_base
	{
	public:
		gradient_polymorphic_wrapper() : m_repeat_adaptor(m_gradient),m_reflect_adaptor(m_gradient), m_mode(0) {}
		virtual int calculate(int x, int y, int d) const
		{
			if (d==0) d=1; // protect from divide by zero in repeat and reflect cases
			switch (m_mode) {
				case W_DRAW_REPEAT: //repeat
					return m_repeat_adaptor.calculate(x, y, d);
				case W_DRAW_REFLECT: //reflect
					return m_reflect_adaptor.calculate(x, y, d);
				default: //normal
				return m_gradient.calculate(x, y, d);
			}
		}
		virtual void mode(int mode) {
			m_mode = mode;
		}
	private:
		int m_mode;
		GradientF m_gradient;
		gradient_repeat_adaptor<GradientF> m_repeat_adaptor;
		gradient_reflect_adaptor<GradientF> m_reflect_adaptor;
	};


	struct color_function_profile
	{
		color_function_profile() {}

		color_function_profile(const rgba8* colors) :
			m_colors(colors) {}

		static unsigned size() { return 256; }
		const rgba8& operator [] (unsigned v) const
		{
			return m_colors[v];
		}

		const rgba8* m_colors;
	};


	enum render_types
	{
		RT_NONE = 0,
		RT_FILL,
		RT_GORAUD,
		RT_IMAGE,
		RT_PERSPECTIVE_IMAGE,
		RT_GRADIENT,
		RT_TEXT,
		RT_EFFECT,
		RT_CLIPPING

	};

	enum filter_types
	{
		FT_NEAREST = 0,
		FT_BILINEAR,
		FT_BICUBIC,
		FT_GAUSSIAN
	};

	enum filter_modes
	{
		FM_NORMAL = 0,
		FM_RESAMPLE
	};

	enum pattern_modes
	{
		PM_NORMAL = 0,
		PM_REPEAT,
		PM_REFLECT
	};


	// Basic path attributes
	struct path_attributes {
		unsigned								index;

		//pens
		rgba8									pen;
		rgba8									fill_pen;
		rgba8									line_pattern_pen;

		//pen colors
		rgba8									g_color1;
		rgba8									g_color2;
		rgba8									g_color3;

		//buffers
		unsigned char *							img_buf;
		unsigned char *							pen_img_buf;
		unsigned char *							fill_pen_img_buf;

		//buffers' sizes
		int										pen_img_buf_x;
		int										pen_img_buf_y;
		int										fill_pen_img_buf_x;
		int										fill_pen_img_buf_y;

		//matrices
		trans_affine							mtx;
		trans_affine							post_mtx;

		//coords
		double									coord_x;
		double									coord_y;
		double									coord_x2;
		double									coord_y2;
		double									coord_x3;
		double									coord_y3;

		//clip
		double									clip_x1;
		double									clip_y1;
		double									clip_x2;
		double									clip_y2;

		//filters
		int										img_filter_type;
		int										img_filter_mode;
		double									img_filter_arg;

		int										pattern_mode;

		//main modes
		bool									stroked;
		int										filled;
		bool									dashed;
		double *								dash_array;

		//line width
		double									line_width;
		int										line_width_mode; //0-scale 1-no scale

		//misc
		filling_rule_e							fill_rule;
		bool									anti_aliased;

		//arrows
		int										arrow_head;
		int										arrow_tail;
		rgba8									arrow_color;

		//gradients
		gradient_polymorphic_wrapper_base *		gradient;
		int										gradient_mode;
		rgba8 *									colors;

		//line/stroke cap and join
		line_join_e				stroke_line_join;
		line_cap_e				stroke_line_cap;
		line_join_e				dash_line_join;
		line_cap_e				dash_line_cap;

		//text or effect
		REBSER *								block;


		//constructor
		path_attributes(unsigned idx = 0) :
			index(idx),
			pen(rgba8(255,255,255,0)),
			fill_pen(rgba8(0,0,0,0)),
			line_pattern_pen(rgba8(0,0,0,0)),
			g_color1(rgba8(0,0,0,0)),
			g_color2(rgba8(0,0,0,0)),
			g_color3(rgba8(0,0,0,0)),
			img_buf(0),
			pen_img_buf(0),
			fill_pen_img_buf(0),
			pen_img_buf_x(0),
			pen_img_buf_y(0),
			fill_pen_img_buf_x(0),
			fill_pen_img_buf_y(0),
			mtx(),
			post_mtx(),
			coord_x(0.0),
			coord_y(0.0),
			coord_x2(0.0),
			coord_y2(0.0),
			coord_x3(0.0),
			coord_y3(0.0),
			clip_x1(0.0),
			clip_y1(0.0),
			clip_x2(0.0),
			clip_y2(0.0),
			img_filter_type(FT_BILINEAR),
			img_filter_mode(FM_NORMAL),
			img_filter_arg(1.0),
			pattern_mode(0),
			stroked(true),
			filled(0),
			fill_rule(fill_non_zero),
			dashed(false),
			dash_array(0),
			anti_aliased(true),
			arrow_head(0),
			arrow_tail(0),
			arrow_color(rgba8(255,255,255,0)),
			gradient(0),
			colors(0),
			gradient_mode(0),
			stroke_line_join(round_join),
			stroke_line_cap(round_cap),
			dash_line_join(round_join),
			dash_line_cap(butt_cap),
			line_width(1),
			line_width_mode(0),
			block(0)
		{
		}
	};

	class agg_graphics
    {
	public:

		typedef rendering_buffer ren_buf;
#ifdef ENDIAN_BIG
		typedef pixfmt_argb32 pixfmt;
		typedef pixfmt_argb32_pre pixfmt_pre;
		typedef order_argb component_order;
#else
#ifdef TO_ANDROID_ARM
		typedef pixfmt_rgba32 pixfmt;
		typedef pixfmt_rgba32_pre pixfmt_pre;
		typedef order_rgba component_order;
#else		
		typedef pixfmt_bgra32 pixfmt;
		typedef pixfmt_bgra32_pre pixfmt_pre;
		typedef order_bgra component_order;
#endif
#endif
		typedef renderer_base<pixfmt> ren_base;
		typedef renderer_base<pixfmt_pre> ren_base_pre;
		typedef renderer_scanline_aa_solid <ren_base> renderer_aa_solid;
		typedef renderer_scanline_bin_solid<ren_base> renderer_bin;
		typedef rasterizer_scanline_aa<> ras_aa;

		typedef pod_deque<path_attributes> attr_storage;
		typedef pod_deque<path_attributes> stack_storage;


		typedef span_interpolator_linear<> interp_linear;
		typedef trans_perspective trans_persp;
		typedef span_interpolator_linear_subdiv<trans_persp> interp_trans;
		typedef span_interpolator_persp_exact<> interp_persp;

		typedef conv_transform<path_storage> trans_path;
		typedef conv_curve<trans_path> curved_trans;
		typedef conv_stroke<curved_trans, vcgen_markers_term> curved_stroked;
		typedef conv_dash<curved_trans> curved_dashed;
		typedef conv_stroke<curved_dashed> curved_dashed_stroked;

		typedef conv_marker<vcgen_markers_term, arrowhead> arrows;

		~agg_graphics();
		agg_graphics(ren_buf* buf, int w, int h, int x=0, int y=0);

		void agg_size(REBPAR* p);
		void agg_set_buffer(ren_buf* buf,int w, int h, int x, int y);
		rendering_buffer* agg_buffer();
		template <class Renderer>
		void agg_render_sbool(Renderer& ren, bool mode);
		void agg_render(ren_base renb);
		void agg_set_gamma(double g);
		void agg_set_clip(double origX,double origY, double margX, double margY);
		void agg_clear_buffer(unsigned int r, unsigned int g, unsigned int b, unsigned int a);
		void agg_resize_buffer(unsigned char *buf, int width, int height);
		void agg_transform(double rot, double ctrX, double ctrY, double sclX ,double sclY, double trX, double trY);
		void agg_rotate(double ang);
		void agg_translate(double trX, double trY);
		void agg_scale(double sclX ,double sclY);
		void agg_skew(double sx ,double sy);
		void agg_line_width(double w, int mode);
		void agg_pen(unsigned r,unsigned g,unsigned b,unsigned a = 0, unsigned char *img_buf = 0, int x = 0, int y = 0);
		void agg_fill_pen(unsigned r,unsigned g,unsigned b,unsigned a = 0, unsigned char *img_buf = 0, int x = 0, int y = 0);
		void agg_fill_rule(filling_rule_e mode);
		void agg_reset_gradient_pen();
		void agg_gradient_pen(int grad, double oftX, double oftY, double begin, double end, double ang, double scX,double scY, unsigned char *colors, REBDEC* offsets, int mode = 0);
		void agg_line_pattern_pen(unsigned r,unsigned g,unsigned b,unsigned a = 0);
		void agg_line_pattern(REBYTE* color, double *dash_array);
		void agg_begin_path();
		void agg_get_attributes(path_attributes& attr);
		void agg_set_attributes(path_attributes& attr);
//		bool agg_exists_path();
		void agg_reset();
		void agg_line(double x1, double y1, double x2, double y2);
		void agg_ellipse(double x,  double y, double rx, double ry);
		void agg_arrows(REBYTE* color, int head, int tail);
		void agg_anti_alias(bool mode);
		void agg_image_filter(int filter, int mode, double blur);
		void agg_arc(double x,  double y, double rx, double ry, double ang1, double ang2, int closed = 1);
		void agg_box(double x1, double y1, double x2, double y2);
		void agg_rounded_rect(double x1, double y1, double x2, double y2, double r);
		void agg_begin_poly (double x,  double y);
		void agg_add_vertex (double x,  double y);
		void agg_curve3 (double x1, double y1, double x2,  double y2, double x3,  double y3);
		void agg_curve4 (double x1, double y1, double x2,  double y2, double x3,  double y3, double x4,  double y4);
		void agg_end_poly ();
		void agg_end_bspline (int step, int closed = 0);
		void agg_end_poly_img(unsigned char *img_buf, int sizX, int sizY); //, int pw = 0, int ph = 0, int ox = 0, int oy = 0, int outline = 0, int pattern = 0, int r1 = 0, int g1 = 0, int b1 = 0, int a1 = 0);
		void agg_gtriangle(REBXYF p1, REBXYF p2, REBXYF p3, REBYTE* c1, REBYTE* c2, REBYTE* c3, double d = 0.0);
		void agg_image(unsigned char *img_buf, double oftX, double oftY, int sizX, int sizY); //, int outline = 0, int r1 = 0, int g1 = 0, int b1 = 0, int a1 = 0);//, double ang, int ctrX, int ctrY);
        void agg_image_options(int r, int g, int b, int a ,int border);
        void agg_image_pattern(int mode, double x, double y, double w, double h);

		int agg_push_mtx();
		int agg_pop_mtx();
		void agg_reset_mtx();
		void agg_invert_mtx();
		void agg_set_mtx(double mtx[]);
		void agg_init();
		void agg_stroke_join(line_join_e mode);
		void agg_dash_join(line_join_e mode);
		void agg_stroke_cap(line_cap_e mode);
		void agg_dash_cap(line_cap_e mode);

		void agg_effect(REBPAR* p1, REBPAR* p2, REBSER* block);
		void agg_text(REBINT vectorial, REBXYF* p1, REBXYF* p2, REBSER* block);

		//PATH sub-commands
        void agg_path_move(int rel, double x, double y);   // M, m
        void agg_path_line(int rel, double x,  double y);  // L, l
        void agg_path_hline(int rel, double x);            // H, h
        void agg_path_vline(int rel, double y);            // V, v
        void agg_path_quadratic_curve(int rel, double x1, double y1,                   // Q, q
                    double x,  double y);
        void agg_path_quadratic_curve_to(int rel, double x, double y);    // T, t
        void agg_path_cubic_curve(int rel, double x1, double y1,                   // C, c
                    double x2, double y2,
                    double x,  double y);
        void agg_path_cubic_curve_to(int rel, double x2, double y2,                   // S, s
                    double x,  double y);
		void agg_path_arc(int rel, double rx, double ry,                   // A, a
							double angle,
                              int large_arc,
                              int sweep,
							  double x,  double y);
        void agg_path_close();                               // Z, z


		path_attributes& curr_attributes();
		path_storage* get_path_storage();

	private:
		//rendering buffer
		ren_buf* m_buf;

		// Rasterizer & scanlines
		rasterizer_scanline_aa<> m_ras;
		rasterizer_scanline_aa<> m_ras_bool;
		scanline_p8 m_p_sl;
		scanline_u8 m_u_sl;
		scanline_u8 m_u_sl1_bool;
		scanline_u8 m_u_sl2_bool;
		scanline_u8 m_u_sl_result_bool;
		double m_gamma;

		//attribute storage
		attr_storage m_attributes;

		//path storage
		path_storage m_path;
		path_storage m_path_bool;
//		bool m_path_opened;

		//transformation matrices
		trans_affine m_resize_mtx;
		trans_affine m_post_mtx;
		trans_affine m_output_mtx;
		trans_affine m_img_mtx;
		trans_affine m_grad_mtx;
		stack_storage m_stack;
		double* m_mtx_store;

		//pipeline

		trans_path m_trans;
		curved_trans m_trans_curved;
		curved_stroked m_stroke;
		curved_dashed m_dash;
		curved_dashed_stroked m_dashed_stroke;

		//interpolators
		interp_linear m_interpolator_linear;
		trans_persp m_trans_perspective;
		interp_trans m_interpolator_trans;
		interp_persp m_interpolator_persp;

		//gradient stuff
		unsigned char m_grad_gamma[256];
		gradient_polymorphic_wrapper_base* m_gradient;
		gradient_polymorphic_wrapper<gradient_circle>   gr_circle;
		gradient_polymorphic_wrapper<gradient_diamond>  gr_diamond;
		gradient_polymorphic_wrapper<gradient_x>        gr_x;
		gradient_polymorphic_wrapper<gradient_xy>       gr_xy;
		gradient_polymorphic_wrapper<gradient_sqrt_xy>  gr_sqrt_xy;
		gradient_polymorphic_wrapper<gradient_conic>    gr_conic;
		rgba8 * m_color_profile;
		int m_gradient_mode;

		//for resizing
		int m_initial_width;
		int m_initial_height;
		int m_actual_width;
		int m_actual_height;
		int m_offset_x;
		int m_offset_y;
		int m_mtx_offset_x;
		int m_mtx_offset_y;
		
		double m_ratio_x;
		double m_ratio_y;

		unsigned char * m_pen_img_buf;
		unsigned char * m_fill_pen_img_buf;
		int m_pen_img_buf_x;
		int m_pen_img_buf_y;
		int m_fill_pen_img_buf_x;
		int m_fill_pen_img_buf_y;

		double m_coord_x;
		double m_coord_y;

		double m_line_width;
		int m_line_width_mode;

		//pens
		rgba8 m_pen;
		rgba8 m_fill_pen;
		rgba8 m_line_pattern_pen;

		//modes
		int m_filled;
		bool m_stroked;
		bool m_dashed;
		bool m_anti_aliased;
		double* m_dash_array;
		int m_img_filter_type;
		int m_img_filter_mode;
		double m_img_filter_arg;
		filling_rule_e m_fill_rule;
		int m_pattern_mode;

		//stroke settings
		line_cap_e m_stroke_cap;
		line_join_e m_stroke_join;
		line_cap_e m_dash_cap;
		line_join_e m_dash_join;

		//arrow
		arrowhead m_ah;
		arrows m_arrow;
		int m_arrow_head;
		int m_arrow_tail;
		rgba8 m_arrow_color;

		//image
		rgba8 m_img_key_color;
		double m_img_border;
		double m_pattern_x;
		double m_pattern_y;
        double m_pattern_w;
		double m_pattern_h;

	};

	//image color keying
	struct color_key
	{
	   color_key(rgba8 key) :  c(key) {}
	   void operator() (int8u* pixel)
	   {
		  if(pixel[agg_graphics::component_order::R] == c.r && pixel[agg_graphics::component_order::G] == c.g && pixel[agg_graphics::component_order::B] == c.b)
		  {
			  pixel[agg_graphics::component_order::R] = 0;
			  pixel[agg_graphics::component_order::G] = 0;
			  pixel[agg_graphics::component_order::B] = 0;
			  pixel[agg_graphics::component_order::A] = 0; // transparent
		  }
	   }
	   rgba8 c;
	};

}
