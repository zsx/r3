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

#include "agg_graphics.h"

//include "agg_effects.h"

#if defined(AGG_WIN32_FONTS) || defined(AGG_FREETYPE)
#include "agg_truetype_text.h"

extern "C" {
	#include "../include/host-text-api.h"
	void* Rich_Text;
}
#endif

namespace agg
{

agg_graphics::agg_graphics(ren_buf* buf,int w, int h, int x, int y) :
//	agg_graphics::agg_graphics(unsigned char* buf, int x, int y) :
//	agg_graphics::agg_graphics(DRAW_DEF *defaults, unsigned char* buf) :
		//setup pipeline
		m_trans(m_path, m_output_mtx),
		m_trans_curved(m_trans),
		m_stroke(m_trans_curved),
		m_dash(m_trans_curved),

		m_dashed_stroke(m_dash),

		m_arrow(m_stroke.markers(), m_ah),


		//setup interpolators
		m_interpolator_linear(m_img_mtx),
		m_interpolator_trans(m_trans_perspective),
		m_interpolator_persp()
	{
		//set buffer
		m_buf = buf;

		m_offset_x = x;
		m_offset_y = y;

		m_mtx_offset_x = x;
		m_mtx_offset_y = y;
		
		m_actual_width = w;
		m_actual_height = h;
/*
		m_defaults = defaults;

		m_actual_width = m_defaults->width;
		m_actual_height = m_defaults->height;
*/
		m_initial_width = m_actual_width;
		m_initial_height = m_actual_height;

		m_ratio_x = 1;
		m_ratio_y = 1;

//		m_resize_mtx = trans_affine_translation(0.5, 0.5);
//		m_resize_mtx = trans_affine_translation(x,y);
		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;

		m_mtx_store = new double [6];
		agg_init();

	}

	agg_graphics::~agg_graphics()
	{
		delete [] m_mtx_store;
		agg_reset();
	}

	void agg_graphics::agg_size(REBPAR* p)
	{
		p->x = m_actual_width-1;
		p->y = m_actual_height-1;
	}

	rendering_buffer* agg_graphics::agg_buffer()
	{
		return m_buf;
	}

	void agg_graphics::agg_set_buffer(ren_buf* buf,int w, int h, int x, int y)
	{
		m_buf = buf;
		m_offset_x = x;
		m_offset_y = y;

		m_actual_width = w;
		m_actual_height = h;
	}

	template <class Renderer>
	void agg_graphics::agg_render_sbool(Renderer& ren, bool mode){
		if (mode){
			sbool_combine_shapes_aa(
				sbool_and,
				m_ras,
				m_ras_bool,
				m_u_sl1_bool,
				m_u_sl2_bool,
				m_u_sl_result_bool,
				ren
			);
		} else {
			sbool_combine_shapes_bin(
				sbool_and,
				m_ras,
				m_ras_bool,
				m_u_sl1_bool,
				m_u_sl2_bool,
				m_u_sl_result_bool,
				ren
			);
		}
	}
	
