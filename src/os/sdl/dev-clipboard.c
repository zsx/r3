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
**  Title: Device: Clipboard access for <platform>
**  Author: Carl Sassenrath, Richard Smolak
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

#include <stdio.h>

#include "reb-host.h"
#include "host-lib.h"
#include "sys-net.h"
#include "SDL.h"

/***********************************************************************
**
*/	DEVICE_CMD Open_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	SET_OPEN(req);
	Signal_Device(req, EVT_OPEN);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Close_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	SET_CLOSED(req);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Read_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	REBYTE *data;
	
	data = SDL_GetClipboardText();

	req->actual = 0;

	if ((data) == NULL) {
		req->error = 30;
		return DR_ERROR;
	}

	//printf("read from clipboard: %s\n", data);
	//make sure "bytes mode" is set
	CLR_FLAG(req->flags, RRF_WIDE);

	req->actual = LEN_STR(data);
	req->data = OS_Make(req->actual + 1);
	COPY_STR(req->data, data, req->actual);
	req->data[req->actual] = '\0';
	Signal_Device(req, EVT_READ);

	SDL_free(data);

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
	int ret = 0;

	req->actual = 0;

	REBINT src_len = req->length;
	if (GET_FLAG(req->flags, RRF_WIDE)) {
		src_len /= sizeof(REBUNI);
		REBCNT len = Length_As_UTF8((REBUNI*)req->data, src_len, GET_FLAG(req->flags, RRF_WIDE), 0);
		char *data = OS_Make(len + 1);
		if (data == NULL) {
			return DR_ERROR;
		}
		REBCNT dst_len = src_len;
		Encode_UTF8(data, len, req->data, &dst_len, TRUE, 0);
		data[len] = '\0';
		ret = SDL_SetClipboardText(data);
		OS_Free(data);
	} else {
		ret = SDL_SetClipboardText(req->data);
	}

	if (ret < 0) {
		req->actual = 0;
		req->error = ret;
		return DR_ERROR;
	} else {
		req->actual = req->length;
		Signal_Device(req, EVT_WROTE);
		return DR_DONE;
	}
}


/***********************************************************************
**
*/	DEVICE_CMD Poll_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	if (SDL_HasClipboardText()) {
		return DR_DONE;
	} else {
		return DR_PEND;
	}
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
	Poll_Clipboard,
};

DEFINE_DEV(Dev_Clipboard, "Clipboard", 1, Dev_Cmds, RDC_MAX, 0);
