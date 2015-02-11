LOCAL_PATH := $(call my-dir)

MY_RAPI_FLAGS=-DTO_ANDROID -DMIN_OS -DENDIAN_LITTLE -ffloat-store 

MY_CORE_FLAGS=-DREB_CORE -DREB_EXE -DCUSTOM_STARTUP $(MY_RAPI_FLAGS)
#MY_VIEW_FLAGS=-DREB_EXE -DCUSTOM_STARTUP $(MY_RAPI_FLAGS)
MY_VIEW_FLAGS=-DREB_EXE -DCUSTOM_STARTUP $(MY_RAPI_FLAGS)

include $(CLEAR_VARS)

LOCAL_MODULE=libffi

MY_LIBFFI_ROOT = ../../../src/libffi
LOCAL_SRC_FILES=\
	$(MY_LIBFFI_ROOT)/src/debug.c \
	$(MY_LIBFFI_ROOT)/src/closures.c \
	$(MY_LIBFFI_ROOT)/src/java_raw_api.c \
	$(MY_LIBFFI_ROOT)/src/prep_cif.c \
	$(MY_LIBFFI_ROOT)/src/raw_api.c \
	$(MY_LIBFFI_ROOT)/src/types.c
ifeq ($(TARGET_ARCH),arm)
	LOCAL_SRC_FILES += \
		$(MY_LIBFFI_ROOT)/src/arm/sysv.S \
		$(MY_LIBFFI_ROOT)/src/arm/ffi.c
	LOCAL_C_INCLUDES := $(LOCAL_PATH)/include/arm
else
	ifeq ($(TARGET_ARCH),x86)
		LOCAL_SRC_FILES += \
			$(MY_LIBFFI_ROOT)/src/x86/sysv.S \
			$(MY_LIBFFI_ROOT)/src/x86/win32.S \
			$(MY_LIBFFI_ROOT)/src/x86/ffi.c
		LOCAL_C_INCLUDES := $(LOCAL_PATH)/include/x86
	endif
endif

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
include $(BUILD_STATIC_LIBRARY)
#include $(BUILD_SHARED_LIBRARY)

#libr3
include $(CLEAR_VARS)

LOCAL_MODULE=libr3