	void agg_graphics::agg_render(ren_base renb){
//			ren_buf rbuf;
//			rbuf.attach(m_buf, m_actual_width, m_actual_height, m_actual_width * 4);
//			pixfmt pixf(rbuf);
//			ren_base renb(pixf);
			renderer_aa_solid ren_aa_s(renb);
			renderer_bin ren_b(renb);

			//update clipping boundaries
			renb.clip_box(m_offset_x, m_offset_y, m_actual_width-1, m_actual_height-1);
			m_ras.clip_box(m_offset_x, m_offset_y, m_actual_width, m_actual_height);
/*
Reb_Print(
			"attributes: %d = %d bytes vertices: %d = %d bytes",
			m_attributes.size(), m_attributes.size() * sizeof(path_attributes)
			,m_path.total_vertices(), m_path.total_vertices() * 17
);
*/
            //helper rectangle used for vectorial text sub-clipping
            rect rcb;
			
			//scanline boolean clip mode indicator
			bool sbool_clip = false;
			
			for(unsigned i = 0; i < m_attributes.size(); i++){
				path_attributes attr = m_attributes[i];
//				RL->print((REBYTE*)"rendering attr: %d type: %d\n", i, attr.filled);
				m_output_mtx = m_resize_mtx;
//#ifdef AGG_OPENGL
				attr.post_mtx *= trans_affine_translation(m_mtx_offset_x, m_mtx_offset_y);
//#endif
				m_output_mtx *= attr.post_mtx;
				m_ras.filling_rule(attr.fill_rule);

				double scl = m_output_mtx.scale();

				// Turn off processing of curve cusps
				m_trans_curved.angle_tolerance(0);


				if (attr.stroked && attr.line_width){
					//AA correction for stroke
					m_output_mtx *= trans_affine_translation(0.5, 0.5);
				}

				bool anti_aliased = attr.anti_aliased;
				
				if (anti_aliased){
					m_ras.gamma(gamma_power(1.0 / m_gamma));
				} else {
					m_ras.gamma(gamma_threshold(0.5));
				}


				double lw;

				(attr.line_width_mode) ? lw = attr.line_width : lw = attr.line_width * scl;

				bool vtext = ((int)attr.block == 1);
				
                if (vtext){
					attr.post_mtx.store_to(m_mtx_store);

					//if there is no rotation(or vtext uses image stroke) use rectangular clipping (faster), othervise use scanline bool clip intersection
					if (
						(m_mtx_store[1] == 0.0 && m_mtx_store[2] == 0.0) ||
						(attr.stroked && attr.pen_img_buf)
					){
						double scx, scy;
						attr.post_mtx.scaling(&scx, &scy);
						REBXYF siz = {
							(attr.coord_x3 - attr.coord_x2) * scx,
							(attr.coord_y3 - attr.coord_y2) * scy
						};
						
						attr.post_mtx.transform(&attr.coord_x2, &attr.coord_y2);
						
						REBXYF oft = {
							attr.coord_x2,
							attr.coord_y2
						};

						//store the current clip
						rcb = renb.clip_box();

						rect tcb = rcb;

						//handle off-by one clip case when "renb.clip_box() = gob/size"
						if (tcb.x2 == m_actual_width-1) tcb.x2++;
						if (tcb.y2 == m_actual_height-1) tcb.y2++;
						
						rect cb((int)oft.x,(int)oft.y,(int)(oft.x + siz.x),(int)(oft.y + siz.y));

						tcb = intersect_rectangles(tcb, cb);

						if(!tcb.is_valid()) continue;
						
//						RL->print((REBYTE*)"draw vtext %dx%d %dx%d\n", tcb.x1,tcb.y1,tcb.x2,tcb.y2);

						renb.clip_box(tcb.x1,tcb.y1,tcb.x2-1,tcb.y2-1);
						m_ras.clip_box(tcb.x1,tcb.y1,tcb.x2,tcb.y2);
						
//						renb.blend_bar(tcb.x1,tcb.y1,tcb.x2,tcb.y2, agg::rgba8(255,0,0), 64);
					
						sbool_clip = false;
					} else {
						double x1 = attr.coord_x2;
						double y1 = attr.coord_y2;
						double x3 = attr.coord_x3;
						double y3 = attr.coord_y3;
						double x2 = x3;
						double y2 = y1;
						double x4 = x1;
						double y4 = y3;
						
						attr.post_mtx.transform(&x1, &y1);
						attr.post_mtx.transform(&x2, &y2);
						attr.post_mtx.transform(&x3, &y3);
						attr.post_mtx.transform(&x4, &y4);
						
						m_path_bool.remove_all();
						m_path_bool.start_new_path();
						m_path_bool.move_to(x1, y1);
						m_path_bool.line_to(x2, y2);
						m_path_bool.line_to(x3, y3);
						m_path_bool.line_to(x4, y4);
						m_path_bool.close_polygon();

						m_ras_bool.reset();
						m_ras_bool.add_path(m_path_bool);
						
						sbool_clip = true;
					}
				}
				
				switch (attr.filled){
					case RT_FILL://filling
						if (attr.fill_pen_img_buf != 0) {
							// image pattern fill
							ren_buf rbuf_img;
							// Create the image rendering buffer
							rbuf_img.attach(attr.fill_pen_img_buf, attr.fill_pen_img_buf_x, attr.fill_pen_img_buf_y, attr.fill_pen_img_buf_x * 4);

							m_img_mtx.reset();
							m_img_mtx = attr.mtx;
							m_img_mtx *= m_output_mtx;
							m_img_mtx.invert();

							typedef wrap_mode_repeat_auto_pow2  wrap_type;

							typedef span_pattern_filter_rgba_bilinear<rgba8,
																		component_order,
																		span_interpolator_linear<>,
																		wrap_type,
																		wrap_type> span_gen_type;
							typedef renderer_scanline_aa<ren_base, span_gen_type> renderer_type;
							typedef renderer_scanline_bin<ren_base, span_gen_type> renderer_bin_type;
							span_allocator<rgba8> sa;
							span_gen_type sg(sa,
											 rbuf_img,
											 m_interpolator_linear);

							m_ras.reset();
							m_ras.add_path(m_trans_curved, attr.index);

							if (anti_aliased){
								renderer_type ri(renb, sg);
								if (sbool_clip)
									agg_render_sbool(ri, anti_aliased);
								else
									render_scanlines(m_ras, m_u_sl, ri);
							} else {
								renderer_bin_type ri(renb, sg);
								if (sbool_clip)
									agg_render_sbool(ri, anti_aliased);
								else
									render_scanlines(m_ras, m_u_sl, ri);
							}
						} else {
							//plain fill
							m_ras.reset();
							m_ras.add_path(m_trans_curved, attr.index);
							if (anti_aliased){
								ren_aa_s.color(attr.fill_pen);
								if (sbool_clip)
									agg_render_sbool(ren_aa_s, anti_aliased);
								else
									render_scanlines(m_ras, m_p_sl, ren_aa_s);
							} else {
								ren_b.color(attr.fill_pen);
								if (sbool_clip)
									agg_render_sbool(ren_b, anti_aliased);
								else
									render_scanlines(m_ras, m_p_sl, ren_b);
							}
						}
						break;
					case RT_GORAUD://Goraud fill
						{
							typedef span_gouraud_rgba<rgba8> gouraud_span_gen_type;
							typedef span_allocator<gouraud_span_gen_type::color_type> gouraud_span_alloc_type;
							typedef renderer_scanline_aa<renderer_base<pixfmt>, gouraud_span_gen_type> renderer_gouraud;
							typedef renderer_scanline_bin<renderer_base<pixfmt>, gouraud_span_gen_type> renderer_bin_gouraud;

							gouraud_span_alloc_type span_alloc;
							gouraud_span_gen_type   span_gen(span_alloc);

							double* vertices;
							vertices = getVertices(m_trans, attr.index);


							span_gen.colors(
								attr.g_color1,
								attr.g_color2,
								attr.g_color3
							);

							span_gen.triangle(vertices[0], vertices[1], vertices[2], vertices[3], vertices[4], vertices[5], attr.coord_x);

							m_ras.reset();
							m_ras.add_path(span_gen);
							if (anti_aliased){
								renderer_gouraud        ren_gouraud(renb, span_gen);
							render_scanlines(m_ras, m_u_sl, ren_gouraud);
							} else {
								renderer_bin_gouraud        ren_gouraud(renb, span_gen);
								render_scanlines(m_ras, m_u_sl, ren_gouraud);
							}
							delete [] vertices;
						}
						break;
					case RT_IMAGE://image
						//rendering buffer for image
						{
							ren_buf rbuf_img;
							ren_buf rbuf_img_out;
							unsigned char* key_img_buffer = 0;

							pixfmt pixf_img(rbuf_img_out);

							pixfmt_pre pixf_pre(*m_buf);
							ren_base_pre renb_pre(pixf_pre);

							if (attr.g_color1.a != 0){//color key enabled
								// attach the original image to  rendering buffer
								rbuf_img.attach(attr.img_buf, (int)attr.coord_x, (int)attr.coord_y, (int)attr.coord_x * 4);
								//create new buffer for keyed image
								key_img_buffer = new unsigned char[(int)attr.coord_x * (int)attr.coord_y * 4];
//								memset(key_img_buffer, 255, (int)attr.coord_x * (int)attr.coord_y * 4);
								rbuf_img_out.attach(key_img_buffer, (int)attr.coord_x, (int)attr.coord_y, (int)attr.coord_x * 4);
								//copy the original image
								rbuf_img_out.copy_from(rbuf_img);
								//apply keying
								pixf_img.for_each_pixel(color_key(attr.g_color1));
							} else {
								//use original image data in no color key
								rbuf_img_out.attach(attr.img_buf, (int)attr.coord_x, (int)attr.coord_y, (int)attr.coord_x * 4);
							}

							//slower/fast blitting decission
//							m_img_mtx.reset();
							attr.post_mtx.store_to(m_mtx_store);

//							if (m_img_mtx == attr.post_mtx){
							if (m_mtx_store[0] == 1.0 && m_mtx_store[1] == 0.0 && m_mtx_store[2] == 0.0 && m_mtx_store[3] == 1.0){
								renb.blend_from(
									pixf_img, 0, (int)attr.coord_x2+(int)m_mtx_store[4], (int)attr.coord_y2+(int)m_mtx_store[5], 255
								);
								if (key_img_buffer !=0){
									delete [] key_img_buffer;
								}
								break;
							}

							m_img_mtx = attr.mtx;
							m_img_mtx *= m_output_mtx;
							m_img_mtx.invert();

							m_ras.reset();
							m_ras.add_path(m_trans_curved, attr.index);

							span_allocator<rgba8> sa;

							if (attr.img_filter_type == FT_NEAREST){
//Reb_Print ("NN filter");
								//nn filter
								typedef span_image_filter_rgba_nn<rgba8, component_order, span_interpolator_linear<> > span_gen_type;
								typedef renderer_scanline_aa<ren_base, span_gen_type> renderer_type;
								typedef renderer_scanline_bin<ren_base, span_gen_type> renderer_bin_type;

								span_gen_type sg(sa,
									rbuf_img_out,
									rgba8(255,0,0,0),
									m_interpolator_linear
									);

								if (anti_aliased){
									renderer_type ri(renb, sg);
									render_scanlines(m_ras, m_u_sl, ri);
								} else {
									renderer_bin_type ri(renb, sg);
									render_scanlines(m_ras, m_u_sl, ri);
								}

							} else {

								image_filter_lut filter;

								switch(attr.img_filter_type){
									case FT_BILINEAR:
										filter.calculate(agg::image_filter_bilinear());
										break;
									case FT_BICUBIC:
										filter.calculate(agg::image_filter_bicubic());
										break;
									case FT_GAUSSIAN:
										filter.calculate(agg::image_filter_gaussian());
									break;
								}

								double* vertices = getVertices(m_trans,attr.index);
								trans_perspective tr(vertices, vertices);
								delete[] vertices;

								switch(attr.img_filter_mode){
									case FM_NORMAL:
										{
//RL_Print ("filter without resampling");
											//filter without resampling

											typedef span_image_filter_rgba<agg::rgba8,component_order, span_interpolator_linear<> > span_gen_type;
											typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_type> renderer_type;
											typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_type> renderer_bin_type;

											span_gen_type sg(sa, rbuf_img_out, rgba8(255,0,0,255), m_interpolator_linear, filter);

											if (anti_aliased){
												renderer_type ri(renb_pre, sg);
												render_scanlines(m_ras, m_u_sl, ri);
											} else {
												renderer_bin_type ri(renb_pre, sg);
												render_scanlines(m_ras, m_u_sl, ri);
											}
										}
										break;
									case FM_RESAMPLE:
										{
//Reb_Print ("filter with resampling");
											//filter with resampling

											typedef span_image_resample_rgba_affine<agg::rgba8,component_order, span_allocator<rgba8> > span_gen_type;
											typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_type> renderer_type;
											typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_type> renderer_bin_type;

											span_gen_type sg(sa, rbuf_img_out, rgba8(255,0,0,255), m_interpolator_linear, filter);

											sg.blur(attr.img_filter_arg);

											if (anti_aliased){
												renderer_type ri(renb_pre, sg);
												render_scanlines(m_ras, m_u_sl, ri);
											} else {
												renderer_bin_type ri(renb_pre, sg);
												render_scanlines(m_ras, m_u_sl, ri);
											}
										}
										break;
								}
							}

							if (key_img_buffer !=0){
								delete [] key_img_buffer;
							}
						}
						break;
					case RT_PERSPECTIVE_IMAGE://perspective image
						{
							ren_buf rbuf_img;
							ren_buf rbuf_img_out;

							unsigned char* key_img_buffer = 0;

							pixfmt pixf_img(rbuf_img_out);

							pixfmt_pre pixf_pre(*m_buf);
							ren_base_pre renb_pre(pixf_pre);

							if (attr.g_color1.a != 0){//color key enabled
								// attach the original image to  rendering buffer
							rbuf_img.attach(attr.img_buf, (int)attr.coord_x, (int)attr.coord_y, (int)attr.coord_x * 4);
								//create new buffer for keyed image
								key_img_buffer = new unsigned char[(int)attr.coord_x * (int)attr.coord_y * 4];
//								memset(key_img_buffer, 255, (int)attr.coord_x * (int)attr.coord_y * 4);
								rbuf_img_out.attach(key_img_buffer, (int)attr.coord_x, (int)attr.coord_y, (int)attr.coord_x * 4);
								//copy the original image
								rbuf_img_out.copy_from(rbuf_img);
								//apply keying
								pixf_img.for_each_pixel(color_key(attr.g_color1));
							} else {
								//use original image data in no color key
								rbuf_img_out.attach(attr.img_buf, (int)attr.coord_x, (int)attr.coord_y, (int)attr.coord_x * 4);
							}

							double* vertices = getVertices(m_trans,attr.index);

                            //fast/slow blit decission
							if (
                                (vertices[1] == vertices[3]) &&
                                (vertices[5] == vertices[7]) &&
                                (vertices[0] == vertices[6]) &&
                                (vertices[2] == vertices[4]) &&
                                (vertices[2] - vertices[0] == attr.coord_x) &&
                                (vertices[7] - vertices[1] == attr.coord_y)
							){
							    attr.post_mtx.store_to(m_mtx_store);
							    if (m_mtx_store[0] == 1.0 && m_mtx_store[1] == 0.0 && m_mtx_store[2] == 0.0 && m_mtx_store[3] == 1.0){
//                                    RL_Print("Fast BLEND2 img %dx%d %dx%d\n", renb.xmin(), renb.ymin(),renb.xmax(), renb.ymax());
                                    renb.blend_from(
                                        pixf_img, 0, (int)vertices[0], (int)vertices[1], 255
                                    );
                                    if (key_img_buffer !=0){
                                        delete [] key_img_buffer;
                                    }
                                    break;
							    }
							}

							typedef span_allocator<rgba8> span_alloc_type;
							span_alloc_type sa;

							typedef wrap_mode_reflect_auto_pow2 wrap_type;

							m_ras.reset();
							m_ras.add_path(m_trans_curved, attr.index);

							if (attr.pattern_mode == PM_NORMAL){
								m_trans_perspective.quad_to_rect(vertices, 0, 0, attr.coord_x, attr.coord_y);
							} else {
								m_trans_perspective.quad_to_rect(vertices, attr.coord_x2, attr.coord_y2, attr.coord_x3, attr.coord_y3);
							}
							if(m_trans_perspective.is_valid()){


								if (attr.img_filter_type == FT_NEAREST){
//Reb_Print ("Perspective nn filter");
									//nn filter
									switch(attr.pattern_mode){
										case PM_NORMAL:
											{
												typedef span_image_filter_rgba_nn<rgba8, component_order, interp_trans > span_gen_normal;
												typedef renderer_scanline_aa<renderer_base<pixfmt>, span_gen_normal> renderer_type;
												typedef renderer_scanline_bin<renderer_base<pixfmt>, span_gen_normal> renderer_bin_type;

												span_gen_normal sg(sa,
														 rbuf_img_out,
														 rgba8(255,0,0,0),
														 m_interpolator_trans);
												if (anti_aliased){
													renderer_type ri(renb, sg);
													render_scanlines(m_ras, m_u_sl, ri);
												} else {
													renderer_bin_type ri(renb, sg);
													render_scanlines(m_ras, m_u_sl, ri);
												}
												break;
											}
										case PM_REPEAT:
											{
												typedef span_pattern_filter_rgba_nn<rgba8, component_order, interp_trans, wrap_mode_repeat_auto_pow2, wrap_mode_repeat_auto_pow2 > span_gen_repeat;
												typedef renderer_scanline_aa<renderer_base<pixfmt>, span_gen_repeat> renderer_type;
												typedef renderer_scanline_bin<renderer_base<pixfmt>, span_gen_repeat> renderer_bin_type;

												span_gen_repeat sg(sa,
														 rbuf_img_out,
														 m_interpolator_trans);
												if (anti_aliased){
													renderer_type ri(renb, sg);
													render_scanlines(m_ras, m_u_sl, ri);
												} else {
													renderer_bin_type ri(renb, sg);
													render_scanlines(m_ras, m_u_sl, ri);
												}
												break;
											}
										case PM_REFLECT:
											{
												typedef span_pattern_filter_rgba_nn<rgba8, component_order, interp_trans, wrap_mode_reflect_auto_pow2, wrap_mode_reflect_auto_pow2 > span_gen_reflect;
												typedef renderer_scanline_aa<renderer_base<pixfmt>, span_gen_reflect> renderer_type;
												typedef renderer_scanline_bin<renderer_base<pixfmt>, span_gen_reflect> renderer_bin_type;

												span_gen_reflect sg(sa,
														 rbuf_img_out,
														 m_interpolator_trans);
												if (anti_aliased){
													renderer_type ri(renb, sg);
													render_scanlines(m_ras, m_u_sl, ri);
												} else {
													renderer_bin_type ri(renb, sg);
													render_scanlines(m_ras, m_u_sl, ri);
												}
												break;
											}
									}
								} else {

									image_filter_lut filter;

									switch(attr.img_filter_type){
										case FT_BILINEAR:
											filter.calculate(agg::image_filter_bilinear());
											break;
										case FT_BICUBIC:
											filter.calculate(agg::image_filter_bicubic());
											break;
										case FT_GAUSSIAN:
											filter.calculate(agg::image_filter_gaussian());
											break;
									}

									switch(attr.img_filter_mode){
										case FM_NORMAL:
											{
//RL_Print ("Perspective filter without resampling");
												//filter without resampling
												switch(attr.pattern_mode){
													case PM_NORMAL:
														{
															typedef span_image_filter_rgba<agg::rgba8,component_order, interp_trans > span_gen_normal;
															typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_normal> renderer_type;
															typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_normal> renderer_bin_type;
															trans_perspective tr(vertices, vertices);
															span_gen_normal sg(sa, rbuf_img_out, rgba8(255,0,0,255), m_interpolator_trans, filter);
															if (anti_aliased){
																renderer_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															} else {
																renderer_bin_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															}
															break;
														}
													case PM_REPEAT:
														{
															typedef span_pattern_filter_rgba<agg::rgba8,component_order, interp_trans, wrap_mode_repeat_auto_pow2, wrap_mode_repeat_auto_pow2> span_gen_repeat;
															typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_repeat> renderer_type;
															typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_repeat> renderer_bin_type;
															span_gen_repeat sg(sa, rbuf_img_out, m_interpolator_trans, filter);
															if (anti_aliased){
																renderer_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															} else {
																renderer_bin_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															}
															break;
														}
													case PM_REFLECT:
														{
															typedef span_pattern_filter_rgba<agg::rgba8,component_order, interp_trans, wrap_mode_reflect_auto_pow2, wrap_mode_reflect_auto_pow2> span_gen_reflect;
															typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_reflect> renderer_type;
															typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_reflect> renderer_bin_type;
															span_gen_reflect sg(sa, rbuf_img_out, m_interpolator_trans, filter);
															if (anti_aliased){
																renderer_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															} else {
																renderer_bin_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															}
															break;
														}
												}
											}
											break;
										case FM_RESAMPLE:
											{
												//filter with resampling

//Reb_Print ("Perspective filter with resampling");

												typedef agg::span_subdiv_adaptor<interp_persp> subdiv_adaptor_type;
												subdiv_adaptor_type subdiv_adaptor(m_interpolator_persp);

												if (attr.pattern_mode == PM_NORMAL){
													m_interpolator_persp.quad_to_rect(vertices, 0, 0, attr.coord_x, attr.coord_y);
												} else {
													m_interpolator_persp.quad_to_rect(vertices, attr.coord_x2, attr.coord_y2, attr.coord_x3,attr.coord_y3);
												}

												switch(attr.pattern_mode){
													case PM_NORMAL:
														{

															typedef span_image_resample_rgba<agg::rgba8, component_order, subdiv_adaptor_type, span_alloc_type> span_gen_normal;
															typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_normal> renderer_type;
															typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_normal> renderer_bin_type;
															trans_perspective tr(vertices, vertices);
															span_gen_normal sg(sa, rbuf_img_out, rgba8(255,0,0,255), subdiv_adaptor, filter);

															sg.blur(attr.img_filter_arg);

															if (anti_aliased){
																renderer_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															} else {
																renderer_bin_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															}
															break;
														}
													case PM_REPEAT:
														{
															typedef span_pattern_resample_rgba<agg::rgba8,component_order, subdiv_adaptor_type, wrap_mode_repeat_auto_pow2, wrap_mode_repeat_auto_pow2, span_alloc_type > span_gen_repeat;
															typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_repeat> renderer_type;
															typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_repeat> renderer_bin_type;

															span_gen_repeat sg(sa, rbuf_img_out, subdiv_adaptor, filter);
															sg.blur(attr.img_filter_arg);

															if (anti_aliased){
																renderer_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															} else {
																renderer_bin_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															}
															break;
														}
													case PM_REFLECT:
														{
															typedef span_pattern_resample_rgba<agg::rgba8,component_order, subdiv_adaptor_type, wrap_mode_reflect_auto_pow2, wrap_mode_reflect_auto_pow2, span_alloc_type > span_gen_reflect;
															typedef agg::renderer_scanline_aa<ren_base_pre, span_gen_reflect> renderer_type;
															typedef agg::renderer_scanline_bin<ren_base_pre, span_gen_reflect> renderer_bin_type;

															span_gen_reflect sg(sa, rbuf_img_out, subdiv_adaptor, filter);
															sg.blur(attr.img_filter_arg);

															if (anti_aliased){
																renderer_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															} else {
																renderer_bin_type ri(renb_pre, sg);
																render_scanlines(m_ras, m_u_sl, ri);
															}
															break;
														}
												}

											}
											break;
									}
								}
							}

							delete [] vertices;

							if (key_img_buffer != 0){
								delete [] key_img_buffer;
							}
						}
						break;
					case RT_GRADIENT://gradient fill
						{

							typedef span_interpolator_linear<> interpolator_type;
							typedef span_gradient<rgba8,
													   interpolator_type,
													   gradient_polymorphic_wrapper_base,
													   color_function_profile> gradient_span_gen;

							typedef span_allocator<gradient_span_gen::color_type> gradient_span_alloc;
							typedef renderer_scanline_aa<renderer_base<pixfmt> , gradient_span_gen> renderer_gradient;

							typedef renderer_scanline_bin<renderer_base<pixfmt> , gradient_span_gen> renderer_bin_gradient;

							gradient_span_alloc    span_alloc;

							m_grad_mtx = attr.mtx;
							m_grad_mtx *= m_output_mtx;
							m_grad_mtx.invert();

							attr.gradient->mode(attr.gradient_mode);
							color_function_profile colors(attr.colors);
							interpolator_type      inter(m_grad_mtx);
							gradient_span_gen      span_gen(span_alloc, inter, *attr.gradient, colors, attr.coord_x, attr.coord_y);
							m_ras.add_path(m_trans_curved, attr.index);

							if (anti_aliased){
								renderer_gradient r1(renb, span_gen);
								if (sbool_clip)
									agg_render_sbool(r1, anti_aliased);
								else
									render_scanlines(m_ras, m_u_sl, r1);
							} else {
								renderer_bin_gradient r1(renb, span_gen);
								if (sbool_clip)
									agg_render_sbool(r1, anti_aliased);
								else
									render_scanlines(m_ras, m_u_sl, r1);
							}

						}
						break;
#if defined(AGG_WIN32_FONTS) || defined(AGG_FREETYPE)
					case RT_TEXT:
						{
							rich_text* rt = (rich_text*)Rich_Text;

							REBXYF siz = {
								attr.coord_x3 - attr.coord_x2,
								attr.coord_y3 - attr.coord_y2
							};
							
							attr.post_mtx.transform(&attr.coord_x2, &attr.coord_y2);
							
							REBXYF oft = {
								attr.coord_x2,
								attr.coord_y2
							};

                            rect cb = renb.clip_box();
							
							////handle off-by one clip case when "renb.clip_box() = gob/size"
							if (cb.x2 == renb.width()-1) cb.x2++;
							if (cb.y2 == renb.height()-1) cb.y2++;
							
							rect cb2((int)oft.x,(int)oft.y,(int)(oft.x + siz.x),(int)(oft.y + siz.y));

							cb = intersect_rectangles(cb, cb2);

							if(!cb.is_valid()) continue;

							rt->rt_reset();
                            rt->rt_attach_buffer(m_buf, (int)siz.x, (int)siz.y, m_offset_x, m_offset_y);
							rt_block_text(rt, attr.block);

//							RL->print((REBYTE*)"raster text %dx%d %dx%d %dx%d\n", cb.x1,cb.y1,cb.x2,cb.y2,(int)siz.x, (int)siz.y);

							//note: rt_set_clip() include bottom-right values
                            rt->rt_set_clip(cb.x1,cb.y1,cb.x2,cb.y2, (int)siz.x, (int)siz.y);
							
//							renb.blend_bar(cb.x1,cb.y1,cb.x2-1,cb.y2-1, agg::rgba8(255,0,0), 64);
							
							rt->rt_draw_text(DRAW_TEXT, &oft);
						}
						break;
#endif						
#ifdef TEMP_REMOVED
					case RT_EFFECT:
						{
							double sx = attr.coord_x2 - attr.coord_x;
							double sy = attr.coord_y2 - attr.coord_y;

							attr.post_mtx.transform(&attr.coord_x, &attr.coord_y);

							rect cb((int)attr.coord_x, (int)attr.coord_y, (int)(attr.coord_x + sx), (int)(attr.coord_y + sy));
							cb = intersect_rectangles(renb.clip_box(), cb);
							if(!cb.is_valid()) break;

							attr.clip_x1 = renb.xmin();
							attr.clip_y1 = renb.ymin();
							attr.clip_x2 = renb.xmax();
							attr.clip_y2 = renb.ymax();

							renb.clip_box(cb.x1, cb.y1, cb.x2, cb.y2);

							((agg_effects*)Effects)->init(renb, *m_buf,(int)sx,(int)sy,0, 0, 0);
							Effect_Gob(Effects, attr.block);

							renb.clip_box((int)attr.clip_x1, (int)attr.clip_y1, (int)attr.clip_x2, (int)attr.clip_y2);
						}
						break;
#endif
					case RT_CLIPPING://set clipping
						attr.post_mtx.transform(&attr.clip_x1, &attr.clip_y1);
						attr.post_mtx.transform(&attr.clip_x2, &attr.clip_y2);

                        //use intersection with the framebuffer to eliminate wrong clipping case when re-rendering GOB old position
                        rect cb(ROUND_TO_INT(attr.clip_x1), ROUND_TO_INT(attr.clip_y1), ROUND_TO_INT(attr.clip_x2), ROUND_TO_INT(attr.clip_y2));
                        rect cb2(m_offset_x, m_offset_y, m_actual_width, m_actual_height);
                        cb = intersect_rectangles(cb, cb2);

                        if (cb.is_valid()){
//                            RL->print((REBYTE*)"rt_clip: %dx%d %dx%d\n",cb.x1,cb.y1,cb.x2,cb.y2);
							//note: renb.clip_box includes bottom-right values
                            renb.clip_box(cb.x1,cb.y1,cb.x2,cb.y2);
                            m_ras.clip_box(cb.x1,cb.y1,cb.x2,cb.y2);
//							RL->print((REBYTE*)"renb.blend_bar: %dx%d %dx%d\n",0,0,cb.x1,cb.y1);
//							renb.blend_bar(0,0,103,300, agg::rgba8(255,0,0), 64);
//							RL->print((REBYTE*)"rt_clip PASSED\n");
                        } else {
//                            RL->print((REBYTE*)"invalid rt_clip\n");
                            renb.reset_clipping(false);
                            m_ras.reset_clipping();
                        }
						continue;
				}

				m_ras.filling_rule(fill_non_zero);

                if (lw != 0.0) {
					if (attr.stroked) {
	                    // If the *visual* line width is considerable we
	                    // turn on processing of curve cusps.
	                    //---------------------
	                    if(lw > 1.0)
	                    {
	                        m_trans_curved.angle_tolerance(0.2);
	                    }

						if (attr.arrow_head == 1)
							m_stroke.shorten(lw * 2.0);
						else
							m_stroke.shorten(0.0);

						if (attr.pen_img_buf != 0) {
							//stroke image pattern
							m_ras.reset();
							pattern_filter_bilinear_rgba8 fltr;

							typedef line_image_pattern<pattern_filter_bilinear_rgba8> pattern_type;
							typedef renderer_outline_image<ren_base, pattern_type> renderer_type;
							typedef rasterizer_outline_aa<renderer_type>                rasterizer_type;

							// Create the image rendering buffer
							rendering_buffer rbuf_img;
							rbuf_img.attach(attr.pen_img_buf, attr.pen_img_buf_x, attr.pen_img_buf_y, attr.pen_img_buf_x * 4);
							pixfmt pixf_img(rbuf_img);
							ren_base renb_img(pixf_img);

							pattern_type patt(fltr, renb_img);

							renderer_type ren_img(renb, patt);
							rasterizer_type ras_img(ren_img);

//							ren_img.scale_x(.5);
//							ren_img.start_x(m_start_x.value());

							ras_img.add_path(m_trans_curved, attr.index);
						} else {
							m_stroke.width(lw);
							m_stroke.line_join(attr.stroke_line_join);
							m_stroke.line_cap(attr.stroke_line_cap);
							m_stroke.miter_limit(4.0);

							//rasterize & render stroke
							m_ras.reset();
//							m_ras.add_path(m_stroked_trans, attr.index);
							m_ras.add_path(m_stroke, attr.index);

							if (anti_aliased){
								ren_aa_s.color(attr.pen);
								if (sbool_clip)
									agg_render_sbool(ren_aa_s, anti_aliased);
								else
									render_scanlines(m_ras, m_p_sl, ren_aa_s);
							} else {
								ren_b.color(attr.pen);
								if (sbool_clip)
									agg_render_sbool(ren_b, anti_aliased);
								else
									render_scanlines(m_ras, m_p_sl, ren_b);
							}
						}

						//render arrows if needed
						if ((attr.arrow_head > 0) || (attr.arrow_tail > 0)){
							double k = ::pow(lw, 0.7);
							switch (attr.arrow_head){
								case 0:
									m_ah.no_head();
									m_ah.no_head_tail();
									break;
								case 1:
									m_ah.head(0, 8 * k, 3 * k, 2 * k);
									m_ah.no_head_tail();
									break;
								case 2:
									m_ah.head(1 * k, 1.5 * k, 3 * k, 5 * k);
//									m_ah.head((1 * k) + (6.5 * k), (1.5 * k) - (6.5 * k), 3 * k, 5 * k);
									m_ah.head_tail();
									break;
							}
							switch (attr.arrow_tail){
								case 0:
									m_ah.no_tail();
									m_ah.no_tail_head();
									break;
								case 1:
									m_ah.tail(0, 8 * k, 3 * k, 2 * k);
									m_ah.tail_head();
									break;
								case 2:
									m_ah.tail(1 * k, 1.5 * k, 3 * k, 5 * k);
//									m_ah.tail((1 * k)  + (6.5 * k), (1.5 * k) - (6.5 * k), 3 * k, 5 * k);
									m_ah.no_tail_head();
									break;
							}
							m_ras.reset();
							m_ras.add_path(m_arrow);

							if (anti_aliased){
								ren_aa_s.color(attr.arrow_color);
								if (sbool_clip)
									agg_render_sbool(ren_aa_s, anti_aliased);
								else
									render_scanlines(m_ras, m_p_sl, ren_aa_s);
							} else {
								ren_b.color(attr.arrow_color);
								if (sbool_clip)
									agg_render_sbool(ren_b, anti_aliased);
								else
									render_scanlines(m_ras, m_p_sl, ren_b);
							}
						} else {
							m_ah.no_head();
							m_ah.no_head_tail();
							m_ah.no_tail();
							m_ah.no_tail_head();
						}

					}

					if (attr.dashed && attr.dash_array){ // Carl - patch to fix bug 4010

						// define dashes pattern
						m_dash.remove_all_dashes();

						double* pattern = attr.dash_array;

						for(int j = 1; j < (int)pattern[0]; j+=2) {
							m_dash.add_dash((attr.line_width_mode) ? pattern[j] : pattern[j]*scl, (attr.line_width_mode) ? pattern[j+1] : pattern[j+1]*scl);
						}

						m_dash.dash_start(.5);
						m_dashed_stroke.miter_limit(4.0);
						m_dashed_stroke.width(lw);
						m_dashed_stroke.line_join(attr.dash_line_join);
						m_dashed_stroke.line_cap(attr.dash_line_cap);

						//rasterize & render dashed stroke
						m_ras.reset();
						m_ras.add_path(m_dashed_stroke, attr.index);

						if (anti_aliased){
							ren_aa_s.color(attr.line_pattern_pen);
							if (sbool_clip)
								agg_render_sbool(ren_aa_s, anti_aliased);
							else
								render_scanlines(m_ras, m_p_sl, ren_aa_s);
						} else {
							ren_b.color(attr.line_pattern_pen);
							if (sbool_clip)
								agg_render_sbool(ren_b, anti_aliased);
							else
								render_scanlines(m_ras, m_p_sl, ren_b);
						}
					}
				}

                if (vtext){
					if (sbool_clip) {
						sbool_clip = false;
					} else {
						//restore the current CLIP settings
						renb.clip_box(rcb.x1,rcb.y1,rcb.x2,rcb.y2);
						m_ras.clip_box(rcb.x1,rcb.y1,rcb.x2,rcb.y2);
					}
                }

			}
		}


