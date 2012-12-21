//extern AGG-based compositor functions

extern void* Create_Compositor(REBGOB* rootGob, REBGOB* gob);

extern void Destroy_Compositor(void* context);

extern REBSER* Gob_To_Image(REBGOB *gob);

extern REBINT Compose_Gob(void* context, REBGOB* winGob, REBGOB* gob);

extern REBYTE* Get_Window_Buffer(void* context);

extern REBOOL Resize_Window_Buffer(void* context, REBGOB* winGob);

extern void Map_Gob(void* context, REBGOB **gob, REBPAR *xy, REBOOL inner);

extern REBINT Draw_Image(REBSER *image, REBSER *block);

extern REBINT Effect_Image(REBSER *image, REBSER *block);
