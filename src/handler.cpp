/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm-tags-transform.
 *
 * Copyright (C) 2022 by Jochen Topf <jochen@topf.org>.
 */

#include "handler.hpp"

#include "lua-init.hpp"
#include "lua-utils.hpp"

#include <osmium/builder/osm_object_builder.hpp>

#include <iostream>
#include <vector>

prepared_lua_function_t::prepared_lua_function_t(lua_State *lua_state,
                                                 calling_context context,
                                                 char const *name, int nresults)
{
    int const index = lua_gettop(lua_state);

    lua_getfield(lua_state, 1, name);

    if (lua_type(lua_state, -1) == LUA_TFUNCTION) {
        m_index = index;
        m_name = name;
        m_nresults = nresults;
        m_calling_context = context;
        return;
    }

    if (lua_type(lua_state, -1) == LUA_TNIL) {
        return;
    }

    throw std::runtime_error{std::string{"ott."} + name +
                             " must be a function."};
}

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-pro-type-const-cast)
static void *osm_tag_transform_object_metatable =
    const_cast<void *>(static_cast<void const *>("ott.object_metatable"));
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-pro-type-const-cast)

static void push_osm_object_to_lua_stack(lua_State *lua_state,
                                         osmium::OSMObject const &object,
                                         osmium::Box const &box)
{
    assert(lua_state);

    // Table will have 3 fields (id, tags, bbox).
    constexpr int const max_table_size = 3;

    lua_createtable(lua_state, 0, max_table_size);

    luaX_add_table_int(lua_state, "id", object.id());

    lua_pushliteral(lua_state, "tags");
    lua_createtable(lua_state, 0, static_cast<int>(object.tags().size()));
    for (auto const &tag : object.tags()) {
        luaX_add_table_str(lua_state, tag.key(), tag.value());
    }
    lua_rawset(lua_state, -3);

    if (box.valid()) {
        lua_pushliteral(lua_state, "bbox");

        lua_createtable(lua_state, 4, 0);

        lua_pushinteger(lua_state, 1);
        lua_pushnumber(lua_state, box.bottom_left().lon());
        lua_rawset(lua_state, -3);

        lua_pushinteger(lua_state, 2);
        lua_pushnumber(lua_state, box.bottom_left().lat());
        lua_rawset(lua_state, -3);

        lua_pushinteger(lua_state, 3);
        lua_pushnumber(lua_state, box.top_right().lon());
        lua_rawset(lua_state, -3);

        lua_pushinteger(lua_state, 4);
        lua_pushnumber(lua_state, box.top_right().lat());
        lua_rawset(lua_state, -3);

        lua_rawset(lua_state, -3);
    }

    // Set the metatable of this object
    lua_pushlightuserdata(lua_state, osm_tag_transform_object_metatable);
    lua_gettable(lua_state, LUA_REGISTRYINDEX);
    lua_setmetatable(lua_state, -2);
}

Handler::Handler(std::string const &filename, std::string const &index_name,
                 geom_proc_type geom_proc, untagged_mode untagged)