	void agg_graphics::agg_set_gamma(double g)
	{
		m_gamma = g;
		m_ras.gamma(gamma_power(1.0 / m_gamma));
	}

	void agg_graphics::agg_set_clip(double origX,double origY, double margX, double margY)
	{
		m_attributes.add(path_attributes(0));
		path_attributes& attr = curr_attributes();
		agg_get_attributes(attr);
		attr.clip_x1 = origX;
		attr.clip_y1 = origY;
		attr.clip_x2 = margX;
		attr.clip_y2 = margY;
		attr.filled = RT_CLIPPING;
	}
/*
	void agg_graphics::agg_clear_buffer(unsigned int r, unsigned int g, unsigned int b, unsigned int a)
	{
		ren_buf rbuf;
		rbuf.attach(m_buf, m_actual_width, m_actual_height, m_actual_width * 4);
		pixfmt pixf(rbuf);
		ren_base renb(pixf);
		renb.clear(rgba8(r,g,b,a));
	}


	void agg_graphics::agg_resize_buffer(unsigned char *buf, int width, int height)
	{

		m_buf = buf;

		m_actual_width = width;
		m_actual_height = height;

		m_resize_mtx.reset();// = trans_affine_translation(0.5, 0.5);

		m_ratio_x = double(width) / m_initial_width;
		m_ratio_y = double(height) / m_initial_height;

		m_resize_mtx *= trans_affine_scaling(m_ratio_x, m_ratio_y);
	}

*/
	void agg_graphics::agg_transform(double rot, double ctrX, double ctrY, double sclX ,double sclY, double trX, double trY)
	{
//		rot = rot - (((int)(rot / 360)) * 360);
		rot = fmod(rot,360);

		ctrX*=m_ratio_x;
		ctrY*=m_ratio_y;
/*
		m_post_mtx *= trans_affine_translation(-ctrX, -ctrY);
		m_post_mtx *= trans_affine_scaling(sclX, sclY);
		m_post_mtx *= trans_affine_rotation(rot * pi / 180.0);
		m_post_mtx *= trans_affine_translation(trX+ctrX, trY+ctrY);
*/
		m_post_mtx.premultiply(trans_affine_translation(trX+ctrX, trY+ctrY));
		m_post_mtx.premultiply(trans_affine_rotation(rot * pi / 180.0));
		m_post_mtx.premultiply(trans_affine_scaling(sclX, sclY));
		m_post_mtx.premultiply(trans_affine_translation(-ctrX, -ctrY));

		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;

	}

