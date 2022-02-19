--
-- Remove all 'source' tags
--

function process(object)
    object.tags.source = nil
    return object.tags
end

ott.process_node = process
ott.process_way = process
ott.process_relation = process

