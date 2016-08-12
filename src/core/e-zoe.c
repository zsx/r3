/**
 * Atronix ZOE Add-on Natives
 *
 * Copyright (2016) Atronix Engineering, Inc
 *
 */

#include "sys-core.h"

static REB_R do_call_widget_method(
    REBVAL *out,
    REBSYM handler,
    REBCTX *instance,
    REBOOL param,
    REBVAL *extra,
    REBOOL class_,
    REBVAL *klass
)
{
    REBCNT n;
    REBVAL *method = NULL;

    if (class_) { //see if user specifies a class to look for
        n = Find_Canon_In_Context(VAL_CONTEXT(klass), Canon(handler), FALSE);
        if (n) {
            method = VAL_CONTEXT_VAR(klass, n);
        }
    } else {// see if the instance has the method
        n = Find_Canon_In_Context(instance, Canon(handler), FALSE);
        if (n) {
            method = CTX_VAR(instance, n);
        } else {// see if the class has the method
            n = Find_Canon_In_Context(instance, Canon(SYM_CLASS), FALSE);
            if (n) {
                klass = CTX_VAR(instance, n);
                if (IS_OBJECT(klass)) {
                    n = Find_Canon_In_Context(VAL_CONTEXT(klass), Canon(handler), FALSE);
                    if (n) method = VAL_CONTEXT_VAR(klass, n);
                }
            }
        }
    }

    if (method && IS_FUNCTION(method)) {
        if (IS_FUNCTION(method)) {
            struct Reb_State state;
            REBCTX *error;
            REBOOL thrown;
            REBVAL old_instance;

            PUSH_TRAP(&error, &state);
            if (error) {
                Val_Init_Object(VAL_CONTEXT_VAR(klass, n), VAL_CONTEXT(&old_instance));
                fail (error);
            }
            if (klass) {
                n = Find_Canon_In_Context(VAL_CONTEXT(klass), Canon(SYM_INSTANCE), FALSE);
                if (n) {
                    old_instance = *VAL_CONTEXT_VAR(klass, n);
                    Val_Init_Object(VAL_CONTEXT_VAR(klass, n), instance);
                }
            }
            if (param) {
                thrown = Apply_Only_Throws(out, TRUE, method, extra, END_CELL);
            } else {
                thrown = Apply_Only_Throws(out, TRUE, method, END_CELL);
            }
            if (klass) {
                *VAL_CONTEXT_VAR(klass, n) = old_instance;
            }
            DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
            return thrown? R_OUT_IS_THROWN : R_OUT;
        }
        else {
            return R_OUT_VOID_IF_UNWRITTEN;
        }
    } else {
        return R_OUT_VOID_IF_UNWRITTEN;
    }

}