	void agg_graphics::agg_rotate(double ang)
	{
//		ang = ang - (((int)(ang / 360)) * 360);

//		m_post_mtx *= trans_affine_rotation(ang * pi / 180.0);
		m_post_mtx.premultiply(trans_affine_rotation(fmod(ang,360) * pi / 180.0));
		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;

	}

	void agg_graphics::agg_translate(double trX, double trY)
	{
//		m_post_mtx *= trans_affine_translation(trX, trY);
		m_post_mtx.premultiply(trans_affine_translation(trX, trY));
		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;

	}

	void agg_graphics::agg_scale(double sclX ,double sclY)
	{
//		m_post_mtx *= trans_affine_scaling(sclX, sclY);
		m_post_mtx.premultiply(trans_affine_scaling(sclX, sclY));
		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;
	}

	void agg_graphics::agg_skew(double sx ,double sy)
	{
//		m_post_mtx *= trans_affine_skewing(sx, sy);
		m_post_mtx.premultiply(trans_affine_skewing(sx, sy));
		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;
	}


	void agg_graphics::agg_line_width(double w, int mode)
	{
		m_line_width = MAX(0.0,w);
		m_line_width_mode = mode;

	}

	path_attributes& agg_graphics::curr_attributes()
    {
		return m_attributes[m_attributes.size() - 1];
	}

