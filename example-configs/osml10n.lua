--
-- Use osml10n library to localize names
--
-- This will transliterate various name tags:
-- * The new tag `osml10n_latin` will contain a name suitable for a label
--   in latin characters only. This is based on all available `name` and
--   `name:*` tags of each object.
-- * The new tag `osml10n_name` will contain a label with the original
--   name and the transliterated name. Street names will get additional
--   processing with some names abbreviated (Road -> Rd etc.). This is
--   based on all available `name` and `name:*` tags.
-- * The new tag `osml10n_transcription` will contain a transscribed
--   version of the `name` tag.
--
-- This is intended as a demo for some of the capabilities of the osml10n
-- library. For production use you have to adapt this to suit your needs.
--
-- Needs https://github.com/giggls/osml10n . Look there for more documentation,
-- the `osml10n.*` functions are documented in the Lua files in the directory
-- `lua_osml10/osml10n/`.
--

local osml10n = require 'osml10n.init'

function process(objtype, object)
    local label = osml10n.get_localized_name_from_tags(objtype .. object.id, object.tags, 'de', object.bbox)
    if label and #label > 0 then
        object.tags['osml10n_latin'] = label
    end

    if object.tags.name then
        local trans = osml10n.geo_transcript(objtype .. object.id, object.tags.name, object.bbox)
        if trans and #trans > 0 then
            object.tags['osml10n_transcription'] = trans
        end
    end

    if object.tags.highway then
        local trans = osml10n.get_streetname_from_tags(objtype .. object.id, object.tags, true, false, '|', 'de', object.bbox)
        if trans and #trans > 0 then
            object.tags['osml10n_name'] = trans
        end
    else
        local trans = osml10n.get_placename_from_tags(objtype .. object.id, object.tags, true, false, '|', 'de', object.bbox)
        if trans and #trans > 0 then
            object.tags['osml10n_name'] = trans
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

