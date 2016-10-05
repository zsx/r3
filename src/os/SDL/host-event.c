
#include <math.h>
#include <string.h>

#include "SDL.h"

#include "reb-host.h"
//#include <unistd.h>
#include "host-view.h"
#include "host-compositor.h"

static struct SDL_REBEVT_Pair {
    SDL_Keycode keycode;
    REBCNT key_event;
} keycode_to_event[] = {
    {SDLK_AC_HOME,       EVK_HOME},
    {SDLK_HOME,          EVK_HOME},
    {SDLK_LEFT,          EVK_LEFT},
    {SDLK_UP,            EVK_UP},
    {SDLK_RIGHT,         EVK_RIGHT},
    {SDLK_DOWN,          EVK_DOWN},
    {SDLK_PAGEUP,        EVK_PAGE_UP},
    {SDLK_PAGEDOWN,      EVK_PAGE_DOWN},
    {SDLK_END,           EVK_END},
    {SDLK_INSERT,        EVK_INSERT},
    {SDLK_DELETE,        EVK_DELETE},

    {SDLK_F1,            EVK_F1},
    {SDLK_F2,            EVK_F2},
    {SDLK_F3,            EVK_F3},
    {SDLK_F4,            EVK_F4},
    {SDLK_F5,            EVK_F5},
    {SDLK_F6,            EVK_F6},
    {SDLK_F7,            EVK_F7},
    {SDLK_F8,            EVK_F8},
    {SDLK_F9,            EVK_F9},
    {SDLK_F10,           EVK_F10},
    {SDLK_F11,           EVK_F11},
    {SDLK_F12,           EVK_F12},
    { 0x0,                0 }
};

 static struct SDL_REBYTE_Pair {
    SDL_Keycode keycode;
    REBCNT charactor;
    REBCNT with_shift;
    REBCNT with_cap;
    REBCNT with_cap_shift;
 } keycode_to_char[] = {
     //SDL_Keycode,    char,     SHIFT,     CAP,     CAP+SHIFT
     { SDLK_0,        '0',     ')',     '0',     ')' },
     { SDLK_1,        '1',     '!',    '1',    '!' },
     { SDLK_2,        '2',    '@',    '2',    '@' },
     { SDLK_3,        '3',    '#',    '3',    '#' },
     { SDLK_4,        '4',    '$',    '4',    '$' },
     { SDLK_5,        '5',    '%',    '5',    '%' },
     { SDLK_6,        '6',    '^',    '6',    '^' },
     { SDLK_7,        '7',    '&',    '7',    '&' },
     { SDLK_8,        '8',    '*',    '8',    '*' },
     { SDLK_9,        '9',    '(',    '9',    '(' },
     { SDLK_a,        'a',    'A',    'A',    'a' },
     { SDLK_b,        'b',    'B',    'B',    'b' },
     { SDLK_c,        'c',    'C',    'C',    'c' },
     { SDLK_d,        'd',    'D',    'D',    'd' },
     { SDLK_e,        'e',    'E',    'E',    'e' },
     { SDLK_f,        'f',    'F',    'F',    'f' },
     { SDLK_g,        'g',    'G',    'G',    'g' },
     { SDLK_h,        'h',    'H',    'H',    'h' },
     { SDLK_i,        'i',    'I',    'I',    'i' },
     { SDLK_j,        'j',    'J',    'J',    'j' },
     { SDLK_k,        'k',    'K',    'K',    'k' },
     { SDLK_l,        'l',    'L',    'L',    'l' },
     { SDLK_m,        'm',    'M',    'M',    'm' },
     { SDLK_n,        'n',    'N',    'N',    'n' },
     { SDLK_o,        'o',    'O',    'O',    'o' },
     { SDLK_p,        'p',    'P',    'P',    'p' },
     { SDLK_q,        'q',    'Q',    'Q',    'q' },
     { SDLK_r,        'r',    'R',    'R',    'r' },
     { SDLK_s,        's',    'S',    'S',    's' },
     { SDLK_t,        't',    'T',    'T',    't' },
     { SDLK_u,        'u',    'U',    'U',    'u' },
     { SDLK_v,        'v',    'V',    'V',    'v' },
     { SDLK_w,        'w',    'W',    'W',    'w' },
     { SDLK_x,        'x',    'X',    'X',    'x' },
     { SDLK_y,        'y',    'Y',    'Y',    'y' },
     { SDLK_z,        'z',    'Z',    'Z',    'z' },
     { SDLK_SPACE,    ' ',    ' ',    ' ',    ' ' },
     { SDLK_COMMA,    ',',    '<',    ',',    '<' },
     { SDLK_PERIOD,   '.',    '>',    '.',    '>' },
     { SDLK_SLASH,    '/',    '?',    '/',    '?' },
     { SDLK_MINUS,    '-',    '_',    '-',    '_' },
     { SDLK_EQUALS,   '=',    '+',    '=',    '+' },
     { SDLK_SEMICOLON, ';',    ':',    ';',    ':' },
     { SDLK_QUOTE,    '\'',    '"',    '\'',    '"' },
     { SDLK_LEFTBRACKET, '[', '{', '[', '{' },
     { SDLK_RIGHTBRACKET, ']', '}', ']', '}' },
     { SDLK_BACKSLASH,    '\\', '|', '\\',    '|' },
     { SDLK_BACKQUOTE, '`',  '~',    '`',    '~' },
     { SDLK_MINUS,   '-',    '_',    '-',    '_' },
     { SDLK_EQUALS,  '=',    '+',    '=',    '+' },
     { SDLK_BACKSPACE, '\b', '\b',   '\b',   '\b' },
     { SDLK_ESCAPE,  '\033', '\033', '\033',    '\033' },
     { SDLK_KP_0,    '0',    '0',    '0',    '0' },
     { SDLK_KP_1,    '1',    '1',    '1',    '1' },
     { SDLK_KP_2,    '2',    '2',    '2',    '2' },
     { SDLK_KP_3,    '3',    '3',    '3',    '3' },
     { SDLK_KP_4,    '4',    '4',    '4',    '4' },
     { SDLK_KP_5,    '5',    '5',    '5',    '5' },
     { SDLK_KP_6,    '6',    '6',    '6',    '6' },
     { SDLK_KP_7,    '7',    '7',    '7',    '7' },
     { SDLK_KP_8,    '8',    '8',    '8',    '8' },
     { SDLK_KP_9,    '9',    '9',    '9',    '9' },
     { SDLK_KP_BACKSPACE,     '\b', '\b', '\b', '\b' },
     { SDLK_KP_ENTER,    '\r', '\r', '\r',     '\r' },
     { SDLK_KP_PLUS,    '+', '+',     '+',     '+' },
     { SDLK_KP_MINUS,    '-', '-',     '-',     '-' },
     { SDLK_KP_MULTIPLY, '*', '*', '*',     '*' },
     { SDLK_KP_DIVIDE,    '/', '/',     '/',     '/' },
     { SDLK_KP_PERIOD,    '.', '.',     '.',     '.' },
     { SDLK_RETURN,    '\r', '\r',    '\r',    '\r' },
     { SDLK_RETURN2,    '\r', '\r',    '\r',    '\r' },
     { SDLK_TAB,    '\t',    '\t',    '\t',    '\t' },
     { 0x0,        0,        0,        0,        0 } 
};

