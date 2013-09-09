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

#include "agg_graphics.h"

#include "agg_truetype_text.h"

extern "C" void RL_Print(char *fmt, ...);//output just for testing
namespace agg
{

	//constructor
#ifdef AGG_WIN32_FONTS
	rich_text::rich_text(HDC dc) :
		m_dc(dc),
		m_feng(dc),
#endif
#ifdef AGG_FREETYPE
	rich_text::rich_text() :
        m_feng(),
#endif
		m_fman(m_feng),
		m_curves(m_fman.path_adaptor()),
		m_caret(0),
		m_hinting(true)
//		m_contour(m_curves)
	{
//		m_contour.auto_detect_orientation(false);

		debug = 0;

		m_gren = glyph_ren_native_mono;
        m_feng.hinting(m_hinting);
		m_feng.width(0);
        m_feng.flip_y(true);

		m_hinfo = 0;

		m_color_changed = 0;

		m_font = new font();
		m_para = new para();

//		rt_push();
//		RL_Print("RICH TEXT created!\n");
    }

	//destructor(can be enhanced)
	rich_text::~rich_text()
	{
		rt_reset();
		delete m_font;
		delete m_para;
//        Reb_Print("RICH TEXT destroyed!\n");
	}

	text_attributes& rich_text::rt_curr_attributes()
    {
		return m_text_attributes[m_text_attributes.size() - 1];
	}

	void rich_text::rt_set_clip(int x1, int y1,int x2, int y2, int wx, int wy)
	{
		m_clip_x1 = x1;
		m_clip_y1 = y1;
		m_clip_x2 = x2-1;
		m_clip_y2 = y2-1;

		if (wx)
			m_wrap_size_x = wx;
		else
			m_wrap_size_x = x2 - x1;

		if (wy)
			m_wrap_size_y = wy;
		else
			m_wrap_size_y = y2 - y1;

	}
#ifdef AGG_FREETYPE
    void rich_text::GetTextExtentPointFT(const wchar_t *string, int c, SIZE *size)
    {
        size->cy = 0;
        size->cx = 0;
        for (int i = 0; i < c; i++ ){
            const glyph_cache* glyph = m_fman.glyph(string[i]);
            size->cx += (int)glyph->glyph_width;
            size->cy = (int)max(glyph->glyph_height, size->cy);
        }
    }
#endif
	void rich_text::rt_reset()
	{
//	    RL_Print("RICH TEXT reset!\n");
		rt_text_mode(0);

		m_color_changed = 0;

        wchar_t *last_text = 0;
        REBCHR *last_name = 0;
        unsigned i;
		for(i = 0; i < m_text_attributes.size(); i++){
			text_attributes attr = m_text_attributes[i];
			if (attr.text != last_text && attr.text_free){
				delete[] attr.text;
				last_text = attr.text;
			}
			if (attr.name != last_name && attr.name_free){
				delete[] attr.name;
				last_name = attr.name;
			}
		}

		m_text_attributes.remove_all();

		m_font->name = FONT_NAME; //"Arial"
		m_font->name_free = false;
		m_font->bold = 0;
		m_font->italic = 0;
		m_font->underline = 0;
		m_font->size = LOG_COORD_Y(12);
		m_font->color[0] = 0;
		m_font->color[1] = 0;
		m_font->color[2] = 0;
		m_font->color[3] = 255;
		m_font->offset_x = 2;
		m_font->offset_y = 2;
		m_font->space_x = 0;
		m_font->space_y = 0;
		m_font->shadow_x = 0;
		m_font->shadow_y = 0;
		m_font->shadow_color[0] = 0;
		m_font->shadow_color[1] = 0;
		m_font->shadow_color[2] = 0;
		m_font->shadow_color[3] = 255;
		m_font->shadow_blur = 0;

		m_para->tabs = 40;
		m_para->wrap = 1;
		m_para->origin_x = 2;
		m_para->origin_y = 2;
		m_para->margin_x = 2;
		m_para->margin_y = 2;
		m_para->indent_x = 0;
		m_para->indent_y = 0;
		m_para->scroll_x = 0;
		m_para->scroll_y = 0;
		m_para->align = W_TEXT_LEFT;
		m_para->valign = W_TEXT_TOP;

		if (m_caret) {
			delete m_caret;
			m_caret = 0;
		}
		if (m_hinfo) {
			delete m_hinfo;
			m_hinfo = 0;
		}

		m_text = 0;

//		rt_push();
	}

	void rich_text::rt_attach_buffer(ren_buf* img_buf, int w, int h, int x, int y)
	{
		m_rbuf = img_buf;
//		m_img_buf = img_buf;

		m_buf_offset_x = x;
		m_buf_offset_y = y;
		m_buf_size_x = img_buf->width();
		m_buf_size_y = img_buf->height();

		rt_set_clip(
			m_buf_offset_x,
			m_buf_offset_y,
			m_buf_size_x,
			m_buf_size_y,
			w,h);

		m_pf.attach(*m_rbuf);
		m_ren_base.attach(m_pf);
		m_ren_solid.attach(m_ren_base);
		m_ren_bin.attach(m_ren_base);
	}

	void rich_text::rt_set_font(font* fnt)
	{
		m_font = fnt;
	}

	font* rich_text::rt_get_font()
	{
		return m_font;
	}


	void rich_text::rt_set_para(para* par)
	{
		m_para = par;
	}

	para* rich_text::rt_get_para()
	{
		return m_para;
	}

	/*-------------------------------------------------
	void rich_text::rt_push(int idx)
	Pushes font, para and text data into the stack
	--------------------------------------------------*/
	void rich_text::rt_push(int idx)
	{
		m_text_attributes.add(text_attributes(idx));
		text_attributes& attr = rt_curr_attributes();

		//push font
		attr.bold = m_font->bold;
		attr.color = rgba8(m_font->color[0],m_font->color[1],m_font->color[2], m_font->color[3]);
		attr.italic = m_font->italic;
		attr.name = (m_font->name) ? m_font->name : FONT_NAME;
		attr.name_free = m_font->name_free;
		attr.offset_x = m_font->offset_x;
		attr.offset_y = m_font->offset_y;
		attr.shadow_x = m_font->shadow_x;
		attr.shadow_y = m_font->shadow_y;
		attr.shadow_color = rgba8(m_font->shadow_color[0],m_font->shadow_color[1],m_font->shadow_color[2], m_font->shadow_color[3]);
		attr.shadow_blur = m_font->shadow_blur;
		attr.size = m_font->size;
		attr.space_x = m_font->space_x;
		attr.space_y = m_font->space_y;
		attr.underline = m_font->underline;

		if (m_para != 0){
			attr.isPara=true;
			//push para
			attr.para.tabs=m_para->tabs;
			attr.para.wrap=m_para->wrap;
			attr.para.origin_x=m_para->origin_x;
			attr.para.origin_y=m_para->origin_y;
			attr.para.margin_x=m_para->margin_x;
			attr.para.margin_y=m_para->margin_y;
			attr.para.indent_x=m_para->indent_x;
			attr.para.indent_y=m_para->indent_y;
			attr.para.scroll_x=m_para->scroll_x;
			attr.para.scroll_y=m_para->scroll_y;
			attr.para.align = m_para->align;
			attr.para.valign = m_para->valign;
		}

		//push text
		attr.text = (wchar_t*)m_text;
		//if set to TRUE deallocate text string..otherwise REBOL GC will handle it
		attr.text_free = m_text_free;
	}


	void rich_text::rt_color_change (){
		if (!m_color_changed)
			m_color_changed = m_text_attributes.size() - 1;
	}

	void rich_text::rt_attr_to_font (text_attributes& attr){
		m_font->bold = attr.bold;
		m_font->color[0] = attr.color.r;
		m_font->color[1] = attr.color.g;
		m_font->color[2] = attr.color.b;
		m_font->color[3] = attr.color.a;
		m_font->italic = attr.italic;
		m_font->name = attr.name;
		m_font->name_free = attr.name_free;
		m_font->offset_x = attr.offset_x;
		m_font->offset_y = attr.offset_y;
		m_font->shadow_x = attr.shadow_x;
		m_font->shadow_y = attr.shadow_y;
		m_font->shadow_blur = attr.shadow_blur;
		m_font->shadow_color[0] = attr.shadow_color.r;
		m_font->shadow_color[1] = attr.shadow_color.g;
		m_font->shadow_color[2] = attr.shadow_color.b;
		m_font->shadow_color[3] = attr.shadow_color.a;
		m_font->size = attr.size;
		m_font->space_x = attr.space_x;
		m_font->space_y = attr.space_y;
		m_font->underline = attr.underline;
	}

