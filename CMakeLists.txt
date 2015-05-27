cmake_minimum_required(VERSION 2.8)
project( projector )
# The version number.
set (STILL_VERSION_MAJOR 0)
set (STILL_VERSION_MINOR 1)
 
# configure a header file to pass some of the CMake settings
# to the source code

set(USE_NVWA "0" CACHE STRING "If 1, use NVWA library to catch new-delete memory leaks.")
set(DEBUG_OUTPUT "0" CACHE STRING "Debug output with timing info.")
set(DEBUG_STDOUT "0" CACHE STRING "If 1, output goes to stdout, if 0, into DEBUG_LOC.")
set(DEBUG_LOC "/tmp/diag.log" CACHE STRING "Debug output location.")
set(OUTPUT_FILE_PREFIX "/tmp/result_" CACHE STRING "Output file prefix including path.")
set(VIDEO_NUM "0" CACHE STRING "/dev/video[num]")
set(GETCH_DELAY "200" CACHE STRING "Wait period in ms during getch in user interface")
set(HANDLER_TIMEOUT "0" CACHE STRING "Timeout in ms for handler processing, 0 if none.")
set(FORCE_HANDLER_EXIT "0" CACHE STRING "Force handler exit without a result on timeout or finish.")
set(USE_STALE_FRAME "0" CACHE STRING "Use stale frame stored duringh previous processing.")
set(STILL_DOWNSAMPLE_EXPONENT "3" CACHE STRING "Downsampling for still scene check happens at 2**exponent.")
set(STILL_CHANGE_TIME "500" CACHE STRING "Time (ms) to consider a still image different from prev. one.")
set(STILL_NOISE_THRESHOLD "10" CACHE STRING "Noise threshold when detecting still images")
set(STILL_SAMPLING_INC "97" CACHE STRING "Sampling increment for detecting still images")
set(STILL_SAMPLING_PERCENT "10" CACHE STRING "Percentage of image to compare for detecting still images")
set(STILL_DEFLECTION_PERCENT "3" CACHE STRING "Allowed deflection percentage for images considered to be the same.")
set(SHARP_TILES_PER_SIDE "10" CACHE STRING "Number of tiles per image side for sharpness detection")
set(SHARP_DIFF_LOW "15" CACHE STRING "Lower limit of adjacent brightness difference")
set(SHARP_DIFF_HIGH "30" CACHE STRING "Upper limit of adjacent brightness difference")
set(SHARP_HIGH_PERCENT "60" CACHE STRING "Required percentage of high differences to low ones.")
set(SHARP_TILES_REQUIRED "4" CACHE STRING "Number of sharp tiles required for sharp image")

configure_file (
  "${PROJECT_SOURCE_DIR}/still_config.h.in"
  "${PROJECT_BINARY_DIR}/still_config.h"
  )

set(include_dirs
	"${PROJECT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/util"
	"${PROJECT_SOURCE_DIR}/still"
	"${PROJECT_SOURCE_DIR}/OpenCV_V4L2_directFormat_videoio"
)
if(USE_NVWA STREQUAL "1")
	set(include_dirs ${include_dirs} "${PROJECT_SOURCE_DIR}/nvwa")
endif()
include_directories( ${include_dirs} )

set(main_hdrs
    ${PROJECT_BINARY_DIR}/still_config.h
    )

set(main_srcs
    ${CMAKE_CURRENT_LIST_DIR}/still_config.cpp
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)

add_subdirectory (OpenCV_V4L2_directFormat_videoio) 
add_subdirectory (still) 
add_subdirectory (util) 
if(USE_NVWA STREQUAL "1")
	add_subdirectory (nvwa) 
endif()

 
find_package( OpenCV REQUIRED )
find_package( Curses REQUIRED )
add_executable( main ${main_hdrs} ${main_srcs} )

set(main_libraries
	main ${OpenCV_LIBS} ${Curses_LIBS} videoio_mod still util
)
if(USE_NVWA STREQUAL "1")
	set(main_libraries ${main_libraries} nvwa)
endif()

target_link_libraries( ${main_libraries} )