void Init_Host_Event()
{
    int i, j;
    /* sort keycode ascendantly */
    for(i = 0; keycode_to_event[i].keycode; i ++) {
        for(j = i + 1; keycode_to_event[j].keycode; j ++) {
            if (keycode_to_event[i].keycode > keycode_to_event[j].keycode) {
                struct SDL_REBEVT_Pair tmp = keycode_to_event[i];
                keycode_to_event[i] = keycode_to_event[j];
                keycode_to_event[j] = tmp;
            }
        }
    }

    for(i = 0; keycode_to_char[i].keycode; i ++) {
        for(j = i + 1; keycode_to_char[j].keycode; j ++) {
            if (keycode_to_char[i].keycode > keycode_to_char[j].keycode) {
                struct SDL_REBYTE_Pair tmp = keycode_to_char[i];
                keycode_to_char[i] = keycode_to_char[j];
                keycode_to_char[j] = tmp;
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
    evt.model = EVM_GUI; evt.data  = xy;
    evt.eventee.ser = (void*)gob;

    RL_Event(&evt);    // returns 0 if queue is full
}

static void Update_Event_XY(REBGOB *gob, REBINT id, REBINT xy, REBINT flags)
{
    REBEVT evt;

    memset(&evt, 0, sizeof(evt));
    evt.type  = id;
    evt.flags = (u8) (flags | (1<<EVF_HAS_XY));
    evt.model = EVM_GUI;
    evt.data  = xy;
    evt.eventee.ser = (void*)gob;

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
    evt.eventee.ser = (void*)gob;

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
    evt.eventee.ser = (void*)gob;

    RL_Event(&evt);    // returns 0 if queue is full
}

static REBFLGS state_to_flags(REBFLGS flags)
{
    SDL_Keymod mod = SDL_GetModState();
    if (mod & KMOD_CTRL) {
        flags |= 1 << EVF_CONTROL;
    }
    if (mod & KMOD_SHIFT) {
        flags |= 1 << EVF_SHIFT;
    }

    return flags;
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
                    win = SDL_GetWindowFromID(evt->window.windowID);
                    if (win) {
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL && GET_GOB_FLAG(gob, GOBF_HIDDEN)) {
                            CLR_GOB_FLAG(gob, GOBF_HIDDEN);
                        }
                    }
                    break;
                case SDL_WINDOWEVENT_HIDDEN:
                    SDL_Log("Window %d hidden", evt->window.windowID);
                    win = SDL_GetWindowFromID(evt->window.windowID);
                    if (win) {
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_HIDDEN)) {
                            SET_GOB_FLAG(gob, GOBF_HIDDEN);
                        }
                    }
                    break;
                case SDL_WINDOWEVENT_EXPOSED:
                    SDL_Log("Window %d exposed", evt->window.windowID);
                    win = SDL_GetWindowFromID(evt->window.windowID);
                    if (win) {
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL) rebcmp_blit(GOB_COMPOSITOR(gob));
                    }
                    break;
                case SDL_WINDOWEVENT_MOVED:
                    SDL_Log("Window %d moved to %d,%d",
                            evt->window.windowID, evt->window.data1,
                            evt->window.data2);
                    win = SDL_GetWindowFromID(evt->window.windowID);
                    if (win) {
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
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
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
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
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_MINIMIZE)) {
                            SET_GOB_FLAG(gob, GOBF_MINIMIZE);
                            CLR_GOB_FLAG(gob, GOBF_RESTORE);
                            CLR_GOB_FLAG(gob, GOBF_MAXIMIZE);
                            CLR_GOB_FLAG(gob, GOBF_FULLSCREEN);
                        }
                    }
                    break;
                case SDL_WINDOWEVENT_MAXIMIZED:
                    SDL_Log("Window %d maximized", evt->window.windowID);
                    win = SDL_GetWindowFromID(evt->window.windowID);
                    if (win) {
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_MAXIMIZE)) {
                            SET_GOB_FLAG(gob, GOBF_MAXIMIZE);
                            CLR_GOB_FLAG(gob, GOBF_RESTORE);
                            CLR_GOB_FLAG(gob, GOBF_MINIMIZE);
                            CLR_GOB_FLAG(gob, GOBF_FULLSCREEN);
                        }
                    }
                    break;
                case SDL_WINDOWEVENT_RESTORED:
                    SDL_Log("Window %d restored", evt->window.windowID);
                    win = SDL_GetWindowFromID(evt->window.windowID);
                    if (win) {
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL && !GET_GOB_FLAG(gob, GOBF_RESTORE)) {
                            SET_GOB_FLAG(gob, GOBF_RESTORE);
                            CLR_GOB_FLAG(gob, GOBF_MAXIMIZE);
                            CLR_GOB_FLAG(gob, GOBF_MINIMIZE);
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
                        gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                        if (gob != NULL)
                            Add_Event_XY(gob, EVT_CLOSE, 0, 0);
                    }
                    break;
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
                gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                if (gob != NULL) {
                    SDL_Keycode keycode = evt->key.keysym.sym;
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "unprocessed keycode: 0x%x", keycode);
                    for (i = 0; keycode_to_event[i].keycode && keycode > keycode_to_event[i].keycode; i ++);
                    if (keycode == keycode_to_event[i].keycode) {
                        key = keycode_to_event[i + 1].key_event << 16;
                    } else {
                        for (i = 0; keycode_to_char[i].keycode && keycode > keycode_to_char[i].keycode; i ++);
                        if (keycode == keycode_to_char[i].keycode) {
                            SDL_Keymod mod = SDL_GetModState();
                            if (mod & KMOD_SHIFT && mod & KMOD_CAPS) {
                                key = keycode_to_char[i].with_cap_shift;
                            } else if (mod & KMOD_CAPS) {
                                key = keycode_to_char[i].with_cap;
                            } else if (mod & KMOD_SHIFT) {
                                key = keycode_to_char[i].with_shift;
                            } else {
                                key = keycode_to_char[i].charactor;
                            }
                        }
                    }
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Key event: 0x%x, keycode: 0x%x", key, keycode);
                    if (key > 0) {
                        Add_Event_Key(gob,
                                      evt->key.state == SDL_PRESSED? EVT_KEY : EVT_KEY_UP,
                                      key, flags);
                    }
                }
            } else {
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Text Input Active");
            }
            break;
        case SDL_TEXTINPUT:
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Text Input: %s", evt->text.text);
            {
                win = SDL_GetWindowFromID(evt->text.windowID);
                gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
                if (gob) {
                    int i = 0;
                    for (i = 0; i < strlen(evt->text.text); i++) {
                        Add_Event_Key(gob, EVT_KEY, evt->text.text[i], 0);
                        //Add_Event_Key(gob, EVT_KEY_UP, evt->text.text[i], 0);
                    }
                }
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            win = SDL_GetWindowFromID(evt->button.windowID);
            gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
            if (gob != NULL) {
                REBFLGS flags = state_to_flags(0);
                int id = 0;
                if (evt->button.clicks == 2) {
                    flags |= 1 << EVF_DOUBLE;
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
            win = SDL_GetWindowFromID(evt->wheel.windowID);
            gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
            if (gob != NULL) {
                int flags = state_to_flags(0);
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Wheel event");
                xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->wheel.x))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->wheel.y)) << 16);
                Add_Event_XY(gob, EVT_SCROLL_LINE, xyd, flags);
            }
            break;
        case SDL_MOUSEMOTION:
            win = SDL_GetWindowFromID(evt->motion.windowID);
            gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
            if (gob != NULL) {
                int flags = state_to_flags(0);
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "motion event");
                xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->motion.x))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->motion.y)) << 16);
                Update_Event_XY(gob, EVT_MOVE, xyd, flags);
            }
            break;
        case SDL_MULTIGESTURE:
            win = SDL_GetMouseFocus();
            if (win) {
                gob = cast(REBGOB*, SDL_GetWindowData(win, "GOB"));
            } else {
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Invalid win: %p", win);
            }
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Multigesture event, gob: %p, loc: %fx%f, numfingers: %d, dist: %f",
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
                xyd = (ROUND_TO_INT(PHYS_COORD_X(evt->mgesture.dDist * diag))) + (ROUND_TO_INT(PHYS_COORD_Y(evt->mgesture.dTheta * diag)) << 16);
                Add_Event_XY(gob, EVT_SCROLL_LINE, xyd, flags);
                //Accumulate_Event_XY(gob, EVT_TOUCH_PINCH, xyd, flags);
            }
            break;
    }
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "win: %x, focus: %p, event on gob: %p\n", win_id, focus, gob);
}
