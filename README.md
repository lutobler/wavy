# Wavy wayland compositor

### Description
Wavy is a tiling wayland compositor based on [wlc](https://github.com/Cloudef/wlc),
inspired by herbstluftwm, i3, awesome and dwm. Parts of the implementation are heavily
inspired by [sway](https://github.com/SirCmpwn/sway).
Its design goal is to be configurable and programmable with a simple Lua interface.
Configuration shouldn't rely on complicated shell scripts, but aim to be a nicely integrated
system that can be easily modified and expanded.
For a commented example configuration see `config/config.lua`

It also comes with a status bar included!

N. B.: Wavy should be consired a _pre-alpha_ stage project.

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
- [ ] Complete wallaper support
- [ ] Complete input configuration
- [ ] Frame gaps
- [ ] Documentation
