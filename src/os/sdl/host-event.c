#include <assert.h>
#include "reb-host.h"
//#include <unistd.h>
#include "host-lib.h"
#include "SDL.h"

typedef struct REBCMP_CTX REBCMP_CTX;

void* Find_Compositor(REBGOB *gob);
void rebcmp_compose(REBCMP_CTX* ctx, REBGOB* winGob, REBGOB* gob, REBOOL only);

static REBCNT keycode_to_event[] = {
	SDLK_AC_HOME,		EVK_HOME,
	SDLK_HOME,			EVK_HOME,
	SDLK_LEFT,			EVK_LEFT,
	SDLK_UP,			EVK_UP,
	SDLK_RIGHT,			EVK_RIGHT,
	SDLK_DOWN,			EVK_DOWN,
	SDLK_PAGEUP,		EVK_PAGE_UP,
	SDLK_PAGEDOWN,		EVK_PAGE_DOWN,
	SDLK_END,			EVK_END,
	SDLK_INSERT,		EVK_INSERT,
	SDLK_DELETE,		EVK_DELETE,

	SDLK_F1,			EVK_F1,
	SDLK_F2,			EVK_F2,
	SDLK_F3,			EVK_F3,
	SDLK_F4,			EVK_F4,
	SDLK_F5,			EVK_F5,
	SDLK_F6,			EVK_F6,
	SDLK_F7,			EVK_F7,
	SDLK_F8,			EVK_F8,
	SDLK_F9,			EVK_F9,
	SDLK_F10,			EVK_F10,
	SDLK_F11,			EVK_F11,
	SDLK_F12,			EVK_F12,
	0x0,				0
};