	path_storage* agg_graphics::get_path_storage()
	{
		return &m_path;
	}

	void agg_graphics::agg_pen(unsigned r,unsigned g,unsigned b,unsigned a, unsigned char *img_buf, int x, int y)
	{
		m_pen.a = a;
		m_pen_img_buf = img_buf;
		m_pen_img_buf_x = x;
		m_pen_img_buf_y = y;
		if (a == 0){
			m_stroked = false;
		} else {
			m_stroked = true;
			m_pen.r = r;
			m_pen.g = g;
			m_pen.b = b;
		}

	}


	void agg_graphics::agg_fill_pen(unsigned r,unsigned g,unsigned b,unsigned a, unsigned char *img_buf, int x, int y)
	{
		m_fill_pen.r = r;
		m_fill_pen.g = g;
		m_fill_pen.b = b;
		m_fill_pen.a = a;
		m_fill_pen_img_buf = img_buf;
		m_fill_pen_img_buf_x = x;
		m_fill_pen_img_buf_y = y;
		if (a == 0){
			m_filled = RT_NONE;
		} else {
			m_filled = RT_FILL;
		}

	}

	void agg_graphics::agg_reset_gradient_pen(){
		m_filled = RT_FILL;
	}

	void agg_graphics::agg_gradient_pen(int grad, double oftX, double oftY, double begin, double end, double ang, double scX,double scY, unsigned char *colors, REBDEC* offsets, int mode)
	{
		switch (grad)
		{
			case W_DRAW_RADIAL:
				m_gradient = &gr_circle;
				break;
			case W_DRAW_CONIC:
				m_gradient = &gr_conic;
				break;
			case W_DRAW_DIAMOND:
				m_gradient = &gr_diamond;
				break;
			case W_DRAW_LINEAR:
				m_gradient = &gr_x;
				break;
			case W_DRAW_DIAGONAL:
				m_gradient = &gr_xy;
				break;
			case W_DRAW_CUBIC:
				m_gradient = &gr_sqrt_xy;
				break;
            default:
                return;
		}
		m_color_profile = new rgba8 [256];
		unsigned num_pnt = colors[0] - 1;

		unsigned o = 3, of = 2, j = 5;

		if (offsets[0] == -1)
			offsets = 0;
		else {
			//add colors on gradient edges if edge offsets are not default
			if (offsets[num_pnt+1] != 1.0) {
				offsets[num_pnt+2] = 1.0;
				unsigned pos = ((num_pnt+1) * 4) + 1;
				memcpy(&colors[pos+4], &colors[pos], 4);
				num_pnt++;
			}
			if (offsets[1] != 0.0) {
				offsets[0] = 0.0;
				memcpy(&colors[1], &colors[5], 4);
				o = 2;
				of = 1;
				j = 1;
				num_pnt++;
			}
		}

		m_coord_x = begin;
		m_coord_y = end;
		m_filled = RT_GRADIENT;
		m_grad_mtx = trans_affine_scaling(scX, scY);
		m_grad_mtx *= trans_affine_rotation(ang * pi / 180.0);
		m_grad_mtx *= trans_affine_translation(oftX, oftY);
		m_gradient_mode = mode;

		unsigned i;
		unsigned k = 0;
		unsigned rng = (offsets) ? unsigned(offsets[of] * 256) : 256 / num_pnt;
		unsigned r = rng;

		for(i = 0; i < num_pnt; i++)
		{
			rgba8 begin = rgba8(colors[j], colors[j + 1], colors[j + 2], colors[j + 3]);
			rgba8 end = rgba8(colors[j + 4], colors[j + 5], colors[j + 6], colors[j + 7]);

			if (i+1 == num_pnt) r = 256;
			if (r>256) r = 256;
			double s = 0.0;
			double inc = 1 / (double(r - k) - 1);
			for(;k < r; k++)
			{
				m_color_profile[k] =  begin.gradient(end, s);
//				printf("%d %f - %d.%d.%d.%d \n", k, s, m_color_profile[k].r, m_color_profile[k].g , m_color_profile[k].b , m_color_profile[k].a);
				s+=inc;
			}
			r+= (offsets) ? unsigned(offsets[i+o] * 256) - r : rng;
			j+=4;
		}
	}

