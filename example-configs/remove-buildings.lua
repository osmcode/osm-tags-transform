--
-- Remove all objects with 'building' tag from the input,
-- leave all others unchanged.
--

function process(object)
    if object.tags.building then
        return false
    end
    return true
end

ott.process_node = process
ott.process_way = process
ott.process_relation = process