#define NUM_MAP_IN_A_ROW	6
static REBCNT keycode_to_char[] = {
//SDL_Keycode,	char, 	SHIFT, 	CAP, CAP+SHIFT,	CTRL
	SDLK_0,		'0', 	')', 	'0', 	')',	'0', 	
	SDLK_1,		'1', 	'!',	'1',	'!',	'1', 	
	SDLK_2,		'2',	'@',	'2',	'@',	'2',	
	SDLK_3,		'3',	'#',	'3',	'#',	'3',	
	SDLK_4,		'4',	'$',	'4',	'$',	'4',	
	SDLK_5,		'5',	'%',	'5',	'%',	'5',	
	SDLK_6,		'6',	'^',	'6',	'^',	'6',	
	SDLK_7,		'7',	'&',	'7',	'&',	'7',	
	SDLK_8,		'8',	'*',	'8',	'*',	'8',	
	SDLK_9,		'9',	'(',	'9',	'(',	'9',	
	SDLK_a,		'a',	'A',	'A',	'a',	'\001',
	SDLK_b,		'b',	'B',	'B',	'b',	'\002',
	SDLK_c,		'c',	'C',	'C',	'c',	'\003',
	SDLK_d,		'd',	'D',	'D',	'd',	'\004',
	SDLK_e,		'e',	'E',	'E',	'e',	'\005',
	SDLK_f,		'f',	'F',	'F',	'f',	'\006',
	SDLK_g,		'g',	'G',	'G',	'g',	'\007',
	SDLK_h,		'h',	'H',	'H',	'h',	'\010',
	SDLK_i,		'i',	'I',	'I',	'i',	'\011',
	SDLK_j,		'j',	'J',	'J',	'j',	'\012',
	SDLK_k,		'k',	'K',	'K',	'k',	'\013',
	SDLK_l,		'l',	'L',	'L',	'l',	'\014',
	SDLK_m,		'm',	'M',	'M',	'm',	'\015',
	SDLK_n,		'n',	'N',	'N',	'n',	'\016',
	SDLK_o,		'o',	'O',	'O',	'o',	'\017',
	SDLK_p,		'p',	'P',	'P',	'p',	'\020',
	SDLK_q,		'q',	'Q',	'Q',	'q',	'\021',
	SDLK_r,		'r',	'R',	'R',	'r',	'\022',
	SDLK_s,		's',	'S',	'S',	's',	'\023',
	SDLK_t,		't',	'T',	'T',	't',	'\024',
	SDLK_u,		'u',	'U',	'U',	'u',	'\025',
	SDLK_v,		'v',	'V',	'V',	'v',	'\026',
	SDLK_w,		'w',	'W',	'W',	'w',	'\027',
	SDLK_x,		'x',	'X',	'X',	'x',	'\030',
	SDLK_y,		'y',	'Y',	'Y',	'y',	'\031',
	SDLK_z,		'z',	'Z',	'Z',	'z',	'\032',
	SDLK_BACKSLASH,	'\\', '|', 	'\\',	'|',	'\\',
	SDLK_COMMA,	',',	'<',	',',	'<',	',',
	SDLK_PERIOD,'.',	'>',	'.',	'>',	'.',
	SDLK_SLASH,	'/',	'?',	'/',	'?',	'/',
	SDLK_MINUS, '-',	'_',	'-',	'_',	'-',
	SDLK_EQUALS, '=',	'+',	'=',	'+',	'=',
	SDLK_SEMICOLON,	';',	':',	';',	':', ';',
	SDLK_QUOTE,	'\'',	'"',	'\'',	'"',	'\'',
	SDLK_ESCAPE, '\033', '\033', '\033',	'\033',	'\033',
	SDLK_BACKQUOTE, '`', '~',	'`',	'~',	'~',
	SDLK_BACKSPACE, '\b',	'\b',	'\b',	'\b',	'\b',
	SDLK_KP_0,	'0', 	'0',	'0',	'0',	'0',
	SDLK_KP_1,	'1',	'1',	'1',	'1',	'1',
	SDLK_KP_2,	'2',	'2',	'2',	'2',	'2',
	SDLK_KP_3,	'3',	'3',	'3',	'3',	'3',
	SDLK_KP_4,	'4',	'4',	'4',	'4',	'4',
	SDLK_KP_5,	'5',	'5',	'5',	'5',	'5',
	SDLK_KP_6,	'6',	'6',	'6',	'6',	'6',
	SDLK_KP_7,	'7',	'7',	'7',	'7',	'7',
	SDLK_KP_8,	'8',	'8',	'8',	'8',	'8',
	SDLK_KP_9,	'9',	'9',	'9',	'9',	'9',
	SDLK_KP_BACKSPACE, 	'\b', '\b', '\b', '\b', '\b',
	SDLK_KP_ENTER,	'\r', '\r', '\r', 	'\r', 	'\r',
	SDLK_KP_PLUS,	'+', '+', 	'+', 	'+', 	'+',
	SDLK_KP_MINUS,	'-', '-', 	'-', 	'-', 	'-',
	SDLK_KP_MULTIPLY, '*', '*', '*', 	'*', 	'*',
	SDLK_KP_DIVIDE,	'/', '/', 	'/', 	'/', 	'/',
	SDLK_KP_PERIOD,	'.', '.', 	'.', 	'.', 	'.',
	SDLK_RETURN,	'\r', '\r',	'\r',	'\r',	'\r',
	SDLK_RETURN2,	'\r', '\r',	'\r',	'\r',	'\r',
	SDLK_TAB,	'\t',	'\t',	'\t',	'\t',	'\t',
	SDLK_SPACE,	' ',	' ',	' ',	' ',	' ',
	SDLK_LEFTBRACKET, '[', '{',	'[',	'{',	'[',
	SDLK_RIGHTBRACKET, ']', '}',	']',	'}',	']',
	0x0,		0,		0,		0,		0,		0,
};

