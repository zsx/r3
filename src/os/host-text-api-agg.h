#ifdef __cplusplus
extern "C" {
#endif

extern void* agg_create_rich_text();
extern void agg_destroy_rich_text(void* rt);
extern void agg_rt_anti_alias(void* rt, REBINT mode);
extern void agg_rt_bold(void* rt, REBINT state);
extern void agg_rt_caret(void* rt, REBXYF* caret, REBXYF* highlightStart, REBXYF highlightEnd);
extern void agg_rt_center(void* rt);
extern void agg_rt_color(void* rt, REBCNT col);
extern void agg_rt_drop(void* rt, REBINT number);
extern void agg_rt_font(void* rt, void* font);
extern void agg_rt_font_size(void* rt, REBINT size);
extern void* agg_rt_get_font(void* rt);
extern void* agg_rt_get_para(void* rt);
extern void agg_rt_italic(void* rt, REBINT state);
extern void agg_rt_left(void* rt);
extern void agg_rt_newline(void* rt, REBINT index);
extern void agg_rt_para(void* rt, void* para);
extern void agg_rt_right(void* rt);
extern void agg_rt_scroll(void* rt, REBXYF offset);
extern void agg_rt_shadow(void* rt, REBXYF d, REBCNT color, REBINT blur);
extern void agg_rt_set_font_styles(void* font, u32 word);
extern void agg_rt_size_text(void* rt, REBGOB* gob, REBXYF* size);
extern void agg_rt_text(void* gr, REBCHR* text, REBINT index, REBCNT gc);
extern void agg_rt_underline(void* rt, REBINT state);

extern void agg_rt_offset_to_caret(void* rt, REBGOB *gob, REBXYF xy, REBINT *element, REBINT *position);
extern void agg_rt_caret_to_offset(void* rt, REBGOB *gob, REBXYF* xy, REBINT element, REBINT position);
extern REBINT agg_rt_gob_text(REBGOB *gob, REBDRW_CTX *draw_ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);
extern void agg_rt_block_text(void *rt, REBSER *block);

#ifdef __cplusplus
}
#endif
