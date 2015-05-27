set(the_description "Filter sharp and still images")

set(still_hdrs
    ${CMAKE_CURRENT_LIST_DIR}/still.h
    ${CMAKE_CURRENT_LIST_DIR}/measure.h
	${PROJECT_BINARY_DIR}/still_config.h
    )

set(still_srcs
    ${CMAKE_CURRENT_LIST_DIR}/measure.cpp
    ${CMAKE_CURRENT_LIST_DIR}/still.cpp
)

add_library(still ${still_srcs} ${still_hdrs})

