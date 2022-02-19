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
    local label = osml10n.get_localized_name_from_tags(objtype .. object.id, object.tags, 'de', object.bbox)
    if label and #label > 0 then
        object.tags['osml10n_label'] = label
    end

    if object.tags.name then
        local trans = osml10n.geo_transcript(objtype .. object.id, object.tags.name, object.bbox)
        if trans and #trans > 0 then
            object.tags['osml10n_name'] = trans
        end
    end

    if object.tags.highway then
        local trans = osml10n.get_streetname_from_tags(objtype .. object.id, object.tags, true, false, '|', 'de', object.bbox)
        if trans and #trans > 0 then
            object.tags['osml10n_streetname'] = trans
        end
    end

    return object.tags
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