	void agg_graphics::agg_line_pattern_pen(unsigned r,unsigned g,unsigned b,unsigned a)
	{
		m_line_pattern_pen.a = a;
		if (a == 0){
			m_dashed = false;
		} else {
			m_dashed = true;
			m_line_pattern_pen.r = r;
			m_line_pattern_pen.g = g;
			m_line_pattern_pen.b = b;
		}
	}
	void agg_graphics::agg_line_pattern(REBYTE* color, double *dash_array)
	{
		if (color){
			m_line_pattern_pen.a = color[3];
			if (m_line_pattern_pen.a == 0){
				m_dashed = false;
			} else {
				m_dashed = true;
				m_line_pattern_pen.r = color[0];
				m_line_pattern_pen.g = color[1];
				m_line_pattern_pen.b = color[2];
			}
		} else {
			m_dashed = false;
		}

		m_dash_array = dash_array;
	}

	void agg_graphics::agg_begin_path()
	{

		unsigned idx = m_path.start_new_path();
		m_attributes.add(path_attributes(idx));
		agg_get_attributes(curr_attributes());
	}

	void agg_graphics::agg_get_attributes(path_attributes& attr){

		attr.filled = m_filled;
		attr.stroked = m_stroked;
		attr.dashed = m_dashed;

		attr.dash_array = m_dash_array;

		attr.arrow_head = m_arrow_head;
		attr.arrow_tail = m_arrow_tail;
		attr.arrow_color = m_arrow_color;
		attr.anti_aliased = m_anti_aliased;
		attr.fill_rule = m_fill_rule;

		attr.pen = m_pen;
		attr.fill_pen = m_fill_pen;
		attr.line_pattern_pen = m_line_pattern_pen;

		attr.colors = m_color_profile;
		attr.mtx = m_grad_mtx;
		attr.post_mtx = m_post_mtx;

		attr.gradient = m_gradient;
		attr.gradient_mode = m_gradient_mode;

		attr.coord_x = m_coord_x;
		attr.coord_y = m_coord_y;

		attr.img_filter_type = m_img_filter_type;
		attr.img_filter_mode = m_img_filter_mode;
		attr.img_filter_arg = m_img_filter_arg;
		attr.pattern_mode = m_pattern_mode;

		attr.line_width = m_line_width;
		attr.line_width_mode = m_line_width_mode;

		attr.stroke_line_cap = m_stroke_cap;
		attr.stroke_line_join = m_stroke_join;
		attr.dash_line_cap = m_dash_cap;
		attr.dash_line_join = m_dash_join;

		attr.fill_pen_img_buf = m_fill_pen_img_buf;
		attr.pen_img_buf = m_pen_img_buf;
		attr.fill_pen_img_buf_x = m_fill_pen_img_buf_x;
		attr.fill_pen_img_buf_y = m_fill_pen_img_buf_y;
		attr.pen_img_buf_x = m_pen_img_buf_x;
		attr.pen_img_buf_y = m_pen_img_buf_y;
	}

	void agg_graphics::agg_set_attributes(path_attributes& attr){

		m_filled = attr.filled;
		m_stroked = attr.stroked;
		m_dashed = attr.dashed;

		m_dash_array = attr.dash_array;

		m_arrow_head = attr.arrow_head;
		m_arrow_tail = attr.arrow_tail;
		m_arrow_color = attr.arrow_color;
		m_anti_aliased = attr.anti_aliased;
		m_fill_rule = attr.fill_rule;

		m_pen = attr.pen;
		m_fill_pen = attr.fill_pen;
		m_line_pattern_pen = attr.line_pattern_pen;

		m_color_profile = attr.colors;
		m_grad_mtx = attr.mtx;
		m_post_mtx = attr.post_mtx;

		m_gradient = attr.gradient;
		m_gradient_mode = attr.gradient_mode;

		m_coord_x = attr.coord_x;
		m_coord_y = attr.coord_y;

		m_img_filter_type = attr.img_filter_type;
		m_img_filter_mode = attr.img_filter_mode;
		m_img_filter_arg = attr.img_filter_arg;
		m_pattern_mode = attr.pattern_mode;

		m_line_width = attr.line_width;
		m_line_width_mode = attr.line_width_mode;

		m_stroke_cap = attr.stroke_line_cap;
		m_stroke_join = attr.stroke_line_join;
		m_dash_cap = attr.dash_line_cap;
		m_dash_join = attr.dash_line_join;

		m_fill_pen_img_buf = attr.fill_pen_img_buf;
		m_pen_img_buf = attr.pen_img_buf;
		m_fill_pen_img_buf_x = attr.fill_pen_img_buf_x;
		m_fill_pen_img_buf_y = attr.fill_pen_img_buf_y;
		m_pen_img_buf_x = attr.pen_img_buf_x;
		m_pen_img_buf_y = attr.pen_img_buf_y;
	}