MY_LIBR3_ROOT = ../../../src
LOCAL_SRC_FILES = \
	$(MY_LIBR3_ROOT)/core/a-constants.c \
	$(MY_LIBR3_ROOT)/core/a-globals.c \
	$(MY_LIBR3_ROOT)/core/a-lib.c \
	$(MY_LIBR3_ROOT)/core/b-boot.c \
	$(MY_LIBR3_ROOT)/core/b-init.c \
	$(MY_LIBR3_ROOT)/core/c-do.c \
	$(MY_LIBR3_ROOT)/core/c-error.c \
	$(MY_LIBR3_ROOT)/core/c-frame.c \
	$(MY_LIBR3_ROOT)/core/c-function.c \
	$(MY_LIBR3_ROOT)/core/c-port.c \
	$(MY_LIBR3_ROOT)/core/c-task.c \
	$(MY_LIBR3_ROOT)/core/c-word.c \
	$(MY_LIBR3_ROOT)/core/d-crash.c \
	$(MY_LIBR3_ROOT)/core/d-dump.c \
	$(MY_LIBR3_ROOT)/core/d-print.c \
	$(MY_LIBR3_ROOT)/core/f-blocks.c \
	$(MY_LIBR3_ROOT)/core/f-deci.c \
	$(MY_LIBR3_ROOT)/core/f-int.c \
	$(MY_LIBR3_ROOT)/core/f-dtoa.c \
	$(MY_LIBR3_ROOT)/core/f-enbase.c \
	$(MY_LIBR3_ROOT)/core/f-extension.c \
	$(MY_LIBR3_ROOT)/core/f-math.c \
	$(MY_LIBR3_ROOT)/core/f-qsort.c \
	$(MY_LIBR3_ROOT)/core/f-modify.c \
	$(MY_LIBR3_ROOT)/core/f-random.c \
	$(MY_LIBR3_ROOT)/core/f-round.c \
	$(MY_LIBR3_ROOT)/core/f-series.c \
	$(MY_LIBR3_ROOT)/core/f-stubs.c \
	$(MY_LIBR3_ROOT)/core/l-scan.c \
	$(MY_LIBR3_ROOT)/core/l-types.c \
	$(MY_LIBR3_ROOT)/core/m-gc.c \
	$(MY_LIBR3_ROOT)/core/m-pools.c \
	$(MY_LIBR3_ROOT)/core/m-series.c \
	$(MY_LIBR3_ROOT)/core/n-control.c \
	$(MY_LIBR3_ROOT)/core/n-data.c \
	$(MY_LIBR3_ROOT)/core/n-io.c \
	$(MY_LIBR3_ROOT)/core/n-loop.c \
	$(MY_LIBR3_ROOT)/core/n-math.c \
	$(MY_LIBR3_ROOT)/core/n-sets.c \
	$(MY_LIBR3_ROOT)/core/n-strings.c \
	$(MY_LIBR3_ROOT)/core/n-system.c \
	$(MY_LIBR3_ROOT)/core/p-console.c \
	$(MY_LIBR3_ROOT)/core/p-dir.c \
	$(MY_LIBR3_ROOT)/core/p-dns.c \
	$(MY_LIBR3_ROOT)/core/p-event.c \
	$(MY_LIBR3_ROOT)/core/p-file.c \
	$(MY_LIBR3_ROOT)/core/p-net.c \
	$(MY_LIBR3_ROOT)/core/p-serial.c \
	$(MY_LIBR3_ROOT)/core/s-cases.c \
	$(MY_LIBR3_ROOT)/core/s-crc.c \
	$(MY_LIBR3_ROOT)/core/s-file.c \
	$(MY_LIBR3_ROOT)/core/s-find.c \
	$(MY_LIBR3_ROOT)/core/s-make.c \
	$(MY_LIBR3_ROOT)/core/s-mold.c \
	$(MY_LIBR3_ROOT)/core/s-ops.c \
	$(MY_LIBR3_ROOT)/core/s-trim.c \
	$(MY_LIBR3_ROOT)/core/s-unicode.c \
	$(MY_LIBR3_ROOT)/core/t-bitset.c \
	$(MY_LIBR3_ROOT)/core/t-block.c \
	$(MY_LIBR3_ROOT)/core/t-char.c \
	$(MY_LIBR3_ROOT)/core/t-datatype.c \
	$(MY_LIBR3_ROOT)/core/t-date.c \
	$(MY_LIBR3_ROOT)/core/t-decimal.c \
	$(MY_LIBR3_ROOT)/core/t-event.c \
	$(MY_LIBR3_ROOT)/core/t-function.c \
	$(MY_LIBR3_ROOT)/core/t-gob.c \
	$(MY_LIBR3_ROOT)/core/t-struct.c \
	$(MY_LIBR3_ROOT)/core/t-library.c \
	$(MY_LIBR3_ROOT)/core/t-routine.c \
	$(MY_LIBR3_ROOT)/core/t-image.c \
	$(MY_LIBR3_ROOT)/core/t-integer.c \
	$(MY_LIBR3_ROOT)/core/t-logic.c \
	$(MY_LIBR3_ROOT)/core/t-map.c \
	$(MY_LIBR3_ROOT)/core/t-money.c \
	$(MY_LIBR3_ROOT)/core/t-none.c \
	$(MY_LIBR3_ROOT)/core/t-object.c \
	$(MY_LIBR3_ROOT)/core/t-pair.c \
	$(MY_LIBR3_ROOT)/core/t-port.c \
	$(MY_LIBR3_ROOT)/core/t-string.c \
	$(MY_LIBR3_ROOT)/core/t-time.c \
	$(MY_LIBR3_ROOT)/core/t-tuple.c \
	$(MY_LIBR3_ROOT)/core/t-typeset.c \
	$(MY_LIBR3_ROOT)/core/t-utype.c \
	$(MY_LIBR3_ROOT)/core/t-vector.c \
	$(MY_LIBR3_ROOT)/core/t-word.c \
	$(MY_LIBR3_ROOT)/core/u-bmp.c \
	$(MY_LIBR3_ROOT)/core/u-compress.c \
	$(MY_LIBR3_ROOT)/core/u-dialect.c \
	$(MY_LIBR3_ROOT)/core/u-gif.c \
	$(MY_LIBR3_ROOT)/core/u-jpg.c \
	$(MY_LIBR3_ROOT)/core/u-md5.c \
	$(MY_LIBR3_ROOT)/core/u-parse.c \
	$(MY_LIBR3_ROOT)/core/u-png.c \
	$(MY_LIBR3_ROOT)/core/u-sha1.c \
	$(MY_LIBR3_ROOT)/core/u-zlib.c \
	$(MY_LIBR3_ROOT)/codecs/aes/aes.c \
	$(MY_LIBR3_ROOT)/codecs/bigint/bigint.c \
	$(MY_LIBR3_ROOT)/codecs/dh/dh.c \
	$(MY_LIBR3_ROOT)/codecs/png/lodepng.c \
	$(MY_LIBR3_ROOT)/codecs/rc4/rc4.c \
	$(MY_LIBR3_ROOT)/codecs/rsa/rsa.c


