--
-- Use osml10n library to localize names
--
-- This will transliterate all 'name' and 'name:*' tags. For each a new tag
-- with '_translit' appended will be set.
--
-- Needs https://github.com/giggls/osml10n
--

local osml10n = require 'osml10n.init'

function process(objtype, object)
    local new_tags = {}
    for k, v in pairs(object.tags) do
        new_tags[k] = v
        if k == 'name' or ott.has_prefix(k, 'name:') then
            local trans = osml10n.geo_transcript(objtype .. object.id, v, object.bbox)
            if trans then
                new_tags[k .. '_translit'] = trans
            end
        end
    end

    return new_tags
end

function ott.process_node(object)
    return process('n', object)
end

function ott.process_way(object)
    return process('w', object)
end

function ott.process_relation(object)
    return process('r', object)
end