	void agg_graphics::agg_init(){
		//initial settings
		m_stroke_cap = butt_cap; //butt_cap square_cap round_cap
		m_stroke_join = miter_join;//miter_join miter_join_revert round_join bevel_join
		m_dash_cap = butt_cap;
		m_dash_join = miter_join;
		m_line_width = 1;
		m_line_width_mode = 0;
		m_pen = rgba8(255,255,255,255);
		m_fill_pen = rgba8(0,0,0,255);

		m_line_pattern_pen = rgba8(0,0,0,255);
		m_filled = RT_NONE;
		m_stroked = true;
		m_dashed = false;
		m_dash_array = 0;
		m_anti_aliased = true;
		m_arrow_head = 0;
		m_arrow_tail = 0;
		m_arrow_color = rgba8(255,255,255,255);
		m_fill_rule = fill_non_zero;
		m_color_profile = 0;
		m_pen_img_buf = 0;
		m_fill_pen_img_buf = 0;
		m_img_filter_type = FT_BILINEAR;
		m_img_filter_mode = FM_NORMAL;
		m_img_filter_arg = 1.0;
		m_pattern_mode = 0;
		m_img_border = 0;
		m_img_key_color = rgba8(255,255,255,0);
		m_pattern_x = 0;
		m_pattern_y = 0;
		m_pattern_w = 0;
		m_pattern_h = 0;

		agg_set_gamma(1);
		m_stroke.inner_join(inner_round);

		m_trans_curved.approximation_scale(1);
		m_stroke.approximation_scale(1);

	}

	void agg_graphics::agg_reset()
	{

		agg_init();

		//free the gradient color profile arrays from memory
		rgba8 * last_profile = 0;
		double * last_dash = 0;
		unsigned i;
		for(i = 0; i < m_attributes.size(); i++){
			path_attributes attr = m_attributes[i];
			if (attr.colors != last_profile){
				delete[] attr.colors;
				last_profile = attr.colors;
			}
			if (attr.dash_array != last_dash){
				delete[] attr.dash_array;
				last_dash = attr.dash_array;
			}
		}

		double* last_mtx = 0;
		for(i = 0; i < m_stack.size(); i++){
			path_attributes attr = m_stack[i];
			if (attr.colors != last_profile){
				delete[] attr.colors;
				last_profile = attr.colors;
			}
			if (attr.dash_array != last_dash){
				delete[] attr.dash_array;
				last_dash = attr.dash_array;
			}
		}

		m_path.remove_all();
		m_stack.remove_all();
		m_attributes.remove_all();
	}


	void agg_graphics::agg_line(
				   double x1, double y1,
				   double x2, double y2
	)
	{
		m_path.move_to(x1,y1);
		m_path.line_to(x2,y2);
	}


	void agg_graphics::agg_ellipse(
					  double x,  double y,
					  double rx, double ry)
	{
		// make ellipse
		bezier_arc e;
		e.init(x, y, rx, ry, 0.0, 2.0 * pi);
		agg_begin_path();
		m_path.add_path(e,0, false);
		m_path.close_polygon();
	}



	void agg_graphics::agg_arc(
					  double x,  double y,
					  double rx, double ry,
					double ang1, double ang2, int closed
	)
	{

		// make arc
		bezier_arc a;
		a.init(x,y,rx,ry, ang1 * pi / 180.0 , ang2 * pi / 180.0);

		agg_begin_path();
		m_path.add_path(a,0,false);
		if ((closed != 0) && (ang2 < 360)){
			m_path.line_to(x,y);
			m_path.close_polygon();
		}
	}


	void agg_graphics::agg_box(
				   double x1, double y1,
				   double x2, double y2
	)
	{
		agg_begin_path();
		m_path.move_to(x1, y1);
		m_path.line_to(x2, y1);
		m_path.line_to(x2, y2);
		m_path.line_to(x1, y2);
		m_path.close_polygon();
	}

	void agg_graphics::agg_rounded_rect(
				   double x1, double y1,
				   double x2, double y2, double r
	)
	{
		agg_begin_path();
		rounded_rect rect(x1, y1, x2, y2, r);
        rect.normalize_radius();
		rect.approximation_scale(m_output_mtx.scale());
		m_path.add_path(rect,0,false);
	}


	void agg_graphics::agg_begin_poly (double x,  double y)
	{
		agg_begin_path();
		m_path.move_to(x, y);
	}


	void agg_graphics::agg_add_vertex (double x,  double y)
	{
		m_path.line_to(x, y);
	}


	void agg_graphics::agg_curve3 (double x1, double y1, double x2,  double y2, double x3,  double y3)
	{
		agg_begin_path();
		m_path.move_to(x1, y1);
		m_path.curve3(x2, y2, x3, y3);
	}


	void agg_graphics::agg_curve4 (double x1, double y1, double x2,  double y2, double x3,  double y3, double x4,  double y4)
	{
		agg_begin_path();
		m_path.move_to(x1, y1);
		m_path.curve4(x2, y2, x3, y3, x4, y4);
	}


	void agg_graphics::agg_end_poly ()
	{
		m_path.close_polygon();
	}

	void agg_graphics::agg_end_bspline (int step, int closed)
	{

		if (closed != 0){
			m_path.close_polygon();
		}

		if (step <=0){
			step = 1;
		}

		path_attributes& cattr = curr_attributes();

		//bspline convertor
		conv_bspline<path_storage> bspline(m_path);
		bspline.interpolation_step(1.0 / step);

		path_storage tmp;
		tmp.add_path(bspline, cattr.index);

		cattr.index = cattr.index + (m_path.total_vertices() - cattr.index);
		m_path.add_path(tmp, 0,false);
	}


	void agg_graphics::agg_end_poly_img (
		unsigned char *img_buf,
		int sizX, int sizY  //, int pw, int ph, int ox, int oy, int pattern,
	){
		path_attributes& cattr = curr_attributes();
		double x,y,w,h;
		if (m_pattern_w == 0 || m_pattern_h == 0){
            m_path.vertex(cattr.index, &x, &y);
            m_path.vertex(cattr.index+2, &w, &h);
		}

		cattr.filled = RT_PERSPECTIVE_IMAGE;
		cattr.coord_x = sizX;
		cattr.coord_y = sizY;
		cattr.coord_x2 = m_pattern_x; //ox;
		cattr.coord_y2 = m_pattern_y; //oy;
		cattr.coord_x3 = m_pattern_x + ((m_pattern_w == 0) ? PHYS_COORD_X(w - x) : m_pattern_w); //ox + pw;
		cattr.coord_y3 = m_pattern_y + ((m_pattern_h == 0) ? PHYS_COORD_Y(h - y) : m_pattern_h); //oy + ph;
		cattr.img_buf = img_buf;
		cattr.pattern_mode = m_pattern_mode; //pattern;
		if (m_img_border == 0){ //outline == 0){
			cattr.dashed = false;
			cattr.stroked = false;
		}
		cattr.g_color1 = m_img_key_color; //rgba8(r1, g1, b1, a1);

		m_path.close_polygon();
	}


	void agg_graphics::agg_gtriangle(
		REBXYF p1, REBXYF p2, REBXYF p3, REBYTE* c1, REBYTE* c2, REBYTE* c3, double d
	)
	{
		//add line points
		agg_begin_path();
		path_attributes& cattr = curr_attributes();
		if (c1){
			cattr.filled = RT_GORAUD;//goraud
			cattr.g_color1 = rgba8(c1[0], c1[1], c1[2], c1[3]);
			cattr.g_color2 = rgba8(c2[0], c2[1], c2[2], c2[3]);
			cattr.g_color3 = rgba8(c3[0], c3[1], c3[2], c3[3]);
		}

		cattr.coord_x = d; //sets triangle dilation value

		m_path.move_to(p1.x,p1.y);
		m_path.line_to(p2.x,p2.y);
		m_path.line_to(p3.x,p3.y);
		m_path.close_polygon();
	}


	void agg_graphics::agg_image(
					unsigned char *img_buf,
				   double oftX, double oftY,
				   int sizX, int sizY //, int outline, int r1, int g1, int b1, int a1
	)
	{
		m_img_mtx.reset();
		m_img_mtx *= trans_affine_translation(oftX, oftY);

		agg_begin_path();
		m_path.move_to(oftX, oftY);
		m_path.line_to(oftX+sizX, oftY);
		m_path.line_to(oftX+sizX, oftY+sizY);
		m_path.line_to(oftX, oftY+sizY);
		m_path.close_polygon();

		path_attributes& cattr = curr_attributes();
		cattr.filled = RT_IMAGE;
		cattr.coord_x2 = oftX;
		cattr.coord_y2 = oftY;
		cattr.coord_x = sizX;
		cattr.coord_y = sizY;
		cattr.img_buf = img_buf;
		cattr.mtx = m_img_mtx;
		if (m_img_border == 0){ //outline == 0){
			cattr.dashed = false;
			cattr.stroked = false;
		}
		cattr.g_color1 = m_img_key_color;//rgba8(r1, g1, b1, a1);
	}