: m_geom_proc(geom_proc), m_untagged(untagged)
{
    if (m_geom_proc != geom_proc_type::none) {
        const auto &map_factory =
            osmium::index::MapFactory<osmium::unsigned_object_id_type,
                                      osmium::Location>::instance();
        m_node_location_index = map_factory.create_map(index_name);
    }

    m_lua_state.reset(luaL_newstate(),
                      [](lua_State *state) { lua_close(state); });

    // Set up global lua libs
    luaL_openlibs(lua_state());

    // Set up global "ott" object
    lua_newtable(lua_state());

    luaX_add_table_str(lua_state(), "version", "0.1");

    /*std::string const dir_path =
    boost::filesystem::path{filename}.parent_path().string();
    luaX_add_table_str(lua_state(), "config_dir", dir_path.c_str());*/

    lua_setglobal(lua_state(), "ott");

    // Clean up stack
    lua_settop(lua_state(), 0);

    // Load compiled in init.lua
    if (luaL_dostring(lua_state(), lua_init())) {
        throw std::runtime_error{std::string{"Internal error in Lua setup: "} +
                                 lua_tostring(lua_state(), -1)};
    }

    // Store the global object "object_metatable" defined in the init.lua
    // script in the registry and then remove the global object. It will
    // later be used as metatable for OSM objects.
    lua_pushlightuserdata(lua_state(), osm_tag_transform_object_metatable);
    lua_getglobal(lua_state(), "object_metatable");
    lua_settable(lua_state(), LUA_REGISTRYINDEX);
    lua_pushnil(lua_state());
    lua_setglobal(lua_state(), "object_metatable");

    // Load user config file
    luaX_set_context(lua_state(), this);
    if (luaL_dofile(lua_state(), filename.c_str())) {
        throw std::runtime_error{std::string{"Error loading lua config: "} +
                                 lua_tostring(lua_state(), -1)};
    }

    // Check whether the process_* functions are available and store them on
    // the Lua stack for fast access later
    lua_getglobal(lua_state(), "ott");

    m_process_node = prepared_lua_function_t{
        lua_state(), calling_context::process_node, "process_node", 1};
    m_process_way = prepared_lua_function_t{
        lua_state(), calling_context::process_way, "process_way", 1};
    m_process_relation = prepared_lua_function_t{
        lua_state(), calling_context::process_relation, "process_relation", 1};

    lua_remove(lua_state(), 1); // global "ott"
}

void Handler::call_lua_function(prepared_lua_function_t func,
                                osmium::OSMObject const &object,
                                osmium::Box const &box)
{
    m_calling_context = func.context();

    lua_pushvalue(lua_state(), func.index()); // the function to call
    push_osm_object_to_lua_stack(lua_state(), object, box);

    luaX_set_context(lua_state(), this);
    if (luaX_pcall(lua_state(), 1, func.nresults())) {
        throw std::runtime_error{
            std::string{"Failed to execute Lua function '"} + func.name() +
            "': " + lua_tostring(lua_state(), -1)};
    }

    m_calling_context = calling_context::main;
}

bool Handler::handle_boolean_return(osmium::OSMObject const &object)
{
    auto const lt = lua_type(lua_state(), -1);

    // true means: return object as is
    // false means: remove object completely
    if (lt == LUA_TBOOLEAN) {
        if (lua_toboolean(lua_state(), -1)) {
            m_out_buffer->add_item(object);
        }
        return true;
    }

    if (lt != LUA_TTABLE) {
        throw std::runtime_error{
            "Processing functions should return true, false, or tags table"};
    }

    return false;
}

template <typename TBuilder>
void add_tags(lua_State *lua_state, TBuilder *builder)
{
    std::vector<std::pair<std::string, std::string>> tags;

    lua_pushnil(lua_state);
    while (lua_next(lua_state, -2) != 0) {
        if ((lua_type(lua_state, -1) != LUA_TSTRING) ||
            (lua_type(lua_state, -2) != LUA_TSTRING)) {
            throw std::runtime_error{
                "Keys and values in tags must be strings!"};
        }
        tags.emplace_back(lua_tostring(lua_state, -2),
                          lua_tostring(lua_state, -1));
        lua_pop(lua_state, 1); // value pushed by lua_next()
    }

    if (tags.empty()) {
        return;
    }

    std::sort(tags.begin(), tags.end());
    osmium::builder::TagListBuilder tl_builder{*builder};
    for (auto const &kv : tags) {
        try {
            tl_builder.add_tag(kv.first, kv.second);
        } catch (std::length_error const &e) {
            std::cerr << "Warning: Length of tag key or value exceeded. "
                         "Ignoring tag...\n";
        }
    }
}

void Handler::node(osmium::Node const &node)
{
    if (!m_process_node || (node.tags().empty() && m_untagged != untagged_mode::process)) {
        if (m_untagged == untagged_mode::copy) {
            m_out_buffer->add_item(node);
            m_out_buffer->commit();
        }
        return;
    }

    osmium::Box box;

    if (m_geom_proc != geom_proc_type::none) {
        m_node_location_index->set(node.positive_id(), node.location());
        box = osmium::Box{node.location(), node.location()};
    }

    m_context_node = &node;
    call_lua_function(m_process_node, node, box);
    m_context_node = nullptr;

    if (!handle_boolean_return(node)) {
        osmium::builder::NodeBuilder builder{*m_out_buffer};
        builder.set_id(node.id());
        builder.set_version(node.version());
        builder.set_changeset(node.changeset());
        builder.set_timestamp(node.timestamp());
        builder.set_uid(node.uid());
        builder.set_location(node.location());
        builder.set_user(node.user());

        add_tags(lua_state(), &builder);
    }

    lua_pop(lua_state(), 1); // return value (a table)

    m_out_buffer->commit();
}

