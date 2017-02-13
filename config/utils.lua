require("libwaveform")
require("math")

-- standard colors (RGBA)
bg = 0x404055ff
fg = 0xffffffff

-- return table when an error occurs
null = {0, 0, ""}

-- execute a shell command and return its result
function exec(cmd)
    local r = io.popen(cmd)
    if not r then
        return ""
    end
    s = r:read()
    r:close()
    if s then
        return s
    else
        return ""
    end
end

function view_title()
    return {bg, fg, get_view_title()}
end

function tiling_symbol()
    return {bg, fg, get_tiling_symbol()}
end

-- see "man strftime" for format strings.
function time()
    return {bg, fg, os.date("%a %b %d %Y %H:%M")}
end

-- battery might be in "BAT0" aswell.
function battery(arg)
    local f_cap = io.open("/sys/class/power_supply/BAT1/capacity", "r")
    local f_stat = io.open("/sys/class/power_supply/BAT1/status", "r")
    if f_cap and f_stat then
        local bat_cap = f_cap:read()
        local bat_stat = f_stat:read()
        f_cap:close()
        f_stat:close()
        local str = "Battery: " .. bat_cap .. "% (" .. bat_stat .. ")"
        local bat = tonumber(bat_cap)

        if arg == "color" then -- 100%: green -> 0%: red
            if bat >= 50 then
                if bat == 100 then
                    r = 0
                else
                    r = math.floor(255 * ((100.0 - bat)/50))
                end
                g = 0xff
            else
                if bat == 0 then
                    g = 0
                else
                    g = math.floor(255 * (bat/50))
                end
                r = 0xff
            end
            _bg =  (((r << 8) | g) << 16) | 0xff
            r = {_bg, bg, str}
        else
            r = {bg, fg, str}
        end
        return r
    else
        if f_cap then
            f_cap:close()
        elseif f_stat then
            f_stat:close()
        end
        return null
    end
end

function fs_used()
    local fs_used = exec("df --output=pcent -h / | tail -n1")
    local str = "Disk: /" .. fs_used .. " used"
    return {bg, fg, str}
end

function kernel()
    local str = "Linux: " .. exec("uname -r")
    return {bg, fg, str}
end

function net_device(dev)
    local cmd = [[ ip addr show dev ]] .. dev
                .. [[| grep -o -m1 inet\ [0-9\.]* ]]
                .. [[ | cut -d ' ' -f 2 ]]
    local ip_str = exec(cmd)

    if ip_str == "" then
        return null
    else
        str = dev .. ": " .. ip_str
        return {bg, fg, str}
    end
end

function wifi()
    local cmd = [[ nmcli device show wlp4s0 ]]
                .. [[ | grep -o GENERAL\.CONNECTION\ *.* ]]
                .. [[ | cut -d ':' -f2 ]]
    local con = exec(cmd)
    local con,_ = string.gsub(con, "^%s*", "")

    if con == "--" then
        return {bg, fg, "Wifi: disconnected"}
    else
        local str = "Wifi: " .. con
        return {bg, fg, str}
    end
end

function volume()
    mute = [[ pamixer --get-mute ]]
    if exec(mute) == "true" then
        return {bg, fg, "Vol: muted"}
    end

    vol = [[ pamixer --get-volume ]]
    local str = "Vol " .. exec(vol) .. "%"
    return {bg, fg, str}
end

function pulsecontrol(action, change)
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
    trigger_hook("hook_user")
end

function brt()
    cmd = [[ light -G | xargs printf "%.0f\n" ]]
    local str = "Screen: " .. exec(cmd) .. "%"
    return {bg, fg, str}
end

function brtcontrol(action)
    cur_cmd = [[ light -G | xargs printf "%.0f\n" ]]
    local cur = tonumber(exec(cmd))

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
    trigger_hook("hook_user")
end
