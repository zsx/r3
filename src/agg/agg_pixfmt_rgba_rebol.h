//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.3
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software 
// is granted provided this copyright notice appears in all copies. 
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//
// Adaptation for high precision colors has been sponsored by 
// Liberty Technology Systems, Inc., visit http://lib-sys.com
//
// Liberty Technology Systems, Inc. is the provider of
// PostScript and PDF technology for software developers.
// 
//----------------------------------------------------------------------------

#ifndef AGG_PIXFMT_RGBA_INCLUDED
#define AGG_PIXFMT_RGBA_INCLUDED

#include <string.h>
#include <math.h>
#include "agg_basics.h"
#include "agg_color_rgba.h"
#include "agg_rendering_buffer.h"

namespace agg
{
#ifdef AGG_OPENGL
    struct vertex {
        GLfloat r,g,b,a,x,y;

        vertex() {}

        vertex(GLfloat r_, GLfloat g_, GLfloat b_, GLfloat a_, GLfloat x_, GLfloat y_) :
            r(r_), g(g_), b(b_), a(a_), x(x_), y(y_) {}
    };
#endif
    //=========================================================multiplier_rgba
    template<class ColorT, class Order> struct multiplier_rgba
    {
        typedef typename ColorT::value_type value_type;
        typedef typename ColorT::calc_type calc_type;

        //--------------------------------------------------------------------
        static AGG_INLINE void premultiply(value_type* p)
        {
            calc_type a = p[Order::A];
            if(a < ColorT::base_mask)
            {
                if(a == 0)
                {
                    p[Order::R] = p[Order::G] = p[Order::B] = 0;
                    return;
                }
                p[Order::R] = value_type((p[Order::R] * a + ColorT::base_mask) >> ColorT::base_shift);
                p[Order::G] = value_type((p[Order::G] * a + ColorT::base_mask) >> ColorT::base_shift);
                p[Order::B] = value_type((p[Order::B] * a + ColorT::base_mask) >> ColorT::base_shift);
            }
        }


        //--------------------------------------------------------------------
        static AGG_INLINE void demultiply(value_type* p)
        {
            calc_type a = p[Order::A];
            if(a < ColorT::base_mask)
            {
                if(a == 0)
                {
                    p[Order::R] = p[Order::G] = p[Order::B] = 0;
                    return;
                }
                calc_type r = (calc_type(p[Order::R]) * ColorT::base_mask) / a;
                calc_type g = (calc_type(p[Order::G]) * ColorT::base_mask) / a;
                calc_type b = (calc_type(p[Order::B]) * ColorT::base_mask) / a;
                p[Order::R] = value_type((r > ColorT::base_mask) ? ColorT::base_mask : r);
                p[Order::G] = value_type((g > ColorT::base_mask) ? ColorT::base_mask : g);
                p[Order::B] = value_type((b > ColorT::base_mask) ? ColorT::base_mask : b);
            }
        }
    };

    //=====================================================apply_gamma_dir_rgba
    template<class ColorT, class Order, class GammaLut> class apply_gamma_dir_rgba
    {
    public:
        typedef typename ColorT::value_type value_type;

        apply_gamma_dir_rgba(const GammaLut& gamma) : m_gamma(gamma) {}

        AGG_INLINE void operator () (value_type* p)
        {
            p[Order::R] = m_gamma.dir(p[Order::R]);
            p[Order::G] = m_gamma.dir(p[Order::G]);
            p[Order::B] = m_gamma.dir(p[Order::B]);
        }

    private:
        const GammaLut& m_gamma;
    };

    //=====================================================apply_gamma_inv_rgba
    template<class ColorT, class Order, class GammaLut> class apply_gamma_inv_rgba
    {
    public:
        typedef typename ColorT::value_type value_type;

        apply_gamma_inv_rgba(const GammaLut& gamma) : m_gamma(gamma) {}

        AGG_INLINE void operator () (value_type* p)
        {
            p[Order::R] = m_gamma.inv(p[Order::R]);
            p[Order::G] = m_gamma.inv(p[Order::G]);
            p[Order::B] = m_gamma.inv(p[Order::B]);
        }

    private:
        const GammaLut& m_gamma;
    };


    







    //=============================================================blender_rgba
    template<class ColorT, class Order> struct blender_rgba
    {
        typedef ColorT color_type;
        typedef Order order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        enum base_scale_e 
        { 
            base_shift = color_type::base_shift,
            base_mask  = color_type::base_mask
        };

        //--------------------------------------------------------------------
        static AGG_INLINE void blend_pix(value_type* p, 
                                         unsigned cr, unsigned cg, unsigned cb,
                                         unsigned alpha, 
                                         unsigned cover=0)
        {
            calc_type r = p[Order::R];
            calc_type g = p[Order::G];
            calc_type b = p[Order::B];
            calc_type a = p[Order::A];
            p[Order::R] = (value_type)(((cr - r) * alpha + (r << base_shift)) >> base_shift);
            p[Order::G] = (value_type)(((cg - g) * alpha + (g << base_shift)) >> base_shift);
            p[Order::B] = (value_type)(((cb - b) * alpha + (b << base_shift)) >> base_shift);
            p[Order::A] = (value_type)((alpha + a) - ((alpha * a + base_mask) >> base_shift));
        }
    };

    //=========================================================blender_rgba_pre
    template<class ColorT, class Order> struct blender_rgba_pre
    {
        typedef ColorT color_type;
        typedef Order order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        enum base_scale_e
        { 
            base_shift = color_type::base_shift,
            base_mask  = color_type::base_mask
        };

        //--------------------------------------------------------------------
        static AGG_INLINE void blend_pix(value_type* p, 
                                         unsigned cr, unsigned cg, unsigned cb,
                                         unsigned alpha,
                                         unsigned cover)
        {
            alpha = color_type::base_mask - alpha;
            cover = (cover + 1) << (base_shift - 8);
            p[Order::R] = (value_type)((p[Order::R] * alpha + cr * cover) >> base_shift);
            p[Order::G] = (value_type)((p[Order::G] * alpha + cg * cover) >> base_shift);
            p[Order::B] = (value_type)((p[Order::B] * alpha + cb * cover) >> base_shift);
            p[Order::A] = (value_type)(base_mask - ((alpha * (base_mask - p[Order::A])) >> base_shift));
        }

        //--------------------------------------------------------------------
        static AGG_INLINE void blend_pix(value_type* p, 
                                         unsigned cr, unsigned cg, unsigned cb,
                                         unsigned alpha)
        {
            alpha = color_type::base_mask - alpha;
            p[Order::R] = (value_type)(((p[Order::R] * alpha) >> base_shift) + cr);
            p[Order::G] = (value_type)(((p[Order::G] * alpha) >> base_shift) + cg);
            p[Order::B] = (value_type)(((p[Order::B] * alpha) >> base_shift) + cb);
            p[Order::A] = (value_type)(base_mask - ((alpha * (base_mask - p[Order::A])) >> base_shift));
        }
    };

