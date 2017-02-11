package.path = os.getenv("HOME") .. "/.config/wavy/?.lua;" .. package.path
package.cpath = "/home/luke/wavy/?.so;" .. package.cpath
require("utils")

-- color format: 32 bit integer, RGBA (red, green, blue, alpha)
config = {
    frame_gaps_size                     = 0,
    frame_border_size                   = 0,
    frame_border_empty_size             = 4,
    frame_border_active_color           = 0x475b74ff,
    frame_border_inactive_color         = 0x475b74ff,
    frame_border_empty_active_color     = 0x0c1cffff,
    frame_border_empty_inactive_color   = 0x6b6c7fff,

    view_border_size                    = 2,
    view_border_active_color            = 0x4897cfff,
    view_border_inactive_color          = 0x475b74ff,
}

-- layouts: vertical, horizontal, grid, fullscreen, fibonacci
layouts = {
    {"vertical",    "[-]"},
    {"horizontal",  "[|]"},
    {"grid",        "[+]"},
    {"fullscreen",  "[ ]"},
    {"fibonacci",   "[@]"}
}

-- commands executed at startup.
autostart = {
    {"xrdb", "$HOME/.Xresources"}
}

--[[ STATUSBAR ]]--
bar = {
    height      = 18,
    font        = "Terminess Powerline 9",
    gap         = 4,        -- between entries
    padding     = 10,       -- inside entry box
    position    = "top",    -- bottom, top

    colors = {
        background              = 0x282828ff,
        active_workspace        = 0x70407fff,
        inactive_workspace      = 0x404055ff,
        active_workspace_font   = 0xffffffff,
        inactive_workspace_font = 0xccccccff,
    },

    -- format: {side, hook, function}
    -- hook: hook_periodic_slow, hook_periodic_fast, hook_view_update, hook_user
    -- side: right, left
    -- function: see utils.lua
    widgets = {
        {"right", "hook_periodic_slow", time},
        {"right", "hook_periodic_slow", battery},
        {"right", "hook_user",          volume},
        {"right", "hook_user",          brt},
        {"right", "hook_periodic_slow", wifi},
        {"right", "hook_periodic_slow", function()
                                            return net_device("wlp4s0")
                                        end},
        {"right", "hook_periodic_slow", fs_used},
        {"right", "hook_periodic_slow", kernel},
        {"left",  "hook_view_update",   tiling_symbol},
        {"left",  "hook_view_update",   view_title},
    }
}

--[[ KEYBINDINGS ]]--
-- availabe modifiers: shift, super, alt, ctrl, caps, mod2, mod3, mod5
m = os.getenv("WAVY_MOD")
if m then
    modkey = m
else
    modkey = "ctrl"
end

qt_wl = "QT_QPA_PLATFORM=wayland-egl "
qt_dec = "QT_WAYLAND_DISABLE_WINDOWDECORATION=1 "

-- using a shell so env variables work
qb = {qt_wl .. qt_dec .. "qutebrowser", "--backend", "webengine"}
dmenu = {"dmenu_run", "-b", "-fn", "\'Terminess Powerline-12:Regular\'", "-p", "\'>>>\'"}

-- application shortcuts
kb_spawn({modkey, "shift"}, "Return", {"xterm"})
kb_spawn({modkey}, "Return", {"urxvt"})
kb_spawn({modkey}, "u", dmenu)
kb_spawn({modkey}, "m", qb)
kb_spawn({modkey}, "n", {"telegram-desktop"})

-- bindings to lua functions
kb_lua({}, "XF86AudioRaiseVolume", function() pulsecontrol("up", 5) end)
kb_lua({}, "XF86AudioLowerVolume", function() pulsecontrol("down", 5) end)
kb_lua({}, "XF86AudioMute", function() pulsecontrol("mute") end)
kb_lua({}, "XF86MonBrightnessUp", function() brtcontrol("up") end)
kb_lua({}, "XF86MonBrightnessDown", function() brtcontrol("down") end)
kb_lua({modkey, "alt"}, "j", function() brtcontrol("down") end)
kb_lua({modkey, "alt"}, "k", function() brtcontrol("up") end)

-- window managing basics
kb_exit({modkey, "shift"}, "e")
kb_close_view({modkey}, "q")
kb_cycle_tiling_mode({modkey}, "space")

-- cycle through the views inside a frame
kb_cycle_view({modkey}, "tab", "forward")
kb_cycle_view({modkey, "shift"}, "tab", "backward")

-- select views/frame via arrow keys
kb_select({modkey}, "h", "left")
kb_select({modkey}, "j", "down")
kb_select({modkey}, "k", "up")
kb_select({modkey}, "l", "right")

-- move views in a direction
kb_move({modkey, "shift"}, "h", "left")
kb_move({modkey, "shift"}, "j", "down")
kb_move({modkey, "shift"}, "k", "up")
kb_move({modkey, "shift"}, "l", "right")

-- frame manipulation
kb_new_frame_right({modkey}, "p")
kb_new_frame_down({modkey}, "o")
kb_delete_frame({modkey}, "r")
kb_resize({"alt"}, "h", "left", 0.05)
kb_resize({"alt"}, "j", "down", 0.05)
kb_resize({"alt"}, "k", "up", 0.05)
kb_resize({"alt"}, "l", "right", 0.05)

-- workspaces
kb_next_workspace({modkey}, "period")
kb_prev_workspace({modkey}, "comma")
kb_add_workspace({modkey}, "plus")

ws_keys = {"1", "2", "3", "4", "5", "6", "7", "8", "9"}
for i,v in ipairs(ws_keys) do
    kb_select_workspace({modkey}, v)
    kb_move_to_workspace({modkey, "shift"}, v)
end
