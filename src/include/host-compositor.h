/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Additional code modifications and improvements Copyright 2012 Saphirion AG
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
**  Title: compositor API functions
**  Author: Richard Smolak
**
************************************************************************
**
**  NOTE to PROGRAMMERS:
**
**    1. Keep code clear and simple.
**    2. Document unusual code, reasoning, or gotchas.
**    3. Use same style for code, vars, indent(4), comments, etc.
**    4. Keep in mind Linux, OS X, BSD, big/little endian CPUs.
**    5. Test everything, then test it again.
**
***********************************************************************/

typedef struct REBCMP_CTX REBCMP_CTX;

extern REBCMP_CTX* rebcmp_create(REBGOB* rootGob, REBGOB* gob);

extern void rebcmp_destroy(REBCMP_CTX* context);

//extern REBSER* Gob_To_Image(REBGOB *gob);

extern void rebcmp_compose(REBCMP_CTX* context, REBGOB* winGob, REBGOB* gob, char *buf);

extern void rebcmp_blit(REBCMP_CTX* context);

extern REBYTE* rebcmp_get_buffer(REBCMP_CTX* context);

extern void rebcmp_release_buffer(REBCMP_CTX* context);

extern REBOOL rebcmp_resize_buffer(REBCMP_CTX* context, REBGOB* winGob);

//extern REBINT Draw_Image(REBSER *image, REBSER *block);

//extern REBINT Effect_Image(REBSER *image, REBSER *block);

void* Find_Window(REBGOB *gob);
REBCMP_CTX* Find_Compositor(REBGOB *gob);
#define GOB_HWIN(gob)	(cast(SDL_Window*, Find_Window(gob)))
#define GOB_COMPOSITOR(gob)	(Find_Compositor(gob)) //gets handle to window's compositor