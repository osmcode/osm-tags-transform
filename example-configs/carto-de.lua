--
-- Use osml10n library to localize names for map rendering
--
-- This will generate a name_l10n_<lang> tag which can be used for rendering
-- instead of generic name tag
--
-- Needs https://github.com/giggls/osml10n
--

-- change this to your (latin script) target language
local targetlang = 'de'

local osml10n = require 'osml10n.init'

function process(objtype, object)

    local l10n_tag = 'name_l10n_' .. targetlang
    
    -- io.stderr:write("http://osm.org/" .. objtype .. "/" .. object.id .. '\n')
    
    -- ignore any object which does not have name or name:de
    if ((object.tags.name ~= nil) or (object.tags['name:' .. targetlang]) ~= nil) then
      if object.tags.highway then
          local name = osml10n.get_streetname_from_tags(objtype .. object.id, object.tags, false, ' - ', targetlang, object.bbox)
          if name and #name > 0 then
              object.tags[l10n_tag] = name
          end
      else
         local name = osml10n.get_placename_from_tags(objtype .. object.id, object.tags, true, '\n', targetlang, object.bbox)
          if name and #name > 0 then
              object.tags[l10n_tag] = name
          end
      end
      return object.tags
    end
    return false
end

function ott.process_node(object)
    return process('node', object)
end

function ott.process_way(object)
    return process('way', object)
end

function ott.process_relation(object)
    return process('relation', object)
end

