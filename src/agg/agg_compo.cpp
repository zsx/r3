#include "agg_compo.h"
//include "agg_effects.h"
#include "agg_truetype_text.h"

extern "C" void RL_Print(char *fmt, ...);//output just for testing
extern "C" void Blit_Rect(REBGOB *gob, REBPAR d, REBPAR dsize, REBYTE *src, REBPAR s, REBPAR ssize);
extern "C" void* Rich_Text;
//extern "C" void* Effects;
extern "C" void *RL_Series(REBSER *ser, REBINT what);
extern "C" REBINT As_OS_Str(REBSER *series, REBCHR **string);

namespace agg
{
	compositor::compositor(REBGOB* rootGob, REBGOB* winGob) :
		m_pixf(m_rbuf_win),
		m_rb_win(m_pixf)
	{
		m_rootGob = rootGob;
        m_shift_x = 0;
        m_shift_y = 0;
		cp_alloc_buffer(winGob);

		m_blit_info = new blit_info [6];
	}

	compositor::compositor(REBYTE* buf, REBINT w, REBINT h) :
		m_pixf(m_rbuf_win),
		m_rb_win(m_pixf)
	{
		m_rootGob = 0;
		cp_set_win_buffer(buf, w, h);

		m_blit_info = new blit_info [6];
	}

	compositor::~compositor()
	{
		delete [] m_blit_info;
		if (m_buf) delete m_buf;
	}

	void compositor::cp_alloc_buffer(REBGOB* winGob){
		//setup and allocate window backbuffer
		m_width = GOB_W_INT(winGob);
		m_height = GOB_H_INT(winGob);
		m_stride = 4 * m_width;
#ifdef AGG_OPENGL		
		m_rbuf_win.attach(m_buf , m_width, m_height, m_stride);
#else
		int buflen = m_width * m_height * 4;
		m_buf = new REBYTE [buflen];
		memset(m_buf, 200, buflen);
#endif
	}

	void compositor::cp_set_win_buffer(REBYTE* buf, REBINT w, REBINT h)
	{
		m_width = w;
		m_height = h;
		m_stride = 4 * w;
#ifdef AGG_OPENGL
        m_rbuf_win.attach(m_buf , m_width, m_height, m_stride);
#else
		m_buf = buf;
		memset(m_buf, 200, w * h * 4);
#endif		
	}

	REBYTE* compositor::cp_get_win_buffer()
	{
		return m_buf;
	}