	void agg_graphics::agg_image_options(int r, int g, int b, int a ,int border){
		m_img_key_color = rgba8(r, g, b, a);
		m_img_border = border;
	}

	void agg_graphics::agg_image_pattern(int mode, double x, double y, double w, double h){
		m_pattern_mode = mode;
		m_pattern_x = x;
		m_pattern_y = x;
		m_pattern_w = w;
		m_pattern_h = h;
	}

    //------------------------------------------------------------------------

	int agg_graphics::agg_push_mtx()
    {
		path_attributes attr = path_attributes();
		agg_get_attributes(attr);
        m_stack.add(attr);
		return m_stack.size();
    }


    int agg_graphics::agg_pop_mtx()
    {
        if(m_stack.size() == 0)
        {
			return -1;
        }

		path_attributes attr = m_stack[m_stack.size() - 1];

		agg_set_attributes(attr);

        m_stack.remove_last();

		return m_stack.size();
    }

    void agg_graphics::agg_set_mtx(double mtx[])
    {
		trans_affine m;
		m.load_from(mtx);

//		m_post_mtx*=m;
		m_post_mtx.premultiply(m);
		m_output_mtx = m_resize_mtx;
		m_output_mtx *= m_post_mtx;
	}


    void agg_graphics::agg_reset_mtx()
    {
		m_post_mtx.reset();
		m_output_mtx = m_resize_mtx;
	}

	void agg_graphics::agg_invert_mtx()
    {
		m_post_mtx.invert();
		m_output_mtx = m_resize_mtx;
	}

    void agg_graphics::agg_anti_alias(bool mode)
    {
		m_anti_aliased = mode;
	}

    void agg_graphics::agg_image_filter(int filter, int mode, double blur)
    {
		m_img_filter_type = filter;
		m_img_filter_mode = mode;
		m_img_filter_arg = blur;
	}

    void agg_graphics::agg_arrows(REBYTE* color, int head, int tail)
    {
		if (color)
			m_arrow_color = rgba8(color[0],color[1],color[2],color[3]);
		else
			m_arrow_color = m_pen;
		m_arrow_head = head;
		m_arrow_tail = tail;
	}

    void agg_graphics::agg_fill_rule(filling_rule_e mode)
    {
		m_fill_rule = mode;
	}

    void agg_graphics::agg_stroke_join(line_join_e mode)
    {
		m_stroke_join = mode;
	}

    void agg_graphics::agg_dash_join(line_join_e mode)
    {
		m_dash_join = mode;
	}

    void agg_graphics::agg_stroke_cap(line_cap_e mode)
    {
		m_stroke_cap = mode;
	}

    void agg_graphics::agg_dash_cap(line_cap_e mode)
    {
		m_dash_cap = mode;
	}

	void agg_graphics::agg_effect(REBPAR* p1, REBPAR* p2, REBSER* block)
	{
			agg_begin_path();
			path_attributes& cattr = curr_attributes();
			cattr.filled = RT_EFFECT;
			cattr.block = block;

			cattr.coord_x = p1->x;
			cattr.coord_y = p1->y;
			cattr.coord_x2 = p2->x;
			cattr.coord_y2 = p2->y;

			m_path.add_vertex(0.0, 0.0, path_cmd_stop); //closes the path as it is not used in future processing...
	}
#if defined(AGG_WIN32_FONTS) || defined(AGG_FREETYPE)
	void agg_graphics::agg_text(REBINT vectorial, REBXYF* p1, REBXYF* p2, REBSER* block)
	{
		agg_begin_path();
		path_attributes& cattr = curr_attributes();

        cattr.coord_x2 = p1->x;
        cattr.coord_y2 = p1->y;
//			RL->print((REBYTE*)"W/H: %dx%d %dx%d\n", m_actual_width, m_actual_height, m_initial_width, m_initial_height);
//			RL->print((REBYTE*)"X/Y: %dx%d %dx%d\n", m_offset_x, m_offset_y, m_mtx_offset_x, m_mtx_offset_y);

        if (p2) {
            cattr.coord_x3 = p2->x;
            cattr.coord_y3 = p2->y;
        } else {
            cattr.coord_x3 = m_initial_width;
            cattr.coord_y3 = m_initial_height;
        }

		
		if (vectorial){
			//vectorial
            cattr.block = (void**)1; //used as 'vectorial text' flag in the render loop

			rich_text* rt = (rich_text*)Rich_Text;
			rt->rt_reset();

			int wx = cattr.coord_x3 - cattr.coord_x2;
			int wy = cattr.coord_y3 - cattr.coord_y2;

			double ox = cattr.coord_x2;
			double oy = cattr.coord_y2;
			
			cattr.post_mtx.transform(&ox, &oy);
			
			rt->rt_attach_buffer(
				m_buf,
				wx,
				wy,
				(int)(m_mtx_offset_x+ox),
				(int)(m_mtx_offset_y+oy)

			);

			rt_block_text(rt, block);

			//force to vectors no matter what was in the dialect block
			rt->rt_text_mode(2);
			rt->rt_set_graphics(this);
			rt->rt_draw_text(DRAW_TEXT, p1);
		} else {
			//raster
			cattr.filled = RT_TEXT;
			cattr.block = block;

			m_path.add_vertex(0.0, 0.0, path_cmd_stop); //closes the path as it is not used in future processing...
		}
	}
#endif

	//PATH sub-commands
    void agg_graphics::agg_path_move(int rel, double x, double y)          // M, m
    {
        if(rel) m_path.rel_to_abs(&x, &y);
        m_path.move_to(x, y);
    }

    void agg_graphics::agg_path_line(int rel, double x,  double y)         // L, l
    {
        if(rel) m_path.rel_to_abs(&x, &y);
        m_path.line_to(x, y);
    }

    void agg_graphics::agg_path_hline(int rel, double x)                   // H, h
    {
        double x2 = 0.0;
        double y2 = 0.0;
        if(m_path.total_vertices())
        {
            m_path.vertex(m_path.total_vertices() - 1, &x2, &y2);
            if(rel) x += x2;
            m_path.line_to(x, y2);
        }
    }

    void agg_graphics::agg_path_vline(int rel, double y)                   // V, v
    {
        double x2 = 0.0;
        double y2 = 0.0;
        if(m_path.total_vertices())
        {
            m_path.vertex(m_path.total_vertices() - 1, &x2, &y2);
            if(rel) y += y2;
            m_path.line_to(x2, y);
        }
    }

    void agg_graphics::agg_path_quadratic_curve(int rel, double x1, double y1,                   // Q, q
                               double x,  double y)
    {
        if(rel)
        {
            m_path.rel_to_abs(&x1, &y1);
            m_path.rel_to_abs(&x,  &y);
        }
        m_path.curve3(x1, y1, x, y);
    }

    void agg_graphics::agg_path_quadratic_curve_to(int rel, double x, double y)           // T, t
    {
        if(rel)
        {
			m_path.curve3_rel(x, y);
		} else {
			m_path.curve3(x, y);
		}
    }


    void agg_graphics::agg_path_cubic_curve(int rel, double x1, double y1,                   // C, c
                               double x2, double y2,
                               double x,  double y)
    {
        if(rel)
        {
            m_path.rel_to_abs(&x1, &y1);
            m_path.rel_to_abs(&x2, &y2);
            m_path.rel_to_abs(&x,  &y);
        }
        m_path.curve4(x1, y1, x2, y2, x, y);
    }


    void agg_graphics::agg_path_cubic_curve_to(int rel, double x2, double y2,                   // S, s
                               double x,  double y)
    {
        if(rel)
        {
			m_path.curve4_rel(x2, y2, x, y);
		} else {
			m_path.curve4(x2, y2, x, y);
		}

    }

    void agg_graphics::agg_path_arc(int rel, double rx, double ry,                   // A, c
							  double angle,
                              int large_arc,
                              int sweep,
							  double x,  double y)
    {
		bool sweep_flag = false;
		bool large_arc_flag = false;
		if (sweep){
			sweep_flag = true;
		}
		if (large_arc){
			large_arc_flag = true;
		}
		angle =  fmod(angle, 360) * pi / 180.0;
        if(!rel)
        {
			m_path.arc_to(
                              rx,ry,
							  angle,
                              large_arc_flag,
                              sweep_flag,
                              x, y);
		} else {
			m_path.arc_rel(
                              rx,ry,
							  angle,
                              large_arc_flag,
                              sweep_flag,
                              x, y);
        }
    }


    void agg_graphics::agg_path_close()								//Z,z
    {
        m_path.end_poly(path_flags_close);
    }

}
