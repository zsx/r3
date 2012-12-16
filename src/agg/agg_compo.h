//AGG stuff
#include "agg_graphics.h"

extern "C" REBINT Draw_Gob(void *graphics, REBSER *block, REBSER *args);
//extern "C" REBINT Effect_Gob(void *effects, REBSER *block);
extern "C" REBINT Text_Gob(void *richtext, REBSER *block);

namespace agg
{
	class compositor
    {

		public:

			typedef rendering_buffer ren_buf;

			compositor(REBGOB* rootGob, REBGOB* gob);
			compositor(REBYTE* buf, REBINT w, REBINT h);
			~compositor();

			void cp_alloc_buffer(REBGOB* winGob);
			void cp_set_win_buffer(REBYTE* buf, REBINT w, REBINT h);

			REBYTE* cp_get_win_buffer();
			REBOOL cp_resize(REBGOB* winGob);

//			REBGOB* cp_upper_to_lower_coord(REBGOB* gob, REBPAR* offset);
//			REBGOB* cp_lower_to_upper_coord(REBGOB* gob, REBPAR* offset);

			REBINT cp_compose_gob(REBGOB* winGob, REBGOB* gob);
			REBINT cp_process_gobs(REBGOB* gob);

		private:

			struct blit_info {
				bool visible;
				REBPAR final_oft;
				REBPAR final_siz;
				REBPAR win_oft;
			};

			blit_info* m_blit_info;

			//window backbuffer
			REBYTE* m_buf;
			ren_buf m_rbuf_win;
			agg_graphics::pixfmt m_pixf;

			agg_graphics::ren_base m_rb_win;

			//buffer size
			int m_width;
			int m_height;
			int m_stride;

			int m_shift_x;
			int m_shift_y;

			//root gob reference
			REBGOB* m_rootGob;

			REBGOB* m_gob;
			REBGOB* m_parent_gob;

			void* m_rich_text;

//          int m_gobs;

			// clipping info
			rect m_clip_box;

			REBPAR m_final_oft;
			REBPAR m_final_siz;

			bool m_opaque;
			bool m_visible;

			//GOB which doesn't have to be rendered(used by refreshing old regions)
			REBGOB* m_hidden;

			rasterizer_scanline_aa<> m_ras;
			scanline_p8              m_sl_p8;
	};

}
