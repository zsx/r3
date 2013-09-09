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
**  Summary: REBOL TEXT dialect renderer
**  Author:  Richard Smolak
**
***********************************************************************/

#include <stdio.h>
#include "agg_basics.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_u.h"
#include "agg_scanline_bin.h"
#include "agg_renderer_scanline.h"
#include "agg_renderer_primitives.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_conv_curve.h"
//#include "agg_conv_contour.h"
#include "agg_pixfmt_rgba_rebol.h"

#ifdef AGG_WIN32_FONTS
#include "agg_font_win32_tt.h"
#endif
#ifdef AGG_FREETYPE
    #ifndef max
    #define max(a,b) ((a)>(b)?(a):(b))
    #endif
    #ifndef min
    #define min(a,b) ((a)<(b)?(a):(b))
    #endif

	#include "agg_font_freetype.h"
#endif

#include "host-ext-text.h"

//default font name setup

#ifdef TO_WIN32
	#define FONT_NAME (REBCHR*)L"Arial"
#endif	
#ifdef TO_ANDROID_ARM
	#define FONT_NAME (REBCHR*)"/system/fonts/DroidSans.ttf"
#endif	
#ifdef TO_OSXI
	#define FONT_NAME (REBCHR*)"/System/Library/Fonts/LucidaGrande.ttc"
#endif

//fallback font
#ifndef FONT_NAME
	#define FONT_NAME (REBCHR*)"/system/fonts/FreeSans.ttf"
#endif


namespace agg
{

	enum {
		DRAW_TEXT,
		OFFSET_TO_CARET,
		CARET_TO_OFFSET,
		SIZE_TEXT
	};

	//caret info
	typedef struct cinfo {
		// line height ascent and descend
		int lh_asc;
		int lh_desc;
		// line height y spacing
		int lh_space_y;
		// current character height ascent and descend
		int ch_asc;
		int ch_desc;
	} CINFO;

	//text highlight info
	typedef struct hinfo {
		REBPAR hStart;
		REBPAR hEnd;
	} HINFO;

	typedef struct font {
		REBCHR *name;
		REBCNT name_free;
		REBINT bold;
		REBINT italic;
		REBINT underline;
		REBINT size;
		REBYTE* color;
		REBINT offset_x;
		REBINT offset_y;
		REBINT space_x;
		REBINT space_y;
		REBINT shadow_x;
		REBINT shadow_y;
		REBYTE* shadow_color;
		REBINT shadow_blur;

		~font() {
			delete [] color;
			delete [] shadow_color;
		}

		font() :
			name(FONT_NAME), //"Arial"
			name_free(FALSE),
			bold(0),
			italic(0),
			underline(0),
			size(LOG_COORD_Y(12)),
			offset_x(2),
			offset_y(2),
			space_x(0),
			space_y(0),
			shadow_x(0),
			shadow_y(0),
			shadow_blur(0)
		{
			color = new unsigned char [4];
			shadow_color = new unsigned char [4];
		}
	} FONT;

	typedef struct para {
		int origin_x;
		int origin_y;
		int margin_x;
		int margin_y;
		int indent_x;
		int indent_y;
		int tabs;
		int wrap;
		float scroll_x;
		float scroll_y;
		int align;
		int valign;
		para():
			tabs(40),
			wrap(1),
			origin_x(2),
			origin_y(2),
			margin_x(2),
			margin_y(2),
			indent_x(0),
			indent_y(0),
			scroll_x(0),
			scroll_y(0),
			align(W_TEXT_LEFT),
			valign(W_TEXT_TOP)
		{
		}
	} PARA;

	union tmp_val {
		REBPAR  pair;
	};
#ifdef AGG_FREETYPE
    typedef struct tagSIZE {
        long cx;
        long cy;
    } SIZE;
#endif
	struct text_attributes {
		unsigned index;
		REBCHR *name;
		bool name_free;
		int bold;
		int italic;
		int underline;
		int size;
		rgba8 color;
		int offset_x;
		int offset_y;
		int space_x;
		int space_y;
		int shadow_x;
		int shadow_y;
		rgba8 shadow_color;
		REBINT shadow_blur;
		wchar_t *text;
		bool text_free;
		bool isPara;
		PARA para;
		long asc;
		long desc;
		long char_height;

		text_attributes(unsigned idx = 0) :
			index(idx),
			name(0),
			name_free(false),
			bold(0),
			italic(0),
			underline(0),
			size(LOG_COORD_Y(12)),
			color(rgba8(255,0,0,255)),
			offset_x(2),
			offset_y(2),
			space_x(0),
			space_y(0),
			shadow_x(0),
			shadow_y(0),
			shadow_color(rgba8(0,0,0,128)),
			shadow_blur(0),
			text(0),
			isPara(false),
			asc(0),
			desc(0),
			char_height(0)
		{
			para.tabs=40;
			para.wrap=0;
			para.origin_x=2;
			para.origin_y=2;
			para.margin_x=0;
			para.margin_y=0;
			para.indent_x=0;
			para.indent_y=0;
			para.scroll_x=0;
			para.scroll_y=0;
			para.align=W_TEXT_LEFT;
			para.valign=W_TEXT_TOP;
		}
	};