void Handler::find_bounding_box(osmium::Box *box, osmium::Way const &way)
{
    for (auto const &nr : way.nodes()) {
        auto const location =
            m_node_location_index->get_noexcept(nr.positive_ref());
        if (location) {
            box->extend(location);
        }
    }
}

void Handler::way(osmium::Way const &way)
{
    if (!m_process_way || (way.tags().empty() && m_untagged != untagged_mode::process)) {
        if (m_untagged == untagged_mode::copy) {
            m_out_buffer->add_item(way);
            m_out_buffer->commit();
        }
        return;
    }

    osmium::Box box;

    if (m_geom_proc != geom_proc_type::none) {
        if (m_must_sort_node_index) {
            m_node_location_index->sort();
            m_must_sort_node_index = false;
        }
        find_bounding_box(&box, way);
        if (box.valid()) {
            m_way_location_index.set(way.positive_id(), box);
        }
    }

    m_context_way = &way;
    call_lua_function(m_process_way, way, box);
    m_context_way = nullptr;

    if (!handle_boolean_return(way)) {
        osmium::builder::WayBuilder builder{*m_out_buffer};
        builder.set_id(way.id());
        builder.set_version(way.version());
        builder.set_changeset(way.changeset());
        builder.set_timestamp(way.timestamp());
        builder.set_uid(way.uid());
        builder.set_user(way.user());

        builder.add_item(way.nodes());

        add_tags(lua_state(), &builder);
    }

    lua_pop(lua_state(), 1); // return value (a table)

    m_out_buffer->commit();
}

void Handler::find_bounding_box(osmium::Box *box,
                                osmium::Relation const &relation)
{
    for (auto const &member : relation.members()) {
        if (member.type() == osmium::item_type::node) {
            auto const location =
                m_node_location_index->get_noexcept(member.positive_ref());
            if (location) {
                box->extend(location);
            }
        } else if (member.type() == osmium::item_type::way) {
            auto const wbox =
                m_way_location_index.get_noexcept(member.positive_ref());
            if (wbox.valid()) {
                box->extend(wbox);
            }
        }
    }
}

void Handler::relation(osmium::Relation const &relation)
{
    if (!m_process_relation || (relation.tags().empty() && m_untagged != untagged_mode::process)) {
        if (m_untagged == untagged_mode::copy) {
            m_out_buffer->add_item(relation);
            m_out_buffer->commit();
        }
        return;
    }

    osmium::Box box;

    if (m_geom_proc != geom_proc_type::none) {
        if (m_must_sort_way_index) {
            m_way_location_index.sort();
            m_must_sort_way_index = false;
        }
        find_bounding_box(&box, relation);
    }

    m_context_relation = &relation;
    call_lua_function(m_process_relation, relation, box);
    m_context_relation = nullptr;

    if (!handle_boolean_return(relation)) {
        osmium::builder::RelationBuilder builder{*m_out_buffer};
        builder.set_id(relation.id());
        builder.set_version(relation.version());
        builder.set_changeset(relation.changeset());
        builder.set_timestamp(relation.timestamp());
        builder.set_uid(relation.uid());
        builder.set_user(relation.user());

        builder.add_item(relation.members());

        add_tags(lua_state(), &builder);
    }

    lua_pop(lua_state(), 1); // return value

    m_out_buffer->commit();
}

void Handler::output_memory_used(osmium::VerboseOutput *vout)
{
    constexpr auto const mbytes = 1024UL * 1024UL;

    if (m_node_location_index) {
        *vout << "Memory used for node locations: "
              << (m_node_location_index->used_memory() / mbytes) << "MBytes\n";
        *vout << "Memory used for way locations: "
              << (m_way_location_index.used_memory() / mbytes) << "MBytes\n";
    }
}
