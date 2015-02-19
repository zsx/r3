#include "reb-host.h"
#include <unistd.h>
#include "host-lib.h"
#include "SDL.h"

static void Add_Event_XY(REBGOB *gob, REBINT id, REBINT xy, REBINT flags)
{
	REBEVT evt;

	memset(&evt, 0, sizeof(evt));
	evt.type  = id;
	evt.flags = (u8) (flags | (1<<EVF_HAS_XY));
	evt.model = EVM_GUI;
	evt.data  = xy;
	evt.ser = (void*)gob;

	RL_Event(&evt);	// returns 0 if queue is full
}

static void Update_Event_XY(REBGOB *gob, REBINT id, REBINT xy, REBINT flags)
{
	REBEVT evt;

	memset(&evt, 0, sizeof(evt));
	evt.type  = id;
	evt.flags = (u8) (flags | (1<<EVF_HAS_XY));
	evt.model = EVM_GUI;
	evt.data  = xy;
	evt.ser = (void*)gob;

	RL_Update_Event(&evt);
}

static void Add_Event_Key(REBGOB *gob, REBINT id, REBINT key, REBINT flags)
{
	REBEVT evt;

	memset(&evt, 0, sizeof(evt));
	evt.type  = id;
	evt.flags = flags;
	evt.model = EVM_GUI;
	evt.data  = key;
	evt.ser = (void*)gob;

	RL_Event(&evt);	// returns 0 if queue is full
}

void dispatch (SDL_Event *evt)
{
	SDL_Window *focus = NULL;
	SDL_Window *win = NULL;
	Uint32 win_id = 0;
	REBGOB *gob = NULL;
	int xyd = 0;

	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "event type: 0x%x\n", evt->type);
	switch (evt->type) {
		case SDL_WINDOWEVENT:
			switch (evt->window.event) {
				case SDL_WINDOWEVENT_SHOWN:
					SDL_Log("Window %d shown", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_HIDDEN:
					SDL_Log("Window %d hidden", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_EXPOSED:
					SDL_Log("Window %d exposed", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_MOVED:
					SDL_Log("Window %d moved to %d,%d",
							evt->window.windowID, evt->window.data1,
							evt->window.data2);
					break;
				case SDL_WINDOWEVENT_RESIZED:
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					SDL_Log("Window %d resized to %dx%d",
							evt->window.windowID, evt->window.data1,
							evt->window.data2);
					win = SDL_GetWindowFromID(evt->window.windowID);
					if (win) {
						gob = SDL_GetWindowData(win, "GOB");
						gob->size.x = ROUND_TO_INT(PHYS_COORD_X(evt->window.data1));
						gob->size.y = ROUND_TO_INT(PHYS_COORD_Y(evt->window.data2));
						xyd = (ROUND_TO_INT((gob->size.x))) + (ROUND_TO_INT(gob->size.y) << 16);
						SDL_Log("gob %p resized to %dx%d", gob, (int)gob->size.x, (int)gob->size.y);
						Update_Event_XY(gob, EVT_RESIZE, xyd, 0);
					}
					break;
					break;
				case SDL_WINDOWEVENT_MINIMIZED:
					SDL_Log("Window %d minimized", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_MAXIMIZED:
					SDL_Log("Window %d maximized", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_RESTORED:
					SDL_Log("Window %d restored", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_ENTER:
					SDL_Log("Mouse entered window %d",
							evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_LEAVE:
					SDL_Log("Mouse left window %d", evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					SDL_Log("Window %d gained keyboard focus",
							evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					SDL_Log("Window %d lost keyboard focus",
							evt->window.windowID);
					break;
				case SDL_WINDOWEVENT_CLOSE:
					SDL_Log("Window %d closed", evt->window.windowID);
					break;
				default:
					SDL_Log("Window %d got unknown evt %d",
							evt->window.windowID, evt->window.event);
					break;
			}
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_TEXTINPUT:
			win = SDL_GetWindowFromID(evt->key.windowID);
			gob = SDL_GetWindowData(win, "GOB");
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			/* FIXME: check modifiers */
			win = SDL_GetWindowFromID(evt->button.windowID);
			gob = SDL_GetWindowData(win, "GOB");
			if (gob != NULL) {
				REBFLG flags = 0;
				int id = 0;
				if (evt->button.clicks == 2) {
					flags != 1 << EVF_DOUBLE;
				}

				switch (evt->button.button) {
					case SDL_BUTTON_LEFT:
						id = (evt->button.state == SDL_PRESSED)? EVT_DOWN : EVT_UP;
						break;
					case SDL_BUTTON_MIDDLE:
						id = (evt->button.state == SDL_PRESSED)? EVT_AUX_DOWN : EVT_AUX_UP;
						break;
					case SDL_BUTTON_RIGHT:
						id = (evt->button.state == SDL_PRESSED)? EVT_ALT_DOWN : EVT_ALT_UP;
						break;
				}
				xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->button.x))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->button.y)) << 16);
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "button event, button: %d, clicks: %d", evt->button.button, evt->button.clicks);
				Add_Event_XY(gob, id, xyd, flags);
			}
			break;

		case SDL_MOUSEWHEEL:
			/* FIXME: check modifiers */
			win = SDL_GetWindowFromID(evt->wheel.windowID);
			gob = SDL_GetWindowData(win, "GOB");
			if (gob != NULL) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Wheel event");
			}
			break;
	}
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "win: %x, focus: %p, event on gob: %p\n", win_id, focus, gob);
}