	class rich_text
	{
		public:
			typedef rendering_buffer ren_buf;
#ifdef ENDIAN_BIG			
			typedef pixfmt_argb32 pixfmt_type;
#else
#ifdef TO_ANDROID_ARM
			typedef pixfmt_rgba32 pixfmt_type;
#else
			typedef pixfmt_bgra32 pixfmt_type;
#endif
#endif
			typedef renderer_base<pixfmt_type> base_ren_type;
			typedef renderer_scanline_aa_solid<base_ren_type> renderer_solid;
			typedef renderer_scanline_bin_solid<base_ren_type> renderer_bin;
#ifdef AGG_WIN32_FONTS
			typedef font_engine_win32_tt_int32 font_engine_type;
#endif
#ifdef AGG_FREETYPE
            typedef font_engine_freetype_int32 font_engine_type;
#endif
			typedef font_cache_manager<font_engine_type> font_manager_type;

			// Pipeline to process the vectors glyph paths (curves + contour)
			typedef conv_curve<font_manager_type::path_adaptor_type> conv_curve_type;
//			typedef conv_contour<conv_curve_type> conv_contour_type;

			typedef pod_deque<text_attributes> attr_storage;
			typedef pod_deque<agg::path_attributes> path_attr_storage;

			~rich_text();

#ifdef AGG_WIN32_FONTS
			rich_text(HDC dc);
#endif
#ifdef AGG_FREETYPE
			rich_text();
#endif
			void rt_pair(REBPAR p);

			void rt_attach_buffer(ren_buf* buf, int w, int h, int x=0, int y=0);
			void rt_push(int idx = 0);
			void rt_reset();
			void rt_set_font(font* fnt);
			font* rt_get_font();
			void rt_set_para(para* par);
			para* rt_get_para();
			void rt_attr_to_font (text_attributes& attr);
			int rt_drop(unsigned int idx = 1);
			int rt_set_text(REBCHR* text, REBCNT gc);
			int rt_text_mode(int mode =0);
			int rt_draw_text(int mode = DRAW_TEXT, REBXYF* offset=0);

			void rt_offset_to_caret(REBXYF offset, REBINT *element, REBINT *position);
			void rt_caret_to_offset(REBXYF* offset, REBINT element, REBINT position);

			int rt_set_caret(REBXYF offset);

			int rt_scroll(REBXYF offset);

			void rt_set_hinfo(REBXYF highlightStart, REBXYF highlightEnd);

			int rt_size_text(REBXYF* size);
			int rt_text_height(int idx);
			int rt_max_line_height(int idx);
			int rt_line_width(int idx, int start = 0);

			void rt_set_graphics(agg_graphics* graphics);
			void rt_set_clip(int x1, int y1,int x2, int y2, int wx = 0, int wy = 0);
			void rt_color_change();

			text_attributes& rt_curr_attributes();

			cinfo caret_info;

			int debug;

		private:
#ifdef AGG_FREETYPE
            void GetTextExtentPointFT(const wchar_t* string, int c, SIZE *size);
#endif
			ren_buf*					m_rbuf;
			font_engine_type			m_feng;
			font_manager_type			m_fman;
			glyph_rendering				m_gren;
#ifdef AGG_WIN32_FONTS
			HDC							m_dc;
#endif
			conv_curve_type				m_curves;
//			conv_contour_type			m_contour;
			unsigned char*				m_img_buf;
			int							m_buf_offset_x;
			int							m_buf_offset_y;
			int							m_buf_size_x;
			int							m_buf_size_y;
			int							m_wrap_size_x;
			int							m_wrap_size_y;
			int							m_right_hang;

			int							m_clip_x1;
			int							m_clip_y1;
			int							m_clip_x2;
			int							m_clip_y2;

			attr_storage				m_text_attributes;

			agg_graphics*				m_graphics;
			path_storage*				m_path_storage;
			path_attributes*			m_cattr;
			double						m_text_pos_x;
			double						m_text_pos_y;
			REBCHR*				        m_text;
			REBCNT				        m_text_free;
			font*						m_font;
			para*						m_para;
			tmp_val						m_tmp_val;
			REBXYF*						m_caret;
			hinfo*						m_hinfo;
			REBPAR						m_hstart;
			REBPAR						m_hend;
            bool                        m_hinting;

			unsigned					m_color_changed;

			pixfmt_type m_pf;
			base_ren_type m_ren_base;
			renderer_solid m_ren_solid;
			renderer_bin m_ren_bin;
	};

}