	/*-------------------------------------------------
	int rich_text::rt_drop(unsigned int idx)
	Drops all attributes back from given attribute index
	--------------------------------------------------*/
	int rich_text::rt_drop(unsigned int idx)
	{
        if(m_text_attributes.size() < ++idx)
        {
			return -1;
        }

		int attr_siz  = m_text_attributes.size();

		while (idx!=0){
			text_attributes& attr = m_text_attributes[--attr_siz];
			if (attr_siz == 0){
				rt_attr_to_font(attr);
				return 0;
			}
			if (attr.index == 0) {
				idx--;
				if (idx==0){
					rt_attr_to_font(attr);
					return 0;
				}
			}
		}

		return 0;
	}

	int rich_text::rt_set_text(REBCHR* text, REBCNT dealloc)
	{
		m_text= (text) ? text : (REBCHR*)L"";
		m_text_free = dealloc;
		return 0;
	}

	/*-------------------------------------------------
	rt_text_height(int idx)
	Given attribute index returns text height of text attribute.
	---------------------------------------------------*/
	int rich_text::rt_text_height(int idx){
		//get the text attribute
		text_attributes& attr = m_text_attributes[idx];

		//setup font engine
#ifdef AGG_WIN32_FONTS
		m_feng.height(attr.size);
		m_feng.italic(attr.italic);
		if (attr.bold){
			m_feng.weight(FW_BOLD);
		} else {
			m_feng.weight(FW_DONTCARE);
		}

		if(m_feng.create_font((const wchar_t*)attr.name, m_gren)){
#endif
#ifdef AGG_FREETYPE
        if (m_feng.load_font((const char*)attr.name, 0, m_gren)){
            m_feng.height(attr.size);
#endif
#ifdef AGG_WIN32_FONTS
			//get ascent & descent values of  given font(used in case the attribute is checked before rendering)
			TEXTMETRIC tm;
			GetTextMetrics( m_dc, &tm );
			attr.desc = tm.tmDescent - 1;
			attr.asc = tm.tmAscent;

			//get text height
			SIZE area;
			GetTextExtentPoint32( m_dc, L"\x004E", 1, &area ); //'N'
			return area.cy;
#endif
#ifdef AGG_FREETYPE
			attr.desc = (int)-m_feng.descender() - 1;
			attr.asc = (int)m_feng.ascender();

            const glyph_cache* glyph = m_fman.glyph(*L"\x004E");
            return ((glyph) ? (int)glyph->glyph_height : 0);
#endif
		}
		return 0;
	}

	/*--------------------------------------------------------------------------------------
	rt_max_line_height(int idx)
	Starting from given text attribute index returns index of attr. with maximum line height.
	---------------------------------------------------------------------------------------*/
	int rich_text::rt_max_line_height(int idx)
	{
		int attrIdx = idx,maxSize = 0;
		unsigned i;

		//find attributes with max font size
		for(i = idx; i < m_text_attributes.size(); i++){
			const text_attributes& attr = m_text_attributes[i];
			if (attr.index == 0) continue;
			if (maxSize < attr.size){
				maxSize = attr.size;
				attrIdx = i;
			}
//			if (strchr(attr.text,'\n')) break;
			if (wcschr(attr.text, L'\x000A')) break; //'\n'

		}

		return attrIdx;
	}

	/*--------------------------------------------------------------------------------------
	rt_line_width(int idx, int offset)
	Starting from given text attribute index and string offset returns width until newline or wrap.
	---------------------------------------------------------------------------------------*/
	int rich_text::rt_line_width(int idx, int offset)
	{
		int width = 0;
		int last_width = 0;
		unsigned i;
		bool init = true;
		bool wrap = false;
		int g_adv_x;
		for(i = idx; i < m_text_attributes.size(); i++){
			const text_attributes& attr = m_text_attributes[i];
			if (attr.index == 0) continue;

			//setup font engine
#ifdef AGG_WIN32_FONTS
			m_feng.height(attr.size);
			m_feng.italic(attr.italic);
			if (attr.bold){
				m_feng.weight(FW_BOLD);
			} else {
				m_feng.weight(FW_DONTCARE);

			}
			if(m_feng.create_font((const wchar_t*)attr.name, m_gren)){
#endif
#ifdef AGG_FREETYPE
			if(m_feng.load_font((const char*)attr.name, 0, m_gren)){
                m_feng.height(attr.size);
#endif
				const wchar_t* p = attr.text;
				if (init){
					init = false;
					p+=offset;
				}
				while (*p){
					if (p[0]==L'\x000A') return width;
					if ((p[0]==L'\x0020') || (p[0]==L'\x0009')) last_width= width;
					const glyph_cache* glyph = m_fman.glyph(*p);
					if(glyph){
						if (p[0] == L'\x0009') {
							int	tab_width = attr.para.tabs;
							if (tab_width == 0){
									tab_width = 40;
							}
							g_adv_x =(tab_width - (width % tab_width));
						} else {
							g_adv_x = (int)glyph->advance_x;
						}
						width+=g_adv_x + attr.space_x;
						if ((attr.para.wrap) && (width >= m_wrap_size_x-m_right_hang)){
							int last_word = width;
							while(*p){
								if ((p[0] == L'\x0020') || (p[0] == L'\x0009') || (p[0] == L'\x000A')){
									break;
								}
								const glyph_cache* glyph = m_fman.glyph(*p);
								if(glyph){
									last_word+=(int)glyph->advance_x + attr.space_x;
								}
								p++;
							}
							if ((last_width == 0) || (last_word - last_width >= m_wrap_size_x-m_right_hang)){
								last_width = width - g_adv_x - attr.space_x;
							}
							return last_width;
						}
					}
					p++;
				}
			}
		}
		return width;
	}


	int rich_text::rt_size_text(REBXYF* size)
	{
		m_tmp_val.pair.x = ROUND_TO_INT(size->x);
		m_tmp_val.pair.y = ROUND_TO_INT(size->y);
		rt_draw_text(SIZE_TEXT);
		size->x = m_tmp_val.pair.x;
		size->y = m_tmp_val.pair.y;
		return 0;
	}


	/*-------------------------------------------------------------------
	cinfo* rich_text::rt_offset_to_caret(REBPAR* offset)
	given screen offset coords sets the offset of caret in text dialect array
	returns carret info structure for useful text handling
	--------------------------------------------------------------------*/
	void rich_text::rt_offset_to_caret(REBXYF offset, REBINT *element, REBINT *position){
		m_tmp_val.pair.x = ROUND_TO_INT(offset.x+1);
		m_tmp_val.pair.y = ROUND_TO_INT(offset.y);
		rt_draw_text(OFFSET_TO_CARET);
		*element = m_tmp_val.pair.x-1;
		*position = m_tmp_val.pair.y-1;
	}


	/*-------------------------------------------------------------------
	given the ofset of caret returns screen offset relative to face
	returns carret info structure for useful text handling
	--------------------------------------------------------------------*/

	void rich_text::rt_caret_to_offset(REBXYF* offset, REBINT element, REBINT position){
		m_tmp_val.pair.x = element+1;
		m_tmp_val.pair.y = position;
		rt_draw_text(CARET_TO_OFFSET);
		offset->x = m_tmp_val.pair.x;
		offset->y = m_tmp_val.pair.y;
	}

	/*-------------------------------------------------------------------
	void rich_text::rt_set_hinfo(hinfo* hinfo)
	sets text marking(highliting) using the hinfo structure
	--------------------------------------------------------------------*/
	void rich_text::rt_set_hinfo(REBXYF highlightStart, REBXYF highlightEnd){
		if (!m_hinfo) m_hinfo = new hinfo();
		m_hinfo->hStart.x = ROUND_TO_INT(highlightStart.x);
		m_hinfo->hStart.y = ROUND_TO_INT(highlightStart.y);
		m_hinfo->hEnd.x = ROUND_TO_INT(highlightEnd.x);
		m_hinfo->hEnd.y = ROUND_TO_INT(highlightEnd.y);
	}

	/*-------------------------------------------------------------------
	int rich_text::rt_text_mode(int mode)
	sets the text rendering mode (0-aliased,1-antialiased,2-vectorial)
	--------------------------------------------------------------------*/
	int rich_text::rt_text_mode(int mode){
//	    Reb_Print("rt_text_mode: %d\n", mode);
        switch(mode)
        {
			case 0:
				m_gren = glyph_ren_native_mono;
				m_feng.gamma(gamma_threshold(.5));
				break;
			case 1:
				m_gren = glyph_ren_agg_gray8;
				m_feng.gamma(gamma_power(1.0));
				break;
			case 2:
				m_gren = glyph_ren_outline;
				m_feng.gamma(gamma_power(1.0));
				break;
			default: return -1;
        }
		return 0;
	}

	/*-------------------------------------------------------------------
	--------------------------------------------------------------------*/
	int rich_text::rt_set_caret(REBXYF offset){
		if (!m_caret) m_caret = new REBXYF();
		m_caret->x = offset.x;
		m_caret->y = offset.y;
		return 0;
	};

	/*--------------------------------------------------------------------
	Scrolls text stored in the attribute stack bypassing the dialect parser.
	--------------------------------------------------------------------*/
	int rich_text::rt_scroll(REBXYF offset){
		rt_draw_text(DRAW_TEXT, &offset);
		return 0;
	}


	void rich_text::rt_set_graphics(agg_graphics* graphics)
	{
		m_graphics = graphics;
		m_path_storage = m_graphics->get_path_storage();
		m_cattr = &m_graphics->curr_attributes();
	}


	/*-------------------------------------------------------------------
	int rich_text::rt_draw_text(int mode, REBXYF offset)
	main rendering function, the offset is optional (for fast scroll)
	it is also used for o-t-c and c-t-o computations
	--------------------------------------------------------------------*/
	int rich_text::rt_draw_text(int mode, REBXYF* offset)
	{
		unsigned const attrSize = m_text_attributes.size();

		if (!attrSize) return 0;

		rect clip_box(m_clip_x1, m_clip_y1, m_clip_x2, m_clip_y2);

		if (!clip_box.is_valid()) return 0;
				
//		m_contour.width(-m_weight.value() * m_height.value() * 0.05);

		int glyph_count = 0;
		double ox,oy;
		double x0 = 0;
		m_right_hang = 0;

		double sx = 0;
		double sy = 0;


		unsigned i;

		//max line height attribute
		int mlha = rt_max_line_height(0);

		//max line height
		int lh = rt_text_height(mlha);

#ifdef AGG_WIN32_FONTS
		TEXTMETRIC tm;
#endif
		//actual proccessed attribute
		const text_attributes& attr = m_text_attributes[mlha];

		//set vertical aligning
		int valign = 0;
		if (mode == DRAW_TEXT && attr.para.valign != W_TEXT_TOP){
			REBXYF total_size;
			rt_size_text(&total_size);
			if (total_size.y == 0) total_size.y = lh;
			switch (attr.para.valign){
				case W_TEXT_BOTTOM:
					valign = ROUND_TO_INT(m_wrap_size_y - total_size.y);
					break;
				case W_TEXT_MIDDLE:
					valign = ROUND_TO_INT((m_wrap_size_y - total_size.y) / 2);
					break;
			}
		}

		//set initial offset
		if (attr.isPara){
			ox = attr.para.origin_x + attr.para.indent_x + attr.para.scroll_x;
			oy = attr.para.origin_y + attr.para.scroll_y + valign;
			m_right_hang = attr.para.origin_x + attr.para.margin_x + attr.para.indent_x;
		} else {
			ox = attr.offset_x;
			oy = attr.offset_y + valign;
		}
		
		if (offset){
			ox += offset->x;
			oy += offset->y;
		}
		
#ifdef AGG_WIN32_FONTS
		m_feng.create_font((const wchar_t*)attr.name, m_gren);
		GetTextMetrics( m_dc, &tm );
		m_text_pos_y = tm.tmAscent + oy + attr.space_y;
#endif
#ifdef AGG_FREETYPE
        m_feng.load_font((const char*)attr.name, 0, m_gren);
        m_feng.height(attr.size);
        m_text_pos_y = m_feng.ascender() + oy + attr.space_y;
#endif
		//find last text attribute
		const text_attributes* last_text_attr = &attr;

		for(i = attrSize-1;i>0;i--){
			if (m_text_attributes[i].index != 0){
				last_text_attr = &m_text_attributes[i];
				break;
			}
		}

		int prev_attr = mlha;

		//precalculate the line width for aligning
		int lw = 0; //no effect by default
		if (attr.para.align == W_TEXT_RIGHT) {
			lw = m_wrap_size_x - rt_line_width(0) - m_right_hang;
		} else if (attr.para.align == W_TEXT_CENTER) {
			lw = (m_wrap_size_x - rt_line_width(0) - m_right_hang) / 2;
		}

		//process attribute stack
		for(i = 0; i < attrSize; i++){
//Reb_Print("processing attr: %d\n", i);
			text_attributes& attr = m_text_attributes[i];

#ifdef AGG_WIN32_FONTS
			m_feng.height(attr.size);
			m_feng.italic(attr.italic);
			if (attr.bold){
				m_feng.weight(FW_BOLD);
			} else {
				m_feng.weight(FW_DONTCARE);
			}
//Reb_Print("create FONT beg\n");
			if(m_feng.create_font((const wchar_t*)attr.name, m_gren))
			{
#endif
#ifdef AGG_FREETYPE
			if(m_feng.load_font((const char*)attr.name, 0, m_gren))
			{
                m_feng.height(attr.size);
#endif
//			    Reb_Print("create FONT OK\n");
//				m_fman.precache(' ', 127);

				if (i == 0){
					//compute start of x coord
#ifdef AGG_WIN32_FONTS
					ABC* widths = new ABC [256];
					if ( GetCharABCWidths( m_dc, 0, 255, widths) == 0 ) {
						if ( GetCharWidth32( m_dc, 0, 255, (int *)widths ) == 0 ) {
							if ( GetCharWidth( m_dc, 0, 255, (int *)widths ) == 0 ) {
								return -3; //unable to load font information
							}
						}
					}
					for (int i=0; i<256; i++ ) {
						if (widths[i].abcA < x0)
							x0 = widths[i].abcA;
					}
					delete [] widths;
#endif
#ifdef AGG_FREETYPE
                    x0 = 0; //fixme
#endif
					m_right_hang -= (int)x0;
					x0 =  ox - x0;
					m_text_pos_x = x0+lw;
				} /* else {
x0 = attr.para.origin_x + attr.para.indent_x + attr.para.scroll_x;
m_text_pos_x = x0+lw;
				}
*/

#ifdef AGG_WIN32_FONTS
				//get ascent & descent values of given font
				GetTextMetrics( m_dc, &tm );
				attr.desc = tm.tmDescent - 1;
				attr.asc = tm.tmAscent;

				//get char_height
				SIZE area;
				GetTextExtentPoint32( m_dc, L"\x004E", 1, &area ); //"N"
#endif
#ifdef AGG_FREETYPE
                SIZE area;
				attr.desc = (int)-m_feng.descender() - 1;
				attr.asc = (int)m_feng.ascender();

                const glyph_cache* glyph = m_fman.glyph(*L"\x004E");
                area.cx = (int)glyph->glyph_width;
                area.cy = (int)glyph->glyph_height;
#endif
				attr.char_height = area.cy;
				if (attr.index == 0) {
					//skip attributes holding other than string info
					continue;
				}

				//render string
				const wchar_t* p = (wchar_t*)attr.text;

//				int slen = strlen(p);
				int slen = wcslen(p);

				//word wrap support
				int wrap = 0;
//				int delim = strcspn (p," \n\t");
				int delim = wcscspn (p,L"\x0020\x000A\x0009"); //" \n\t"
				if ((delim > slen)){
					area.cx =0;
				} else {
#ifdef AGG_WIN32_FONTS
					GetTextExtentPoint32( m_dc, p , delim, &area );
#endif
#ifdef AGG_FREETYPE
                GetTextExtentPointFT( p , delim, &area );
#endif
					area.cx+= (attr.space_x * (delim - 1));
					if (area.cx >= m_wrap_size_x-m_right_hang){
						area.cx = 0;
					}
				}

				switch (mode){
					case DRAW_TEXT:
						{
//						    Reb_Print("DRAW TEXT!\n");
						ren_buf tmp_rb;
						pixfmt_type tmp_pf(tmp_rb);

						bool shadow;
						if ((attr.shadow_x !=0) || (attr.shadow_y !=0) || (attr.shadow_blur > 0)){
							shadow = true;
						} else {
							shadow = false;
						}
//RL_Print("CLIP BOX: %dx%d %dx%d\n", (int)m_clip_x1, (int)m_clip_y1, (int)m_clip_x2, (int)m_clip_y2);
						m_ren_base.clip_box(m_clip_x1, m_clip_y1, m_clip_x2, m_clip_y2);

						//draw text mode
						while(*p){
//Reb_Print("GET GLYPH!\n");
							//get glyph
							const glyph_cache* glyph = m_fman.glyph(*p);
//Reb_Print("GET GLYPH OK!\n");
							if(glyph)
							{
//Reb_Print("DRAW GLYPH!\n");
								double g_adv_x = glyph->advance_x;

								bool tabbing = false;

								//tabbing
								if ((attr.isPara)  && (p[0]==L'\x0009')) {
									int	tab_width = attr.para.tabs;
									if (tab_width == 0){
										tab_width = 40;
									}
									g_adv_x = (tab_width - ((int)(m_text_pos_x-lw-x0) % tab_width));
									tabbing = true;
								}

								//word wrapping
								if ((attr.isPara) && (attr.para.wrap)){
									if (wrap == 3) {
										wrap = 1;
									}

									if (((p[0]==L'\x0020') || (p[0]==L'\x0009') || ((p - attr.text != 0) && ((p-1)[0]==L'\x000A'))) && (p[1]!=0)){
										//get new word width for wrap check
										wrap = 0;
//										delim = strcspn (p+1," \n\t");
										delim = wcscspn (p+1,L"\x0020\x000A\x0009"); //" \n\t"
//										Reb_Print("delim: %d %d %d %d %d", strlen(p+1), slen, p, attr.text, slen + (attr.text - p) - 1 );
										if (delim > slen + (attr.text - p) - 1){ //optimized! this means: (delim > (int)strlen(p+1))
											area.cx =0;
										} else {
#ifdef AGG_WIN32_FONTS
											GetTextExtentPoint32( m_dc, p , delim+1, &area );
#endif
#ifdef AGG_FREETYPE
                                            GetTextExtentPointFT( p , delim+1, &area );
#endif
											area.cx+= (attr.space_x * delim);
											if (area.cx >= m_wrap_size_x-m_right_hang){
												area.cx = 0;
											}
										}
									}

									if (
										(wrap == 0) &&
										(((m_text_pos_x-lw-x0) + area.cx) >= m_wrap_size_x-m_right_hang)
									){
										//wrap the line
										wrap = 1;
										if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
									}

									if (
										(area.cx == 0) &&
										(((m_text_pos_x-lw-x0) + g_adv_x) >= m_wrap_size_x-m_right_hang)
									){
										//truncate line
										if ((m_text_pos_x-lw-x0 == 0) && (g_adv_x >= m_wrap_size_x-m_right_hang)){
											//avoid infinite loop
											if (wrap==1) p++;										
										
											//special case (1st char is wider than destination)
											wrap = 3;
										} else {
											wrap = 1;
											if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
										}
									}
								}

								//line break
								if (
									(p[0]==L'\x000A') || //'\n'
									(wrap == 1)
								){
									if (
										(m_caret !=0) && ((p[0] == L'\x000A') || (p[0] == L'\x0020') || (p[0] == L'\x0009')) &&
										(m_caret->x == (int)attr.index) &&
										(m_caret->y == (p - attr.text)+1)
									){
										//render caret at the end of line
										if (m_gren == glyph_ren_outline){
											m_path_storage->move_to(
												m_text_pos_x,
												m_text_pos_y-m_text_attributes[prev_attr].asc
											);
											m_path_storage->hline_rel(2);
											m_path_storage->vline_to(
												m_text_pos_y+m_text_attributes[prev_attr].desc
											);
											m_path_storage->hline_rel(-2);
											m_path_storage->close_polygon();
										} else {
											m_ren_base.blend_bar((int)m_text_pos_x,(int)(m_text_pos_y-m_text_attributes[prev_attr].asc),(int)(m_text_pos_x+1),(int)(m_text_pos_y+m_text_attributes[prev_attr].desc), rgba8(0,0,0,255) ,255);
										}
									}

									if (&attr == last_text_attr){
										//process last string attribute
										if (m_text_attributes[mlha].desc == attr.desc){
											m_text_pos_y += attr.asc + attr.desc + attr.space_y + 1;
										} else {
											m_text_pos_y += attr.asc + m_text_attributes[mlha].desc + attr.space_y + 1;
										}
										mlha = i;
										lh = attr.char_height;
									} else {
										int prev_max_desc = m_text_attributes[mlha].desc;

										//get new line height of next line
										int next_mlha = rt_max_line_height(i+1);
										int next_lh = rt_text_height(next_mlha);

//										int slen = strlen(attr.text);

										if ((slen != 1) && (attr.char_height <= next_lh)){
//											if (strchr(p+1,'\n') != NULL) {
											if (wcschr(p+1,L'\x000A') != NULL) { //'\n'
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											} else {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											}
										} else {
											if (slen == (p - attr.text)+1) {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											} else {
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											}
										}
									}

									//end of 'visible area' reached
									if (m_gren == glyph_ren_outline){
										if ((m_text_pos_y - m_text_attributes[mlha].asc) > 2000)
											//clip the vectorial text after 2k pixels - temp solution
											return glyph_count;
									} else if ((m_text_pos_y - m_text_attributes[mlha].asc) > m_buf_size_y) {
										return glyph_count;
									}

//x0 = attr.para.origin_x + attr.para.indent_x + attr.para.scroll_x;

									//reset x position
									if (wrap==1){
										m_text_pos_x = x0 - attr.para.indent_x;
									} else {
										m_text_pos_x = x0;
										m_text_pos_y += attr.para.indent_y;
									}

									p++;

									//precalculate the line width for aligning
									if (attr.para.align == W_TEXT_RIGHT) {
										lw = m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang;
									} else if (attr.para.align == W_TEXT_CENTER) {
										lw = (m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang) / 2;
									} else {
										lw = 0; //no effect by defaut
									}

									m_text_pos_x+=lw;

									//setup font engine
#ifdef AGG_WIN32_FONTS
									m_feng.height(attr.size);
									m_feng.italic(attr.italic);
									if (attr.bold){
										m_feng.weight(FW_BOLD);
									} else {
										m_feng.weight(FW_DONTCARE);
									}
									m_feng.create_font((const wchar_t*)attr.name, m_gren);
#endif
#ifdef AGG_FREETYPE
                                    m_feng.load_font((const char*)attr.name, 0, m_gren);
                                    m_feng.height(attr.size);
#endif
									prev_attr = i;
									wrap = 0;
									continue;
								} else {
									if (wrap == 0) wrap = 2;
								}

								prev_attr = i;

								//check visibility against clipped area
								rect glyph_rect = glyph->bounds;
								glyph_rect.x1 += (int)m_text_pos_x;
								glyph_rect.y1 += (int)m_text_pos_y;
								glyph_rect.x2 += (int)m_text_pos_x;
								glyph_rect.y2 += (int)m_text_pos_y;

								//don't clip vectorial glyphs
								bool clip = (m_gren == glyph_ren_outline) ? true : glyph_rect.clip(clip_box);

								//render caret
								if (
									((clip) || (glyph->bounds.x1>=glyph->bounds.x2)) &&
									(m_caret !=0) &&
									(m_caret->x == (int)attr.index) &&
									(m_caret->y == (p - attr.text)+1)
								){
									if (m_gren == glyph_ren_outline){
										m_path_storage->move_to(
											m_text_pos_x-1,
											m_text_pos_y-attr.asc
										);
										m_path_storage->hline_rel(2);
										m_path_storage->vline_to(
											m_text_pos_y+attr.desc
										);
										m_path_storage->hline_rel(-2);
										m_path_storage->close_polygon();
									} else {
										m_ren_base.blend_bar((int)m_text_pos_x-1,(int)(m_text_pos_y-attr.asc),(int)(m_text_pos_x),(int)(m_text_pos_y+attr.desc), rgba8(0,0,0,255) ,255);
									}
								}

								bool highlight = false;

								if (
									(clip) || (glyph->bounds.x1>=glyph->bounds.x2)
								){
									//render text highliting
									if (
										(m_hinfo != 0)
									){
										if (
											(m_hinfo->hStart.x < m_hinfo->hEnd.x) ||
											(
												(m_hinfo->hStart.x == m_hinfo->hEnd.x) &&
												(m_hinfo->hStart.y < m_hinfo->hEnd.y)
											)
										){
											m_hstart.x = m_hinfo->hStart.x;
											m_hstart.y = m_hinfo->hStart.y;
											m_hend.x = m_hinfo->hEnd.x;
											m_hend.y = m_hinfo->hEnd.y;
										} else {
											m_hstart.x = m_hinfo->hEnd.x;
											m_hstart.y = m_hinfo->hEnd.y;
											m_hend.x = m_hinfo->hStart.x;
											m_hend.y = m_hinfo->hStart.y;
										}

										if (
											(m_hstart.x <= (int)attr.index) &&
											(m_hend.x >= (int)attr.index)
										){
											if (
												(
													(m_hstart.x == (int)attr.index) &&
													(m_hstart.y > (p - attr.text)+1)
												) ||
												(
													(m_hend.x == (int)attr.index) &&
													(m_hend.y < (p - attr.text)+2)
												)
											){
											} else {
												if (shadow && m_gren != glyph_ren_outline){
													m_ren_base.blend_bar((int)m_text_pos_x+attr.shadow_x,(int)(m_text_pos_y-attr.asc)+attr.shadow_y,(int)(m_text_pos_x+g_adv_x)+attr.shadow_x-1, (int)(m_text_pos_y+attr.desc)+attr.shadow_y, attr.shadow_color ,255);
												}
												if (m_gren == glyph_ren_outline) {
													m_path_storage->move_to(
														m_text_pos_x,
														m_text_pos_y-attr.asc
													);
													m_path_storage->hline_to(m_text_pos_x+g_adv_x + 1);
													m_path_storage->vline_to(
														m_text_pos_y+attr.desc + 1
													);
													m_path_storage->hline_to(m_text_pos_x);
													m_path_storage->close_polygon();
												} else {
													m_ren_base.blend_bar((int)m_text_pos_x,(int)(m_text_pos_y-attr.asc),(int)(m_text_pos_x+g_adv_x), (int)(m_text_pos_y+attr.desc), rgba8(0,0,0,255) ,255);
												}
												highlight = true;
											}

										}
									}
								}

								//skip rest
								if (tabbing) {
									m_text_pos_x += g_adv_x + attr.space_x;
									p++;
									wrap = 0;
									continue;
								}

								if(clip){
									//is visible - render glyph
									glyph_count++;

//									m_fman.add_kerning(&m_text_pos_x, &m_text_pos_y);


									switch(glyph->data_type){
										case glyph_data_mono:
											//render aliased text
											if (highlight){
												m_ren_bin.color(rgba8(255,255,255,255));
											} else {
												if (shadow){
													unsigned bl = attr.shadow_blur;
													if (bl) {
														//shadow with blur

														int bld = bl<<2;
														int w = (int)g_adv_x + (bld);
														int h = attr.asc+attr.desc + (bld);
														int len = w * h * 4;
														unsigned char* tmp = new unsigned char[len];

														tmp_rb.attach(tmp, w, h, w * 4);
														base_ren_type tmp_renb(tmp_pf);
														tmp_renb.clear(rgba8(attr.shadow_color.r,attr.shadow_color.g,attr.shadow_color.b,0));

														m_ren_bin.attach(tmp_renb);

														m_fman.init_embedded_adaptors(glyph, bl, bl + attr.asc);
														m_ren_bin.color(attr.shadow_color);

														render_scanlines(m_fman.mono_adaptor(),
																		  m_fman.mono_scanline(),
																		  m_ren_bin);


														stack_blur_rgba32(tmp_pf, bl, bl);
														m_ren_base.blend_from(tmp_pf,0,(int)(m_text_pos_x+attr.shadow_x-bl), (int)(m_text_pos_y+attr.shadow_y-attr.asc-bl), 255);
														delete[] tmp;
														m_ren_bin.attach(m_ren_base);
													} else {
														//simple shadow outline
														m_fman.init_embedded_adaptors(glyph, m_text_pos_x+attr.shadow_x, m_text_pos_y+attr.shadow_y);
														m_ren_bin.color(attr.shadow_color);

														render_scanlines(m_fman.mono_adaptor(),
																		  m_fman.mono_scanline(),
																		  m_ren_bin);
													}
												}

												m_ren_bin.color(attr.color);
											}

											m_fman.init_embedded_adaptors(glyph, m_text_pos_x, m_text_pos_y);

											render_scanlines(m_fman.mono_adaptor(),
																  m_fman.mono_scanline(),
																  m_ren_bin);
											break;

										case glyph_data_gray8:
											//render anti-aliased
											if (highlight){
												m_ren_solid.color(rgba8(255,255,255,255));
											} else {
												if (shadow){
													unsigned bl = attr.shadow_blur;
													if (bl) {
														//shadow with blur

														int bld = bl<<2;
														int w = (int)g_adv_x + (bld);
														int h = attr.asc+attr.desc + (bld);
														int len = w * h * 4;
														unsigned char* tmp = new unsigned char[len];

														tmp_rb.attach(tmp, w, h, w * 4);
														base_ren_type tmp_renb(tmp_pf);
														tmp_renb.clear(rgba8(attr.shadow_color.r,attr.shadow_color.g,attr.shadow_color.b,0));

														m_ren_solid.attach(tmp_renb);

														m_fman.init_embedded_adaptors(glyph, bl, bl + attr.asc);
														m_ren_solid.color(attr.shadow_color);

														render_scanlines(m_fman.gray8_adaptor(),
																			  m_fman.gray8_scanline(),
																			  m_ren_solid);


														stack_blur_rgba32(tmp_pf, bl, bl);
														m_ren_base.blend_from(tmp_pf,0,(int)(m_text_pos_x+attr.shadow_x-bl), (int)(m_text_pos_y+attr.shadow_y-attr.asc-bl), 255);
														delete[] tmp;
														m_ren_solid.attach(m_ren_base);
													} else {
														//simple shadow outline
														m_fman.init_embedded_adaptors(glyph, m_text_pos_x+attr.shadow_x, m_text_pos_y+attr.shadow_y);
														m_ren_solid.color(attr.shadow_color);
														render_scanlines(m_fman.gray8_adaptor(),
																			  m_fman.gray8_scanline(),
																			  m_ren_solid);
													}
												}

												m_ren_solid.color(attr.color);
											}
											m_fman.init_embedded_adaptors(glyph, m_text_pos_x, m_text_pos_y);
											render_scanlines(m_fman.gray8_adaptor(),
																  m_fman.gray8_scanline(),
																  m_ren_solid);
											break;

										case glyph_data_outline:
											//render vectorial text
											m_fman.init_embedded_adaptors(glyph, m_text_pos_x, m_text_pos_y);
											m_path_storage->add_path(m_curves, m_cattr->index, false);
											break;
									}
								}

								//just temporary added for debugging
								if (debug == 1){
										m_ren_base.blend_hline((int)m_text_pos_x,(int)(m_text_pos_y-attr.asc),(int)(m_text_pos_x+g_adv_x), attr.color ,255);
										m_ren_base.blend_hline((int)m_text_pos_x,(int)(m_text_pos_y+attr.desc),(int)(m_text_pos_x+g_adv_x), attr.color ,255);
										m_ren_base.blend_hline((int)m_text_pos_x,(int)(m_text_pos_y),(int)(m_text_pos_x+g_adv_x), attr.color ,255);
								}

								if (attr.underline){
									//render underline
									if (m_gren == glyph_ren_outline) {
										m_path_storage->move_to(m_text_pos_x, m_text_pos_y+attr.desc);
										m_path_storage->hline_to(m_text_pos_x+g_adv_x);
										m_path_storage->vline_rel(1);
										m_path_storage->hline_to(m_text_pos_x);
										m_path_storage->close_polygon();
									} else
										m_ren_base.blend_hline((int)m_text_pos_x,(int)(m_text_pos_y+attr.desc),(int)(m_text_pos_x+g_adv_x), attr.color ,255);
								}

								// increment pen position
								m_text_pos_x += g_adv_x + attr.space_x;
								m_text_pos_y += glyph->advance_y;

							} else {
								return -2; //unable to create glyph
							}
							++p;
						}

						if (m_gren == glyph_ren_outline){

							if (m_color_changed && i >= m_color_changed) {
								//use richtext colors once it is change in the dialect
								m_cattr->fill_pen = attr.color;
								m_cattr->filled = RT_FILL;
								m_cattr->fill_pen_img_buf = NULL;
							}

							if (i + 1 != attrSize) {
								//close previous path
								m_path_storage->move_to(0,0);
								m_path_storage->add_vertex(0.0, 0.0, path_cmd_stop);


								//add new path for next vectorial shapes
								path_attributes* prev_attr = m_cattr;
								m_graphics->agg_begin_path();
								m_cattr = &m_graphics->curr_attributes();
								//inherit 'manually' also current text clipping attributes (that are not inherited by default)
								m_cattr->block = prev_attr->block; //used as 'vectorial text' flag in the render loop
								m_cattr->coord_x2 = prev_attr->coord_x2;
								m_cattr->coord_y2 = prev_attr->coord_y2;
								m_cattr->coord_x3 = prev_attr->coord_x3;
								m_cattr->coord_y3 = prev_attr->coord_y3;
							}
						}

						}
						break;
					case OFFSET_TO_CARET:
						//check if the offset is 'before text'
						if (
							(m_tmp_val.pair.y < oy+attr.space_y) ||
							((m_tmp_val.pair.x <= ox) && (m_tmp_val.pair.y <= (m_text_pos_y+m_text_attributes[mlha].desc)))
						){
							m_tmp_val.pair.x=attr.index;
							m_tmp_val.pair.y=1;
							caret_info.lh_asc=m_text_attributes[mlha].asc;
							caret_info.lh_desc=m_text_attributes[mlha].desc;
							caret_info.lh_space_y=m_text_attributes[mlha].space_y;
							caret_info.ch_asc=attr.asc;
							caret_info.ch_desc=attr.desc;
							return 0;
						}

						while(*p){

							//get glyph
							const glyph_cache* glyph = m_fman.glyph(*p);

							if(glyph)
							{

								double g_adv_x = glyph->advance_x;

								bool tabbing = false;

								//tabbing
								if ((attr.isPara)  && (p[0]==L'\x0009')) {
									int	tab_width = attr.para.tabs;
									if (tab_width == 0){
										tab_width = 40;
									}
									g_adv_x = (tab_width - ((int)(m_text_pos_x-lw-x0) % tab_width));
									tabbing = true;
								}

								//word wrapping
								if ((attr.isPara) && (attr.para.wrap)){
									if (wrap == 3) {
										wrap = 1;
									}
									if (((p[0]==L'\x0020') || (p[0]==L'\x0009') || ((p - attr.text != 0) && ((p-1)[0]==L'\x000A'))) && (p[1] != 0)){
										//get new word width for wrap check
										wrap = 0;
//										delim = strcspn (p+1," \n\t");
										delim = wcscspn (p+1,L"\x0020\x000A\x0009"); //" \n\t"
										if (delim > slen + (attr.text - p) - 1){ //optimized! this means: (delim > (int)strlen(p+1))
											area.cx =0;
										} else {
#ifdef AGG_WIN32_FONTS
											GetTextExtentPoint32( m_dc, p , delim+1, &area );
#endif
#ifdef AGG_FREETYPE
                                            GetTextExtentPointFT( p , delim+1, &area );
#endif
											area.cx+= (attr.space_x * delim);
											if (area.cx >= m_wrap_size_x-m_right_hang){
												area.cx = 0;
											}
										}
									}

									if (
										(wrap == 0) &&
										(((m_text_pos_x-lw-x0) + area.cx) >= m_wrap_size_x-m_right_hang)
									){
										//wrap the line
										wrap = 1;
										if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
									}

									if (
										(area.cx == 0) &&
										(((m_text_pos_x-lw-x0) + g_adv_x) >= m_wrap_size_x-m_right_hang)
									){
										//truncate line
										if ((m_text_pos_x-lw-x0 == 0) && (g_adv_x >= m_wrap_size_x-m_right_hang)){
											//avoid infinite loop
											if (wrap==1) p++;										
										
											//special case (1st char is wider than destination)
											wrap = 3;
										} else {
											wrap = 1;
											if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
										}
									}
								}


								//line break
								if (
									(p[0]==L'\x000A') ||
									(wrap == 1)
								){
									//check the offset to caret condition
									if (m_tmp_val.pair.y <= m_text_pos_y + m_text_attributes[mlha].desc){
										m_tmp_val.pair.x=attr.index;
										m_tmp_val.pair.y=(int)(p - attr.text)+1;
										caret_info.lh_asc=m_text_attributes[mlha].asc;
										caret_info.lh_desc=m_text_attributes[mlha].desc;
										caret_info.lh_space_y=m_text_attributes[mlha].space_y;
										caret_info.ch_asc=attr.asc;
										caret_info.ch_desc=attr.desc;
										return 0;//(attr.asc + attr.desc + attr.space_y + 1);
									}


									if (&attr == last_text_attr){
										//process last string attribute
										if (m_text_attributes[mlha].desc == attr.desc){
											m_text_pos_y += attr.asc + attr.desc + attr.space_y + 1;
										} else {
											m_text_pos_y += attr.asc + m_text_attributes[mlha].desc + attr.space_y + 1;
										}
										mlha = i;
										lh = attr.char_height;
									} else {
										int prev_max_desc = m_text_attributes[mlha].desc;

										//get new line height of next line
										int next_mlha = rt_max_line_height(i+1);
										int next_lh = rt_text_height(next_mlha);

//										int slen = strlen(attr.text);

										if ((slen != 1) && (attr.char_height <= next_lh)){
//											if (strchr(p+1,'\n') != NULL) {
											if (wcschr(p+1,L'\x000A') != NULL) { //'\n'
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											} else {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											}
										} else {
											if (slen == (p - attr.text)+1) {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											} else {
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											}
										}
									}


									//reset x position
									if (wrap==1){
										m_text_pos_x = x0 - attr.para.indent_x;
									} else {
										m_text_pos_x = x0;
										m_text_pos_y += attr.para.indent_y;
									}

									p++;

									//precalculate the line width for aligning
									if (attr.para.align == W_TEXT_RIGHT) {
										lw = m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang;
									} else if (attr.para.align == W_TEXT_CENTER) {
										lw = (m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang) / 2;
									} else {
										lw = 0; //no effect by defaut
									}

									m_text_pos_x+=lw;

									//setup font engine
#ifdef AGG_WIN32_FONTS
									m_feng.height(attr.size);
									m_feng.italic(attr.italic);
									if (attr.bold){
										m_feng.weight(FW_BOLD);
									} else {
										m_feng.weight(FW_DONTCARE);
									}
									m_feng.create_font((const wchar_t*)attr.name, m_gren);
#endif
#ifdef AGG_FREETYPE
                                    m_feng.load_font((const char*)attr.name, 0, m_gren);
                                    m_feng.height(attr.size);
#endif
									wrap = 0;
									continue;
								} else {
									if (wrap == 0) wrap = 2;
								}

//								m_fman.add_kerning(&m_text_pos_x, &m_text_pos_y);

								//check the offset to caret condition
								int px = m_tmp_val.pair.x;
								int py = m_tmp_val.pair.y;
								int x = (int)m_text_pos_x;
								int y = (int)m_text_pos_y - m_text_attributes[mlha].asc;//attr.asc;
								if (
									(px>=x) &&
									(py>=y) &&
									(px <= x + g_adv_x + attr.space_x) &&
									(py <= m_text_pos_y+attr.space_y+m_text_attributes[mlha].desc)//attr.desc)
								){
									m_tmp_val.pair.x=attr.index;
									m_tmp_val.pair.y=(int)(p - attr.text)+1;
									if (px > x + (g_adv_x / 2)){
										m_tmp_val.pair.y++;
									}
									if (m_tmp_val.pair.y > slen){
										int tmp = m_tmp_val.pair.y;
										m_tmp_val.pair.y = 1;
										m_tmp_val.pair.x = i+1;
										while (m_text_attributes[m_tmp_val.pair.x].index==0){
											m_tmp_val.pair.x++;
											if (m_tmp_val.pair.x == (int)attrSize) {
												m_tmp_val.pair.x = i;
												m_tmp_val.pair.y = tmp;
												break;
											}
										}

										m_tmp_val.pair.x = m_text_attributes[m_tmp_val.pair.x].index;
									}
									caret_info.lh_asc=m_text_attributes[mlha].asc;
									caret_info.lh_desc=m_text_attributes[mlha].desc;
									caret_info.lh_space_y=m_text_attributes[mlha].space_y;
									caret_info.ch_asc=attr.asc;
									caret_info.ch_desc=attr.desc;
									return 0;//(attr.asc + attr.desc + attr.space_y + 1);
								}

								//skip rest
								if (tabbing) {
									m_text_pos_x += g_adv_x + attr.space_x;
									p++;
									wrap = 0;
									continue;
								}

								// increment pen position
								m_text_pos_x += g_adv_x + attr.space_x;
								m_text_pos_y += glyph->advance_y;
							} else {
								return -2; //unable to create glyph
							}
							++p;
						}
						break;
					case CARET_TO_OFFSET:
						while(*p){

							//get glyph
							const glyph_cache* glyph = m_fman.glyph(*p);

							if(glyph)
							{

								double g_adv_x = glyph->advance_x;

								bool tabbing = false;

								//tabbing
								if ((attr.isPara)  && (p[0]==L'\x0009')) {
									int	tab_width = attr.para.tabs;
									if (tab_width == 0){
										tab_width = 40;
									}
									g_adv_x = (tab_width - ((int)(m_text_pos_x-lw-x0) % tab_width));
									tabbing = true;
								}

								//word wrapping
								if ((attr.isPara) && (attr.para.wrap)){
									if (wrap == 3) {
										wrap = 1;
									}
									if (((p[0]==L'\x0020') || (p[0]==L'\x0009') || ((p - attr.text != 0) && ((p-1)[0]==L'\x000A'))) && (p[1]!= 0)){
										//get new word width for wrap check
										wrap = 0;
//										delim = strcspn (p+1," \n\t");
										delim = wcscspn (p+1,L"\x0020\x000A\x0009"); //" \n\t"
										if (delim > slen + (attr.text - p) - 1){ //optimized! this means: (delim > (int)strlen(p+1))
											area.cx =0;
										} else {
#ifdef AGG_WIN32_FONTS
											GetTextExtentPoint32( m_dc, p , delim+1, &area );
#endif
#ifdef AGG_FREETYPE
                                            GetTextExtentPointFT( p , delim+1, &area );
#endif
											area.cx+= (attr.space_x * delim);
											if (area.cx >= m_wrap_size_x-m_right_hang){
												area.cx = 0;
											}
										}
									}

									if (
										(wrap == 0) &&
										(((m_text_pos_x-lw-x0) + area.cx) >= m_wrap_size_x-m_right_hang)
									){
										//wrap the line
										wrap = 1;
										if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
									}

									if (
										(area.cx == 0) &&
										(((m_text_pos_x-lw-x0) + g_adv_x) >= m_wrap_size_x-m_right_hang)
									){
										//truncate line
										if ((m_text_pos_x-lw-x0 == 0) && (g_adv_x >= m_wrap_size_x-m_right_hang)){
											//avoid infinite loop
											if (wrap==1) p++;										

											//special case (1st char is wider than destination)
											wrap = 3;
										} else {
											wrap = 1;
											if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
										}
									}
								}

								//line break
								if (
									(p[0]==L'\x000A') || //'\n'
									(wrap == 1)
								){

									//check the caret to offset condition
									if (
										(m_tmp_val.pair.x==(int)attr.index) &&
										(m_tmp_val.pair.y==(p - attr.text))
									){
										m_tmp_val.pair.x=(int)m_text_pos_x;
										m_tmp_val.pair.y=(int)m_text_pos_y-attr.asc;
										caret_info.lh_asc=m_text_attributes[mlha].asc;
										caret_info.lh_desc=m_text_attributes[mlha].desc;
										caret_info.lh_space_y=m_text_attributes[mlha].space_y;
										caret_info.ch_asc=attr.asc;
										caret_info.ch_desc=attr.desc;
										return 0;//attr.asc + m_text_attributes[mlha].desc + 1;//lh;//(attr.asc + attr.desc + 1);
									}

									if (&attr == last_text_attr){
										//process last string attribute
										if (m_text_attributes[mlha].desc == attr.desc){
											m_text_pos_y += attr.asc + attr.desc + attr.space_y + 1;
										} else {
											m_text_pos_y += attr.asc + m_text_attributes[mlha].desc + attr.space_y + 1;
										}
										mlha = i;
										lh = attr.char_height;
									} else {
										int prev_max_desc = m_text_attributes[mlha].desc;

										//get new line height of next line
										int next_mlha = rt_max_line_height(i+1);
										int next_lh = rt_text_height(next_mlha);

//										int slen = strlen(attr.text);

										if ((slen != 1) && (attr.char_height <= next_lh)){
//											if (strchr(p+1,'\n') != NULL) {
											if (wcschr(p+1,L'\x000A') != NULL) { //'\n'
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											} else {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											}
										} else {
											if (slen == (p - attr.text)+1) {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											} else {
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											}
										}
									}

									//reset x position
									if (wrap==1){
										m_text_pos_x = x0 - attr.para.indent_x;
									} else {
										m_text_pos_x = x0;
										m_text_pos_y += attr.para.indent_y;
									}

									p++;

									//precalculate the line width for aligning
									if (attr.para.align == W_TEXT_RIGHT) {
										lw = m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang;
									} else if (attr.para.align == W_TEXT_CENTER) {
										lw = (m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang) / 2;
									} else {
										lw = 0; //no effect by defaut
									}

									m_text_pos_x+=lw;

									//setup font engine
#ifdef AGG_WIN32_FONTS
									m_feng.height(attr.size);
									m_feng.italic(attr.italic);
									if (attr.bold){
										m_feng.weight(FW_BOLD);
									} else {
										m_feng.weight(FW_DONTCARE);
									}
									m_feng.create_font((const wchar_t*)attr.name, m_gren);
#endif
#ifdef AGG_FREETYPE
                                    m_feng.load_font((const char*)attr.name, 0, m_gren);
                                    m_feng.height(attr.size);
#endif

									wrap = 0;
									continue;
								} else {
									if (wrap == 0) wrap = 2;
								}

//								m_fman.add_kerning(&m_text_pos_x, &m_text_pos_y);

								//check the caret to offset condition
								if (
									(m_tmp_val.pair.x==(int)attr.index) &&
									(m_tmp_val.pair.y==(p - attr.text))
								){
									m_tmp_val.pair.x=(int)m_text_pos_x;
									m_tmp_val.pair.y=(int)m_text_pos_y-attr.asc;

									caret_info.lh_asc=m_text_attributes[mlha].asc;
									caret_info.lh_desc=m_text_attributes[mlha].desc;
									caret_info.lh_space_y=m_text_attributes[mlha].space_y;
									caret_info.ch_asc=attr.asc;
									caret_info.ch_desc=attr.desc;
									return 0;//lh;//(attr.asc + attr.desc + 1);
								}

								//skip rest
								if (tabbing) {
									m_text_pos_x += g_adv_x + attr.space_x;
									p++;
									wrap = 0;
									continue;
								}

								// increment pen position
								m_text_pos_x += g_adv_x + attr.space_x;
								m_text_pos_y += glyph->advance_y;
							} else {
								return -2; //unable to create glyph
							}
							++p;
						}
						break;
					case SIZE_TEXT:
						while(*p){

							//get glyph
							const glyph_cache* glyph = m_fman.glyph(*p);

							if(glyph)
							{

								double g_adv_x = glyph->advance_x;

								bool tabbing = false;

								//tabbing
								if ((attr.isPara)  && (p[0]==L'\x0009')) {
									int	tab_width = attr.para.tabs;
									if (tab_width == 0){
										tab_width = 40;
									}
									g_adv_x = (tab_width - ((int)(m_text_pos_x-lw-x0) % tab_width));
									tabbing = true;
								}

								//word wrapping
								if ((attr.isPara) && (attr.para.wrap)){
									if (wrap == 3) {
										wrap = 1;
									}
									if (((p[0]==L'\x0020') || (p[0]==L'\x0009') || ((p - attr.text != 0) && ((p-1)[0]==L'\x000A'))) && (p[1] != 0)){
										//get new word width for wrap check
										wrap = 0;
//										delim = strcspn (p+1," \n\t");
                                        delim = wcscspn (p+1,L"\x0020\x000A\x0009"); //" \n\t"
										if (delim > slen + (attr.text - p) - 1){ //optimized! this means: (delim > (int)strlen(p+1))
											area.cx =0;
										} else {
#ifdef AGG_WIN32_FONTS
											GetTextExtentPoint32( m_dc, p , delim+1, &area );
#endif
#ifdef AGG_FREETYPE
                                            GetTextExtentPointFT( p , delim+1, &area );
#endif
											area.cx+= (attr.space_x * delim);
											if (area.cx >= m_wrap_size_x-m_right_hang){
												area.cx = 0;
											}
										}
									}

									if (
										(wrap == 0) &&
										(((m_text_pos_x-lw-x0) + area.cx) >= m_wrap_size_x-m_right_hang)
									){
										//wrap the line
										wrap = 1;
										if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
									}

									if (
										(area.cx == 0) &&
										(((m_text_pos_x-lw-x0) + g_adv_x) >= m_wrap_size_x-m_right_hang)
									){
										//truncate line
										if ((m_text_pos_x-lw-x0 == 0) && (g_adv_x >= m_wrap_size_x-m_right_hang)){
											//avoid infinite loop
											if (wrap==1) p++;
											
											//special case (1st char is wider than destination)
											wrap = 3;
										} else {
											wrap = 1;
											if ((p[0] !=L'\x0020') && (p[0] !=L'\x000A') && (p[0] != L'\x0009')) p--;
										}
									}
								}

								//line break
								if (
									(p[0]==L'\x000A') || //'\n'
									(wrap == 1)
								){

									if (&attr == last_text_attr){
										//process last string attribute
										if (m_text_attributes[mlha].desc == attr.desc){
											m_text_pos_y += attr.asc + attr.desc + attr.space_y + 1;
										} else {
											m_text_pos_y += attr.asc + m_text_attributes[mlha].desc + attr.space_y + 1;
										}
										mlha = i;
										lh = attr.char_height;
									} else {
										int prev_max_desc = m_text_attributes[mlha].desc;

										//get new line height of next line
										int next_mlha = rt_max_line_height(i+1);
										int next_lh = rt_text_height(next_mlha);

//										int slen = strlen(attr.text);

										if ((slen != 1) && (attr.char_height <= next_lh)){
//											if (strchr(p+1,'\n') != NULL) {
											if (wcschr(p+1,L'\x000A') != NULL) { //'\n'
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											} else {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											}
										} else {
											if (slen == (p - attr.text)+1) {
												m_text_pos_y += m_text_attributes[next_mlha].asc + prev_max_desc + attr.space_y + 1;
												mlha = next_mlha;
												lh = next_lh;
											} else {
												m_text_pos_y += attr.asc + prev_max_desc + attr.space_y + 1;
												mlha = i;
												lh = attr.char_height;
											}
										}
									}

									//reset x position
									if (wrap==1){
										m_text_pos_x = x0 - attr.para.indent_x;
									} else {
										m_text_pos_x = x0;
										m_text_pos_y += attr.para.indent_y;
									}

									//check the size-text conditions
									if (sx < m_text_pos_x-lw-ox+1){
										sx = m_text_pos_x-lw-ox+1;
									}
									if (sy < m_text_pos_y+m_text_attributes[mlha].desc-oy+1){
										sy = m_text_pos_y+m_text_attributes[mlha].desc-oy+1;
									}


									p++;

									//precalculate the line width for aligning
									if (attr.para.align == W_TEXT_RIGHT) {
										lw = m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang;
									} else if (attr.para.align == W_TEXT_CENTER) {
										lw = (m_wrap_size_x - rt_line_width(i,(p - attr.text)) - m_right_hang) / 2;
									} else {
										lw = 0; //no effect by defaut
									}

									m_text_pos_x+=lw;

									//setup font engine
#ifdef AGG_WIN32_FONTS
									m_feng.height(attr.size);
									m_feng.italic(attr.italic);
									if (attr.bold){
										m_feng.weight(FW_BOLD);
									} else {
										m_feng.weight(FW_DONTCARE);
									}
									m_feng.create_font((const wchar_t*)attr.name, m_gren);
#endif
#ifdef AGG_FREETYPE
                                    m_feng.load_font((const char*)attr.name, 0, m_gren);
                                    m_feng.height(attr.size);
#endif

									wrap = 0;
									continue;
								} else {
									if (wrap == 0) wrap = 2;
								}

//								m_fman.add_kerning(&m_text_pos_x, &m_text_pos_y);


								//skip rest
								if (tabbing) {
									m_text_pos_x += g_adv_x + attr.space_x;
									p++;
									wrap = 0;
								} else {
									// increment pen position
									m_text_pos_x += g_adv_x + attr.space_x;
									m_text_pos_y += glyph->advance_y;
								}

								//check the size-text conditions
								if (sx < m_text_pos_x-lw-ox+1){
									sx = m_text_pos_x-lw-ox+1;
								}
								if (sy < m_text_pos_y+m_text_attributes[mlha].desc-oy+1){
									sy = m_text_pos_y+m_text_attributes[mlha].desc-oy+1;
								}


							} else {
								return -2; //unable to create glyph
							}
							++p;
						}
					break;
				}
			} else {
				return -1; //unable to create font
			}
		}
		switch (mode){
			case DRAW_TEXT:
					//render caret
					if (
			//			((clip) || (glyph->bounds.x1>=glyph->bounds.x2)) &&
						(m_caret !=0) &&
						(m_caret->x == (int)last_text_attr->index) &&
//						(m_caret->y > (int)strlen(last_text_attr->text))
						(m_caret->y > (int)wcslen(last_text_attr->text))
					){
						if (m_gren == glyph_ren_outline){
							m_path_storage->move_to(
								m_text_pos_x-1,
								m_text_pos_y-last_text_attr->asc
							);
							m_path_storage->hline_rel(2);
							m_path_storage->vline_to(
								m_text_pos_y+last_text_attr->desc
							);
							m_path_storage->hline_rel(-2);
							m_path_storage->close_polygon();
						} else {
							m_ren_base.blend_bar((int)m_text_pos_x-1,(int)(m_text_pos_y-last_text_attr->asc),(int)(m_text_pos_x),(int)(m_text_pos_y+last_text_attr->desc), rgba8(0,0,0,255) ,255);
						}
					}

					if (m_gren == glyph_ren_outline){
						//close last path
						m_path_storage->move_to(0,0);
						m_path_storage->add_vertex(0.0, 0.0, path_cmd_stop);
					}
				break;
			case CARET_TO_OFFSET:
					m_tmp_val.pair.x=(int)m_text_pos_x-1;
					m_tmp_val.pair.y=(int)m_text_pos_y-last_text_attr->asc;

					caret_info.lh_asc=m_text_attributes[mlha].asc;
					caret_info.lh_desc=m_text_attributes[mlha].desc;
					caret_info.lh_space_y=m_text_attributes[mlha].space_y;
					caret_info.ch_asc=last_text_attr->asc;
					caret_info.ch_desc=last_text_attr->desc;
				return 0;
			case OFFSET_TO_CARET:
                   //return if there is no string
                    if(last_text_attr->text==0) return 0;

					//return tail of last string
					m_tmp_val.pair.x=last_text_attr->index;
//					m_tmp_val.pair.y=1+strlen(last_text_attr->text);
					m_tmp_val.pair.y=1+wcslen(last_text_attr->text);

					caret_info.lh_asc=m_text_attributes[mlha].asc;
					caret_info.lh_desc=m_text_attributes[mlha].desc;
					caret_info.lh_space_y=m_text_attributes[mlha].space_y;
					caret_info.ch_asc=last_text_attr->asc;
					caret_info.ch_desc=last_text_attr->desc;
				return 0;
			case SIZE_TEXT:
				m_tmp_val.pair.x = (int)sx;
				m_tmp_val.pair.y = (int)sy;
				return 0;
		}

		return glyph_count;
	}
}