void Init_Host_Event()
{
	int i, j, k;
	/* sort keycode ascendantly */
	for(i = 0; keycode_to_event[i]; i += 2) {
		for(j = i + 2; keycode_to_event[j]; j += 2) {
			if (keycode_to_event[i] > keycode_to_event[j]) {
				REBCNT key = keycode_to_event[i];
				keycode_to_event[i] = keycode_to_event[j];
				keycode_to_event[j] = key;

				key = keycode_to_event[i + 1];
				keycode_to_event[i + 1] = keycode_to_event[j + 1];
				keycode_to_event[j + 1] = key;
			}
		}
	}

	for(i = 0; keycode_to_char[i]; i += NUM_MAP_IN_A_ROW) {
		for(j = i + NUM_MAP_IN_A_ROW; keycode_to_char[j]; j += NUM_MAP_IN_A_ROW) {
			if (keycode_to_char[i] > keycode_to_char[j]) {
				for (k = 0; k < NUM_MAP_IN_A_ROW; k ++) {
					REBCNT key = keycode_to_char[i + k];
					keycode_to_char[i + k] = keycode_to_char[j + k];
					keycode_to_char[j + k] = key;
				}
			}
		}
	}

	SDL_StopTextInput();
}

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

static void Accumulate_Event_XY(REBGOB *gob, REBINT id, REBINT xy, REBINT flags)
{
	REBEVT evt;

	memset(&evt, 0, sizeof(evt));
	evt.type  = id;
	evt.flags = (u8) (flags | (1<<EVF_HAS_XY));
	evt.model = EVM_GUI;
	evt.data  = xy;
	evt.ser = (void*)gob;

	RL_Accumulate_Event(&evt);
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

struct Modifiers {
	char shift_down;
	char ctrl_down;
	char caps_down;
};

static struct Modifiers modifiers = {0, 0, 0 };

static REBFLG state_to_flags(REBFLG flags)
{
	if (modifiers.ctrl_down) {
		flags |= 1 << EVF_CONTROL;
	}
	if (modifiers.shift_down) {
		flags |= 1 << EVF_SHIFT;
	}

	return flags;
}

static int n_fingers = 0;

void dispatch (SDL_Event *evt)
{
	SDL_Window *focus = NULL;
	SDL_Window *win = NULL;
	Uint32 win_id = 0;
	REBGOB *gob = NULL;
	int xyd = 0;

	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "event type: 0x%x\n", evt->type);
	switch (evt->type) {
	case SDL_QUIT:
		SDL_Log("Quiting ...");
		break;
	case SDL_WINDOWEVENT:
		switch (evt->window.event) {
		case SDL_WINDOWEVENT_SHOWN:
			SDL_Log("Window %d shown", evt->window.windowID);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL) {
					if (GET_GOB_FLAG(gob, GOBF_HIDDEN)) {
						CLR_GOB_FLAG(gob, GOBF_HIDDEN);
					}
					REBCMP_CTX *compositor = Find_Compositor(gob);
					rebcmp_compose(compositor, gob, gob, FALSE);
					rebcmp_blit(compositor);
				}
			}
			break;
		case SDL_WINDOWEVENT_HIDDEN:
			SDL_Log("Window %d hidden", evt->window.windowID);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_HIDDEN)) {
					SET_GOB_FLAG(gob, GOBF_HIDDEN);
				}
			}
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			SDL_Log("Window %d exposed", evt->window.windowID);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL) {
					REBCMP_CTX *compositor = Find_Compositor(gob);
					rebcmp_compose(compositor, gob, gob, FALSE);
					rebcmp_blit(compositor);
				}
			}
			break;
		case SDL_WINDOWEVENT_MOVED:
			SDL_Log("Window %d moved to %d,%d",
				evt->window.windowID, evt->window.data1,
				evt->window.data2);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL) {
					gob->offset.x = ROUND_TO_INT(PHYS_COORD_X(evt->window.data1));
					gob->offset.y = ROUND_TO_INT(PHYS_COORD_Y(evt->window.data2));
					xyd = (ROUND_TO_INT((gob->offset.x))) + (ROUND_TO_INT(gob->offset.y) << 16);
					Update_Event_XY(gob, EVT_OFFSET, xyd, 0);
				}
			}
			break;
		case SDL_WINDOWEVENT_RESIZED:
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			SDL_Log("Window %d resized to %dx%d",
				evt->window.windowID, evt->window.data1,
				evt->window.data2);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL) {
					gob->size.x = ROUND_TO_INT(PHYS_COORD_X(evt->window.data1));
					gob->size.y = ROUND_TO_INT(PHYS_COORD_Y(evt->window.data2));
					xyd = (ROUND_TO_INT((gob->size.x))) + (ROUND_TO_INT(gob->size.y) << 16);
					SDL_Log("gob %p resized to %dx%d", gob, (int)gob->size.x, (int)gob->size.y);
					Update_Event_XY(gob, EVT_RESIZE, xyd, 0);
					Resize_Window(gob, TRUE);
				}
			}
			break;
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			SDL_Log("Window %d minimized", evt->window.windowID);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_MINIMIZE)) {
					SET_GOB_FLAG(gob, GOBF_MINIMIZE);
					CLR_GOB_FLAGS(gob, GOBF_RESTORE, GOBF_MAXIMIZE);
					CLR_GOB_FLAG(gob, GOBF_FULLSCREEN);
				}
			}
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			SDL_Log("Window %d maximized", evt->window.windowID);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_MAXIMIZE)) {
					SET_GOB_FLAG(gob, GOBF_MAXIMIZE);
					CLR_GOB_FLAGS(gob, GOBF_RESTORE, GOBF_MINIMIZE);
					CLR_GOB_FLAG(gob, GOBF_FULLSCREEN);
				}
			}
			break;
		case SDL_WINDOWEVENT_RESTORED:
			SDL_Log("Window %d restored", evt->window.windowID);
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_RESTORE)) {
					SET_GOB_FLAG(gob, GOBF_RESTORE);
					CLR_GOB_FLAGS(gob, GOBF_MAXIMIZE, GOBF_MINIMIZE);
					CLR_GOB_FLAG(gob, GOBF_FULLSCREEN);
				}
			}
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
			win = SDL_GetWindowFromID(evt->window.windowID);
			if (win) {
				gob = SDL_GetWindowData(win, "GOB");
				if (gob != NULL)
					Add_Event_XY(gob, EVT_CLOSE, 0, 0);
			}
			break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
	    case SDL_WINDOWEVENT_TAKE_FOCUS:
            SDL_Log("Window %d is offered a focus", evt->window.windowID);
            break;
        case SDL_WINDOWEVENT_HIT_TEST:
            SDL_Log("Window %d has a special hit test", evt->window.windowID);
            break;