LOCAL_CFLAGS=$(MY_RAPI_FLAGS)

LOCAL_C_INCLUDES := jni/$(MY_LIBR3_ROOT)/include jni/$(MY_LIBR3_ROOT)/codecs
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
#LOCAL_SHARED_LIBRARIES = ffi
LOCAL_STATIC_LIBRARIES = libffi
include $(BUILD_STATIC_LIBRARY)
#include $(BUILD_SHARED_LIBRARY)

#r3
include $(CLEAR_VARS)

LOCAL_MODULE=r3-core

MY_R3_ROOT = ../../../src
MY_CORE_SRC_FILES=\
	$(MY_R3_ROOT)/os/host-main.c \
	$(MY_R3_ROOT)/os/host-args.c \
	$(MY_R3_ROOT)/os/host-device.c \
	$(MY_R3_ROOT)/os/host-stdio.c \
	$(MY_R3_ROOT)/os/host-core.c \
	$(MY_R3_ROOT)/os/dev-dns.c \
	$(MY_R3_ROOT)/os/dev-net.c \
	$(MY_R3_ROOT)/os/linux/host-lib.c \
	$(MY_R3_ROOT)/os/linux/host-readline.c \
	$(MY_R3_ROOT)/os/linux/iso-639.c \
	$(MY_R3_ROOT)/os/linux/iso-3166.c \
	$(MY_R3_ROOT)/os/linux/dev-event.c \
	$(MY_R3_ROOT)/os/linux/dev-file.c \
	$(MY_R3_ROOT)/os/linux/dev-serial.c \
	$(MY_R3_ROOT)/os/linux/dev-stdio.c \

	#$(MY_R3_ROOT)/os/linux/dev-signal.c

LOCAL_SRC_FILES = $(MY_CORE_SRC_FILES)

LOCAL_CFLAGS=$(MY_CORE_FLAGS)

LOCAL_STATIC_LIBRARIES = libffi libr3
include $(BUILD_EXECUTABLE)

#FreeType2
include $(CLEAR_VARS)
LOCAL_MODULE=freetype2
MY_FREETYPE_ROOT = ../../../src/freetype2

LOCAL_SRC_FILES = \
	$(MY_FREETYPE_ROOT)/src/autofit/autofit.c \
	$(MY_FREETYPE_ROOT)/src/base/basepic.c \
	$(MY_FREETYPE_ROOT)/src/base/ftapi.c \
	$(MY_FREETYPE_ROOT)/src/base/ftbase.c \
	$(MY_FREETYPE_ROOT)/src/base/ftbbox.c \
	$(MY_FREETYPE_ROOT)/src/base/ftbitmap.c \
	$(MY_FREETYPE_ROOT)/src/base/ftdbgmem.c \
	$(MY_FREETYPE_ROOT)/src/base/ftdebug.c \
	$(MY_FREETYPE_ROOT)/src/base/ftglyph.c \
	$(MY_FREETYPE_ROOT)/src/base/ftinit.c \
	$(MY_FREETYPE_ROOT)/src/base/ftpic.c \
	$(MY_FREETYPE_ROOT)/src/base/ftstroke.c \
	$(MY_FREETYPE_ROOT)/src/base/ftsynth.c \
	$(MY_FREETYPE_ROOT)/src/base/ftsystem.c \
	$(MY_FREETYPE_ROOT)/src/cff/cff.c \
	$(MY_FREETYPE_ROOT)/src/pshinter/pshinter.c \
	$(MY_FREETYPE_ROOT)/src/psnames/psnames.c \
	$(MY_FREETYPE_ROOT)/src/raster/raster.c \
	$(MY_FREETYPE_ROOT)/src/sfnt/sfnt.c \
	$(MY_FREETYPE_ROOT)/src/smooth/smooth.c \
	$(MY_FREETYPE_ROOT)/src/truetype/truetype.c