	REBOOL compositor::cp_resize(REBGOB* winGob)
	{
		if ((GOB_W(winGob) != GOB_WO(winGob)) || (GOB_H(winGob) != GOB_HO(winGob))) {
//			Reb_Print("resize window");
			//resize window buffer if size changed
			if ((GOB_W(winGob) != m_width) || (GOB_H(winGob) != m_height)) {
//				Reb_Print("resize buffer");
#ifdef AGG_OPENGL
                glViewport(0, 0, m_width, m_height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0 , m_width, m_height, 0, -10000, 10000);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glTranslatef(0.375, 0.375, 0.0);
#else
				delete m_buf;
				cp_alloc_buffer(winGob);
#endif
			}
			winGob->old_size.x = winGob->size.x;
			winGob->old_size.y = winGob->size.y;
			return TRUE;
		}
		return FALSE;
	}

/***********************************************************************
**
*/	REBINT compositor::cp_compose_gob(REBGOB* winGob, REBGOB* gob)
/*
**	 Compose the specified gob(and all its subgobs).
**
***********************************************************************/
	{
#ifndef AGG_OPENGL
		//Resize buffer in case win size changed programatically
		if ((m_rootGob) && ((GOB_W(winGob) != m_width) || (GOB_H(winGob) != m_height))) {
//			RL_Print("resize buffer ONLY");
			delete m_buf;
			cp_alloc_buffer(winGob);
		}
#endif
		REBINT result;

		REBINT ox = GOB_X_INT(gob);
		REBINT oy = GOB_Y_INT(gob);
		REBINT sx = GOB_W_INT(gob);
		REBINT sy = GOB_H_INT(gob);

		REBINT oox = GOB_XO_INT(gob);
		REBINT ooy = GOB_YO_INT(gob);
		REBINT osx = GOB_WO_INT(gob);
		REBINT osy = GOB_HO_INT(gob);

        //m_gobs = 0;

		m_gob = gob;
		m_parent_gob = GOB_PARENT(gob);

		if (
			(m_parent_gob) && (m_parent_gob != m_rootGob)
		){
			//calculate absolute offset

			REBGOB* parent = m_parent_gob;
			while (GOB_PARENT(parent) && GOB_PARENT(parent) != m_rootGob){
				REBINT pox = GOB_X_INT(parent);
				REBINT poy = GOB_Y_INT(parent);

				ox += pox;
				oy += poy;

				oox += pox;
				ooy += poy;
				parent = GOB_PARENT(parent);
			}
		} else {
			//top window refresh - always set m_visible flag
			m_parent_gob = gob;
			ox = 0;
			oy = 0;
			oox = 0;
			ooy = 0;
		}
#ifndef AGG_OPENGL
		int i;
		for (i = 0; i < 6; i++)
			m_blit_info[i].visible = false;

        if (m_rootGob == 0) {//special settings for to-image case
            //shift the 'viewport' so it renders to-image at the specific gob offset
            m_shift_x = ox;
            m_shift_y = oy;

            //force the offset/size values to 0x0->gob/size rectangle (will be used in clipbox)
            ox = 0;
            oy = 0;
            oox = 0;
            ooy = 0;
            osx = sx;
            osy = sy;
        }

		if (
			(oox != ox) ||
			(ooy != oy) ||
			(osx != sx) ||
			(osy != sy)
		){
			//GOB changed offset or size, rerender the old position area
			REBPAR winOft;

			REBINT obx = oox + osx;
			REBINT oby = ooy + osy;

			m_hidden = gob;

			if (
				(ox < obx) &&
				(oy < oby) &&
				((ox + sx) > oox) &&
				((oy + sy) > ooy)
			){
				//tesselate and render the old GOB position area(multiclipping)

				REBINT p1x = MAX(oox, MIN(obx, ox));
				REBINT p1y = MAX(ooy, oby);

				m_clip_box.x1 = oox;
				m_clip_box.y1 = ooy;
				m_clip_box.x2 = p1x;
				m_clip_box.y2 = p1y;

				//refresh from left
				if ((p1x - oox > 0) && (p1y - ooy > 0)) {

					m_visible = false;
					result = cp_process_gobs(winGob);
					if (result < 0) return result;

					if (m_visible){
						winOft.x = m_final_oft.x;
						winOft.y = m_height - m_final_siz.y - m_final_oft.y;
//RL_Print("left OLD gob: %dx%d %dx%d", m_final_oft.x,m_final_oft.y,m_final_siz.x,m_final_siz.y);
						m_blit_info[0].final_oft = m_final_oft;
						m_blit_info[0].final_siz = m_final_siz;
						m_blit_info[0].win_oft = winOft;
						m_blit_info[0].visible = true;
//						Blit_Rect(winGob, m_final_oft, m_final_siz, m_buf, winOft , m_final_siz);
					}
				}

				REBINT p2x = MAX(oox, MIN(obx, ox + sx));
				REBINT p2y = MAX(ooy, MIN(oby, ooy));

				m_clip_box.x1 = p2x;
				m_clip_box.y1 = p2y;
				m_clip_box.x2 = obx;
				m_clip_box.y2 = oby;

				//refresh from right
				if ((obx - p2x > 0) && (oby - p2y > 0)) {

					m_visible = false;
					result = cp_process_gobs(winGob);
					if (result < 0) return result;

					if (m_visible){
						winOft.x = m_final_oft.x;
						winOft.y = m_height - m_final_siz.y - m_final_oft.y;
//RL_Print("right OLD gob: %dx%d %dx%d", m_final_oft.x,m_final_oft.y,m_final_siz.x,m_final_siz.y);
						m_blit_info[1].final_oft = m_final_oft;
						m_blit_info[1].final_siz = m_final_siz;
						m_blit_info[1].win_oft = winOft;
						m_blit_info[1].visible = true;
//						Blit_Rect(winGob, m_final_oft, m_final_siz, m_buf, winOft , m_final_siz);
					}
				}

				REBINT p3x = MAX(oox, MIN(obx, p1x));
				REBINT p3y = MAX(ooy, MIN(oby, p2y));
				REBINT p4x = MAX(oox, MIN(obx, p2x));
				REBINT p4y = MAX(ooy, MIN(oby, oy));

				m_clip_box.x1 = p3x;
				m_clip_box.y1 = p3y;
				m_clip_box.x2 = p4x;
				m_clip_box.y2 = p4y;

				//refresh from top
				if ((p4x - p3x > 0) && (p4y - p3y > 0)){

					m_visible = false;
					result = cp_process_gobs(winGob);
					if (result < 0) return result;

					if (m_visible){
						winOft.x = m_final_oft.x;
						winOft.y = m_height - m_final_siz.y - m_final_oft.y;
//RL_Print("top OLD gob: %dx%d %dx%d", m_final_oft.x,m_final_oft.y,m_final_siz.x,m_final_siz.y);
						m_blit_info[2].final_oft = m_final_oft;
						m_blit_info[2].final_siz = m_final_siz;
						m_blit_info[2].win_oft = winOft;
						m_blit_info[2].visible = true;
//						Blit_Rect(winGob, m_final_oft, m_final_siz, m_buf, winOft , m_final_siz);
					}
				}

				REBINT p5x = MAX(oox, MIN(obx, p1x));
				REBINT p5y = MAX(ooy, MIN(oby, oy + sy));
				REBINT p6x = MAX(oox, MIN(obx, p5x + sx));
				REBINT p6y = MAX(ooy, MIN(oby, p1y));

				m_clip_box.x1 = p5x;
				m_clip_box.y1 = p5y;
				m_clip_box.x2 = p6x;
				m_clip_box.y2 = p6y;

				//refresh from bottom
				if ((p6x - p5x > 0) && (p6y - p5y > 0)){

					m_visible = false;
					result = cp_process_gobs(winGob);
					if (result < 0) return result;

					if (m_visible){
						winOft.x = m_final_oft.x;
						winOft.y = m_height - m_final_siz.y - m_final_oft.y;
//RL_Print("bottom OLD gob: %dx%d %dx%d", m_final_oft.x,m_final_oft.y,m_final_siz.x,m_final_siz.y);
						m_blit_info[3].final_oft = m_final_oft;
						m_blit_info[3].final_siz = m_final_siz;
						m_blit_info[3].win_oft = winOft;
						m_blit_info[3].visible = true;
//						Blit_Rect(winGob, m_final_oft, m_final_siz, m_buf, winOft , m_final_siz);
					}
				}

			} else {
				//no need to do multiclip - refresh full old area

				m_clip_box.x1 = oox;
				m_clip_box.y1 = ooy;
				m_clip_box.x2 = obx;
				m_clip_box.y2 = oby;

				m_visible = false;
				result = cp_process_gobs(winGob);
				if (result < 0) return result;

				if (m_visible){
					winOft.x = m_final_oft.x;
					winOft.y = m_height - m_final_siz.y - m_final_oft.y;
//RL_Print("full OLD gob: %dx%d %dx%d", m_final_oft.x,m_final_oft.y,m_final_siz.x,m_final_siz.y);
					m_blit_info[4].final_oft = m_final_oft;
					m_blit_info[4].final_siz = m_final_siz;
					m_blit_info[4].win_oft = winOft;
					m_blit_info[4].visible = true;
//					Blit_Rect(winGob, m_final_oft, m_final_siz, m_buf, winOft , m_final_siz);
				}
			}
		}
//Reb_Print("GOBS OLD: %d", m_gobs);
//		m_gobs = 0;
		//refresh the current GOB area
#endif
		m_clip_box.x1 = ox;
		m_clip_box.y1 = oy;
		m_clip_box.x2 = ox + sx;
		m_clip_box.y2 = oy + sy;

		m_hidden = 0;
		m_opaque = false;
		m_visible = false;

		if (
			(IS_GOB_OPAQUE(gob)) &&
			(GOB_ALPHA(gob) == 0)
		){
//			Reb_Print("OPAQUE SET");
			m_opaque = true;
		}

		result = cp_process_gobs(winGob);

		if (result < 0) return result;

/*
							HDC hdcScreen = CreateDC("DISPLAY", NULL, NULL, NULL);
							HDC hdcCompatible = CreateCompatibleDC(hdcScreen);
							HBITMAP bm = CreateCompatibleBitmap(hdcScreen, m_width,m_height);
							SetBitmapBits(bm,m_width * m_height * 4,m_buf);
							SelectObject(hdcCompatible, bm);
							int x,y;
							for (y = 0; y < m_height; y++)
								for (x = 0; x < m_width; x++)
									SetPixel(hdcCompatible, x, y, 0xFF0000FF);
							DeleteDC(hdcScreen);
							DeleteDC(hdcCompatible);
							DeleteObject(bm);

*/
#ifndef AGG_OPENGL
		if (m_visible){
			REBPAR winOft;
			winOft.x = m_final_oft.x;
			winOft.y = m_height - m_final_siz.y - m_final_oft.y;
//RL_Print("NEW gob: %dx%d %dx%d %dx%d\n", m_final_oft.x,m_final_oft.y,m_final_siz.x,m_final_siz.y, winOft.x, winOft.y);
			m_blit_info[5].final_oft = m_final_oft;
			m_blit_info[5].final_siz = m_final_siz;
			m_blit_info[5].win_oft = winOft;
			m_blit_info[5].visible = true;
//			Blit_Rect(winGob, m_final_oft, m_final_siz, m_buf, winOft , m_final_siz);
		}

		for (i = 0; i < 6; i++)
			if (m_blit_info[i].visible)
				Blit_Rect(winGob, m_blit_info[i].final_oft, m_blit_info[i].final_siz, m_buf, m_blit_info[i].win_oft , m_blit_info[i].final_siz);
#endif
//Reb_Print("old-offset/size: %dx%d %dx%d", gob->old_offset.x,gob->old_offset.y,gob->old_size.x,gob->old_size.y);
		//re-set old GOB area
		gob->old_offset.x = gob->offset.x;
		gob->old_offset.y = gob->offset.y;
		gob->old_size.x = gob->size.x;
		gob->old_size.y = gob->size.y;

//Reb_Print("GOBS: %d", m_gobs);

		return 0;
	}

/***********************************************************************
**
*/	REBINT compositor::cp_process_gobs(REBGOB* gob)
/*
**	Recursively process(and render) gob and its subfaces.
**
***********************************************************************/
	{


		if (GET_GOB_STATE(gob, GOBS_NEW)){
			//reset old-offset and old-size if newly added
			gob->old_offset.x = gob->offset.x;
			gob->old_offset.y = gob->offset.y;
			gob->old_size.x = gob->size.x;
			gob->old_size.y = gob->size.y;
			CLR_GOB_STATE(gob, GOBS_NEW);
		}

		REBINT ox = GOB_X_INT(gob);
		REBINT oy = GOB_Y_INT(gob);
		REBINT sx = GOB_W_INT(gob);
		REBINT sy = GOB_H_INT(gob);
//		Reb_Print("%dx%d %dx%d %d pane: %d type: %d %dx%d %dx%d", ox, oy, sx, sy, GOB_PANE(gob), (GOB_PANE(gob)) ? GOB_TAIL(gob) : 0, GOB_TYPE(gob), m_clip_box.x1, m_clip_box.y1, m_clip_box.x2, m_clip_box.y2);
		REBINT tx = sx;
		REBINT ty = sy;

		rect clip;

		if (GOB_PARENT(gob) && (GOB_PARENT(gob) != m_rootGob)){
			//clip against parent GOB
			REBGOB* parent = GOB_PARENT(gob);
			REBINT pox = 0;
			REBINT poy = 0;
			REBINT pw = GOB_W_INT(parent);
			REBINT ph = GOB_H_INT(parent);

			while (GOB_PARENT(parent) && GOB_PARENT(parent) != m_rootGob){
                pox += GOB_X_INT(parent);
                poy += GOB_Y_INT(parent);
				parent = GOB_PARENT(parent);
                if (GET_GOB_FLAG(parent, GOBF_WINDOW)){
                    //use possible offset shift for every first level gob in window
                    pox-=m_shift_x;
                    poy-=m_shift_y;
                }
			}

			ox += pox;
			oy += poy;

			rect cb(pox, poy, pox + pw, poy + ph);
			clip = intersect_rectangles(m_clip_box, cb);

			if(!cb.clip(clip)) return 0;

		} else {
			//otherwise clip against 'window GOB'
			ox = 0;
			oy = 0;
			rect cb(ox, oy, ox + sx, oy + sy);
			clip = intersect_rectangles(m_clip_box, cb);
		}

		if (m_hidden == gob){
			//skip it - don't render the 'hidden' GOB
			m_final_oft.x = clip.x1;
			m_final_oft.y = clip.y1;
			return 0;
		}

		if(!clip.is_valid()) return 0;

		if (oy > clip.y2)
			oy = clip.y2;

		if (ox + sx > clip.x2)
			sx = clip.x2 - ox;

		if (oy + sy > clip.y2)
			sy = clip.y2 - oy;
#ifdef AGG_OPENGL
        sx+=ox-1;
        sy+=oy-1;

		rect gob_cb(ox, oy, sx, sy);
		if(!gob_cb.clip(clip)) return 0;
#else
		//check if the clipbox is visible
		rect gob_cb(ox, oy, ox + sx-1, oy + sy-1);
		if(!gob_cb.clip(clip)) return 0;

		//clipbox is visible do the rendering
/*
		prim_pixfmt_type pixf(m_rbuf_win);
		m_rb_win(pixf);
*/

		m_rbuf_win.attach(m_buf + (ox * 4) + (oy * m_stride), sx, sy, m_stride);

		int abs_ox = ox;
		int abs_oy = oy;

		if (ox < clip.x1) {
			ox = -ox + clip.x1;
		} else {
			ox = 0;
		}
		if (oy < clip.y1) {
			oy = -oy + clip.y1;
		} else {
			oy = 0;
		}
#endif
		m_rb_win.clip_box(ox,oy,sx,sy);

		//try to store final area for later blitting
		if (m_gob == gob){
			m_final_oft.x = clip.x1;
			m_final_oft.y = clip.y1;
			m_opaque = false;
		}
		if (m_parent_gob == gob){
			m_visible = true;
			m_final_siz.x = sx - ox;
			m_final_siz.y = sy - oy;
		}

		if (!m_opaque){

				switch (GOB_TYPE(gob)) {
					case GOBT_COLOR:
						{
							//draw background color
							//FIXME: this is valid for LITTLE ENDIAN only
							long color = (long)GOB_CONTENT(gob);
							REBYTE alpha = GOB_ALPHA(gob);

							alpha = (alpha == 0) ? (color >> 24) & 255 : alpha;

							if (alpha == 0){
								m_rb_win.copy_bar(
#ifdef AGG_OPENGL
									ox,oy,
									sx,sy,									
#else
									0,0,
									tx,ty,
#endif
									rgba8(
										(color >> 16) & 255 ,
										(color >> 8) & 255 ,
										color & 255
									)
								);
							} else {
								m_rb_win.blend_bar(
#ifdef AGG_OPENGL
									ox,oy,
									sx,sy,
#else
									0,0,
									tx,ty,
#endif
									rgba8(
										(color >> 16) & 255 ,
										(color >> 8) & 255 ,
										color & 255,
										255 - alpha
									),
									255
								);
							}
						}
						break;
					case GOBT_IMAGE:
						{
						    //FIXME: temporary hack for getting image w,h
                            u16* d = (u16*)GOB_CONTENT(gob);
							int w = d[8];
							int h = d[9];
							//render image
							ren_buf m_rbuf_img;
							m_rbuf_img.attach(GOB_BITMAP(gob),w,h,w * 4);
							agg_graphics::pixfmt pixf_img(m_rbuf_img);

							if (GOB_ALPHA(gob) == 0){
//								m_rb_win.copy_from(m_rbuf_img);
	//							m_gobs++;
								// this will be enabled if image doesn't have alpha set but contains transparency by default.
								m_rb_win.blend_from(pixf_img);
							} else {
								m_rb_win.blend_from(pixf_img,0,0,0,255 - GOB_ALPHA(gob));
							}
						}
						break;
					case GOBT_DRAW:
						{
//							Reb_Print("GOB DRAW");
							agg_graphics::ren_base rb;

							REBYTE* tmp_buf = 0;
							ren_buf* renbuf;

							ren_buf rbuf_tmp;
							agg_graphics::pixfmt pixf_tmp(rbuf_tmp);
							agg_graphics::ren_base rb_tmp(pixf_tmp);
#ifdef AGG_OPENGL
							//render directly to the main buffer
							rb = m_rb_win;
							renbuf = &m_rbuf_win;
							void* graphics = new agg_graphics(renbuf, sx+1, sy+1, ox, oy);
#else
							if (GOB_ALPHA(gob) == 0){
								//render directly to the main buffer
								rb = m_rb_win;
								renbuf = &m_rbuf_win;
							} else {
								//create temporary buffer for later blending
								tmp_buf = new REBYTE [sx * sy * 4];
								memset(tmp_buf, 255, sx * sy * 4);
								rbuf_tmp.attach(tmp_buf,sx,sy,sx * 4);
								rb = rb_tmp;
								rb.clip_box(ox,oy,sx,sy);
								renbuf = &rbuf_tmp;
							}
							void* graphics = new agg_graphics(renbuf, tx, ty, ox, oy);
#endif
						
							REBSER *args = 0;

							REBINT result = Draw_Gob(graphics, (REBSER *)GOB_CONTENT(gob), args);

							if (result < 0) goto do_cleanup;

							result = ((agg_graphics*)graphics)->agg_render(rb);

						do_cleanup:
							delete (agg_graphics*)graphics;

							if (tmp_buf){

								//blend with main buffer
								m_rb_win.blend_from(pixf_tmp,0,0,0,255 - GOB_ALPHA(gob));

								//deallocate temoprary buffer
								delete tmp_buf;
							}

							if (result < 0) return result;
						}
						break;
					case GOBT_STRING:
#ifndef AGG_OPENGL
						if (
							!(
								(GOB_PARENT(gob) == m_rootGob) &&
								(GET_GOB_FLAG(gob, GOBF_WINDOW))
							)
						){
						    REBCHR* str;
						    REBOOL dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);

//							RL_Print("GOB string: %d %s\n" ,res, str);
                            if (str){
                                rich_text* rt = (rich_text*)Rich_Text;

                                rt->rt_reset();
                                rt->rt_attach_buffer(&m_rbuf_win, tx, ty, ox, oy);

                                rt->rt_set_text(str, dealloc);
                                rt->rt_push(1);

                                rt->rt_draw_text(DRAW_TEXT);
                            }
                        }
#endif
						break;
					case GOBT_TEXT:
//						RL_Print("GOB TEXT block!: %dx%d %dx%d\n", ox,oy,tx, ty);
#ifndef AGG_OPENGL
						rich_text* rt = (rich_text*)Rich_Text;

						rt->rt_reset();
						rt->rt_attach_buffer(&m_rbuf_win, tx, ty, ox, oy);

						REBINT result = Text_Gob(rt, (REBSER *)GOB_CONTENT(gob));

						rt->rt_draw_text();

						if (result < 0) return result;
#endif
						break;

#ifdef TEMP_REMOVED
					case GOBT_EFFECT:
						{
//							Reb_Print("GOB EFFECT %dx%d", abs_ox, abs_oy);
							((agg_effects*)Effects)->init(m_rb_win,m_rbuf_win,tx,ty,abs_ox, abs_oy, GOB_ALPHA(gob));
							REBINT result = Effect_Gob(Effects, (REBSER *)GOB_CONTENT(gob));
							if (result < 0) return result;
						}
						break;
#endif
				}

		}

		//recursively process sub GOBs
		if (GOB_PANE(gob)) {
			REBINT n;
			REBINT len = GOB_TAIL(gob);
			REBGOB **gp = GOB_HEAD(gob);

			//store the old clip box
			rect clip_tmp = m_clip_box;

			m_clip_box = clip;

			for (n = 0; n < len; n++, gp++) {
				REBINT result = cp_process_gobs(*gp);
				if (result < 0) return result;
			}

			m_clip_box = clip_tmp;
		}

		return 0;
	}
}
