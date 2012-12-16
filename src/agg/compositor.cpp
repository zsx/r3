//exported functions
#include "agg_compo.h"

namespace agg
{
	extern "C" void* Create_Compositor(REBGOB* rootGob, REBGOB* gob)
	{
	   return (void *)new compositor(rootGob, gob);
	}

	extern "C" void Destroy_Compositor(void* context)
	{
	   delete (compositor*)context;
	}

	extern "C" REBSER* Gob_To_Image(REBGOB *gob)
	{
	    REBINT w,h,result;
	    REBSER* img;
        compositor* cp;
        REBGOB* parent;
        REBGOB* wingob;

	    w = (REBINT)GOB_W(gob);
	    h = (REBINT)GOB_H(gob);
	    img = (REBSER*)RL_MAKE_IMAGE(w,h);

        //serach the window gob
		wingob = gob;
		while (GOB_PARENT(wingob) && GOB_PARENT(wingob) != Gob_Root
			&& GOB_PARENT(wingob) != wingob) // avoid infinite loop
			wingob = GOB_PARENT(wingob);

	    //reset the parent so gob composing stops at the wingob
        parent = GOB_PARENT(wingob);
        GOB_PARENT(wingob) = 0;

        cp = new compositor ((REBYTE *)RL_SERIES(img, RXI_SER_DATA), w, h);
		result = cp->cp_compose_gob(wingob, gob);
		cp->cp_set_win_buffer(0, 0, 0); //set the buffer to 0 so the image is not deleted on next line
		delete cp;

		//restore wingob parent
		GOB_PARENT(wingob) = parent;

		return ((result == 0) ? img : 0);
	}

	extern "C" REBINT Compose_Gob(void* context, REBGOB* winGob,REBGOB* gob)
	{
		return ((compositor*)context)->cp_compose_gob(winGob, gob);
	}

	extern "C" REBYTE* Get_Window_Buffer(void* context)
	{
		return ((compositor*)context)->cp_get_win_buffer();
	}

	extern "C" REBOOL Resize_Window_Buffer(void* context, REBGOB* winGob)
	{
		return ((compositor*)context)->cp_resize(winGob);
	}

#ifdef moved_to_gob_internals
	extern "C" void Map_Gob(void* context, REBGOB **gob, REBPAR *xy, REBOOL inner)
	{
		REBGOB* result;
		if (inner){
			result = ((compositor*)context)->cp_upper_to_lower_coord(*gob, xy);
		} else {
			result = ((compositor*)context)->cp_lower_to_upper_coord(*gob, xy);
		}
		if (result) *gob = result;
	}
#endif
}

