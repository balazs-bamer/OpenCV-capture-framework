set(the_description "Util")

set(util_hdrs
    ${CMAKE_CURRENT_LIST_DIR}/util.h
	${PROJECT_BINARY_DIR}/still_config.h
    )

set(util_srcs
    ${CMAKE_CURRENT_LIST_DIR}/util.cpp
)

add_library(util ${util_srcs} ${util_hdrs})

