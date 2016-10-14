#include "sys-core.h"

#define SYM_FUNC(x) #x, cast(void*, x)
#define SYM_DATA(x) #x, &x

const void *rebol_symbols [] = {
    SYM_FUNC(Extension_Lib), // a-lib.c
    SYM_FUNC(Set_Root_Series), // b-init.c
    SYM_FUNC(Codec_Text), // b-init.c
    SYM_FUNC(Codec_UTF16), // b-init.c
    SYM_FUNC(Codec_UTF16LE), // b-init.c
    SYM_FUNC(Codec_UTF16BE), // b-init.c
    SYM_FUNC(Register_Codec), // b-init.c
    SYM_FUNC(Init_Task), // b-init.c
    SYM_FUNC(Init_Year), // b-init.c
    SYM_FUNC(Init_Core), // b-init.c
    SYM_FUNC(Shutdown_Core), // b-init.c
    SYM_FUNC(Bind_Values_Core), // c-bind.c
    SYM_FUNC(Unbind_Values_Core), // c-bind.c
    SYM_FUNC(Try_Bind_Word), // c-bind.c
    SYM_FUNC(Copy_And_Bind_Relative_Deep_Managed), // c-bind.c
    SYM_FUNC(Rebind_Values_Deep), // c-bind.c
    SYM_FUNC(Snap_State_Core), // c-error.c
    SYM_FUNC(Trapped_Helper_Halted), // c-error.c
    SYM_FUNC(Fail_Core), // c-error.c
    SYM_FUNC(Stack_Depth), // c-error.c
    SYM_FUNC(Find_Error_For_Code), // c-error.c
    SYM_FUNC(Make_Error_Object_Throws), // c-error.c
    SYM_FUNC(Make_Error_Core), // c-error.c
    SYM_FUNC(Error), // c-error.c
    SYM_FUNC(Error_Lookback_Quote_Too_Late), // c-error.c
    SYM_FUNC(Error_Lookback_Quote_Set_Soft), // c-error.c
    SYM_FUNC(Error_Non_Logic_Refinement), // c-error.c
    SYM_FUNC(Error_Bad_Func_Def), // c-error.c
    SYM_FUNC(Error_No_Arg), // c-error.c
    SYM_FUNC(Error_Invalid_Datatype), // c-error.c
    SYM_FUNC(Error_No_Memory), // c-error.c
    SYM_FUNC(Error_Invalid_Arg_Core), // c-error.c
    SYM_FUNC(Error_Invalid_Arg), // c-error.c
    SYM_FUNC(Error_Bad_Refine_Revoke), // c-error.c
    SYM_FUNC(Error_No_Value_Core), // c-error.c
    SYM_FUNC(Error_No_Value), // c-error.c
    SYM_FUNC(Error_No_Catch_For_Throw), // c-error.c
    SYM_FUNC(Error_Invalid_Type), // c-error.c
    SYM_FUNC(Error_Out_Of_Range), // c-error.c
    SYM_FUNC(Error_Protected_Key), // c-error.c
    SYM_FUNC(Error_Illegal_Action), // c-error.c
    SYM_FUNC(Error_Math_Args), // c-error.c
    SYM_FUNC(Error_Unexpected_Type), // c-error.c
    SYM_FUNC(Error_Arg_Type), // c-error.c
    SYM_FUNC(Error_Bad_Return_Type), // c-error.c
    SYM_FUNC(Error_Bad_Make), // c-error.c
    SYM_FUNC(Error_Cannot_Reflect), // c-error.c
    SYM_FUNC(Error_On_Port), // c-error.c
    SYM_FUNC(Exit_Status_From_Value), // c-error.c
    SYM_FUNC(Init_Errors), // c-error.c
    SYM_FUNC(Security_Policy), // c-error.c
    SYM_FUNC(Trap_Security), // c-error.c
    SYM_FUNC(Check_Security), // c-error.c
    SYM_FUNC(Do_Core), // c-eval.c
    SYM_FUNC(Alloc_Context), // c-frame.c
    SYM_FUNC(Expand_Context_Keylist_Core), // c-frame.c
    SYM_FUNC(Ensure_Keylist_Unique_Invalidated), // c-frame.c
    SYM_FUNC(Expand_Context), // c-frame.c
    SYM_FUNC(Append_Context_Core), // c-frame.c
    SYM_FUNC(Append_Context), // c-frame.c
    SYM_FUNC(Copy_Context_Shallow_Extra), // c-frame.c
    SYM_FUNC(Copy_Context_Shallow), // c-frame.c
    SYM_FUNC(Collect_Keys_Start), // c-frame.c
    SYM_FUNC(Grab_Collected_Keylist_Managed), // c-frame.c
    SYM_FUNC(Collect_Keys_End), // c-frame.c
    SYM_FUNC(Collect_Context_Keys), // c-frame.c
    SYM_FUNC(Collect_Keylist_Managed), // c-frame.c
    SYM_FUNC(Collect_Words), // c-frame.c
    SYM_FUNC(Rebind_Context_Deep), // c-frame.c
    SYM_FUNC(Make_Selfish_Context_Detect), // c-frame.c
    SYM_FUNC(Construct_Context), // c-frame.c
    SYM_FUNC(Context_To_Array), // c-frame.c
    SYM_FUNC(Merge_Contexts_Selfish), // c-frame.c
    SYM_FUNC(Resolve_Context), // c-frame.c
    SYM_FUNC(Find_Canon_In_Context), // c-frame.c
    SYM_FUNC(Select_Canon_In_Context), // c-frame.c
    SYM_FUNC(Find_Word_In_Array), // c-frame.c
    SYM_FUNC(Obj_Value), // c-frame.c
    SYM_FUNC(Init_Collector), // c-frame.c
    SYM_FUNC(List_Func_Words), // c-function.c
    SYM_FUNC(List_Func_Typesets), // c-function.c
    SYM_FUNC(Make_Paramlist_Managed_May_Fail), // c-function.c
    SYM_FUNC(Find_Param_Index), // c-function.c
    SYM_FUNC(Make_Function), // c-function.c
    SYM_FUNC(Make_Expired_Frame_Ctx_Managed), // c-function.c
    SYM_FUNC(Get_Maybe_Fake_Func_Body), // c-function.c
    SYM_FUNC(Make_Interpreted_Function_May_Fail), // c-function.c
    SYM_FUNC(Make_Frame_For_Function), // c-function.c
    SYM_FUNC(Specialize_Function_Throws), // c-function.c
    SYM_FUNC(Clonify_Function), // c-function.c
    SYM_FUNC(Action_Dispatcher), // c-function.c
    SYM_FUNC(Unchecked_Dispatcher), // c-function.c
    SYM_FUNC(Voider_Dispatcher), // c-function.c
    SYM_FUNC(Returner_Dispatcher), // c-function.c
    SYM_FUNC(Specializer_Dispatcher), // c-function.c
    SYM_FUNC(Hijacker_Dispatcher), // c-function.c
    SYM_FUNC(Adapter_Dispatcher), // c-function.c
    SYM_FUNC(Chainer_Dispatcher), // c-function.c
    SYM_FUNC(Get_If_Word_Or_Path_Arg), // c-function.c
    SYM_FUNC(Apply_Frame_Core), // c-function.c
    SYM_FUNC(Next_Path_Throws), // c-path.c
    SYM_FUNC(Do_Path_Throws_Core), // c-path.c
    SYM_FUNC(Error_Bad_Path_Select), // c-path.c
    SYM_FUNC(Error_Bad_Path_Set), // c-path.c
    SYM_FUNC(Error_Bad_Path_Range), // c-path.c
    SYM_FUNC(Error_Bad_Path_Field_Set), // c-path.c
    SYM_FUNC(Pick_Path), // c-path.c
    SYM_FUNC(Get_Simple_Value_Into), // c-path.c
    SYM_FUNC(Resolve_Path), // c-path.c
    SYM_FUNC(Is_Port_Open), // c-port.c
    SYM_FUNC(Set_Port_Open), // c-port.c
    SYM_FUNC(Use_Port_State), // c-port.c
    SYM_FUNC(Pending_Port), // c-port.c
    SYM_FUNC(Awake_System), // c-port.c
    SYM_FUNC(Wait_Ports), // c-port.c
    SYM_FUNC(Sieve_Ports), // c-port.c
    SYM_FUNC(Find_Action), // c-port.c
    SYM_FUNC(Redo_Func_Throws), // c-port.c
    SYM_FUNC(Do_Port_Action), // c-port.c
    SYM_FUNC(Secure_Port), // c-port.c
    SYM_FUNC(Validate_Port), // c-port.c
    SYM_FUNC(Register_Scheme), // c-port.c
    SYM_FUNC(Init_Ports), // c-port.c
    SYM_FUNC(Shutdown_Ports), // c-port.c
    SYM_FUNC(Do_Signals_Throws), // c-signal.c
    SYM_FUNC(Do_Task), // c-task.c
#if defined(__cplusplus) && !defined(NDEBUG)
    SYM_FUNC(Assert_Cell_Writable), // c-value.c
#endif
    SYM_FUNC(Get_Hash_Prime), // c-word.c
    SYM_FUNC(Intern_UTF8_Managed), // c-word.c
    SYM_FUNC(GC_Kill_Interning), // c-word.c
    SYM_FUNC(Val_Init_Word_Bound), // c-word.c
    SYM_FUNC(Get_Type_Name), // c-word.c
    SYM_FUNC(Compare_Word), // c-word.c
    SYM_FUNC(Init_Symbols), // c-word.c
    SYM_FUNC(Init_Words), // c-word.c
    SYM_FUNC(Do_Breakpoint_Throws), // d-break.c
    SYM_FUNC(Panic_Core), // d-crash.c
    SYM_FUNC(Init_StdIO), // d-print.c
    SYM_FUNC(Shutdown_StdIO), // d-print.c
    SYM_FUNC(Print_OS_Line), // d-print.c
    SYM_FUNC(Prin_OS_String), // d-print.c
    SYM_FUNC(Out_Value), // d-print.c
    SYM_FUNC(Out_Str), // d-print.c
    SYM_FUNC(Enable_Backtrace), // d-print.c
    SYM_FUNC(Display_Backtrace), // d-print.c
    SYM_FUNC(Echo_File), // d-print.c
    SYM_FUNC(Form_Hex_Pad), // d-print.c
    SYM_FUNC(Form_Hex2), // d-print.c
    SYM_FUNC(Form_Hex2_Uni), // d-print.c
    SYM_FUNC(Form_Hex_Esc_Uni), // d-print.c
    SYM_FUNC(Form_RGB_Uni), // d-print.c
    SYM_FUNC(Form_Uni_Hex), // d-print.c
    SYM_FUNC(Form_Args_Core), // d-print.c
    SYM_FUNC(Form_Args), // d-print.c
    SYM_FUNC(Form_Value_Throws), // d-print.c
    SYM_FUNC(Print_Value_Throws), // d-print.c
    SYM_FUNC(Print_Value), // d-print.c
    SYM_FUNC(Init_Raw_Print), // d-print.c
    SYM_FUNC(Collapsify_Array), // d-stack.c
    SYM_FUNC(Make_Where_For_Frame), // d-stack.c
    SYM_FUNC(Frame_For_Stack_Level), // d-stack.c
    SYM_FUNC(Eval_Depth), // d-trace.c
    SYM_FUNC(Frame_At_Depth), // d-trace.c
    SYM_FUNC(Trace_Line), // d-trace.c
    SYM_FUNC(Trace_Func), // d-trace.c
    SYM_FUNC(Trace_Return), // d-trace.c
    SYM_FUNC(Trace_Value), // d-trace.c
    SYM_FUNC(Trace_String), // d-trace.c
    SYM_FUNC(Trace_Error), // d-trace.c
    SYM_FUNC(Copy_Array_At_Extra_Shallow), // f-blocks.c
    SYM_FUNC(Copy_Array_At_Max_Shallow), // f-blocks.c
    SYM_FUNC(Copy_Values_Len_Extra_Skip_Shallow), // f-blocks.c
    SYM_FUNC(Clonify_Values_Len_Managed), // f-blocks.c
    SYM_FUNC(Copy_Array_Core_Managed), // f-blocks.c
    SYM_FUNC(Copy_Array_At_Extra_Deep_Managed), // f-blocks.c
    SYM_FUNC(Copy_Rerelativized_Array_Deep_Managed), // f-blocks.c
    SYM_FUNC(Alloc_Tail_Array), // f-blocks.c
    SYM_FUNC(Find_Same_Array), // f-blocks.c
    SYM_FUNC(Unmark_Array), // f-blocks.c
    SYM_FUNC(Unmark), // f-blocks.c
    SYM_FUNC(Decode_Binary), // f-enbase.c
    SYM_FUNC(Encode_Base2), // f-enbase.c
    SYM_FUNC(Encode_Base16), // f-enbase.c
    SYM_FUNC(Encode_Base64), // f-enbase.c
    SYM_FUNC(Make_Command), // f-extension.c
    SYM_FUNC(Command_Dispatcher), // f-extension.c
    SYM_FUNC(Grab_Int), // f-math.c
    SYM_FUNC(Grab_Int_Scale), // f-math.c
    SYM_FUNC(Form_Int_Len), // f-math.c
    SYM_FUNC(Form_Int_Pad), // f-math.c
    SYM_FUNC(Form_Int), // f-math.c
    SYM_FUNC(Form_Integer), // f-math.c
    SYM_FUNC(Emit_Integer), // f-math.c
    SYM_FUNC(Emit_Decimal), // f-math.c
    SYM_FUNC(Modify_Array), // f-modify.c
    SYM_FUNC(Modify_String), // f-modify.c
    SYM_FUNC(Set_Random), // f-random.c
    SYM_FUNC(Random_Int), // f-random.c
    SYM_FUNC(Random_Range), // f-random.c
    SYM_FUNC(Random_Dec), // f-random.c
    SYM_FUNC(Get_Round_Flags), // f-round.c
    SYM_FUNC(Round_Dec), // f-round.c
    SYM_FUNC(Round_Int), // f-round.c
    SYM_FUNC(Round_Deci), // f-round.c
    SYM_FUNC(Series_Common_Action_Returns), // f-series.c
    SYM_FUNC(Cmp_Array), // f-series.c
    SYM_FUNC(Cmp_Value), // f-series.c
    SYM_FUNC(Find_In_Array_Simple), // f-series.c
    SYM_FUNC(Destroy_External_Storage), // f-series.c
    SYM_FUNC(REBCNT_To_Bytes), // f-stubs.c
    SYM_FUNC(Bytes_To_REBCNT), // f-stubs.c
    SYM_FUNC(Find_Int), // f-stubs.c
    SYM_FUNC(Get_Num_From_Arg), // f-stubs.c
    SYM_FUNC(Float_Int16), // f-stubs.c
    SYM_FUNC(Int32), // f-stubs.c
    SYM_FUNC(Int32s), // f-stubs.c
    SYM_FUNC(Int64), // f-stubs.c
    SYM_FUNC(Dec64), // f-stubs.c
    SYM_FUNC(Int64s), // f-stubs.c
    SYM_FUNC(Int8u), // f-stubs.c
    SYM_FUNC(Find_Refines), // f-stubs.c
    SYM_FUNC(Val_Init_Datatype), // f-stubs.c
    SYM_FUNC(Get_Type), // f-stubs.c
    SYM_FUNC(Type_Of_Core), // f-stubs.c
    SYM_FUNC(Get_Field_Name), // f-stubs.c
    SYM_FUNC(Get_Field), // f-stubs.c
    SYM_FUNC(Get_Object), // f-stubs.c
    SYM_FUNC(In_Object), // f-stubs.c
    SYM_FUNC(Get_System), // f-stubs.c
    SYM_FUNC(Get_System_Int), // f-stubs.c
    SYM_FUNC(Val_Init_Series_Index_Core), // f-stubs.c
    SYM_FUNC(Set_Tuple), // f-stubs.c
    SYM_FUNC(Val_Init_Context_Core), // f-stubs.c
    SYM_FUNC(Partial1), // f-stubs.c
    SYM_FUNC(Partial), // f-stubs.c
    SYM_FUNC(Clip_Int), // f-stubs.c
    SYM_FUNC(memswapl), // f-stubs.c
    SYM_FUNC(Add_Max), // f-stubs.c
    SYM_FUNC(Mul_Max), // f-stubs.c
    SYM_FUNC(Make_OS_Error), // f-stubs.c
    SYM_FUNC(Collect_Set_Words), // f-stubs.c
    SYM_FUNC(Scan_Item_Push_Mold), // l-scan.c
    SYM_FUNC(Scan_UTF8_Managed), // l-scan.c
    SYM_FUNC(Scan_Header), // l-scan.c
    SYM_FUNC(Init_Scanner), // l-scan.c
    SYM_FUNC(Shutdown_Scanner), // l-scan.c
    SYM_FUNC(Scan_Word), // l-scan.c
    SYM_FUNC(Scan_Issue), // l-scan.c
    SYM_FUNC(Scan_Hex), // l-types.c
    SYM_FUNC(Scan_Hex2), // l-types.c
    SYM_FUNC(Scan_Hex_Bytes), // l-types.c
    SYM_FUNC(Scan_Hex_Value), // l-types.c
    SYM_FUNC(Scan_Dec_Buf), // l-types.c
    SYM_FUNC(Scan_Decimal), // l-types.c
    SYM_FUNC(Scan_Integer), // l-types.c
    SYM_FUNC(Scan_Money), // l-types.c
    SYM_FUNC(Scan_Date), // l-types.c
    SYM_FUNC(Scan_File), // l-types.c
    SYM_FUNC(Scan_Email), // l-types.c
    SYM_FUNC(Scan_URL), // l-types.c
    SYM_FUNC(Scan_Pair), // l-types.c
    SYM_FUNC(Scan_Tuple), // l-types.c
    SYM_FUNC(Scan_Binary), // l-types.c
    SYM_FUNC(Scan_Any), // l-types.c
    SYM_FUNC(Queue_Mark_Value_Deep), // m-gc.c
    SYM_FUNC(Recycle_Core), // m-gc.c
    SYM_FUNC(Recycle), // m-gc.c
    SYM_FUNC(Guard_Series_Core), // m-gc.c
    SYM_FUNC(Guard_Value_Core), // m-gc.c
    SYM_FUNC(Init_GC), // m-gc.c
    SYM_FUNC(Shutdown_GC), // m-gc.c
    SYM_FUNC(Alloc_Mem), // m-pools.c
    SYM_FUNC(Free_Mem), // m-pools.c
    SYM_FUNC(Init_Pools), // m-pools.c
    SYM_FUNC(Shutdown_Pools), // m-pools.c
    SYM_FUNC(Make_Node), // m-pools.c
    SYM_FUNC(Free_Node), // m-pools.c
    SYM_FUNC(Series_Allocation_Unpooled), // m-pools.c
    SYM_FUNC(Make_Series), // m-pools.c
    SYM_FUNC(Make_Pairing), // m-pools.c
    SYM_FUNC(Manage_Pairing), // m-pools.c
    SYM_FUNC(Free_Pairing), // m-pools.c
    SYM_FUNC(Swap_Underlying_Series_Data), // m-pools.c
    SYM_FUNC(Expand_Series), // m-pools.c
    SYM_FUNC(Remake_Series), // m-pools.c
    SYM_FUNC(GC_Kill_Series), // m-pools.c
    SYM_FUNC(Free_Series), // m-pools.c
    SYM_FUNC(Widen_String), // m-pools.c
    SYM_FUNC(Manage_Series), // m-pools.c
    SYM_FUNC(Is_Value_Managed), // m-pools.c
    SYM_FUNC(Free_Gob), // m-pools.c
    SYM_FUNC(Series_In_Pool), // m-pools.c
    SYM_FUNC(Extend_Series), // m-series.c
    SYM_FUNC(Insert_Series), // m-series.c
    SYM_FUNC(Append_Series), // m-series.c
    SYM_FUNC(Append_Values_Len), // m-series.c
    SYM_FUNC(Copy_Sequence), // m-series.c
    SYM_FUNC(Copy_Sequence_At_Len), // m-series.c
    SYM_FUNC(Copy_Sequence_At_Position), // m-series.c
    SYM_FUNC(Remove_Series), // m-series.c
    SYM_FUNC(Unbias_Series), // m-series.c
    SYM_FUNC(Reset_Series), // m-series.c
    SYM_FUNC(Reset_Array), // m-series.c
    SYM_FUNC(Clear_Series), // m-series.c
    SYM_FUNC(Resize_Series), // m-series.c
    SYM_FUNC(Reset_Buffer), // m-series.c
    SYM_FUNC(Copy_Buffer), // m-series.c
    SYM_FUNC(Init_Stacks), // m-stacks.c
    SYM_FUNC(Shutdown_Stacks), // m-stacks.c
    SYM_FUNC(Expand_Data_Stack_May_Fail), // m-stacks.c
    SYM_FUNC(Pop_Stack_Values), // m-stacks.c
    SYM_FUNC(Pop_Stack_Values_Reversed), // m-stacks.c
    SYM_FUNC(Pop_Stack_Values_Into), // m-stacks.c
    SYM_FUNC(Context_For_Frame_May_Reify_Core), // m-stacks.c
    SYM_FUNC(Context_For_Frame_May_Reify_Managed), // m-stacks.c
    SYM_FUNC(Protect_Value), // n-control.c
    SYM_FUNC(Protect_Series), // n-control.c
    SYM_FUNC(Protect_Object), // n-control.c
    SYM_FUNC(Make_Thrown_Exit_Value), // n-control.c
    SYM_FUNC(Brancher_Dispatcher), // n-control.c
    SYM_FUNC(Block_To_String_List), // n-io.c
    SYM_FUNC(Catching_Break_Or_Continue), // n-loop.c
    SYM_FUNC(Compare_Modify_Values), // n-math.c
    SYM_FUNC(Reduce_Any_Array_Throws), // n-reduce.c
    SYM_FUNC(Compose_Any_Array_Throws), // n-reduce.c
    SYM_FUNC(Init_Clipboard_Scheme), // p-clipboard.c
    SYM_FUNC(Init_Console_Scheme), // p-console.c
    SYM_FUNC(Init_Dir_Scheme), // p-dir.c
    SYM_FUNC(Init_DNS_Scheme), // p-dns.c
    SYM_FUNC(Append_Event), // p-event.c
    SYM_FUNC(Find_Last_Event), // p-event.c
    SYM_FUNC(Init_Event_Scheme), // p-event.c
    SYM_FUNC(Shutdown_Event_Scheme), // p-event.c
    SYM_FUNC(Ret_Query_File), // p-file.c
    SYM_FUNC(Init_File_Scheme), // p-file.c
    SYM_FUNC(Init_TCP_Scheme), // p-net.c
    SYM_FUNC(Init_UDP_Scheme), // p-net.c
    SYM_FUNC(Init_Serial_Scheme), // p-serial.c
#ifdef HAS_POSIX_SIGNAL
    SYM_FUNC(Init_Signal_Scheme), // p-signal.c
#endif
#ifdef HAS_TIMER
    SYM_FUNC(Init_Timer_Scheme), // p-timer.c
#endif
    SYM_FUNC(Init_Char_Cases), // s-cases.c
    SYM_FUNC(Shutdown_Char_Cases), // s-cases.c
    SYM_FUNC(Compute_CRC), // s-crc.c
    SYM_FUNC(Hash_Word), // s-crc.c
    SYM_FUNC(Hash_Value), // s-crc.c
    SYM_FUNC(Make_Hash_Sequence), // s-crc.c
    SYM_FUNC(Val_Init_Map), // s-crc.c
    SYM_FUNC(Hash_Block), // s-crc.c
    SYM_FUNC(Compute_IPC), // s-crc.c
    SYM_FUNC(CRC32), // s-crc.c
    SYM_FUNC(Hash_String), // s-crc.c
    SYM_FUNC(Init_CRC), // s-crc.c
    SYM_FUNC(Shutdown_CRC), // s-crc.c
    SYM_FUNC(To_REBOL_Path), // s-file.c
    SYM_FUNC(Value_To_REBOL_Path), // s-file.c
    SYM_FUNC(To_Local_Path), // s-file.c
    SYM_FUNC(Value_To_Local_Path), // s-file.c
    SYM_FUNC(Value_To_OS_Path), // s-file.c
    SYM_FUNC(Compare_Binary_Vals), // s-find.c
    SYM_FUNC(Compare_Bytes), // s-find.c
    SYM_FUNC(Match_Bytes), // s-find.c
    SYM_FUNC(Match_Sub_Path), // s-find.c
    SYM_FUNC(Compare_Uni_Byte), // s-find.c
    SYM_FUNC(Compare_Uni_Str), // s-find.c
    SYM_FUNC(Compare_String_Vals), // s-find.c
    SYM_FUNC(Compare_UTF8), // s-find.c
    SYM_FUNC(Find_Byte_Str), // s-find.c
    SYM_FUNC(Find_Str_Str), // s-find.c
    SYM_FUNC(Find_Str_Char), // s-find.c
    SYM_FUNC(Find_Str_Bitset), // s-find.c
    SYM_FUNC(Count_Lines), // s-find.c
    SYM_FUNC(Next_Line), // s-find.c
    SYM_FUNC(Make_Binary), // s-make.c
    SYM_FUNC(Make_Unicode), // s-make.c
    SYM_FUNC(Copy_Bytes), // s-make.c
    SYM_FUNC(Copy_Bytes_To_Unicode), // s-make.c
    SYM_FUNC(Copy_Wide_Str), // s-make.c
    SYM_FUNC(Copy_OS_Str), // s-make.c
    SYM_FUNC(Insert_Char), // s-make.c
    SYM_FUNC(Insert_String), // s-make.c
    SYM_FUNC(Copy_String_Slimming), // s-make.c
    SYM_FUNC(Val_Str_To_OS_Managed), // s-make.c
    SYM_FUNC(Append_Unencoded_Len), // s-make.c
    SYM_FUNC(Append_Unencoded), // s-make.c
    SYM_FUNC(Append_Codepoint_Raw), // s-make.c
    SYM_FUNC(Make_Series_Codepoint), // s-make.c
    SYM_FUNC(Append_Uni_Bytes), // s-make.c
    SYM_FUNC(Append_Uni_Uni), // s-make.c
    SYM_FUNC(Append_String), // s-make.c
    SYM_FUNC(Append_Boot_Str), // s-make.c
    SYM_FUNC(Append_Int), // s-make.c
    SYM_FUNC(Append_Int_Pad), // s-make.c
    SYM_FUNC(Append_UTF8_May_Fail), // s-make.c
    SYM_FUNC(Join_Binary), // s-make.c
    SYM_FUNC(Emit), // s-mold.c
    SYM_FUNC(Prep_String), // s-mold.c
    SYM_FUNC(Prep_Uni_Series), // s-mold.c
    SYM_FUNC(Pre_Mold), // s-mold.c
    SYM_FUNC(End_Mold), // s-mold.c
    SYM_FUNC(Post_Mold), // s-mold.c
    SYM_FUNC(New_Indented_Line), // s-mold.c
    SYM_FUNC(Mold_Binary), // s-mold.c
    SYM_FUNC(Mold_Array_At), // s-mold.c
    SYM_FUNC(Mold_Value), // s-mold.c
    SYM_FUNC(Copy_Form_Value), // s-mold.c
    SYM_FUNC(Copy_Mold_Value), // s-mold.c
    SYM_FUNC(Form_Reduce_Throws), // s-mold.c
    SYM_FUNC(Form_Tight_Block), // s-mold.c
    SYM_FUNC(Push_Mold), // s-mold.c
    SYM_FUNC(Throttle_Mold), // s-mold.c
    SYM_FUNC(Pop_Molded_String_Core), // s-mold.c
    SYM_FUNC(Pop_Molded_UTF8), // s-mold.c
    SYM_FUNC(Drop_Mold_Core), // s-mold.c
    SYM_FUNC(Init_Mold), // s-mold.c
    SYM_FUNC(Shutdown_Mold), // s-mold.c
    SYM_FUNC(All_Bytes_ASCII), // s-ops.c
    SYM_FUNC(Is_Wide), // s-ops.c
    SYM_FUNC(Temp_Byte_Chars_May_Fail), // s-ops.c
    SYM_FUNC(Temp_Bin_Str_Managed), // s-ops.c
    SYM_FUNC(Xandor_Binary), // s-ops.c
    SYM_FUNC(Complement_Binary), // s-ops.c
    SYM_FUNC(Shuffle_String), // s-ops.c
    SYM_FUNC(Cloak), // s-ops.c
    SYM_FUNC(Trim_Tail), // s-ops.c
    SYM_FUNC(Deline_Bytes), // s-ops.c
    SYM_FUNC(Deline_Uni), // s-ops.c
    SYM_FUNC(Enline_Bytes), // s-ops.c
    SYM_FUNC(Enline_Uni), // s-ops.c
    SYM_FUNC(Entab_Bytes), // s-ops.c
    SYM_FUNC(Entab_Unicode), // s-ops.c
    SYM_FUNC(Detab_Bytes), // s-ops.c
    SYM_FUNC(Detab_Unicode), // s-ops.c
    SYM_FUNC(Change_Case), // s-ops.c
    SYM_FUNC(Split_Lines), // s-ops.c
    SYM_FUNC(Trim_String), // s-trim.c
    SYM_FUNC(What_UTF), // s-unicode.c
    SYM_FUNC(Legal_UTF8_Char), // s-unicode.c
    SYM_FUNC(Check_UTF8), // s-unicode.c
    SYM_FUNC(Back_Scan_UTF8_Char), // s-unicode.c
    SYM_FUNC(Decode_UTF8_Negative_If_Latin1), // s-unicode.c
    SYM_FUNC(Decode_UTF16), // s-unicode.c
    SYM_FUNC(Decode_UTF_String), // s-unicode.c
    SYM_FUNC(Length_As_UTF8), // s-unicode.c
    SYM_FUNC(Encode_UTF8_Char), // s-unicode.c
    SYM_FUNC(Encode_UTF8), // s-unicode.c
    SYM_FUNC(Encode_UTF8_Line), // s-unicode.c
    SYM_FUNC(Make_UTF8_Binary), // s-unicode.c
    SYM_FUNC(Make_UTF8_From_Any_String), // s-unicode.c
    SYM_FUNC(Strlen_Uni), // s-unicode.c
    SYM_FUNC(CT_Bitset), // t-bitset.c
    SYM_FUNC(Make_Bitset), // t-bitset.c
    SYM_FUNC(Mold_Bitset), // t-bitset.c
    SYM_FUNC(MAKE_Bitset), // t-bitset.c
    SYM_FUNC(TO_Bitset), // t-bitset.c
    SYM_FUNC(Find_Max_Bit), // t-bitset.c
    SYM_FUNC(Check_Bit), // t-bitset.c
    SYM_FUNC(Check_Bit_Str), // t-bitset.c
    SYM_FUNC(Set_Bit), // t-bitset.c
    SYM_FUNC(Set_Bit_Str), // t-bitset.c
    SYM_FUNC(Set_Bits), // t-bitset.c
    SYM_FUNC(Check_Bits), // t-bitset.c
    SYM_FUNC(PD_Bitset), // t-bitset.c
    SYM_FUNC(Trim_Tail_Zeros), // t-bitset.c
    SYM_FUNC(CT_Array), // t-block.c
    SYM_FUNC(MAKE_Array), // t-block.c
    SYM_FUNC(TO_Array), // t-block.c
    SYM_FUNC(Find_In_Array), // t-block.c
    SYM_FUNC(Shuffle_Block), // t-block.c
    SYM_FUNC(PD_Array), // t-block.c
    SYM_FUNC(Pick_Block), // t-block.c
    SYM_FUNC(CT_Char), // t-char.c
    SYM_FUNC(MAKE_Char), // t-char.c
    SYM_FUNC(TO_Char), // t-char.c
    SYM_FUNC(CT_Datatype), // t-datatype.c
    SYM_FUNC(MAKE_Datatype), // t-datatype.c
    SYM_FUNC(TO_Datatype), // t-datatype.c
    SYM_FUNC(Set_Date_UTC), // t-date.c
    SYM_FUNC(Set_Date), // t-date.c
    SYM_FUNC(CT_Date), // t-date.c
    SYM_FUNC(Emit_Date), // t-date.c
    SYM_FUNC(Julian_Date), // t-date.c
    SYM_FUNC(Diff_Date), // t-date.c
    SYM_FUNC(Week_Day), // t-date.c
    SYM_FUNC(Normalize_Time), // t-date.c
    SYM_FUNC(Adjust_Date_Zone), // t-date.c
    SYM_FUNC(Subtract_Date), // t-date.c
    SYM_FUNC(Cmp_Date), // t-date.c
    SYM_FUNC(MAKE_Date), // t-date.c
    SYM_FUNC(TO_Date), // t-date.c
    SYM_FUNC(PD_Date), // t-date.c
    SYM_FUNC(MAKE_Decimal), // t-decimal.c
    SYM_FUNC(TO_Decimal), // t-decimal.c
    SYM_FUNC(Eq_Decimal), // t-decimal.c
    SYM_FUNC(Eq_Decimal2), // t-decimal.c
    SYM_FUNC(CT_Decimal), // t-decimal.c
    SYM_FUNC(CT_Event), // t-event.c
    SYM_FUNC(Cmp_Event), // t-event.c
    SYM_FUNC(Set_Event_Vars), // t-event.c
    SYM_FUNC(MAKE_Event), // t-event.c
    SYM_FUNC(TO_Event), // t-event.c
    SYM_FUNC(PD_Event), // t-event.c
    SYM_FUNC(Mold_Event), // t-event.c
    SYM_FUNC(CT_Function), // t-function.c
    SYM_FUNC(MAKE_Function), // t-function.c
    SYM_FUNC(TO_Function), // t-function.c
    SYM_FUNC(CT_Gob), // t-gob.c
    SYM_FUNC(Make_Gob), // t-gob.c
    SYM_FUNC(Cmp_Gob), // t-gob.c
    SYM_FUNC(Gob_To_Array), // t-gob.c
    SYM_FUNC(Extend_Gob_Core), // t-gob.c
    SYM_FUNC(MAKE_Gob), // t-gob.c
    SYM_FUNC(TO_Gob), // t-gob.c
    SYM_FUNC(PD_Gob), // t-gob.c
    SYM_FUNC(CT_Image), // t-image.c
    SYM_FUNC(MAKE_Image), // t-image.c
    SYM_FUNC(TO_Image), // t-image.c
    SYM_FUNC(Reset_Height), // t-image.c
    SYM_FUNC(Set_Pixel_Tuple), // t-image.c
    SYM_FUNC(Set_Tuple_Pixel), // t-image.c
    SYM_FUNC(Fill_Line), // t-image.c
    SYM_FUNC(Fill_Rect), // t-image.c
    SYM_FUNC(Fill_Alpha_Line), // t-image.c
    SYM_FUNC(Fill_Alpha_Rect), // t-image.c
    SYM_FUNC(Find_Color), // t-image.c
    SYM_FUNC(Find_Alpha), // t-image.c
    SYM_FUNC(RGB_To_Bin), // t-image.c
    SYM_FUNC(Bin_To_RGB), // t-image.c
    SYM_FUNC(Bin_To_RGBA), // t-image.c
    SYM_FUNC(Alpha_To_Bin), // t-image.c
    SYM_FUNC(Bin_To_Alpha), // t-image.c
    SYM_FUNC(Array_Has_Non_Tuple), // t-image.c
    SYM_FUNC(Tuples_To_RGBA), // t-image.c
    SYM_FUNC(Image_To_RGBA), // t-image.c
    SYM_FUNC(Mold_Image_Data), // t-image.c
    SYM_FUNC(Make_Image_Binary), // t-image.c
    SYM_FUNC(Make_Image), // t-image.c
    SYM_FUNC(Clear_Image), // t-image.c
    SYM_FUNC(Modify_Image), // t-image.c
    SYM_FUNC(Find_Image), // t-image.c
    SYM_FUNC(Image_Has_Alpha), // t-image.c
    SYM_FUNC(Copy_Rect_Data), // t-image.c
    SYM_FUNC(PD_Image), // t-image.c
    SYM_FUNC(CT_Integer), // t-integer.c
    SYM_FUNC(MAKE_Integer), // t-integer.c
    SYM_FUNC(TO_Integer), // t-integer.c
    SYM_FUNC(Value_To_Int64), // t-integer.c
    SYM_FUNC(CT_Library), // t-library.c
    SYM_FUNC(MAKE_Library), // t-library.c
    SYM_FUNC(TO_Library), // t-library.c
    SYM_FUNC(CT_Logic), // t-logic.c
    SYM_FUNC(MAKE_Logic), // t-logic.c
    SYM_FUNC(TO_Logic), // t-logic.c
    SYM_FUNC(CT_Map), // t-map.c
    SYM_FUNC(Find_Key_Hashed), // t-map.c
    SYM_FUNC(Expand_Hash), // t-map.c
    SYM_FUNC(Length_Map), // t-map.c
    SYM_FUNC(PD_Map), // t-map.c
    SYM_FUNC(MAKE_Map), // t-map.c
    SYM_FUNC(TO_Map), // t-map.c
    SYM_FUNC(Map_To_Array), // t-map.c
    SYM_FUNC(Mutate_Array_Into_Map), // t-map.c
    SYM_FUNC(Alloc_Context_From_Map), // t-map.c
    SYM_FUNC(CT_Money), // t-money.c
    SYM_FUNC(MAKE_Money), // t-money.c
    SYM_FUNC(TO_Money), // t-money.c
    SYM_FUNC(Emit_Money), // t-money.c
    SYM_FUNC(Bin_To_Money_May_Fail), // t-money.c
    SYM_FUNC(CT_Unit), // t-none.c
    SYM_FUNC(MAKE_Unit), // t-none.c
    SYM_FUNC(TO_Unit), // t-none.c
    SYM_FUNC(CT_Context), // t-object.c
    SYM_FUNC(MAKE_Context), // t-object.c
    SYM_FUNC(TO_Context), // t-object.c
    SYM_FUNC(PD_Context), // t-object.c
    SYM_FUNC(CT_Pair), // t-pair.c
    SYM_FUNC(MAKE_Pair), // t-pair.c
    SYM_FUNC(TO_Pair), // t-pair.c
    SYM_FUNC(Cmp_Pair), // t-pair.c
    SYM_FUNC(Min_Max_Pair), // t-pair.c
    SYM_FUNC(PD_Pair), // t-pair.c
    SYM_FUNC(CT_Port), // t-port.c
    SYM_FUNC(MAKE_Port), // t-port.c
    SYM_FUNC(TO_Port), // t-port.c
    SYM_FUNC(Routine_Dispatcher), // t-routine.c
    SYM_FUNC(Free_Routine), // t-routine.c
    SYM_FUNC(Alloc_Ffi_Function_For_Spec), // t-routine.c
    SYM_FUNC(CT_String), // t-string.c
    SYM_FUNC(MAKE_String), // t-string.c
    SYM_FUNC(TO_String), // t-string.c
    SYM_FUNC(PD_String), // t-string.c
    SYM_FUNC(PD_File), // t-string.c
    SYM_FUNC(Get_FFType_Enum_Info_Core), // t-struct.c
    SYM_FUNC(Struct_To_Array), // t-struct.c
    SYM_FUNC(Init_Struct_Fields), // t-struct.c
    SYM_FUNC(MAKE_Struct), // t-struct.c
    SYM_FUNC(TO_Struct), // t-struct.c
    SYM_FUNC(PD_Struct), // t-struct.c
    SYM_FUNC(Cmp_Struct), // t-struct.c
    SYM_FUNC(CT_Struct), // t-struct.c
    SYM_FUNC(Copy_Struct_Managed), // t-struct.c
    SYM_FUNC(Split_Time), // t-time.c
    SYM_FUNC(Join_Time), // t-time.c
    SYM_FUNC(Scan_Time), // t-time.c
    SYM_FUNC(Emit_Time), // t-time.c
    SYM_FUNC(CT_Time), // t-time.c
    SYM_FUNC(Make_Time), // t-time.c
    SYM_FUNC(MAKE_Time), // t-time.c
    SYM_FUNC(TO_Time), // t-time.c
    SYM_FUNC(Cmp_Time), // t-time.c
    SYM_FUNC(PD_Time), // t-time.c
    SYM_FUNC(CT_Tuple), // t-tuple.c
    SYM_FUNC(MAKE_Tuple), // t-tuple.c
    SYM_FUNC(TO_Tuple), // t-tuple.c
    SYM_FUNC(Cmp_Tuple), // t-tuple.c
    SYM_FUNC(PD_Tuple), // t-tuple.c
    SYM_FUNC(Emit_Tuple), // t-tuple.c
    SYM_FUNC(CT_Typeset), // t-typeset.c
    SYM_FUNC(Init_Typesets), // t-typeset.c
    SYM_FUNC(Val_Init_Typeset), // t-typeset.c
    SYM_FUNC(Update_Typeset_Bits_Core), // t-typeset.c
    SYM_FUNC(MAKE_Typeset), // t-typeset.c
    SYM_FUNC(TO_Typeset), // t-typeset.c
    SYM_FUNC(Typeset_To_Array), // t-typeset.c
    SYM_FUNC(Do_Vararg_Op_May_Throw), // t-varargs.c
    SYM_FUNC(MAKE_Varargs), // t-varargs.c
    SYM_FUNC(TO_Varargs), // t-varargs.c
    SYM_FUNC(CT_Varargs), // t-varargs.c
    SYM_FUNC(Mold_Varargs), // t-varargs.c
    SYM_FUNC(Vector_To_Array), // t-vector.c
    SYM_FUNC(Compare_Vector), // t-vector.c
    SYM_FUNC(Shuffle_Vector), // t-vector.c
    SYM_FUNC(Set_Vector_Value), // t-vector.c
    SYM_FUNC(Make_Vector), // t-vector.c
    SYM_FUNC(Make_Vector_Spec), // t-vector.c
    SYM_FUNC(MAKE_Vector), // t-vector.c
    SYM_FUNC(TO_Vector), // t-vector.c
    SYM_FUNC(CT_Vector), // t-vector.c
    SYM_FUNC(PD_Vector), // t-vector.c
    SYM_FUNC(Mold_Vector), // t-vector.c
    SYM_FUNC(CT_Word), // t-word.c
    SYM_FUNC(MAKE_Word), // t-word.c
    SYM_FUNC(TO_Word), // t-word.c
    SYM_FUNC(Codec_BMP_Image), // u-bmp.c
    SYM_FUNC(Init_BMP_Codec), // u-bmp.c
    SYM_FUNC(Compress), // u-compress.c
    SYM_FUNC(Decompress), // u-compress.c
    SYM_FUNC(Find_Mutable_In_Contexts), // u-dialect.c
    SYM_FUNC(Do_Dialect), // u-dialect.c
    SYM_FUNC(Trace_Delect), // u-dialect.c
    SYM_FUNC(Decode_LZW), // u-gif.c
    SYM_FUNC(Decode_GIF_Image), // u-gif.c
    SYM_FUNC(Codec_GIF_Image), // u-gif.c
    SYM_FUNC(Init_GIF_Codec), // u-gif.c
    SYM_FUNC(Codec_JPEG_Image), // u-jpg.c
    SYM_FUNC(Init_JPEG_Codec), // u-jpg.c
    SYM_FUNC(Encode_PNG_Image), // u-png.c
    SYM_FUNC(Decode_PNG_Image), // u-png.c
    SYM_FUNC(Codec_PNG_Image), // u-png.c
    SYM_FUNC(Init_PNG_Codec), // u-png.c
#ifdef HAS_MD4
    SYM_FUNC(MD4), // u-md4.c ??
#endif
#ifdef HAS_MD5
    SYM_FUNC(MD5), // u-md5.c
#endif
#ifdef HAS_SHA1
    SYM_FUNC(SHA1), // u-sha1.c
#endif

#if !defined(NDEBUG)
    SYM_FUNC(Assert_State_Balanced_Debug), // c-error.c
    SYM_FUNC(Assert_Context_Core), // c-frame.c
    SYM_FUNC(Assert_No_Relative), // c-value.c
    SYM_FUNC(Panic_Value_Debug), // c-value.c
    SYM_FUNC(SET_END_Debug), // c-value.c
    SYM_FUNC(IS_END_Debug), // c-value.c
    SYM_FUNC(VAL_SPECIFIC_Debug), // c-value.c
    SYM_FUNC(INIT_WORD_INDEX_Debug), // c-value.c
    SYM_FUNC(IS_RELATIVE_Debug), // c-value.c
    SYM_FUNC(Probe_Core_Debug), // c-value.c
    SYM_FUNC(Do_Core_Entry_Checks_Debug), // d-eval.c
    SYM_FUNC(Do_Core_Expression_Checks_Debug), // d-eval.c
    SYM_FUNC(Do_Core_Exit_Checks_Debug), // d-eval.c
    SYM_FUNC(In_Legacy_Function_Debug), // d-legacy.c
    SYM_FUNC(Debug_String), // d-print.c
    SYM_FUNC(Debug_Line), // d-print.c
    SYM_FUNC(Debug_Str), // d-print.c
    SYM_FUNC(Debug_Uni), // d-print.c
    SYM_FUNC(Debug_Series), // d-print.c
    SYM_FUNC(Debug_Num), // d-print.c
    SYM_FUNC(Debug_Chars), // d-print.c
    SYM_FUNC(Debug_Space), // d-print.c
    SYM_FUNC(Debug_Word), // d-print.c
    SYM_FUNC(Debug_Type), // d-print.c
    SYM_FUNC(Debug_Value), // d-print.c
    SYM_FUNC(Debug_Values), // d-print.c
    SYM_FUNC(Debug_Buf), // d-print.c
    SYM_FUNC(Debug_Fmt_), // d-print.c
    SYM_FUNC(Debug_Fmt), // d-print.c
    SYM_FUNC(Dump_Series), // d-dump.c
    SYM_FUNC(Dump_Bytes), // d-dump.c
    SYM_FUNC(Dump_Values), // d-dump.c
    SYM_FUNC(Dump_Info), // d-dump.c
    SYM_FUNC(Dump_Stack), // d-dump.c
    SYM_FUNC(Dump_Frame_Location), // d-eval.c
    SYM_FUNC(Legacy_Convert_Function_Args), // d-legacy.c
    SYM_FUNC(Make_Guarded_Arg123_Error), // d-legacy.c
    SYM_FUNC(Trace_Fetch_Debug), // d-trace.c
    SYM_FUNC(Try_Find_Containing_Series_Debug), // m-pools.c
    SYM_FUNC(Check_Memory), // m-pools.c
    SYM_FUNC(Dump_All), // m-pools.c
    SYM_FUNC(Dump_Series_In_Pool), // m-pools.c
    SYM_FUNC(Inspect_Series), // m-pools.c
    SYM_FUNC(Panic_Series_Debug), // m-series.c
    SYM_FUNC(Assert_Series_Term_Core), // m-series.c
    SYM_FUNC(Assert_Series_Core), // m-series.c
    SYM_FUNC(Underlying_Function_Debug), // m-stacks.c
    SYM_FUNC(COMPARE_BYTES_), // n-strings.c
    SYM_FUNC(APPEND_BYTES_LIMIT_), // n-strings.c
    SYM_FUNC(b_cast_), // n-strings.c
    SYM_FUNC(cb_cast_), // n-strings.c
    SYM_FUNC(s_cast_), // n-strings.c
    SYM_FUNC(cs_cast_), // n-strings.c
    SYM_FUNC(COPY_BYTES_), // n-strings.c
    SYM_FUNC(LEN_BYTES_), // n-strings.c
    SYM_FUNC(OS_STRNCPY_), // n-strings.c
    SYM_FUNC(OS_STRNCAT_), // n-strings.c
    SYM_FUNC(OS_STRNCMP_), // n-strings.c
    SYM_FUNC(OS_STRLEN_), // n-strings.c
    SYM_FUNC(OS_STRCHR_), // n-strings.c
    SYM_FUNC(OS_MAKE_CH_), // n-strings.c
    SYM_FUNC(Assert_Array_Core), // t-block.c
#endif

//Globals from sys-globals.h
    SYM_DATA(PG_Boot_Phase),
    SYM_DATA(PG_Boot_Level),
    SYM_DATA(PG_Boot_Strs),

#if !defined(NDEBUG)
    SYM_DATA(PG_Reb_Stats),
#endif

    SYM_DATA(PG_Mem_Usage),
    SYM_DATA(PG_Mem_Limit),

    SYM_DATA(PG_Symbol_Canons),
    SYM_DATA(PG_Canons_By_Hash),
    SYM_DATA(PG_Num_Canon_Slots_In_Use),
#if !defined(NDEBUG)
    SYM_DATA(PG_Num_Canon_Deleteds),
#endif

    SYM_DATA(PG_Root_Context),
    SYM_DATA(Root_Vars),

    SYM_DATA(Lib_Context),
    SYM_DATA(Sys_Context),

    SYM_DATA(White_Chars),
    SYM_DATA(Upper_Cases),
    SYM_DATA(Lower_Cases),

    SYM_DATA(PG_Pool_Map),

    SYM_DATA(PG_Boot_Time),
    SYM_DATA(Current_Year),
    SYM_DATA(Reb_Opts),

#ifndef NDEBUG
    SYM_DATA(PG_Always_Malloc),
#endif

    SYM_DATA(PG_End_Cell),
    SYM_DATA(PG_Void_Cell),

    SYM_DATA(PG_Blank_Value),
    SYM_DATA(PG_Bar_Value),
    SYM_DATA(PG_False_Value),
    SYM_DATA(PG_True_Value),

    SYM_DATA(PG_Va_List_Pending),
    SYM_DATA(Eval_Signals),

    SYM_DATA(PG_Breakpoint_Quitting_Hook),


    /***********************************************************************
     **
     **  Thread Globals - Local to each thread
     **
     ***********************************************************************/

    SYM_DATA(TG_Task_Context),
    SYM_DATA(Task_Vars),

    SYM_DATA(TG_Thrown_Arg),

    SYM_DATA(Mem_Pools),
    SYM_DATA(GC_Disabled),
    SYM_DATA(GC_Ballast),
    SYM_DATA(GC_Active),
    SYM_DATA(GC_Series_Guard),
    SYM_DATA(GC_Value_Guard),
    SYM_DATA(GC_Mark_Stack),
    SYM_DATA(Prior_Expand),

    SYM_DATA(GC_Mark_Hook),

    SYM_DATA(GC_Manuals),

    SYM_DATA(Stack_Limit),

#if !defined(NDEBUG)
    // This counter is incremented each time through the DO loop, &and can be
    // used for many purposes...including setting breakpoints in routines
    // other than Do_Next that are contingent on a certain "tick" elapsing.
    //
    SYM_DATA(TG_Do_Count),
#endif

    SYM_DATA(TG_Frame_Stack),

    SYM_DATA(DS_Array),
    SYM_DATA(DS_Index),
    SYM_DATA(DS_Movable_Base),

    SYM_DATA(TG_Top_Chunk),
    SYM_DATA(TG_Head_Chunk),
    SYM_DATA(TG_Root_Chunker),

    SYM_DATA(Saved_State),

#if !defined(NDEBUG)
    // In debug builds, &the `panic` and `fail` macros capture the file and
    // line number of instantiation so any Make_Error can pick it up.
    SYM_DATA(TG_Erroring_C_File),
    SYM_DATA(TG_Erroring_C_Line),

    SYM_DATA(TG_Pushing_Mold),
#endif

    SYM_DATA(TG_Command_Execution_Context),

    SYM_DATA(Eval_Cycles),
    SYM_DATA(Eval_Limit),
    SYM_DATA(Eval_Count),
    SYM_DATA(Eval_Dose),
    SYM_DATA(Eval_Sigmask),

    SYM_DATA(Trace_Flags),
    SYM_DATA(Trace_Level),
    SYM_DATA(Trace_Depth),
    SYM_DATA(Trace_Limit),
    SYM_DATA(Trace_Buffer),

    SYM_DATA(Eval_Functions),

    SYM_DATA(Callback_Error),
    NULL, NULL //Terminator
};
