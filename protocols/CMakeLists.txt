include(Wayland)

WAYLAND_ADD_PROTOCOL_SERVER(proto-server-gamma-control
    gamma-control.xml
    gamma-control
)

WAYLAND_ADD_PROTOCOL_SERVER(proto-server-background
    background.xml
    background
)

WAYLAND_ADD_PROTOCOL_CLIENT(proto-client-background
    background.xml
    background
)

add_library(wavy-protocols STATIC
    ${proto-server-gamma-control}
    ${proto-server-background}
    ${proto-client-background}
)