    //======================================================blender_rgba_plain
    template<class ColorT, class Order> struct blender_rgba_plain
    {
        typedef ColorT color_type;
        typedef Order order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        enum base_scale_e { base_shift = color_type::base_shift };


        //--------------------------------------------------------------------
        static AGG_INLINE void blend_pix(value_type* p, 
                                         unsigned cr, unsigned cg, unsigned cb,
                                         unsigned alpha,
                                         unsigned cover=0)
        {
            if(alpha == 0) return;
            calc_type a = p[Order::A];
            calc_type r = p[Order::R] * a;
            calc_type g = p[Order::G] * a;
            calc_type b = p[Order::B] * a;
            a = ((alpha + a) << base_shift) - alpha * a;
            p[Order::A] = (value_type)(a >> base_shift);
            p[Order::R] = (value_type)((((cr << base_shift) - r) * alpha + (r << base_shift)) / a);
            p[Order::G] = (value_type)((((cg << base_shift) - g) * alpha + (g << base_shift)) / a);
            p[Order::B] = (value_type)((((cb << base_shift) - b) * alpha + (b << base_shift)) / a);
        }
    };
























    //===============================================copy_or_blend_rgba_wrapper
    template<class Blender> struct copy_or_blend_rgba_wrapper
    {
        typedef typename Blender::color_type color_type;
        typedef typename Blender::order_type order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        enum base_scale_e
        {
            base_shift = color_type::base_shift,
            base_size  = color_type::base_size,
            base_mask  = color_type::base_mask
        };

        //--------------------------------------------------------------------
        static AGG_INLINE void copy_or_blend_pix(value_type* p, 
                                                 unsigned cr, unsigned cg, unsigned cb,
                                                 unsigned alpha)
        {
			if(alpha)
            {
                if(alpha == base_mask)
                {
                    p[order_type::R] = cr;
                    p[order_type::G] = cg;
                    p[order_type::B] = cb;
                    p[order_type::A] = base_mask;
                }
                else
                {
                    Blender::blend_pix(p, cr, cg, cb, alpha);
                }
            }
        }

        //--------------------------------------------------------------------
        static AGG_INLINE void copy_or_blend_pix(value_type* p, 
                                                 unsigned cr, unsigned cg, unsigned cb,
                                                 unsigned alpha,
                                                 unsigned cover)
        {
            if(cover == 255)
            {
                copy_or_blend_pix(p, cr, cg, cb, alpha);
            }
            else
            {
                if(alpha)
                {
                    alpha = (alpha * (cover + 1)) >> 8;
                    if(alpha == base_mask)
                    {
                        p[order_type::R] = cr;
                        p[order_type::G] = cg;
                        p[order_type::B] = cb;
                        p[order_type::A] = base_mask;
                    }
                    else
                    {
                        Blender::blend_pix(p, cr, cg, cb, alpha, cover);
                    }
                }
            }
        }
    };





    
    //=======================================================pixel_formats_rgba
    template<class Blender, class PixelT = int32u> class pixel_formats_rgba
    {
    public:
        typedef rendering_buffer::row_data row_data;
        typedef rendering_buffer::span_data span_data;
        typedef PixelT   pixel_type;
        typedef Blender  blender_type;
        typedef typename blender_type::color_type color_type;
        typedef typename blender_type::order_type order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        typedef copy_or_blend_rgba_wrapper<blender_type> cob_type;
#ifdef AGG_OPENGL
        typedef pod_array<vertex> vertex_array;
#endif
        enum base_scale_e
        {
            base_shift = color_type::base_shift,
            base_size  = color_type::base_size,
            base_mask  = color_type::base_mask,
			pix_width  = sizeof(pixel_type)
        };

        //--------------------------------------------------------------------
        pixel_formats_rgba() : m_rbuf(0) {}
#ifdef AGG_OPENGL        
        pixel_formats_rgba(rendering_buffer& rb) :
         m_rbuf(&rb),
         m_point_array(),
         m_line_array(),
         m_va_chunk(3  *  sizeof(GLfloat) * 1024)
        {
        }
#else
        pixel_formats_rgba(rendering_buffer& rb) : m_rbuf(&rb) {}        
#endif        
        void attach(rendering_buffer& rb) { m_rbuf = &rb; }

        //--------------------------------------------------------------------
        template<class PixFmt>
        bool attach(PixFmt& pixf, int x1, int y1, int x2, int y2)
        {
            rect r(x1, y1, x2, y2);
            if(r.clip(rect(0, 0, pixf.width()-1, pixf.height()-1)))
            {
                int stride = pixf.stride();
                m_rbuf->attach(pixf.pix_ptr(r.x1, stride < 0 ? r.y2 : r.y1), 
                               (r.x2 - r.x1) + 1,
                               (r.y2 - r.y1) + 1,
                               stride);
                return true;
            }
            return false;
        }

        //--------------------------------------------------------------------
        AGG_INLINE unsigned width()  const { return m_rbuf->width();  }
        AGG_INLINE unsigned height() const { return m_rbuf->height(); }
		AGG_INLINE int      stride() const { return m_rbuf->stride(); }
#ifdef AGG_OPENGL
        void begin_va(vertex_array* va = 0) {
            if (va == 0) {
                begin_va(&m_point_array);
                begin_va(&m_line_array);
                return;
            }
            va->capacity(m_va_chunk);
//            glEnableClientState (GL_COLOR_ARRAY);
//            glEnableClientState (GL_VERTEX_ARRAY);

        }

        void end_va(vertex_array* va = 0)
        {
            if (va == 0) {
                end_va(&m_point_array);
                end_va(&m_line_array);
                return;
            }

            unsigned s = va->size();
            float* data = (float*)va->data();
            glColorPointer (4 , GL_FLOAT, 6 * sizeof(GLfloat), data);
            glVertexPointer (2 , GL_FLOAT, 6 * sizeof(GLfloat), &data[4]);
//RL_Print("glDrawArrays: %d\n", s);
            glDrawArrays((va == &m_point_array) ? GL_POINTS : GL_LINES, 0, s);
//glFlush();
//            glDisableClientState(GL_VERTEX_ARRAY);
//            glDisableClientState(GL_COLOR_ARRAY);
        }

        AGG_INLINE void add_vertex(const vertex& v, vertex_array &va)
        {
            unsigned s = va.size();

            if (s == m_va_chunk){
                end_va(&va);
                begin_va(&va);
            }

            va.add(v);
        }
#endif

        //--------------------------------------------------------------------
        AGG_INLINE color_type pixel(int x, int y) const
        {
            const value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            return color_type(p[order_type::R], 
                              p[order_type::G], 
                              p[order_type::B], 
                              p[order_type::A]);
        }

        //--------------------------------------------------------------------
        row_data row(int x, int y) const
        {
            return row_data(x, 
                            width() - 1, 
                            m_rbuf->row(y) + x * 4 * sizeof(value_type));
        }

        //--------------------------------------------------------------------
        span_data span(int x, int y, unsigned len)
        {
            return span_data(x, len, 
                             m_rbuf->row(y) + x * 4 * sizeof(value_type));
        }

        AGG_INLINE int8u* pix_ptr(int x, int y)
        {
            return m_rbuf->row(y) + x * pix_width;
        }

        AGG_INLINE const int8u* pix_ptr(int x, int y) const
        {
            return m_rbuf->row(y) + x * pix_width;
        }


        //--------------------------------------------------------------------
        AGG_INLINE void copy_pixel(int x, int y, const color_type& c)
        {
#ifndef AGG_OPENGL
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            p[order_type::R] = c.r;
            p[order_type::G] = c.g;
            p[order_type::B] = c.b;
            p[order_type::A] = c.a;
#endif
        }

        //--------------------------------------------------------------------
        AGG_INLINE void blend_pixel(int x, int y, const color_type& c, int8u cover)
        {
#ifndef AGG_OPENGL
            cob_type::copy_or_blend_pix((value_type*)m_rbuf->row(y) + (x << 2), 
                                        c.r, c.g, c.b, c.a, cover);
#endif
        }


        //--------------------------------------------------------------------
        AGG_INLINE void copy_hline(int x, int y, 
                                   unsigned len, 
                                   const color_type& c)
        {
#ifdef AGG_OPENGL
/*
			glColor4f( c.r * color_factor, c.g * color_factor ,c.b * color_factor, 1.0f);
			glVertex2f( x, y );
			glVertex2f( x + len , y );
*/
                float r = c.r * color_factor;
                float g = c.g * color_factor;
                float b = c.b * color_factor;

                add_vertex(agg::vertex(r,g,b,1.0,x,y), m_line_array);
                add_vertex(agg::vertex(r,g,b,1.0,x+len,y), m_line_array);
#else
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            pixel_type v;
            ((value_type*)&v)[order_type::R] = c.r;
            ((value_type*)&v)[order_type::G] = c.g;
            ((value_type*)&v)[order_type::B] = c.b;
            ((value_type*)&v)[order_type::A] = c.a;
            do
            {
                *(pixel_type*)p = v;
                p += 4;
            }
            while(--len);
#endif
        }


        //--------------------------------------------------------------------
        AGG_INLINE void copy_vline(int x, int y,
                                   unsigned len, 
                                   const color_type& c)
        {
#ifndef AGG_OPENGL
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            pixel_type v;
            ((value_type*)&v)[order_type::R] = c.r;
            ((value_type*)&v)[order_type::G] = c.g;
            ((value_type*)&v)[order_type::B] = c.b;
            ((value_type*)&v)[order_type::A] = c.a;
            do
            {
                *(pixel_type*)p = v;
                p = (value_type*)m_rbuf->next_row(p);
            }
            while(--len);
#endif
        }


        //--------------------------------------------------------------------
        void blend_hline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         int8u cover)
        {
#ifdef AGG_OPENGL
            if (c.a){
                calc_type alpha = (calc_type(c.a) * (calc_type(cover) + 1)) >> 8;
                float al = alpha * color_factor;
//                glBegin( GL_LINES );
//                glColor4f( c.r * color_factor, c.g * color_factor ,c.b * color_factor, al);
//                glColor4f(1,0,0,1);
//                glVertex2f( x, y );
//                glVertex2f( x + len , y );
/*
                glColor4f( c.r * color_factor, c.g * color_factor ,c.b * color_factor, al);
                glVertex2f( x, y );
                glVertex2f( x + len , y );
*/
                float r = c.r * color_factor;
                float g = c.g * color_factor;
                float b = c.b * color_factor;

                add_vertex(agg::vertex(r,g,b,al,x,y), m_line_array);
                add_vertex(agg::vertex(r,g,b,al,x+len,y), m_line_array);

//                glEnd();
            }

#else
            if (c.a)
            {
                value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
                calc_type alpha = (calc_type(c.a) * (cover + 1)) >> 8;
                if(alpha == base_mask)
                {
                    pixel_type v;
                    ((value_type*)&v)[order_type::R] = c.r;
                    ((value_type*)&v)[order_type::G] = c.g;
                    ((value_type*)&v)[order_type::B] = c.b;
                    ((value_type*)&v)[order_type::A] = c.a;
                    do
                    {
                        *(pixel_type*)p = v;
                        p += 4;
                    }
                    while(--len);
                }
                else
                {
                    if(cover == 255)
                    {
                        do
                        {
                            blender_type::blend_pix(p, c.r, c.g, c.b, alpha);
                            p += 4;
                        }
                        while(--len);
                    }
                    else
                    {
                        do
                        {
                            blender_type::blend_pix(p, c.r, c.g, c.b, alpha, cover);
                            p += 4;
                        }
                        while(--len);
                    }
                }
            }
#endif
        }


