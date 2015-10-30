/***********************************************************************
 **
 **  REBOL [R3] Language Interpreter and Run-time Environment
 **
 **  Copyright 2012 REBOL Technologies
 **  REBOL is a trademark of REBOL Technologies
 **
 **  Additional code modifications and improvements Copyright 2015 Atronix Engineering, Inc
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
 **  Title: DRAW dialect API functions
 **  Author: Shixin Zeng
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

#include <nanovg.h>
#include <SDL.h>

struct REBDRW_CTX {
	SDL_Window 	*win;
	SDL_GLContext 	*sdl;
	NVGcontext	*nvg;
	NVGlayer	*win_layer;
	NVGlayer	*gob_layer;
	NVGlayer	*tmp_layer;
	REBINT		ww;
	REBINT		wh;
	float		pixel_ratio;

	/* gob clip, in gob's local coordinates */
	float		clip_x; 
	float		clip_y;
	float		clip_w;
	float		clip_h;

	/* gob offset, in window coordinates*/
	float		offset_x;
	float		offset_y;

	/* for shapes */
	float 		last_x;
	float 		last_y;

	/* fill or stroke */
	int 		fill_image;
	int 		stroke_image;

	NVGcolor	key_color;
	unsigned int fill: 1;
	unsigned int stroke: 1;
	unsigned int img_border: 1; /* draw the image border or not */
	unsigned int key_color_enabled: 1; /* key_color enabled or not */

	char		last_shape_cmd;

	/* control point of last 'C' or 'Q' */
	float		last_control_x;
	float		last_control_y;
};
