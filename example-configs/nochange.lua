--
-- Simple config that returns all tags unchanged.
--

function ott.process_node(object)
    return object.tags
end

function ott.process_way(object)
    return object.tags
end

function ott.process_relation(object)
    return object.tags
end