static REB_R do_zoom(REBVAL *out,
        REBGOB *gob,
		REBVAL *zoom,
		REBOOL skip_first,
        REBOOL sticky,
        REBVAL *sticky_offset,
        REBOOL reinit
    )
{
    REBSER *pane;

    if (GOB_PANE(gob)) {
        REBCNT pane_len = SER_LEN(GOB_PANE(gob));
        pane = Make_Series(skip_first ? pane_len : pane_len + 1, sizeof(REBGOB*), MKS_NONE);
        if (!skip_first) Append_Series(pane, cast(REBYTE*, &gob), 1);
        Append_Series(pane, SER_HEAD(REBYTE, GOB_PANE(gob)), SER_LEN(GOB_PANE(gob)));
    }

    while (SER_LEN(pane) > 0) {
        gob = *SER_HEAD(REBGOB*, pane);
	    if (GOB_DTYPE(gob) == GOBD_OBJECT) {
            REBCTX *data = AS_CONTEXT(GOB_DATA(gob));
            REBCNT n = Find_Canon_In_Context(data, Canon(SYM_PLACEMENT_ZOOM), FALSE);
            if (n) {
                REBVAL *placement = CTX_VAR(data, n);
                if (IS_INTEGER(placement)
                    || IS_DECIMAL(placement)) {
                    REBDEC zoom_x, zoom_y;
                    if (IS_DECIMAL(placement)) {
                        zoom_x = VAL_PAIR_X(zoom) / VAL_DECIMAL(placement);
                        zoom_y = VAL_PAIR_Y(zoom) / VAL_DECIMAL(placement);
                    }
                    else {
                        zoom_x = VAL_PAIR_X(zoom) / VAL_INT64(placement);
                        zoom_y = VAL_PAIR_Y(zoom) / VAL_INT64(placement);
                    }
                    n = Find_Canon_In_Context(data, Canon(SYM_INITIAL_OFFSET), FALSE);
                    if (n) {
                        REBVAL *init_offset = CTX_VAR(data, n);
                        if (IS_PAIR(init_offset)) {
                            if (sticky) {
                                gob->offset.x = VAL_PAIR_X(init_offset) * VAL_PAIR_X(zoom) + VAL_PAIR_X(sticky_offset);
                                gob->offset.y = VAL_PAIR_Y(init_offset) * VAL_PAIR_Y(zoom) + VAL_PAIR_Y(sticky_offset);
                            }
                            else {
                                gob->offset.x = VAL_PAIR_X(init_offset) * zoom_x;
                                gob->offset.y = VAL_PAIR_Y(init_offset) * zoom_y;
                            }
                        }
                        else {
                            //fail: init-offset is not a pair!
                        }
                    }
                    else {
                        // fail: init-offset not found
                    }
                    // gob/size: gob/data/init-size * zoom / (gob/data/placement-zoom)
                    n = Find_Canon_In_Context(data, Canon(SYM_INITIAL_SIZE), FALSE);
                    if (n) {
                        REBVAL *init_size = CTX_VAR(data, n);
                        if (IS_PAIR(init_size)) {
                            gob->size.x = VAL_PAIR_X(init_size) * zoom_x;
                            gob->size.y = VAL_PAIR_Y(init_size) * zoom_y;
                        }
                        else {
                            //fail: init-size is not a pair!
                        }
                    }
                    else {
                        // fail: init-size not found
                    }

                    if (sticky) {
                        //gob/sticky-zoom: zoom/x / gob/data/placement-zoom
                        n = Find_Canon_In_Context(data, Canon(SYM_STICKY_ZOOM), FALSE);
                        if (n) SET_DECIMAL(CTX_VAR(data, n), zoom_x);
                    }

                    if (reinit) {
                        if (R_OUT_IS_THROWN == do_call_widget_method(out, SYM_ON_INIT, data, FALSE, NULL, FALSE, NULL)) {
                            return R_OUT_IS_THROWN;
                        }
                    }
                }
                else {
                    //fail: placement-zoom is not a number!
                }
            }
            else {
                // fail: placement-zoom is not found
            }
	    } else if (GOB_DTYPE(gob) == GOBD_NONE) {
		    if (GOB_PARENT(gob))
			    gob->size = GOB_PARENT(gob)->size;
	    } else {
		    //fail: data is not an object
	    }

        Remove_Series(pane, 0, 1);
        if (GOB_PANE(gob)) {
            Append_Series(pane, SER_HEAD(REBYTE, GOB_PANE(gob)), SER_LEN(GOB_PANE(gob)));
        }
    }

    Free_Series(pane);

    return R_OUT_VOID_IF_UNWRITTEN;
}

//
//  zoe-zoom: native [
//  
//  {Recursivly zoom a gob based on the zoom level and gob/data/placement-zoom}
//  
//      gob [gob!]
//      zoom [pair!] "current zoom level"
//      /skip-first "Skip the top level GOB"
//      /sticky "Calculate sticky-zoom"
//      offset "Calculated translation offset"
//      /reinit "Call on-init afterward"
//  ]
//
REBNATIVE(zoe_zoom)
{
    PARAM(1, gob);
    PARAM(2, zoom);
    REFINE(3, skip_first);
    REFINE(4, sticky);
    PARAM(5, sticky_offset);
    REFINE(6, reinit);

    return do_zoom(D_OUT, VAL_GOB(ARG(gob)), ARG(zoom),
        REF(skip_first),
        REF(sticky), ARG(sticky_offset),
        REF(reinit));

}

//
//  zoe-call-widget-method: native [
//
//  "Call the action handler on the widget"
//
//      handler[word!] "the action handler name (e.g. on-click)"
//      this[object!] "widget instance object"
//      /param extra[block!] "Extra parameters to the function"
//      /class klass [object!] "The ancester class whose method should be called"
//  ]
//
REBNATIVE(zoe_call_widget_method)
{
    PARAM(1, handler);
    PARAM(2, instance);
    REFINE(3, param);
    PARAM(4, extra);
    REFINE(5, class_);
    PARAM(6, klass);

    return do_call_widget_method(D_OUT,
                               VAL_WORD_SYM(ARG(handler)), VAL_CONTEXT(ARG(instance)),
                               REF(param), ARG(extra),
                               REF(class_), ARG(klass));
}

