#ifndef R3_NVTX_H
#define R3_NVTX_H

#ifdef WITH_NVTX
#include <nvToolsExt.h>
#define NVTX_MARK_START(x) nvtxRangePushA(x)
#define NVTX_MARK_END() nvtxRangePop() 
#else
#define NVTX_MARK_START(x) 
#define NVTX_MARK_END() 
#endif
#define NVTX_MARK_FUNC_START() NVTX_MARK_START(__FUNCTION__);
#define NVTX_MARK_FUNC_END() NVTX_MARK_END()

#endif