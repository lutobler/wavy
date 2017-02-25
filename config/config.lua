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

keys = {
    -- application shortcuts
    {"spawn", {modkey}, "Return", {terminal}},
    {"spawn", {modkey}, "d",      dmenu},

    -- bindings to lua functions
    {"lua", {}, "XF86AudioRaiseVolume",  function() utils.pulsecontrol("up", 5) end},
    {"lua", {}, "XF86AudioLowerVolume",  function() utils.pulsecontrol("down", 5) end},
    {"lua", {}, "XF86AudioMute",         function() utils.pulsecontrol("mute") end},
    {"lua", {}, "XF86MonBrightnessUp",   function() utils.brtcontrol("up") end},
    {"lua", {}, "XF86MonBrightnessDown", function() utils.brtcontrol("down") end},

    -- window managing basics
    {"exit",              {modkey, "shift"}, "e"},
    {"close_view",        {modkey},          "q"},
    {"cycle_tiling_mode", {modkey},          "space"},

    -- cycle through the views inside a frame
    {"cycle_view", {modkey},          "tab", "next"},
    {"cycle_view", {modkey, "shift"}, "tab", "previous"},

    -- select views/frame via arrow keys
    {"select", {modkey}, "h", "left"},
    {"select", {modkey}, "j", "down"},
    {"select", {modkey}, "k", "up"},
    {"select", {modkey}, "l", "right"},

    -- move views in a direction
    {"move", {modkey, "shift"}, "h", "left"},
    {"move", {modkey, "shift"}, "j", "down"},
    {"move", {modkey, "shift"}, "k", "up"},
    {"move", {modkey, "shift"}, "l", "right"},

    -- frame manipulation
    {"new_frame",    {modkey}, "p", "right"},
    {"new_frame",    {modkey}, "o", "down"},
    {"delete_frame", {modkey}, "r"},
    {"resize",       {"alt"},  "h", "left",  0.05},
    {"resize",       {"alt"},  "j", "down",  0.05},
    {"resize",       {"alt"},  "k", "up",    0.05},
    {"resize",       {"alt"},  "l", "right", 0.05},

    -- workspaces
    {"cycle_workspace", {modkey}, "period", "next"},
    {"cycle_workspace", {modkey}, "comma",  "previous"},
    {"add_workspace",   {modkey}, "plus"},
}

-- select/move to workspaces with keys 1 to 9
ws_keys = {"1", "2", "3", "4", "5", "6", "7", "8", "9"}
for i,v in ipairs(ws_keys) do
    table.insert(keys, {"select_workspace", {modkey}, v, i})
    table.insert(keys, {"move_to_workspace", {modkey, "shift"}, v, i})
end
