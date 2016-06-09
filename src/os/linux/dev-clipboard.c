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
**  Title: Device: Clipboard access for X
**  Author: Shixin Zeng
**  Purpose:
**      Provides a very simple interface to the clipboard for text.
**      May be expanded in the future for images, etc.
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
#ifndef REB_CORE //only available with graphics

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include "reb-host.h"
#include "host-lib.h"

#include "host-window.h"
extern REBCNT Length_As_UTF8(REBUNI *src, REBCNT len, REBOOL uni, REBOOL ccr); // s-unicode.c
extern REBCNT Encode_UTF8(REBYTE *dst, REBINT max, void *src, REBCNT *len, REBFLG uni, REBFLG ccr); // s-unicode.c

extern x_info_t *global_x_info;
extern void Signal_Device(REBREQ *req, REBINT type);

/***********************************************************************
**
*/	DEVICE_CMD Open_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	//RL_Print("Open_Clipboard\n");
	SET_OPEN(req);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Close_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	//RL_Print("Close_Clipboard\n");
	SET_CLOSED(req);
	return DR_DONE;
}

static REBINT copy_to_req(REBREQ *req, char* data, REBCNT data_len)
{
	if (req->data) {
		req->data = realloc(req->data, req->actual + data_len + 1);
	} else {
		req->data = OS_Make(data_len + 1);
	}
	if (req->data == NULL){
		return DR_ERROR;
	}
	COPY_STR(req->data + req->actual, data, data_len);
	req->data[req->actual + data_len] = '\0';
	req->actual += data_len;
	return DR_DONE;
}

static REBINT do_read_clipboard(REBREQ * req, Atom property)
{
	Atom     actual_type;
	int      actual_format;
	long     nitems;
	long     bytes = 0;
	char     *data = NULL;
	int      status;

	Atom XA_INCR = x_atom_list_find_atom(global_x_info->x_atom_list,
										   global_x_info->display,
										   "INCR",
										   False);

	if (global_x_info->selection.property){
		do {
			status = XGetWindowProperty(global_x_info->display,
										global_x_info->selection.win,
										property,
										0,
										(~0L),
										True,
										AnyPropertyType,
										&actual_type,
										&actual_format,
										&nitems,
										&bytes,
										(unsigned char**)&data);
			if (nitems == 0
				&& global_x_info->selection.status == SEL_STATUS_COPY_INCR_DATA){
				global_x_info->selection.status = SEL_STATUS_COPY_DONE;
				//printf("%d, changed status to COPY_DONE because None is received\n", __LINE__);
				break;
			}
			if (nitems <= 0){
				global_x_info->selection.status = SEL_STATUS_COPY_DONE;
				//printf("%d, changed status to COPY_DONE because None is received\n", __LINE__);
				return DR_ERROR;
			}

			if (actual_type == XA_INCR) {
				global_x_info->selection.status = SEL_STATUS_COPY_INCR_WAIT;
				//printf("%d, changed status to COPY_INCR_WAIT because type == INCR\n", __LINE__);
				XFree(data);
				return DR_PEND;
			}
			

			if (DR_ERROR == copy_to_req(req, data, nitems)){;
				XFree(data);
				goto error;
			}
			XFree(data);
		} while (bytes > 0);

		if (global_x_info->selection.status == SEL_STATUS_COPY_DATA) {
			global_x_info->selection.status = SEL_STATUS_COPY_DONE;
			//printf("%d, changed status to COPY_DONE because data is small\n", __LINE__);
		} else if (global_x_info->selection.status == SEL_STATUS_COPY_INCR_DATA) {
			global_x_info->selection.status = SEL_STATUS_COPY_INCR_WAIT;
			//printf("%d, changed status to INCR_WAIT because more data is expecting\n", __LINE__);
			return DR_PEND;
		}

		//RL_Print("do_read_clipboard succeeded\n");
		goto close;
	}

	//RL_Print("do_read_clipboard failed\n");
error:
	req->actual = 0;
close:

	// Informing the owner the data has been transferred by deleting the property
	return DR_DONE;
}

