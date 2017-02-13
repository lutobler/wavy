include(Wayland)

WAYLAND_ADD_PROTOCOL_SERVER(proto-server-gamma-control
    gamma-control.xml
    gamma-control
)

# WAYLAND_ADD_PROTOCOL_CLIENT(proto-client-background
#     background.xml
#     background
# )

add_library(wavy-protocols STATIC
    # ${proto-client-background}
    ${proto-server-gamma-control}
)

# set(PROTOCOLS_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/protocols PARENT_SCOPE)
# INCLUDE_DIRECTORIES(${WAYLAND_INCLUDE_DIR} ${XKBCOMMON_INCLUDE_DIRS})
