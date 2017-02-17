package.path = os.getenv("HOME") .. "/.config/wavy/?.lua;" .. package.path
package.cpath = "/home/luke/wavy/?.so;" .. package.cpath
utils = require("wavy_utils")

-- [[ WAVY CONFIGURATION ]] --

config = {
    frame_gaps_size                     = 0,
    frame_border_size                   = 0,
    frame_border_empty_size             = 5,
    frame_border_active_color           = 0x475b74ff,
    frame_border_inactive_color         = 0x475b74ff,
    frame_border_empty_active_color     = 0x0c1cff80,
    frame_border_empty_inactive_color   = 0x6b6c7f80,

    view_border_size                    = 2,
    view_border_active_color            = 0x4897cfff,
    view_border_inactive_color          = 0x475b74ff,

    wallpaper                           = "~/wavy/assets/wavy_wallpaper.png"
}

layouts = {
    {"vertical",   "[-]"},
    {"horizontal", "[|]"},
    {"grid",       "[+]"},
    {"fullscreen", "[ ]"},
    {"fibonacci",  "[@]"}
}

autostart = {
    {"xrdb", "$HOME/.Xresources"},
}

--[[ STATUSBAR ]]--

bar = {
    height      = 18,
    font        = "monospace 10",
    gap         = 4,                            -- gap between elements
    padding     = 10,                           -- padding around text
    position    = "top",                        -- top, bottom

    colors = {
        background              = 0x282828a0,
        active_workspace        = 0x70407fc0,
        inactive_workspace      = 0x404055c0,
        active_workspace_font   = 0xffffffff,
        inactive_workspace_font = 0xccccccff,
    },

    widgets = {
        {"right", "hook_periodic_slow", utils.time},
        {"right", "hook_periodic_slow", utils.battery},
        {"right", "hook_user",          utils.volume},
        {"right", "hook_user",          utils.brt},
        {"right", "hook_periodic_slow", utils.wifi},
        {"right", "hook_periodic_slow", function()
                                            return utils.net_device("wlp4s0")
                                        end},
        {"right", "hook_periodic_slow", utils.fs_used},
        {"right", "hook_periodic_slow", utils.kernel},
        {"left",  "hook_view_update",   utils.tiling_symbol},
        {"left",  "hook_view_update",   utils.view_title},
    }
}

--[[ KEYBINDINGS ]]--
-- modifiers: shift, super, alt, ctrl, caps, mod2, mod3, mod5

modkey = "super"
dmenu = {"dmenu_run"}

terminal = os.getenv("TERMINAL")
if not terminal then
    terminal = "urxvt"
end

-- application shortcuts
kb_spawn({modkey}, "Return", {terminal})
kb_spawn({modkey}, "d", dmenu)

-- bindings to lua functions
kb_lua({}, "XF86AudioRaiseVolume",  function() utils.pulsecontrol("up", 5) end)
kb_lua({}, "XF86AudioLowerVolume",  function() utils.pulsecontrol("down", 5) end)
kb_lua({}, "XF86AudioMute",         function() utils.pulsecontrol("mute") end)
kb_lua({}, "XF86MonBrightnessUp",   function() utils.brtcontrol("up") end)
kb_lua({}, "XF86MonBrightnessDown", function() utils.brtcontrol("down") end)

-- window managing basics
kb_exit({modkey, "shift"}, "e")
kb_close_view({modkey}, "q")
kb_cycle_tiling_mode({modkey}, "space")

-- cycle through the views inside a frame
kb_cycle_view({modkey}, "tab", "next")
kb_cycle_view({modkey, "shift"}, "tab", "previous")

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
kb_cycle_workspace({modkey}, "period", "next")
kb_cycle_workspace({modkey}, "comma", "previous")
kb_add_workspace({modkey}, "plus")

ws_keys = {"1", "2", "3", "4", "5", "6", "7", "8", "9"}
for i,v in ipairs(ws_keys) do
    kb_select_workspace({modkey}, v)
    kb_move_to_workspace({modkey, "shift"}, v)
end