        //--------------------------------------------------------------------
        void blend_vline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         int8u cover)
        {
#ifndef AGG_OPENGL
            if (c.a)
            {
                value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
                calc_type alpha = (calc_type(c.a) * (cover + 1)) >> 8;
                if(alpha == base_mask)
                {
                    pixel_type v;
                    ((value_type*)&v)[order_type::R] = c.r;
                    ((value_type*)&v)[order_type::G] = c.g;
                    ((value_type*)&v)[order_type::B] = c.b;
                    ((value_type*)&v)[order_type::A] = c.a;
                    do
                    {
                        *(pixel_type*)p = v;
                        p = (value_type*)m_rbuf->next_row(p);
                    }
                    while(--len);
                }
                else
                {
                    if(cover == 255)
                    {
                        do
                        {
                            blender_type::blend_pix(p, c.r, c.g, c.b, alpha);
                            p = (value_type*)m_rbuf->next_row(p);
                        }
                        while(--len);
                    }
                    else
                    {
                        do
                        {
                            blender_type::blend_pix(p, c.r, c.g, c.b, alpha, cover);
                            p = (value_type*)m_rbuf->next_row(p);
                        }
                        while(--len);
                    }
                }
            }
#endif
        }


        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const int8u* covers)
        {
#ifdef AGG_OPENGL
            if (c.a)
            {
                float r = c.r * color_factor;
                float g = c.g * color_factor;
                float b = c.b * color_factor;

                if (len <= 2){
//                    glBegin( GL_POINTS );
                    do
                    {
                        calc_type alpha = (calc_type(c.a) * (calc_type(*covers) + 1)) >> 8;
                        float al = alpha * color_factor;
/*
                        glColor4f( r, g, b, al);
//                        glColor4f(1,0,0,1);
                        glVertex2f( x++, y );
*/

//                        glColor4f( r, g, b, al);
//                        glVertex2f( x++, y );

                        add_vertex(agg::vertex(r,g,b,al,x++,y), m_point_array);
//                        add_vertex(agg::vertex(r,g,b,al,x,y));

//                        glVertex2f( x  , y );

                        ++covers;
                    }
                    while(--len);
//                    glEnd();
                    return;
                }

                const int8u* cb = covers;
                do
                {
                    calc_type cc = calc_type(*covers);
                    calc_type cn = calc_type(*(covers + 1));

                    if (cc == cn && len != 1){
                        if (cb == covers)
                            cb = covers;
                        covers++;
                        continue;
                    } else {
                        calc_type alpha = ((calc_type(c.a) * (calc_type(*cb) + 1)) >> 8);
                        float al = alpha * color_factor;
                        unsigned ln = covers - cb;

                        if (ln == 0){
/*
                            glBegin( GL_POINTS );
//                            glColor4f(1,0,0,1);
                            glColor4f( r, g, b, al);
                            glVertex2f( x++, y );
//                            glVertex3f( 10.0f, 0.0f, 0.0f );
//                            glVertex3f( 10.0f, 329.0f, 0.0f );
                            glEnd();
*/
//                            glColor4f( r, g, b, al);
//                            glVertex2f( x++, y );
//                            glVertex2f( x  , y );

                            add_vertex(agg::vertex(r,g,b,al,x++,y), m_point_array);
//                            add_vertex(agg::vertex(r,g,b,al,x,y));

                        } else {
                            ln++;
//                            glBegin( GL_LINES );
//                            glColor4f(0,0,1,1);
//                            glColor4f( r, g, b, al);
//                            glVertex2f( x, y );
//                            glVertex2f( x+ln  , y );

                            add_vertex(agg::vertex(r,g,b,al,x,y), m_line_array);
                            add_vertex(agg::vertex(r,g,b,al,x+ln,y), m_line_array);
/*
                            glVertex2f( 8, 0 );
                            glVertex2f( 8, 1 );
                            glVertex2f( 8, 329 );
                            glVertex2f( 8, 330 );

*/
//                            glEnd();
                            x+=ln;
                        }
                        cb = ++covers;
                    }
                }
                while(--len);
            }


#else
            if (c.a)
            {
                value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
                do 
                {
                    calc_type alpha = (calc_type(c.a) * (calc_type(*covers) + 1)) >> 8;
                    if(alpha == base_mask)
                    {
                        p[order_type::R] = c.r;
                        p[order_type::G] = c.g;
                        p[order_type::B] = c.b;
                        p[order_type::A] = base_mask;
                    }
                    else
                    {
                        blender_type::blend_pix(p, c.r, c.g, c.b, alpha, *covers);
                    }
                    p += 4;
                    ++covers;
                }
                while(--len);
            }
#endif
        }


        //--------------------------------------------------------------------
        void blend_solid_vspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const int8u* covers)
        {
#ifndef AGG_OPENGL
            if (c.a)
            {
                value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
                do 
                {
                    calc_type alpha = (calc_type(c.a) * (calc_type(*covers) + 1)) >> 8;
                    if(alpha == base_mask)
                    {
                        p[order_type::R] = c.r;
                        p[order_type::G] = c.g;
                        p[order_type::B] = c.b;
                        p[order_type::A] = base_mask;
                    }
                    else
                    {
                        blender_type::blend_pix(p, c.r, c.g, c.b, alpha, *covers);
                    }
                    p = (value_type*)m_rbuf->next_row(p);
                    ++covers;
                }
                while(--len);
            }
#endif
        }


        //--------------------------------------------------------------------
        void copy_color_hspan(int x, int y,
                              unsigned len, 
                              const color_type* colors)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do 
            {
                p[order_type::R] = colors->r;
                p[order_type::G] = colors->g;
                p[order_type::B] = colors->b;
                p[order_type::A] = colors->a;
                ++colors;
                p += 4;
            }
            while(--len);
        }


        //--------------------------------------------------------------------
        void blend_color_hspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const int8u* covers,
                               int8u cover)
        {

            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            if(covers)
            {
                do 
                {
                    cob_type::copy_or_blend_pix(p, 
                                                colors->r, 
                                                colors->g, 
                                                colors->b, 
                                                colors->a, 
                                                *covers++);
                    p += 4;
                    ++colors;
                }
                while(--len);
            }
            else
            {
                if(cover == 255)
                {
                    do 
                    {
                        cob_type::copy_or_blend_pix(p, 
                                                    colors->r, 
                                                    colors->g, 
                                                    colors->b, 
                                                    colors->a);
                        p += 4;
                        ++colors;
                    }
                    while(--len);
                }
                else
                {
                    do 
                    {
                        cob_type::copy_or_blend_pix(p, 
                                                    colors->r, 
                                                    colors->g, 
                                                    colors->b, 
                                                    colors->a, 
                                                    cover);
                        p += 4;
                        ++colors;
                    }
                    while(--len);
                }
            }
        }



        //--------------------------------------------------------------------
        void blend_color_vspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const int8u* covers,
                               int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            if(covers)
            {
                do 
                {
                    cob_type::copy_or_blend_pix(p, 
                                                colors->r, 
                                                colors->g, 
                                                colors->b, 
                                                colors->a,
                                                *covers++);
                    p = (value_type*)m_rbuf->next_row(p);
                    ++colors;
                }
                while(--len);
            }
            else
            {
                if(cover == 255)
                {
                    do 
                    {
                        cob_type::copy_or_blend_pix(p, 
                                                    colors->r, 
                                                    colors->g, 
                                                    colors->b, 
                                                    colors->a);
                        p = (value_type*)m_rbuf->next_row(p);
                        ++colors;
                    }
                    while(--len);
                }
                else
                {
                    do 
                    {
                        cob_type::copy_or_blend_pix(p, 
                                                    colors->r, 
                                                    colors->g, 
                                                    colors->b, 
                                                    colors->a, 
                                                    cover);
                        p = (value_type*)m_rbuf->next_row(p);
                        ++colors;
                    }
                    while(--len);
                }
            }
        }


        //--------------------------------------------------------------------
        template<class Function> void for_each_pixel(Function f)
        {
            unsigned y;
            for(y = 0; y < height(); ++y)
            {
                unsigned len = width();
                value_type* p = (value_type*)m_rbuf->row(y);
                do
                {
                    f(p);
                    p += 4;
                }
                while(--len);
            }
        }

        //--------------------------------------------------------------------
        void premultiply()
        {
            for_each_pixel(multiplier_rgba<color_type, order_type>::premultiply);
        }

        //--------------------------------------------------------------------
        void demultiply()
        {
            for_each_pixel(multiplier_rgba<color_type, order_type>::demultiply);
        }

        //--------------------------------------------------------------------
        template<class GammaLut> void apply_gamma_dir(const GammaLut& g)
        {
            for_each_pixel(apply_gamma_dir_rgba<color_type, order_type, GammaLut>(g));
        }

        //--------------------------------------------------------------------
        template<class GammaLut> void apply_gamma_inv(const GammaLut& g)
        {
            for_each_pixel(apply_gamma_inv_rgba<color_type, order_type, GammaLut>(g));
        }

        //--------------------------------------------------------------------
        void copy_from(const rendering_buffer& from, 
                       int xdst, int ydst,
                       int xsrc, int ysrc,
                       unsigned len)
        {
            memmove((value_type*)m_rbuf->row(ydst) + xdst * 4, 
                    (value_type*)from.row(ysrc) + xsrc * 4, 
                    sizeof(value_type) * 4 * len);
        }


        //--------------------------------------------------------------------
        template<class SrcPixelFormatRenderer>
        void blend_from(const SrcPixelFormatRenderer& from, 
                        const int8u* psrc_,
                        int xdst, int ydst,
                        int xsrc, int ysrc,
                        unsigned len,
                        int8u cover)
        {
            typedef typename SrcPixelFormatRenderer::order_type src_order;
            const value_type* psrc = (const value_type*)psrc_;
            value_type* pdst = (value_type*)m_rbuf->row(ydst) + (xdst << 2);
            int incp = 4;
            if(xdst > xsrc)
            {
                psrc += (len-1) << 2;
                pdst += (len-1) << 2;
                incp = -4;
            }

            if(cover == 255)
            {
                do 
                {
                    cob_type::copy_or_blend_pix(pdst, 
                                                psrc[src_order::R],
                                                psrc[src_order::G],
                                                psrc[src_order::B],
                                                psrc[src_order::A]);
                    psrc += incp;
                    pdst += incp;
                }
                while(--len);
            }
            else
            {
                do 
                {
                    cob_type::copy_or_blend_pix(pdst, 
                                                psrc[src_order::R],
                                                psrc[src_order::G],
                                                psrc[src_order::B],
                                                psrc[src_order::A],
                                                cover);
                    psrc += incp;
                    pdst += incp;
                }
                while(--len);
            }
        }

    public:
        rendering_buffer* m_rbuf;
