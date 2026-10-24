file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

foreach(_dep IN ITEMS rigAudio rigMidi)
	if(NOT TARGET ${_dep})
		if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
			set(_dep_dir "${RIGKIT_ROOT}/packs/${_dep}")
		else()
			set(_dep_dir "${CMAKE_SOURCE_DIR}/packs/${_dep}")
		endif()
		if(EXISTS "${_dep_dir}/CMakeLists.txt")
			add_subdirectory("${_dep_dir}" "${CMAKE_BINARY_DIR}/packs/${_dep}")
		else()
			message(FATAL_ERROR "rigVolca requires ${_dep} under packs/${_dep}")
		endif()
	endif()
endforeach()

set(SYRO_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/korg_syro")
set(SYRO_SOURCES
	${SYRO_DIR}/korg_syro_volcasample.c
	${SYRO_DIR}/korg_syro_comp.c
	${SYRO_DIR}/korg_syro_func.c
)
if(MSVC)
	set_source_files_properties(${SYRO_SOURCES} PROPERTIES COMPILE_OPTIONS "/w")
else()
	set_source_files_properties(${SYRO_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
endif()

add_library(${PACK_NAME} STATIC
	src/rigVolca.cpp
	src/rigVolca.h
	src/Syro.cpp
	src/Syro.h
	src/SyroPlayer.cpp
	src/SyroPlayer.h
	src/VolcaMidi.cpp
	src/VolcaMidi.h
	${SYRO_SOURCES}
)
target_link_libraries(${PACK_NAME} PUBLIC rigkit rigAudio rigMidi)
target_include_directories(${PACK_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
)
if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
	set(_miniaudio_dir "${RIGKIT_ROOT}/packs/rigAudio/third_party/miniaudio")
else()
	set(_miniaudio_dir "${CMAKE_CURRENT_LIST_DIR}/../rigAudio/third_party/miniaudio")
endif()
target_include_directories(${PACK_NAME} PRIVATE
	"${SYRO_DIR}"
	"${_miniaudio_dir}"
)
message(STATUS "Configured pack: ${PACK_NAME} (volca sample Syro + MIDI)")
