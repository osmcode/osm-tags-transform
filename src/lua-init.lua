--
--  This Lua initialization code will be compiled into osm-tags-transform.
--

function ott.has_prefix(str, prefix)
    if str == nil then
        return nil
    end
    return str:sub(1, prefix:len()) == prefix
end

-- This will be the metatable for the OSM objects given to the process callback
-- functions.
local inner_metatable = {
    __index = function(table, key)
        if key == 'version' or key == 'timestamp' or
           key == 'changeset' or key == 'uid' or key == 'user' or key == 'bbox' then
            return nil
        end
        error("unknown field '" .. key .. "'", 2)
    end
}

object_metatable = {
    __index = {}
}

setmetatable(object_metatable.__index, inner_metatable)

