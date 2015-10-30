# vim: ts=2 shiftwidth=2 filetype=CMAKE
cmake_minimum_required (VERSION 2.8)

project (Rebol3 C CXX)
set (TOP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set (CORE_DIR "${TOP_SRC_DIR}/core")
set (OS_DIR "${TOP_SRC_DIR}/os")
set (AGG_DIR "${TOP_SRC_DIR}/agg")
set (TOOLS_DIR "${TOP_SRC_DIR}/tools")
set (EXTERNAL_DIR "${TOP_SRC_DIR}/../external")
set (FFI_DIR "${EXTERNAL_DIR}/libffi")
if(CMAKE_HOST_WIN32)
	set (REBOL "${CMAKE_CURRENT_BINARY_DIR}/r3-make.exe")
else()
	set (REBOL "${CMAKE_CURRENT_BINARY_DIR}/r3-make")
endif()

option(R3_EXTERNAL_FFI "Build with external FFI" OFF)
option(R3_BUILD_VIEW "Build with view system" OFF)
option(R3_EXTERNAL_SDL "Build with external SDL2" OFF)
option(R3_EXTERNAL_GLEW "Build with external GLEW" OFF)

if (NOT EXISTS ${REBOL})
	message(FATAL_ERROR "${REBOL} doesn't exist, an executable r3 is required")
endif()

if (NOT DEFINED R3_OS_ID)
	message("Trying to obtain OS_ID from r3-make, as R3_OS_ID is not defined ...")
	execute_process(COMMAND ${REBOL} --do "print rejoin [\"0.\" system/version/4 \".\" system/version/5]" OUTPUT_VARIABLE R3_OS_ID)
endif()

if (NOT DEFINED R3_OS_ID)
	message(FATAL_ERROR "R3_OS_ID is not defined")
endif()
message("Building Rebol 3 for OS_ID: ${R3_OS_ID}")

set(COMMON_MACROS REB_EXE)
if (WIN32)
	list (APPEND COMMON_MACROS
		WIN32
		TO_WINDOWS
		ENDIAN_LITTLE
		UNICODE
		)
	set(LIBS wsock32 comdlg32)
elseif (APPLE)
	list (APPEND COMMON_MACROS
		ENDIAN_LITTLE
		HAS_LL_CONSTS
		)
elseif (UNIX)
	if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		list (APPEND COMMON_MACROS
			TO_LINUX
			TO_LINUX_X86
			ENDIAN_LITTLE
			HAS_LL_CONSTS
			)
		set(LIBS m dl)
	else ()
		message(FATAL_ERROR "Unsupported Unix system: ${CMAKE_SYSTEM_NAME}")
	endif ()
else ()
	message(FATAL_ERROR "Unsupported system")
endif ()

if (R3_OS_ID STREQUAL "0.2.5")
	list (APPEND COMMON_MACROS
		TO_OSX_X86
		)
	set(CMAKE_OSX_ARCHITECTURE i386)
elseif (R3_OS_ID STREQUAL "0.2.40")
	list (APPEND COMMON_MACROS
		TO_OSX_X64
		__LP64__
		)
	set(CMAKE_OSX_ARCHITECTURE x86_64)
elseif (R3_OS_ID STREQUAL "0.3.1")
	list (APPEND COMMON_MACROS
		TO_WINDOWS_X86
		)
elseif (R3_OS_ID STREQUAL "0.3.40")
	list (APPEND COMMON_MACROS
		TO_WINDOWS_X64
		__LLP64__
		)
elseif (R3_OS_ID STREQUAL "0.4.4")
	list (APPEND COMMON_MACROS
		TO_LINUX_X86
		)
elseif (R3_OS_ID STREQUAL "0.4.20") #linux arm
	list (APPEND COMMON_MACROS
		TO_LINUX_ARM
		)
elseif (R3_OS_ID STREQUAL "0.4.40") #linux x86_64
	list (APPEND COMMON_MACROS
		TO_LINUX_X64
		__LLP64__
		)
endif ()

if (MSVC)
	set (LINK_FLAGS /STACK:4194304)
endif()

if (WIN32 OR CYGWIN)
	set (CORE_PLATFORM_SOURCE
		${OS_DIR}/windows/host-lib.c
		${OS_DIR}/windows/dev-stdio.c
		${OS_DIR}/windows/dev-file.c
		${OS_DIR}/windows/dev-serial.c
		${OS_DIR}/windows/dev-event.c
		${OS_DIR}/windows/dev-clipboard.c
		)
else ()
	set (CORE_PLATFORM_SOURCE
		${OS_DIR}/generic/host-memory.c
		${OS_DIR}/generic/host-locale.c
		${OS_DIR}/generic/iso-639.c
		${OS_DIR}/generic/iso-3166.c
		${OS_DIR}/generic/host-gob.c
		${OS_DIR}/posix/host-readline.c
		${OS_DIR}/posix/dev-stdio.c
		${OS_DIR}/posix/dev-event.c
		${OS_DIR}/posix/dev-file.c
		${OS_DIR}/posix/host-browse.c
		${OS_DIR}/posix/host-config.c
		${OS_DIR}/posix/host-error.c
		${OS_DIR}/posix/host-library.c
		${OS_DIR}/posix/host-process.c
		${OS_DIR}/posix/host-thread.c
		${OS_DIR}/posix/host-time.c
		)
	if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		set (CORE_PLATFORM_SOURCE ${CORE_PLATFORM_SOURCE}
			# Linux has support for ELF format encapping
			${OS_DIR}/linux/host-encap.c

			# There is a Linux serial device
			${OS_DIR}/linux/dev-serial.c

			# Linux supports siginfo_t-style signals
			${OS_DIR}/linux/dev-signal.c
			)
	else ()
		list (APPEND CORE_PLATFORM_SOURCE
			${OS_DIR}/stub/host-encap.c
			)
	endif ()
endif ()

if (R3_EXTERNAL_FFI)
	find_package(PkgConfig)
	pkg_search_module(FFI libffi)
	if (${FFI_FOUND})
		set (COMMON_MACROS ${COMMON_MACROS} HAVE_LIBFFI_AVAILABLE)
	else ()
		message(WARNING "FFI is not found, External Library Access will not work!")
	endif ()
else ()
	add_subdirectory(${FFI_DIR} ${CMAKE_CURRENT_BINARY_DIR}/ffi EXCLUDE_FROM_ALL)
	set (COMMON_MACROS ${COMMON_MACROS} HAVE_LIBFFI_AVAILABLE FFI_BUILDING)
	set (FFI_LIBRARIES ffi_s)
endif ()

#CORE
set (CORE_SOURCE 
	${CORE_DIR}/a-constants.c
	${CORE_DIR}/a-globals.c
	${CORE_DIR}/a-lib.c
	${CORE_DIR}/a-stubs.c
	${CORE_DIR}/b-boot.c
	${CORE_DIR}/b-init.c
	${CORE_DIR}/c-do.c
	${CORE_DIR}/c-error.c
	${CORE_DIR}/c-frame.c
	${CORE_DIR}/c-function.c
	${CORE_DIR}/c-port.c
	${CORE_DIR}/c-task.c
	${CORE_DIR}/c-word.c
	${CORE_DIR}/d-crash.c
	${CORE_DIR}/d-dump.c
	${CORE_DIR}/d-print.c
	${CORE_DIR}/f-blocks.c
	${CORE_DIR}/f-deci.c
	${CORE_DIR}/f-dtoa.c
	${CORE_DIR}/f-enbase.c
	${CORE_DIR}/f-extension.c
	${CORE_DIR}/f-int.c
	${CORE_DIR}/f-math.c
	${CORE_DIR}/f-modify.c
	${CORE_DIR}/f-qsort.c
	${CORE_DIR}/f-random.c
	${CORE_DIR}/f-round.c
	${CORE_DIR}/f-series.c
	${CORE_DIR}/f-stubs.c
	${CORE_DIR}/l-scan.c
	${CORE_DIR}/l-types.c
	${CORE_DIR}/m-gc.c
	${CORE_DIR}/m-pools.c
	${CORE_DIR}/m-series.c
	${CORE_DIR}/m-stacks.c
	${CORE_DIR}/n-control.c
	${CORE_DIR}/n-data.c
	${CORE_DIR}/n-io.c
	${CORE_DIR}/n-loop.c
	${CORE_DIR}/n-math.c
	${CORE_DIR}/n-sets.c
	${CORE_DIR}/n-strings.c
	${CORE_DIR}/n-system.c
	${CORE_DIR}/p-clipboard.c
	${CORE_DIR}/p-console.c
	${CORE_DIR}/p-dir.c
	${CORE_DIR}/p-dns.c
	${CORE_DIR}/p-event.c
	${CORE_DIR}/p-file.c
	${CORE_DIR}/p-net.c
	${CORE_DIR}/p-serial.c
	${CORE_DIR}/p-signal.c
	${CORE_DIR}/s-cases.c
	${CORE_DIR}/s-crc.c
	${CORE_DIR}/s-file.c
	${CORE_DIR}/s-find.c
	${CORE_DIR}/s-make.c
	${CORE_DIR}/s-mold.c
	${CORE_DIR}/s-ops.c
	${CORE_DIR}/s-trim.c
	${CORE_DIR}/s-unicode.c
	${CORE_DIR}/t-bitset.c
	${CORE_DIR}/t-block.c
	${CORE_DIR}/t-char.c
	${CORE_DIR}/t-datatype.c
	${CORE_DIR}/t-date.c
	${CORE_DIR}/t-decimal.c
	${CORE_DIR}/t-event.c
	${CORE_DIR}/t-function.c
	${CORE_DIR}/t-gob.c
	${CORE_DIR}/t-image.c
	${CORE_DIR}/t-integer.c
	${CORE_DIR}/t-library.c
	${CORE_DIR}/t-logic.c
	${CORE_DIR}/t-map.c
	${CORE_DIR}/t-money.c
	${CORE_DIR}/t-none.c
	${CORE_DIR}/t-object.c
	${CORE_DIR}/t-pair.c
	${CORE_DIR}/t-port.c
	${CORE_DIR}/t-routine.c
	${CORE_DIR}/t-string.c
	${CORE_DIR}/t-struct.c
	${CORE_DIR}/t-time.c
	${CORE_DIR}/t-tuple.c
	${CORE_DIR}/t-typeset.c
	${CORE_DIR}/t-utype.c
	${CORE_DIR}/t-vector.c
	${CORE_DIR}/t-word.c
	${CORE_DIR}/u-bmp.c
	${CORE_DIR}/u-compress.c
	${CORE_DIR}/u-dialect.c
	${CORE_DIR}/u-gif.c
	${CORE_DIR}/u-jpg.c
	${CORE_DIR}/u-md5.c
	${CORE_DIR}/u-parse.c
	${CORE_DIR}/u-png.c
	${CORE_DIR}/u-sha1.c
	${CORE_DIR}/u-zlib.c
	${CORE_DIR}/../codecs/aes/aes.c
	${CORE_DIR}/../codecs/bigint/bigint.c
	${CORE_DIR}/../codecs/dh/dh.c
	${CORE_DIR}/../codecs/png/lodepng.c
	${CORE_DIR}/../codecs/rc4/rc4.c
	${CORE_DIR}/../codecs/rsa/rsa.c
	)
add_custom_command(OUTPUT
	${TOP_SRC_DIR}/include/tmp-funcs.h
	${TOP_SRC_DIR}/include/tmp-funcargs.h
	${TOP_SRC_DIR}/include/tmp-strings.h
	COMMAND ${REBOL} ${TOOLS_DIR}/make-headers.r
	)
add_custom_command(OUTPUT
	${TOP_SRC_DIR}/include/tmp-evaltypes.h
	${TOP_SRC_DIR}/include/tmp-maketypes.h
	${TOP_SRC_DIR}/include/tmp-comptypes.h
	${TOP_SRC_DIR}/include/reb-types.h
	${TOP_SRC_DIR}/include/ext-types.h
	${TOP_SRC_DIR}/include/tmp-exttypes.h
	${TOP_SRC_DIR}/include/tmp-bootdefs.h
	${TOP_SRC_DIR}/include/tmp-sysobj.h
	${TOP_SRC_DIR}/include/reb-dialect.h
	${TOP_SRC_DIR}/include/reb-evtypes.h
	${TOP_SRC_DIR}/include/tmp-errnums.h
	${TOP_SRC_DIR}/include/tmp-portmodes.h
	${TOP_SRC_DIR}/include/tmp-sysctx.h
	${TOP_SRC_DIR}/include/tmp-boot.h
	${TOP_SRC_DIR}/core/b-boot.c
	COMMAND ${REBOL} ${TOOLS_DIR}/make-boot.r ${R3_OS_ID}
	)
add_custom_command(OUTPUT
	${TOP_SRC_DIR}/include/host-ext-core.h
	COMMAND ${REBOL} ${TOOLS_DIR}/core-ext.r
	)
add_custom_command(OUTPUT
	${TOP_SRC_DIR}/include/reb-lib.h
	${TOP_SRC_DIR}/include/reb-lib-lib.h
	COMMAND ${REBOL} ${TOOLS_DIR}/make-reb-lib.r
	)
if (BUILD_VIEW)
	add_custom_command(OUTPUT
		${TOP_SRC_DIR}/include/host-lib.h
		${TOP_SRC_DIR}/include/host-table.inc
		COMMAND ${REBOL} ${TOOLS_DIR}/make-os-ext.r ${R3_OS_ID} gfx
		)
else ()
	add_custom_command(OUTPUT
		${TOP_SRC_DIR}/include/host-lib.h
		${TOP_SRC_DIR}/include/host-table.inc
		COMMAND ${REBOL} ${TOOLS_DIR}/make-os-ext.r ${R3_OS_ID}
		)
endif ()

set (GENERATED_CORE_SOURCE 
	${TOP_SRC_DIR}/include/tmp-funcs.h
	${TOP_SRC_DIR}/include/tmp-funcargs.h
	${TOP_SRC_DIR}/include/tmp-strings.h
	${TOP_SRC_DIR}/include/tmp-evaltypes.h
	${TOP_SRC_DIR}/include/tmp-maketypes.h
	${TOP_SRC_DIR}/include/tmp-comptypes.h
	${TOP_SRC_DIR}/include/reb-types.h
	${TOP_SRC_DIR}/include/ext-types.h
	${TOP_SRC_DIR}/include/tmp-exttypes.h
	${TOP_SRC_DIR}/include/tmp-bootdefs.h
	${TOP_SRC_DIR}/include/tmp-sysobj.h
	${TOP_SRC_DIR}/include/reb-dialect.h
	${TOP_SRC_DIR}/include/reb-evtypes.h
	${TOP_SRC_DIR}/include/tmp-errnums.h
	${TOP_SRC_DIR}/include/tmp-portmodes.h
	${TOP_SRC_DIR}/include/tmp-sysctx.h
	${TOP_SRC_DIR}/include/tmp-boot.h
	${TOP_SRC_DIR}/core/b-boot.c
	${TOP_SRC_DIR}/include/host-ext-core.h
	${TOP_SRC_DIR}/include/reb-lib.h
	${TOP_SRC_DIR}/include/reb-lib-lib.h
	${TOP_SRC_DIR}/include/host-lib.h
	${TOP_SRC_DIR}/include/host-table.inc
	)
#HOST
set (OS_SOURCE 
	${OS_DIR}/host-main.c
	${OS_DIR}/host-args.c
	${OS_DIR}/host-device.c
	${OS_DIR}/host-stdio.c
	${OS_DIR}/host-core.c
	${OS_DIR}/dev-net.c
	${OS_DIR}/dev-dns.c
	${OS_DIR}/generic/host-memory.c
	${CORE_PLATFORM_SOURCE}
	)
set (GENERATED_OS_SOURCE 
	)

if(NOT (MSVC_IDE OR XCODE))
	add_custom_target(clean-generated COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/clean-generated.cmake)
	add_custom_target(clean-all
		COMMAND ${CMAKE_BUILD_TOOL} clean
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/clean-generated.cmake
		)
endif()
add_library(libr3_core_core OBJECT ${CORE_SOURCE} ${GENERATED_CORE_SOURCE})
add_library(libr3_core_os OBJECT ${OS_SOURCE} ${GENERATED_OS_SOURCE})
add_dependencies(libr3_core_os libr3_core_core)
add_executable(r3-core $<TARGET_OBJECTS:libr3_core_core> $<TARGET_OBJECTS:libr3_core_os>)
target_include_directories(libr3_core_core PUBLIC ${TOP_SRC_DIR}/include ${TOP_SRC_DIR}/codecs ${FFI_INCLUDE_DIRS})
target_include_directories(libr3_core_os PUBLIC ${TOP_SRC_DIR}/include ${TOP_SRC_DIR}/codecs)
target_compile_definitions(libr3_core_core PUBLIC REB_API REB_CORE ${COMMON_MACROS})
if (WIN32)
	#WINSYS_WIN32 is needed for dev-event.c
	target_compile_definitions(libr3_core_os PUBLIC REB_CORE REB_EXE WINSYS_WIN32 ${COMMON_MACROS})
else ()
	target_compile_definitions(libr3_core_os PUBLIC REB_CORE REB_EXE ${COMMON_MACROS})
endif ()
set_target_properties(r3-core PROPERTIES LINKER_LANGUAGE C)
if (DEFINED LINK_FLAGS)
	set_target_properties(r3-core PROPERTIES LINK_FLAGS ${LINK_FLAGS})
endif()

target_link_libraries(r3-core ${LIBS} ${FFI_LIBRARIES})

if (R3_BUILD_VIEW)
	include (CMakeLists-view.txt)
endif ()