#endif
        default:
            SDL_Log("Window %d got unknown evt %d",
                evt->window.windowID, evt->window.event);
            break;
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (!SDL_IsTextInputActive()) {
            int i, key = -1;
			int flags = state_to_flags(0);
            win = SDL_GetWindowFromID(evt->key.windowID);
            if (!win) {
                win = SDL_GetMouseFocus();
            }
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Key event for window 0x%p", win);
            if (!win) {
                return;
            }
            gob = SDL_GetWindowData(win, "GOB");
            if (gob != NULL) {
                SDL_Keycode keycode = evt->key.keysym.sym;
				Uint16 mod = evt->key.keysym.mod;
				if (keycode == SDLK_RSHIFT || keycode == SDLK_LSHIFT) {
					modifiers.shift_down = (evt->type == SDL_KEYDOWN);
					break;
				}
				else if (keycode == SDLK_RCTRL || keycode == SDLK_LCTRL) {
					modifiers.ctrl_down = (evt->type == SDL_KEYDOWN);
					break;
				}

                for (i = 0; keycode_to_event[i] && keycode > keycode_to_event[i]; i += 2);
                if (keycode == keycode_to_event[i]) {
                    key = keycode_to_event[i + 1] << 16;
				} else {
                    for (i = 0; keycode_to_char[i] && keycode > keycode_to_char[i]; i += NUM_MAP_IN_A_ROW);
                    if (keycode == keycode_to_char[i]) {
						if (mod & KMOD_CTRL) {
							key = keycode_to_char[i + 5];
						} else if (mod & KMOD_SHIFT && mod & KMOD_CAPS) {
							key = keycode_to_char[i + 4];
						} else if (mod & KMOD_CAPS) {
							key = keycode_to_char[i + 3];
						} else if (mod & KMOD_SHIFT) {
							key = keycode_to_char[i + 2];
						} else {
							key = keycode_to_char[i + 1];
                        }
                    }
                }
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Key event: 0x%x, keycode: 0x%x, flags: 0x%x", key, keycode, flags);
                if (key > 0) {
                    Add_Event_Key(gob,
                        evt->key.state == SDL_PRESSED ? EVT_KEY : EVT_KEY_UP,
                        key, flags);
                }
            }
		} else {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Text Input Active");
        }
        break;
    case SDL_TEXTINPUT:
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Text Input: %s", evt->text.text);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (evt->button.which == SDL_TOUCH_MOUSEID) {
            // already processed by finger up/down
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "SDL_TOUCH_MOUSEID");
            //break;
        }
        win = SDL_GetWindowFromID(evt->button.windowID);
        gob = SDL_GetWindowData(win, "GOB");
        if (gob != NULL) {
            REBFLG flags = state_to_flags(0);
            int id = 0;
            if (evt->button.clicks == 2) {
                flags |= 1 << EVF_DOUBLE;
            }

            switch (evt->button.button) {
            case SDL_BUTTON_LEFT:
                id = (evt->button.state == SDL_PRESSED) ? EVT_DOWN : EVT_UP;
                break;
            case SDL_BUTTON_MIDDLE:
                id = (evt->button.state == SDL_PRESSED) ? EVT_AUX_DOWN : EVT_AUX_UP;
                break;
            case SDL_BUTTON_RIGHT:
                id = (evt->button.state == SDL_PRESSED) ? EVT_ALT_DOWN : EVT_ALT_UP;
                break;
            default: /* unrecognized buttons */
                return;
            }
            xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->button.x))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->button.y)) << 16);
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "button event@%d, xy: %dx%d, which: %d, button: %d, clicks: %d, state: %s",
                evt->button.timestamp,
                evt->button.x, evt->button.y,
                evt->button.which,
                evt->button.button,
                evt->button.clicks,
                (evt->button.state == SDL_PRESSED) ? "PRESSED" : "RELEASED");
            Add_Event_XY(gob, id, xyd, flags);
        }
        break;

    case SDL_MOUSEWHEEL:
        win = SDL_GetWindowFromID(evt->wheel.windowID);
        gob = SDL_GetWindowData(win, "GOB");
        if (gob != NULL) {
			int flags = state_to_flags(0);
            const int nw_num_lines = 3;
            xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->wheel.x)) * nw_num_lines) + ((ROUND_TO_INT(PHYS_COORD_Y(evt->wheel.y)) * nw_num_lines) << 16);
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Wheel event (%d, %d), xyd: 0x%x, flags: 0x%x", evt->wheel.x, evt->wheel.y, xyd, flags);
			if (xyd != 0) {
                Add_Event_XY(gob, (flags & (1 << EVF_CONTROL)) ? EVT_SCROLL_PAGE : EVT_SCROLL_LINE, xyd, 0);
            }
        }
        break;
    case SDL_MOUSEMOTION:
        if (evt->motion.which == SDL_TOUCH_MOUSEID) {
            // already processed by finger motion
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "SDL_TOUCH_MOUSEID");
            //break;
        }
        win = SDL_GetWindowFromID(evt->motion.windowID);
        gob = SDL_GetWindowData(win, "GOB");
        if (gob != NULL) {
            int flags = state_to_flags(0);
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Mouse motion event");
            xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->motion.x))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->motion.y)) << 16);
            Update_Event_XY(gob, EVT_MOVE, xyd, flags);
        }
        break;
	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
		// touch finger doesn't have an associated window, because it might not be attached to a screen.
		// use the current focused windows instead.
		if (evt->type == SDL_FINGERDOWN) {
			n_fingers++;
		}
		win = SDL_GetKeyboardFocus();
		gob = SDL_GetWindowData(win, "GOB");
		if (gob != NULL) {
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "finger event %s, number of active finger touchs: %d, finger ID: %d\n",
				evt->type == SDL_FINGERDOWN ? "DOWN"
				: evt->type == SDL_FINGERUP ? "UP"
				: "MOTION",
				n_fingers,
				evt->tfinger.fingerId
			);
			if (n_fingers != 1) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "finger event is ignored, because the number of currently active fingers is not 1: %d\n",
					n_fingers
				);
				if (evt->type == SDL_FINGERUP) {
					n_fingers--;
				}
				break;
			}
			REBFLG flags = state_to_flags(0);

			SDL_Rect rect;
			SDL_GetDisplayBounds(0, &rect);
			xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->tfinger.x * rect.w))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->tfinger.y * rect.h)) << 16);
			if (evt->type == SDL_FINGERMOTION) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Finger motion event: %dx%d", xyd & 0xFFFF, xyd >> 16);
				Update_Event_XY(gob, EVT_MOVE, xyd, flags);
			}
			else {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "tfinger event@%d, xy: %fx%f, xyd: %dx%d, state: %s",
					evt->tfinger.timestamp,
					evt->tfinger.x, evt->tfinger.y,
					xyd & 0xFFFF, xyd >> 16,
					(evt->type == SDL_FINGERDOWN) ? "DOWN" : "UP");
				Add_Event_XY(gob, evt->type == SDL_FINGERDOWN ? EVT_DOWN : EVT_UP, xyd, flags);
			}
		}
		if (evt->type == SDL_FINGERUP) {
			n_fingers--;
		}
		break;
	case SDL_MULTIGESTURE:
		assert(n_fingers == evt->mgesture.numFingers);
		win = SDL_GetMouseFocus();
		if (win) {
			gob = SDL_GetWindowData(win, "GOB");
		} else {
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Invalid win: %d", win);
		}
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Multigesture event@%d, gob: %p, loc: %fx%f, numfingers: %d, dist: %f",
					evt->mgesture.timestamp,
					 gob,
					 evt->mgesture.x, evt->mgesture.y,
					 evt->mgesture.numFingers,
					 evt->mgesture.dDist);
		if (gob != NULL && evt->mgesture.numFingers == 2) {
			SDL_Rect rect;
			REBINT diag = 0;
			REBINT flags = 0;

			SDL_GetDisplayBounds(0, &rect);
			diag = sqrt (rect.w * rect.w + rect.h * rect.h);
#define SCROLL_THREADHOLD 10 // in pixels
#define SCROLL_SEGMENT 10 // in pixels
			int scroll = ROUND_TO_INT(PHYS_COORD_X(evt->mgesture.dDist * diag));
			if (abs(scroll) > SCROLL_THREADHOLD) {
				xyd = (scroll / SCROLL_SEGMENT)  << 16;
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Simulated wheel event, scroll: %d, xyd: 0x%x, flags: 0x%x", scroll, xyd, flags);
				Accumulate_Event_XY(gob, EVT_SCROLL_LINE, xyd, flags);
			}
			//Accumulate_Event_XY(gob, EVT_TOUCH_PINCH, xyd, flags);
		}
        break;
	}
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "win: %x, focus: %p, event on gob: %p\n", win_id, focus, gob);
}

int poll_SDL_events()
{
	SDL_Event evt;
	SDL_Event mgesture;
	int found = 0;
	int in_mgesture = 0;
	while (SDL_PollEvent(&evt)) {
		found = 1;
		if (evt.type == SDL_MULTIGESTURE) {
			if (!in_mgesture) {
				mgesture = evt;
				in_mgesture = 1;
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Starting mgestures");
			} else {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Accumulating mgestures");
				mgesture.mgesture.dDist += evt.mgesture.dDist;
				mgesture.mgesture.dDist += evt.mgesture.dDist;
			}
		} else if ((evt.type == SDL_MOUSEMOTION && evt.motion.which == SDL_TOUCH_MOUSEID)
				   || evt.type == SDL_FINGERMOTION) {
			dispatch(&evt);
		} else {
			if (in_mgesture) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Stopping mgestures");
				dispatch(&mgesture);
				in_mgesture = 0;
			}
			dispatch(&evt);
		}
	}

	if (in_mgesture) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Handling mgestures before returing");
		dispatch(&mgesture);
	}
	return found;
}