/***********************************************************************
**
*/	DEVICE_CMD Read_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	REBYTE *clip = NULL;
	REBINT len;
	enum selection_status status = global_x_info->selection.status;
	Window win = global_x_info->selection.win;

	//RL_Print("Read_Clipboard\n");
	//req->actual = 0;
	Window owner = 0;
	Display *display = global_x_info->display;
	if (display == NULL) {
		goto error;
	}
	Atom XA_CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
	Atom XA_SELECTION = XInternAtom(display, "REBOL_SELECTION", False);
	//XSync(display, False);
	owner = XGetSelectionOwner(display, XA_CLIPBOARD);
	if (owner == None) {
		goto error;
	}
	if (global_x_info->selection.win == owner){
		/* same process, bypass the server */
		if (global_x_info->selection.data != NULL){
			if (DR_ERROR == copy_to_req(req,
										global_x_info->selection.data,
										global_x_info->selection.data_length)){
				goto error;
			}
			global_x_info->selection.status = SEL_STATUS_NONE;
			//printf("%d, changed status to NONE because copying from itself\n", __LINE__);
			goto close;
		} else {
			goto error;
		}
	}

	if (status == SEL_STATUS_NONE
		|| status == SEL_STATUS_PASTE_INCR // paste/write not done yet, just overwrite it
		|| status == SEL_STATUS_PASTE_DONE) { /* request not sent yet */
		if (win == 0){
			win = global_x_info->selection.win
				= XCreateWindow(display,
								RootWindow(display, 0),
								0, 0, 50, 50, 0,
								CopyFromParent, InputOnly,
							   	CopyFromParent, 0,0);
			XSelectInput(display, win, PropertyChangeMask);
		}
		//XSync(display, False);
		Atom XA_TARGETS = XInternAtom(global_x_info->display, "TARGETS", False);
		XConvertSelection(display,
						  XA_CLIPBOARD, XA_TARGETS, XA_SELECTION,
						  global_x_info->selection.win,
						  CurrentTime); //FIXME: shouldn't use CurrentTime, ICCCM sec 2.4 
		global_x_info->selection.status = SEL_STATUS_COPY_TARGETS_CONVERTED; /* pending */
		//printf("%d, changed status to TARGET_CONVERTED because of reading\n", __LINE__);
		global_x_info->selection.property = XA_SELECTION;
		req->actual = 0;
		return DR_PEND;
	} else if (status == SEL_STATUS_COPY_INCR_DATA
			   || status == SEL_STATUS_COPY_DATA) { /* response received */
		int ret = do_read_clipboard(req, XA_SELECTION);
		if (ret == DR_ERROR) goto error;
		if (global_x_info->selection.status == SEL_STATUS_COPY_DONE) {
			Signal_Device(req, EVT_READ);
			global_x_info->selection.status = SEL_STATUS_NONE; /* prep for next read */
			//printf("%d, changed status to NONE for next read\n", __LINE__);
			return DR_DONE;
		} else {
			return DR_PEND;
		}
	} else { /* request sent and response not received yet */
		return DR_PEND;
	}
error:
	req->actual = 0;
close:
	Signal_Device(req, EVT_CLOSE);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Write_Clipboard(REBREQ *req)
/*
**		Works for Unicode and ASCII strings.
**		Length is number of bytes passed (not number of chars).
**
***********************************************************************/
{
	//RL_Print("Write_Clipboard\n");
	//XStoreBytes(global_x_info->display, req->data, len);
	Window win = global_x_info->selection.win;
	Display *display = global_x_info->display;
	if (display == NULL) {
		return DR_ERROR;
	}
	Atom XA_CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);

	if (win == 0){
		win = global_x_info->selection.win
			= XCreateWindow(display,
							RootWindow(display, 0),
							0, 0, 50, 50, 0,
							CopyFromParent, InputOnly,
							CopyFromParent, 0,0);
		//RL_Print("window = %d\n", win);
		XSelectInput(display, win, PropertyChangeMask);
	}
	void *data = global_x_info->selection.data;
	if (data != NULL) {
		OS_Free(data);
	}
	REBINT src_len = req->length;
	if (GET_FLAG(req->flags, RRF_WIDE)){
		src_len /= sizeof(REBUNI);
	}
	REBCNT len = Length_As_UTF8((REBUNI*)req->data, src_len, GET_FLAG(req->flags, RRF_WIDE), 0);
	data = global_x_info->selection.data = OS_Make(len);
	if (data == NULL) {
		return DR_ERROR;
	}
	REBCNT dst_len = src_len;
	Encode_UTF8(data, len, req->data, &dst_len, TRUE, 0);
	global_x_info->selection.data_length = dst_len;
	//global_x_info->selection.offset = 0;
	//XSync(display, False);
	XSetSelectionOwner(display, XA_CLIPBOARD, win, CurrentTime);
	//XFlush(display);
	req->actual = dst_len;
	Signal_Device(req, EVT_WROTE);
	return DR_DONE;
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] =
{
	0,
	0,
	Open_Clipboard,
	Close_Clipboard,
	Read_Clipboard,
	Write_Clipboard,
	0,
};

DEFINE_DEV(Dev_Clipboard, "Clipboard", 1, Dev_Cmds, RDC_MAX, 0);

#endif //REB_CORE
