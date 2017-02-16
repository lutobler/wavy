# Wavy wayland compositor

### Description
Wavy is a tiling wayland compositor based on [wlc](https://github.com/Cloudef/wlc),
inspired by herbstluftwm, i3 (and sway), awesome and dwm.
Its design goal is to be configurable and programmable with a flexible Lua API.
For an example configuration see `config/config.lua`

### Building

Dependencies:

- cmake
- [wlc](https://github.com/Cloudef/wlc)
- wayland
- xwayland
- xkbcommon
- cairo
- pango
- lua (5.3)

Build steps:

    cmake .
    make

Thats it, you can run the binary now. Installation is not implemented at this
point.

### Todo list
- Wallapers
- Frame gaps
- Documentation
