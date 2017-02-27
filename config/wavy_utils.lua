require("libwaveform")
require("math")

local wavy = {}
wavy.utils = {}                 -- utility functions
wavy.widgets = {}
wavy.widgets.default = {}       -- some standard widgets
wavy.widgets.callbacks = {}     -- statusbar callback functions
local bg = 0x404055e8           -- default background (RGBA)
local fg = 0xffffffff           -- default foregroudn (RGBA)

wavy.hooks = {
    periodic_slow = "hook_periodic_slow",
    periodic_fast = "hook_periodic_fast",
    view_update = "hook_view_update",
    user = "hook_user"
}

wavy.alignment = {
    right = "right",
    left = "left"
}

function wavy.utils.exec(cmd)   -- execute a shell command and return its result
    local r = io.popen(cmd)
    if not r then
        return ""
    end
    local s = r:read()
    r:close()
    if s then
        return s
    else
        return ""
    end
end

-- pulseaudio control using 'pamixer'.
function wavy.utils.pulsectl(action, change)
    if action == "up" then
        cmd = [[ pamixer -i ]] .. change
    elseif action == "down" then
        cmd = [[ pamixer -d ]] .. change
    elseif action == "mute" then
        cmd = [[ pamixer -t ]]
    else
        return
    end

    local r = io.popen(cmd)
    if r then
        r:close()
    end
    trigger_hook(wavy.hooks.user)
end

-- control display brightness using 'light'. uses more accuracy in the 1% - 20%
-- range.
function wavy.utils.brtctl(action)
    local cur_cmd = [[ light -G | xargs printf "%.0f\n" ]]
    local cur = tonumber(wavy.utils.exec(cur_cmd))

    if action == "up" and cur >= 20 then
        cmd = [[ light -A 5 ]]
    elseif action == "up" and cur < 20 then
        cmd = [[ light -A 1 ]]
    elseif action == "down" and cur >= 25 then
        cmd = [[ light -U 5 ]]
    elseif action == "down" and cur < 25 then
        cmd = [[ light -U 1 ]]
    else
        return
    end

    local r = io.popen(cmd)
    if r then
        r:close()
    end
    trigger_hook(wavy.hooks.user)
end

-- current time. set 'man strftime' for formatting options.
function wavy.widgets.callbacks.time()
    return {bg, fg, os.date("%a %b %d %Y %H:%M")}
end

-- kernel version.
function wavy.widgets.callbacks.kernel()
    local s = "Linux: " .. wavy.utils.exec("uname -r")
    return {bg, fg, s}
end

-- display backlight brightness. depends on 'light'.
function wavy.widgets.callbacks.brightness()
    local cmd = [[ light -G | xargs printf "%.0f\n" ]]
    local str = "Screen: " .. wavy.utils.exec(cmd) .. "%"
    return {bg, fg, str}
end

-- audio volume. depends on pulseaudio and pamixer.
function wavy.widgets.callbacks.volume()
    local mute = [[ pamixer --get-mute ]]
    if wavy.utils.exec(mute) == "true" then
        return {bg, fg, "Vol: muted"}
    end

    local vol = [[ pamixer --get-volume ]]
    local str = "Vol: " .. wavy.utils.exec(vol) .. "%"
    return {bg, fg, str}
end

-- battery status. the path might be different (BAT0, BAT1, etc.)
function wavy.widgets.callbacks.battery()
    local f_cap = io.open("/sys/class/power_supply/BAT1/capacity", "r")
    local f_stat = io.open("/sys/class/power_supply/BAT1/status", "r")
    if f_cap and f_stat then
        local bat_cap = f_cap:read()
        local bat_stat = f_stat:read()
        f_cap:close()
        f_stat:close()
        local str = "Battery: " .. bat_cap .. "% (" .. bat_stat .. ")"
        return {bg, fg, str}
    else
        if f_cap then
            f_cap:close()
        elseif f_stat then
            f_stat:close()
        end
        return {0, 0, ""}
    end
end

-- file system usage.
-- must be wrapped in a lambda function that takes no arguments.
function wavy.widgets.callbacks.fs_status(directory)
    cmd = [[ df --output=pcent -h ]] .. directory .. [[ | tail -n1 ]]
    local fs_used = wavy.utils.exec(cmd)
    local str = "Disk: " .. directory .. fs_used .. " used"
    return {bg, fg, str}
end

-- ip of network device.
-- must be wrapped in a lambda function that takes no arguments.
function wavy.widgets.callbacks.net_device(dev)
    local cmd = [[ ip addr show dev ]] .. dev
                .. [[| grep -o -m1 inet\ [0-9\.]* ]]
                .. [[ | cut -d ' ' -f 2 ]]
    local ip_str = wavy.utils.exec(cmd)

    if ip_str == "" then
        return {0, 0, ""}
    else
        str = dev .. ": " .. ip_str
        return {bg, fg, str}
    end
end

-- wifi status.
-- must be wrapped in a lambda function that takes no arguments.
function wavy.widgets.callbacks.wifi(dev)
    local cmd = [[ nmcli device show ]] .. dev
                .. [[ | grep -o GENERAL\.CONNECTION\ *.* ]]
                .. [[ | cut -d ':' -f2 ]]
    local con = wavy.utils.exec(cmd)
    local con,_ = string.gsub(con, "^%s*", "")

    if con == "--" then
        return {bg, fg, "Wifi: disconnected"}
    else
        local str = "Wifi: " .. con
        return {bg, fg, str}
    end
end

-- calls the C library
function wavy.widgets.callbacks.view_title()
    return {bg, fg, get_view_title()}
end

-- calls the C library
function wavy.widgets.callbacks.tiling_symbol()
    return {bg, fg, get_tiling_symbol()}
end

wavy.widgets.default.time = {
    wavy.alignment.right,
    wavy.hooks.periodic_slow,
    wavy.widgets.callbacks.time
}

wavy.widgets.default.kernel = {
    wavy.alignment.right,
    wavy.hooks.periodic_slow,
    wavy.widgets.callbacks.kernel
}

wavy.widgets.default.brightness = {
    wavy.alignment.right,
    wavy.hooks.user,
    wavy.widgets.callbacks.brightness
}

wavy.widgets.default.volume = {
    wavy.alignment.right,
    wavy.hooks.user,
    wavy.widgets.callbacks.volume
}

wavy.widgets.default.battery = {
    wavy.alignment.right,
    wavy.hooks.periodic_slow,
    wavy.widgets.callbacks.battery
}

wavy.widgets.default.view_title = {
    wavy.alignment.left,
    wavy.hooks.view_update,
    wavy.widgets.callbacks.view_title
}

wavy.widgets.default.tiling_symbol = {
    wavy.alignment.left,
    wavy.hooks.view_update,
    wavy.widgets.callbacks.tiling_symbol
}

return wavy