static REB_R do_draw_widget(REBVAL *out, REBGOB *gob, REBDEC zoom, REBOOL redraw, int level)
{
    if (level != 0) {//skip first
        if (GOB_DTYPE(gob) == GOBD_OBJECT) {
            REBCTX *data = AS_CONTEXT(GOB_DATA(gob));
            REBCNT n = Find_Canon_In_Context(data, Canon(SYM_SELECTED_Q), FALSE);
            if (n) {
                if (!VAL_LOGIC(CTX_VAR(data, n))) { // not selected?
                    if (redraw) {
                        n = Find_Canon_In_Context(data, Canon(SYM_MIN_ZOOM), FALSE);
                        if (n) {
                            REBDEC min_zoom = VAL_DECIMAL(CTX_VAR(data, n));
                            n = Find_Canon_In_Context(data, Canon(SYM_MAX_ZOOM), FALSE);
                            if (n) {
                                REBDEC max_zoom = VAL_DECIMAL(CTX_VAR(data, n));
                                if (zoom < min_zoom || zoom > max_zoom) {
                                    goto recurse;
                                }
                            }
                        }
                        if (R_OUT_IS_THROWN == do_call_widget_method(out, SYM_ON_DRAW, data, FALSE, NULL, FALSE, NULL)) {
                            return R_OUT_IS_THROWN;
                        }
                    }

                    // unless none? in gob/data 'color [gob/color: gob/data/color] ;for LED widget
                    n = Find_Canon_In_Context(data, Canon(SYM_COLOR), FALSE);
                    if (n) {
                        REBVAL *color = CTX_VAR(data, n);

                        // Copy from Set_GOB_Var in t-gob.c, please keep in sync
                        CLR_GOB_OPAQUE(gob);
                        if (IS_TUPLE(color)) {
                            SET_GOB_TYPE(gob, GOBT_COLOR);
                            Set_Pixel_Tuple((REBYTE*)&GOB_CONTENT(gob), color);
                            if (VAL_TUPLE_LEN(color) < 4 || VAL_TUPLE(color)[3] == 0)
                                SET_GOB_OPAQUE(gob);
                        } else if (IS_VOID(color)) {
                            SET_GOB_TYPE(gob, GOBT_NONE);
                        }
                    }

					// unless none? in gob/data 'draw [gob/draw: bind/only compose/only gob/data/draw import 'draw] ;for most other widgets
                    n = Find_Canon_In_Context(data, Canon(SYM_DRAW), FALSE);
                    if (n) {
                        REBVAL *draw = CTX_VAR(data, n);
                        REBVAL *modules = VAL_CONTEXT_VAR(ROOT_SYSTEM, SYS_MODULES);
                        REBCTX *context = NULL;
                        REBVAL word_draw;
                        REBCNT draw_index;

                        Val_Init_Word(&word_draw, REB_WORD, Canon(SYM_DRAW));

                        //find/skip sys/modules 'draw 3
                        draw_index = Find_In_Array(VAL_ARRAY(modules), 0, VAL_LEN_HEAD(modules), &word_draw, 1, 0, 3);
                        if (draw_index) {
                            REBVAL *val = ARR_AT(VAL_ARRAY(modules), draw_index + 1);
                            context = VAL_CONTEXT(val);
                        }

                        assert(context != NULL);

                        //compose/only draw
                        if (R_OUT_IS_THROWN == Compose_Any_Array_Throws(out, VAL_ARRAY_AT(draw), FALSE, TRUE, FALSE)) {
                            return R_OUT_IS_THROWN;
                        }

                        //*draw = *out;

                        //bind/only draw import 'draw
                        Bind_Values_Core(VAL_ARRAY_HEAD(draw), context, TS_ANY_WORD, 0, BIND_0);

                        // Copy from Set_GOB_Var in t-gob.c, please keep in sync
                        CLR_GOB_OPAQUE(gob);
                        if (IS_BLOCK(draw)) {
                            SET_GOB_TYPE(gob, GOBT_DRAW);
                            GOB_CONTENT(gob) = VAL_SERIES(draw);
                        } else if (IS_VOID(draw)) {
                            SET_GOB_TYPE(gob, GOBT_NONE);
                        }
                    }
                }
            }
        }
    }

recurse:
    if (GOB_PANE(gob)) {
        REBCNT n;
        for (n = 0; n < SER_LEN(GOB_PANE(gob)); n++) {
            do_draw_widget(out, *SER_AT(REBGOB*, GOB_PANE(gob), n), zoom, redraw, level + 1);
        }
    }

    return R_OUT;
}

//
//  zoe-draw-widget: native [
//
//  "Update the gob from its embedded widget"
//
//      gob [gob!]
//      zoom [pair!] "current zoom level"
//      /skip-first "Skip the top level GOB"
//      /redraw "call on-draw to update draw block?"
//  ]
//
REBNATIVE(zoe_draw_widget)
{
    PARAM(1, gob);
    PARAM(2, zoom);
    REFINE(3, skip_first);
    REFINE(4, redraw);
    
    return do_draw_widget(D_OUT, VAL_GOB(ARG(gob)), VAL_PAIR_X(ARG(zoom)), REF(redraw), REF(skip_first) ? 0 : 1);
}