LOCAL_CFLAGS=-DFT2_BUILD_LIBRARY=1
LOCAL_C_INCLUDES=jni/$(MY_FREETYPE_ROOT)/include 
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
include $(BUILD_STATIC_LIBRARY)

#AGG
include $(CLEAR_VARS)

LOCAL_MODULE=agg

MY_AGG_ROOT = ../../../src
LOCAL_SRC_FILES = \
	$(MY_AGG_ROOT)/agg/agg_arc.cpp \
	$(MY_AGG_ROOT)/agg/agg_arrowhead.cpp \
	$(MY_AGG_ROOT)/agg/agg_bezier_arc.cpp \
	$(MY_AGG_ROOT)/agg/agg_bspline.cpp \
	$(MY_AGG_ROOT)/agg/agg_curves.cpp \
	$(MY_AGG_ROOT)/agg/agg_image_filters.cpp \
	$(MY_AGG_ROOT)/agg/agg_line_aa_basics.cpp \
	$(MY_AGG_ROOT)/agg/agg_path_storage.cpp \
	$(MY_AGG_ROOT)/agg/agg_rasterizer_scanline_aa.cpp \
	$(MY_AGG_ROOT)/agg/agg_rounded_rect.cpp \
	$(MY_AGG_ROOT)/agg/agg_sqrt_tables.cpp \
	$(MY_AGG_ROOT)/agg/agg_trans_affine.cpp \
	$(MY_AGG_ROOT)/agg/agg_trans_single_path.cpp \
	$(MY_AGG_ROOT)/agg/agg_vcgen_bspline.cpp \
	$(MY_AGG_ROOT)/agg/agg_vcgen_contour.cpp \
	$(MY_AGG_ROOT)/agg/agg_vcgen_dash.cpp \
	$(MY_AGG_ROOT)/agg/agg_vcgen_markers_term.cpp \
	$(MY_AGG_ROOT)/agg/agg_vcgen_smooth_poly1.cpp \
	$(MY_AGG_ROOT)/agg/agg_vcgen_stroke.cpp \
	$(MY_AGG_ROOT)/agg/agg_vpgen_segmentator.cpp \
	$(MY_AGG_ROOT)/agg/agg_graphics.cpp \
	$(MY_AGG_ROOT)/agg/agg_truetype_text.cpp

LOCAL_CFLAGS=$(MY_VIEW_FLAGS)
LOCAL_C_INCLUDES=jni/$(MY_AGG_ROOT)/include
LOCAL_STATIC_LIBRARIES = freetype2
LOCAL_EXPORT_C_INCLUDES=$(LOCAL_C_INCLUDES)
include $(BUILD_STATIC_LIBRARY)

#r3-view
include $(CLEAR_VARS)

LOCAL_MODULE=r3-view
LOCAL_SRC_FILES=$(MY_CORE_SRC_FILES) \
	$(MY_R3_ROOT)/os/linux/dev-clipboard.c\
	$(MY_R3_ROOT)/os/host-draw-api-agg.cpp\
	$(MY_R3_ROOT)/os/host-text-api-agg.cpp\
	$(MY_R3_ROOT)/os/host-view.c\

LOCAL_STATIC_LIBRARIES = libffi libagg libfreetype2 libr3
LOCAL_CFLAGS=$(MY_VIEW_FLAGS) -DREB_CORE
#LOCAL_C_INCLUDES=
include $(BUILD_EXECUTABLE)