#ifdef AGG_OPENGL
        vertex_array m_point_array;
        vertex_array m_line_array;
        unsigned          m_va_chunk;
#endif
    };





    //===========================================pixfmt_custom_blend_rgba
    template<class Blender> class pixfmt_custom_blend_rgba
    {
    public:
        typedef rendering_buffer::row_data row_data;
        typedef rendering_buffer::span_data span_data;
        typedef Blender  blender_type;
        typedef typename blender_type::color_type color_type;
        typedef typename blender_type::order_type order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        enum base_scale_e
        {
            base_shift = color_type::base_shift,
            base_size  = color_type::base_size,
            base_mask  = color_type::base_mask
        };

        //--------------------------------------------------------------------
        pixfmt_custom_blend_rgba() : m_rbuf(0), m_comp_op(3) {}
        pixfmt_custom_blend_rgba(rendering_buffer& rb, unsigned comp_op=3) :
            m_rbuf(&rb),
            m_comp_op(comp_op)
        {}
        void attach(rendering_buffer& rb) { m_rbuf = &rb; }


        //--------------------------------------------------------------------
        AGG_INLINE unsigned width()  const { return m_rbuf->width();  }
        AGG_INLINE unsigned height() const { return m_rbuf->height(); }

        //--------------------------------------------------------------------
        void comp_op(unsigned op) { m_comp_op = op; }
        unsigned comp_op() const  { return m_comp_op; }

        //--------------------------------------------------------------------
        AGG_INLINE color_type pixel(int x, int y) const
        {
            const value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            return color_type(p[order_type::R], 
                              p[order_type::G], 
                              p[order_type::B], 
                              p[order_type::A]);
        }

        //--------------------------------------------------------------------
        row_data row(int x, int y) const
        {
            return row_data(x, 
                            width() - 1, 
                            m_rbuf->row(y) + x * 4 * sizeof(value_type));
        }

        //--------------------------------------------------------------------
        span_data span(int x, int y, unsigned len)
        {
            return span_data(x, len,
                             m_rbuf->row(y) + x * 4 * sizeof(value_type));
        }

        //--------------------------------------------------------------------
        AGG_INLINE void copy_pixel(int x, int y, const color_type& c)
        {
            blender_type::blend_pix(m_comp_op, 
                                    (value_type*)m_rbuf->row(y) + (x << 2), 
                                    c.r, c.g, c.b, c.a, 255);
        }

        //--------------------------------------------------------------------
        AGG_INLINE void blend_pixel(int x, int y, const color_type& c, int8u cover)
        {
            blender_type::blend_pix(m_comp_op, 
                                    (value_type*)m_rbuf->row(y) + (x << 2), 
                                    c.r, c.g, c.b, c.a,
                                    cover);
        }

        //--------------------------------------------------------------------
        AGG_INLINE void copy_hline(int x, int y, 
                                   unsigned len, 
                                   const color_type& c)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, 255);
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        AGG_INLINE void copy_vline(int x, int y,
                                   unsigned len, 
                                   const color_type& c)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, 255);
                p = (value_type*)m_rbuf->next_row(p);
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_hline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, cover);
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_vline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, cover);
                p = (value_type*)m_rbuf->next_row(p);
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const int8u* covers)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do 
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, *covers++);
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_solid_vspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const int8u* covers)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do 
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, *covers++);
                p = (value_type*)m_rbuf->next_row(p);
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void copy_color_hspan(int x, int y,
                              unsigned len, 
                              const color_type* colors)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do 
            {
                p[order_type::R] = colors->r;
                p[order_type::G] = colors->g;
                p[order_type::B] = colors->b;
                p[order_type::A] = colors->a;
                ++colors;
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_color_hspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const int8u* covers,
                               int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        p, 
                                        colors->r, 
                                        colors->g, 
                                        colors->b, 
                                        colors->a, 
                                        covers ? *covers++ : cover);
                p += 4;
                ++colors;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_color_vspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const int8u* covers,
                               int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->row(y) + (x << 2);
            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        p, 
                                        colors->r, 
                                        colors->g, 
                                        colors->b, 
                                        colors->a, 
                                        covers ? *covers++ : cover);
                p = (value_type*)m_rbuf->next_row(p);
                ++colors;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void copy_from(const rendering_buffer& from, 
                       int xdst, int ydst,
                       int xsrc, int ysrc,
                       unsigned len)
        {
            memmove((value_type*)m_rbuf->row(ydst) + xdst * 4, 
                    (value_type*)from.row(ysrc) + xsrc * 4, 
                    sizeof(value_type) * 4 * len);
        }

        //--------------------------------------------------------------------
        template<class SrcPixelFormatRenderer>
        void blend_from(const SrcPixelFormatRenderer& from, 
                        const int8u* psrc_,
                        int xdst, int ydst,
                        int xsrc, int ysrc,
                        unsigned len,
                        int8u cover)
        {
            typedef typename SrcPixelFormatRenderer::order_type src_order;
            const value_type* psrc = (const value_type*)psrc_;
            value_type* pdst = (value_type*)m_rbuf->row(ydst) + (xdst << 2);
            int incp = 4;
            if(xdst > xsrc)
            {
                psrc += (len-1) << 2;
                pdst += (len-1) << 2;
                incp = -4;
            }

            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        pdst, 
                                        psrc[src_order::R],
                                        psrc[src_order::G],
                                        psrc[src_order::B],
                                        psrc[src_order::A],
                                        cover);
                psrc += incp;
                pdst += incp;
            }
            while(--len);
        }

    private:
        rendering_buffer* m_rbuf;
        unsigned m_comp_op;
    };








    //================================================pixfmt_custom_rbuf_rgba
    template<class Blender, class RenBuf, class PixelT = int32u> 
    class pixfmt_custom_rbuf_rgba
    {
    public:
        typedef PixelT   pixel_type;
        typedef Blender  blender_type;
        typedef typename blender_type::color_type color_type;
        typedef typename blender_type::order_type order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        typedef copy_or_blend_rgba_wrapper<blender_type> cob_type;
        enum base_scale_e
        {
            base_shift = color_type::base_shift,
            base_size  = color_type::base_size,
            base_mask  = color_type::base_mask
        };

        typedef RenBuf rbuf_type;
        typedef typename rbuf_type::row_data row_data;
        typedef typename rbuf_type::span_data span_data;

        //--------------------------------------------------------------------
        pixfmt_custom_rbuf_rgba() : m_rbuf(0) {}
        pixfmt_custom_rbuf_rgba(rbuf_type& rb) : m_rbuf(&rb) {}
        void attach(rbuf_type& rb) { m_rbuf = &rb; }

        //--------------------------------------------------------------------
        unsigned width()  const { return m_rbuf->width();  }
        unsigned height() const { return m_rbuf->height(); }

        //--------------------------------------------------------------------
        color_type pixel(int x, int y) const
        {
            const value_type* p = m_rbuf->span_ptr(x, y, 1);
            return p ? color_type(p[order_type::R], 
                                  p[order_type::G], 
                                  p[order_type::B], 
                                  p[order_type::A]) :
                       color_type::no_color();
        }

        //--------------------------------------------------------------------
        row_data row(int x, int y) const
        {
            return m_rbuf->row(x, y);
        }

        //--------------------------------------------------------------------
        span_data span(int x, int y, unsigned len)
        {
            return m_rbuf->span(x, y, len);
        }

        //--------------------------------------------------------------------
        void copy_pixel(int x, int y, const color_type& c)
        {
            int8u* p = m_rbuf->span_ptr(x, y, 1);
            p[order_type::R] = c.r;
            p[order_type::G] = c.g;
            p[order_type::B] = c.b;
            p[order_type::A] = c.b;
        }

        //--------------------------------------------------------------------
        void blend_pixel(int x, int y, const color_type& c, int8u cover)
        {
            blender_type::copy_or_blend_pix((value_type*)m_rbuf->span_ptr(x, y, 1), 
                                            c.r, c.g, c.b, c.a, cover);
        }

        //--------------------------------------------------------------------
        void copy_hline(int x, int y, unsigned len, const color_type& c)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            pixel_type v;
            ((value_type*)&v)[order_type::R] = c.r;
            ((value_type*)&v)[order_type::G] = c.g;
            ((value_type*)&v)[order_type::B] = c.b;
            ((value_type*)&v)[order_type::A] = c.a;
            do
            {
                *(pixel_type*)p = v;
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void copy_vline(int x, int y, unsigned len, const color_type& c)
        {
            pixel_type v;
            ((value_type*)&v)[order_type::R] = c.r;
            ((value_type*)&v)[order_type::G] = c.g;
            ((value_type*)&v)[order_type::B] = c.b;
            ((value_type*)&v)[order_type::A] = c.a;
            do
            {
                *(pixel_type*)(m_rbuf->span_ptr(x, y++, 1)) = v;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_hline(int x, int y, unsigned len, 
                         const color_type& c, int8u cover)
        {
            if (c.a)
            {
                value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
                calc_type alpha = (calc_type(c.a) * (cover + 1)) >> 8;
                if(alpha == base_mask)
                {
                    pixel_type v;
                    ((value_type*)&v)[order_type::R] = c.r;
                    ((value_type*)&v)[order_type::G] = c.g;
                    ((value_type*)&v)[order_type::B] = c.b;
                    ((value_type*)&v)[order_type::A] = c.a;
                    do
                    {
                        *(pixel_type*)p = v;
                        p += 4;
                    }
                    while(--len);
                }
                else
                {
                    if(cover == 255)
                    {
                        do
                        {
                            blender_type::blend_pix(p, c.r, c.g, c.b, alpha);
                            p += 4;
                        }
                        while(--len);
                    }
                    else
                    {
                        do
                        {
                            blender_type::blend_pix(p, c.r, c.g, c.b, alpha, cover);
                            p += 4;
                        }
                        while(--len);
                    }
                }
            }
        }

        //--------------------------------------------------------------------
        void blend_vline(int x, int y, unsigned len, 
                         const color_type& c, int8u cover)
        {
            if (c.a)
            {
                calc_type alpha = (calc_type(c.a) * (cover + 1)) >> 8;
                if(alpha == base_mask)
                {
                    pixel_type v;
                    ((value_type*)&v)[order_type::R] = c.r;
                    ((value_type*)&v)[order_type::G] = c.g;
                    ((value_type*)&v)[order_type::B] = c.b;
                    ((value_type*)&v)[order_type::A] = c.a;
                    do
                    {
                        *(pixel_type*)(m_rbuf->span_ptr(x, y++, 1)) = v;
                    }
                    while(--len);
                }
                else
                {
                    if(cover == 255)
                    {
                        do
                        {
                            blender_type::blend_pix((value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                                    c.r, c.g, c.b, alpha);
                        }
                        while(--len);
                    }
                    else
                    {
                        do
                        {
                            blender_type::blend_pix((value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                                    c.r, c.g, c.b, alpha, cover);
                        }
                        while(--len);
                    }
                }
            }
        }


        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y, unsigned len, 
                               const color_type& c, const int8u* covers)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do 
            {
                if(*covers == 255)
                {
                    cob_type::copy_or_blend_pix(p, c.r, c.g, c.b, c.a);
                }
                else
                {
                    cob_type::copy_or_blend_pix(p, c.r, c.g, c.b, c.a, *covers);
                }
                p += 4;
                ++covers;
            }
            while(--len);
        }



        //--------------------------------------------------------------------
        void blend_solid_vspan(int x, int y, unsigned len, 
                               const color_type& c, const int8u* covers)
        {
            do 
            {
                if(*covers == 255)
                {
                    cob_type::copy_or_blend_pix((value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                                c.r, c.g, c.b, c.a);
                }
                else
                {
                    cob_type::copy_or_blend_pix((value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                                c.r, c.g, c.b, c.a, 
                                                *covers);
                }
                ++covers;
            }
            while(--len);
        }


        //--------------------------------------------------------------------
        void copy_color_hspan(int x, int y,
                              unsigned len, 
                              const color_type* colors)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do 
            {
                p[order_type::R] = colors->r;
                p[order_type::G] = colors->g;
                p[order_type::B] = colors->b;
                p[order_type::A] = colors->a;
                ++colors;
                p += 4;
            }
            while(--len);
        }


        //--------------------------------------------------------------------
        void blend_color_hspan(int x, int y, unsigned len, 
                               const color_type* colors, 
                               const int8u* covers,
                               int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do 
            {
                cob_type::copy_or_blend_pix(p, 
                                            colors->r,
                                            colors->g,
                                            colors->b,
                                            colors->a,
                                            covers ? *covers++ : cover);
                p += 4;
                ++colors;
            }
            while(--len);
        }


        //--------------------------------------------------------------------
        void blend_color_vspan(int x, int y, unsigned len, 
                               const color_type* colors, 
                               const int8u* covers,
                               int8u cover)
        {
            do 
            {
                cob_type::copy_or_blend_pix((value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                            colors->r,
                                            colors->g,
                                            colors->b,
                                            colors->a,
                                            covers ? *covers++ : cover);
                ++colors;
            }
            while(--len);

        }

        //--------------------------------------------------------------------
        template<class Function> void for_each_pixel(Function f)
        {
            unsigned y;
            for(y = 0; y < height(); ++y)
            {
                row_data sp = row(0, y);
                if(sp.ptr)
                {
                    value_type* p = (value_type*)sp.ptr;
                    while(sp.x1 <= sp.x2)
                    {
                        f(p);
                        p += 4;
                        ++sp.x1;
                    }
                }
            }
        }


        //--------------------------------------------------------------------
        void premultiply()
        {
            for_each_pixel(multiplier_rgba<color_type, order_type>::premultiply);
        }

        //--------------------------------------------------------------------
        void demultiply()
        {
            for_each_pixel(multiplier_rgba<color_type, order_type>::demultiply);
        }

        //--------------------------------------------------------------------
        template<class GammaLut> void apply_gamma_dir(const GammaLut& g)
        {
            for_each_pixel(apply_gamma_dir_rgba<color_type, order_type, GammaLut>(g));
        }

        //--------------------------------------------------------------------
        template<class GammaLut> void apply_gamma_inv(const GammaLut& g)
        {
            for_each_pixel(apply_gamma_inv_rgba<color_type, order_type, GammaLut>(g));
        }

        //--------------------------------------------------------------------
        template<class RenBuf2> void copy_from(const RenBuf2& from, 
                                               int xdst, int ydst,
                                               int xsrc, int ysrc,
                                               unsigned len)
        {
            const int8u* p = from.row(ysrc);
            if(p)
            {
                p += xsrc * 4 * sizeof(value_type);
                memmove(m_rbuf->span_ptr(xdst, ydst, len), 
                        p, 
                        len * 4 * sizeof(value_type));
            }
        }



        //--------------------------------------------------------------------
        template<class SrcPixelFormatRenderer> 
        void blend_from(const SrcPixelFormatRenderer& from, 
                        const int8u* psrc_,
                        int xdst, int ydst,
                        int xsrc, int ysrc,
                        unsigned len,
                        int8u cover)
        {
            typedef typename SrcPixelFormatRenderer::order_type src_order;
            const value_type* psrc = (const value_type*)psrc_;
            value_type* pdst = (value_type*)m_rbuf->span_ptr(xdst, ydst, len);
            int incp = 4;
            if(xdst > xsrc)
            {
                psrc += (len-1) << 2;
                pdst += (len-1) << 2;
                incp = -4;
            }

            if(cover == 255)
            {
                do 
                {
                    cob_type::copy_or_blend_pix(pdst, 
                                                psrc[src_order::R],
                                                psrc[src_order::G],
                                                psrc[src_order::B],
                                                psrc[src_order::A]);
                    psrc += incp;
                    pdst += incp;
                }
                while(--len);
            }
            else
            {
                do 
                {
                    cob_type::copy_or_blend_pix(pdst, 
                                                psrc[src_order::R],
                                                psrc[src_order::G],
                                                psrc[src_order::B],
                                                psrc[src_order::A],
                                                cover);
                    psrc += incp;
                    pdst += incp;
                }
                while(--len);
            }
        }


    private:
        rbuf_type* m_rbuf;
    };











    //===========================================pixfmt_custom_blend_rbuf_rgba
    template<class Blender, class RenBuf> class pixfmt_custom_blend_rbuf_rgba
    {
    public:
        typedef Blender  blender_type;
        typedef typename blender_type::color_type color_type;
        typedef typename blender_type::order_type order_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;
        enum base_scale_e
        {
            base_shift = color_type::base_shift,
            base_size  = color_type::base_size,
            base_mask  = color_type::base_mask
        };

        typedef RenBuf rbuf_type;
        typedef typename rbuf_type::row_data row_data;
        typedef typename rbuf_type::span_data span_data;

        //--------------------------------------------------------------------
        pixfmt_custom_blend_rbuf_rgba() : m_rbuf(0), m_comp_op(3) {}
        pixfmt_custom_blend_rbuf_rgba(rbuf_type& rb, unsigned comp_op=3) : 
            m_rbuf(&rb),
            m_comp_op(comp_op)
        {}

        void attach(rbuf_type& rb) { m_rbuf = &rb; }

        //--------------------------------------------------------------------
        unsigned width()  const { return m_rbuf->width();  }
        unsigned height() const { return m_rbuf->height(); }

        //--------------------------------------------------------------------
        void comp_op(unsigned op) { m_comp_op = op; }
        unsigned comp_op() const  { return m_comp_op; }

        //--------------------------------------------------------------------
        color_type pixel(int x, int y) const
        {
            const value_type* p = m_rbuf->span_ptr(x, y, 1);
            return p ? color_type(p[order_type::R], 
                                  p[order_type::G], 
                                  p[order_type::B], 
                                  p[order_type::A]) :
                       color_type::no_color();
        }

        //--------------------------------------------------------------------
        row_data row(int x, int y) const
        {
            return m_rbuf->row(x, y);
        }

        //--------------------------------------------------------------------
        span_data span(int x, int y, unsigned len)
        {
            return m_rbuf->span(x, y, len);
        }

        //--------------------------------------------------------------------
        void copy_pixel(int x, int y, const color_type& c)
        {
            blender_type::blend_pix(m_comp_op, 
                                    (value_type*)m_rbuf->span_ptr(x, y, 1), 
                                    c.r, c.g, c.b, c.a, 255);
        }

        //--------------------------------------------------------------------
        void blend_pixel(int x, int y, const color_type& c, int8u cover)
        {
            blender_type::blend_pix(m_comp_op, 
                                    (value_type*)m_rbuf->span_ptr(x, y, 1), 
                                    c.r, c.g, c.b, c.a, 
                                    cover);
        }

        //--------------------------------------------------------------------
        void copy_hline(int x, int y, unsigned len, const color_type& c)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, 255);
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void copy_vline(int x, int y, unsigned len, const color_type& c)
        {
            do
            {
                blender_type::blend_pix(m_comp_op, 
                                        (value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                        c.r, c.g, c.b, c.a, 255);
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_hline(int x, int y, unsigned len, 
                         const color_type& c, int8u cover)
        {

            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, cover);
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_vline(int x, int y, unsigned len, 
                         const color_type& c, int8u cover)
        {

            do
            {
                blender_type::blend_pix(m_comp_op, 
                                        (value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                        c.r, c.g, c.b, c.a, 
                                        cover);
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y, unsigned len, 
                               const color_type& c, const int8u* covers)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do 
            {
                blender_type::blend_pix(m_comp_op, p, c.r, c.g, c.b, c.a, *covers++);
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_solid_vspan(int x, int y, unsigned len, 
                               const color_type& c, const int8u* covers)
        {
            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        (value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                        c.r, c.g, c.b, c.a, 
                                        *covers++);
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void copy_color_hspan(int x, int y,
                              unsigned len, 
                              const color_type* colors)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do 
            {
                p[order_type::R] = colors->r;
                p[order_type::G] = colors->g;
                p[order_type::B] = colors->b;
                p[order_type::A] = colors->a;
                ++colors;
                p += 4;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_color_hspan(int x, int y, unsigned len, 
                               const color_type* colors, 
                               const int8u* covers,
                               int8u cover)
        {
            value_type* p = (value_type*)m_rbuf->span_ptr(x, y, len);
            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        p, 
                                        colors->r, 
                                        colors->g, 
                                        colors->b, 
                                        colors->a, 
                                        covers ? *covers++ : cover);
                p += 4;
                ++colors;
            }
            while(--len);
        }

        //--------------------------------------------------------------------
        void blend_color_vspan(int x, int y, unsigned len, 
                               const color_type* colors, 
                               const int8u* covers,
                               int8u cover)
        {
            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        (value_type*)m_rbuf->span_ptr(x, y++, 1), 
                                        colors->r,
                                        colors->g,
                                        colors->b,
                                        colors->a,
                                        covers ? *covers++ : cover);
                ++colors;
            }
            while(--len);

        }

        //--------------------------------------------------------------------
        template<class RenBuf2> void copy_from(const RenBuf2& from, 
                                               int xdst, int ydst,
                                               int xsrc, int ysrc,
                                               unsigned len)
        {
            const int8u* p = from.row(ysrc);
            if(p)
            {
                p += xsrc * 4 * sizeof(value_type);
                memmove(m_rbuf->span_ptr(xdst, ydst, len), 
                        p, 
                        len * 4 * sizeof(value_type));
            }
        }

        //--------------------------------------------------------------------
        template<class SrcPixelFormatRenderer> 
        void blend_from(const SrcPixelFormatRenderer& from, 
                        const int8u* psrc_,
                        int xdst, int ydst,
                        int xsrc, int ysrc,
                        unsigned len,
                        int8u cover)
        {
            typedef typename SrcPixelFormatRenderer::order_type src_order;
            const value_type* psrc = (const value_type*)psrc_;
            value_type* pdst = (value_type*)m_rbuf->span_ptr(xdst, ydst, len);
            int incp = 4;
            if(xdst > xsrc)
            {
                psrc += (len-1) << 2;
                pdst += (len-1) << 2;
                incp = -4;
            }

            do 
            {
                blender_type::blend_pix(m_comp_op, 
                                        pdst, 
                                        psrc[src_order::R],
                                        psrc[src_order::G],
                                        psrc[src_order::B],
                                        psrc[src_order::A],
                                        cover);
                psrc += incp;
                pdst += incp;
            }
            while(--len);
        }

    private:
        rbuf_type* m_rbuf;
        unsigned m_comp_op;
    };









    //-----------------------------------------------------------------------
    typedef blender_rgba<rgba8, order_rgba> blender_rgba32; //----blender_rgba32
    typedef blender_rgba<rgba8, order_argb> blender_argb32; //----blender_argb32
    typedef blender_rgba<rgba8, order_abgr> blender_abgr32; //----blender_abgr32
    typedef blender_rgba<rgba8, order_bgra> blender_bgra32; //----blender_bgra32

    typedef blender_rgba_pre<rgba8, order_rgba> blender_rgba32_pre; //----blender_rgba32_pre
    typedef blender_rgba_pre<rgba8, order_argb> blender_argb32_pre; //----blender_argb32_pre
    typedef blender_rgba_pre<rgba8, order_abgr> blender_abgr32_pre; //----blender_abgr32_pre
    typedef blender_rgba_pre<rgba8, order_bgra> blender_bgra32_pre; //----blender_bgra32_pre

    typedef blender_rgba_plain<rgba8, order_rgba> blender_rgba32_plain; //----blender_rgba32_plain
    typedef blender_rgba_plain<rgba8, order_argb> blender_argb32_plain; //----blender_argb32_plain
    typedef blender_rgba_plain<rgba8, order_abgr> blender_abgr32_plain; //----blender_abgr32_plain
    typedef blender_rgba_plain<rgba8, order_bgra> blender_bgra32_plain; //----blender_bgra32_plain

    typedef blender_rgba<rgba16, order_rgba> blender_rgba64; //----blender_rgba64
    typedef blender_rgba<rgba16, order_argb> blender_argb64; //----blender_argb64
    typedef blender_rgba<rgba16, order_abgr> blender_abgr64; //----blender_abgr64
    typedef blender_rgba<rgba16, order_bgra> blender_bgra64; //----blender_bgra64

    typedef blender_rgba_pre<rgba16, order_rgba> blender_rgba64_pre; //----blender_rgba64_pre
    typedef blender_rgba_pre<rgba16, order_argb> blender_argb64_pre; //----blender_argb64_pre
    typedef blender_rgba_pre<rgba16, order_abgr> blender_abgr64_pre; //----blender_abgr64_pre
    typedef blender_rgba_pre<rgba16, order_bgra> blender_bgra64_pre; //----blender_bgra64_pre


    //-----------------------------------------------------------------------
    typedef int32u pixel32_type;
    typedef pixel_formats_rgba<blender_rgba32, pixel32_type> pixfmt_rgba32; //----pixfmt_rgba32
    typedef pixel_formats_rgba<blender_argb32, pixel32_type> pixfmt_argb32; //----pixfmt_argb32
    typedef pixel_formats_rgba<blender_abgr32, pixel32_type> pixfmt_abgr32; //----pixfmt_abgr32
    typedef pixel_formats_rgba<blender_bgra32, pixel32_type> pixfmt_bgra32; //----pixfmt_bgra32

    typedef pixel_formats_rgba<blender_rgba32_pre, pixel32_type> pixfmt_rgba32_pre; //----pixfmt_rgba32_pre
    typedef pixel_formats_rgba<blender_argb32_pre, pixel32_type> pixfmt_argb32_pre; //----pixfmt_argb32_pre
    typedef pixel_formats_rgba<blender_abgr32_pre, pixel32_type> pixfmt_abgr32_pre; //----pixfmt_abgr32_pre
    typedef pixel_formats_rgba<blender_bgra32_pre, pixel32_type> pixfmt_bgra32_pre; //----pixfmt_bgra32_pre

    typedef pixel_formats_rgba<blender_rgba32_plain, pixel32_type> pixfmt_rgba32_plain; //----pixfmt_rgba32_plain
    typedef pixel_formats_rgba<blender_argb32_plain, pixel32_type> pixfmt_argb32_plain; //----pixfmt_argb32_plain
    typedef pixel_formats_rgba<blender_abgr32_plain, pixel32_type> pixfmt_abgr32_plain; //----pixfmt_abgr32_plain
    typedef pixel_formats_rgba<blender_bgra32_plain, pixel32_type> pixfmt_bgra32_plain; //----pixfmt_bgra32_plain

    struct  pixel64_type { int16u c[4]; };
    typedef pixel_formats_rgba<blender_rgba64, pixel64_type> pixfmt_rgba64; //----pixfmt_rgba64
    typedef pixel_formats_rgba<blender_argb64, pixel64_type> pixfmt_argb64; //----pixfmt_argb64
    typedef pixel_formats_rgba<blender_abgr64, pixel64_type> pixfmt_abgr64; //----pixfmt_abgr64
    typedef pixel_formats_rgba<blender_bgra64, pixel64_type> pixfmt_bgra64; //----pixfmt_bgra64

    typedef pixel_formats_rgba<blender_rgba64_pre, pixel64_type> pixfmt_rgba64_pre; //----pixfmt_rgba64_pre
    typedef pixel_formats_rgba<blender_argb64_pre, pixel64_type> pixfmt_argb64_pre; //----pixfmt_argb64_pre
    typedef pixel_formats_rgba<blender_abgr64_pre, pixel64_type> pixfmt_abgr64_pre; //----pixfmt_abgr64_pre
    typedef pixel_formats_rgba<blender_bgra64_pre, pixel64_type> pixfmt_bgra64_pre; //----pixfmt_bgra64_pre
}

#endif

