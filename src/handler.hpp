#ifndef HANDLER_HPP
#define HANDLER_HPP

/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm-tags-transform.
 *
 * Copyright (C) 2022 by Jochen Topf <jochen@topf.org>.
 */

#include <osmium/handler.hpp>
#include <osmium/index/map/flex_mem.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm.hpp>
#include <osmium/util/verbose_output.hpp>

extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

#include <memory>
#include <string>

enum class geom_proc_type
{
    none = 0,
    bbox = 1
};

enum class untagged_mode {
    drop = 0,
    copy = 1,
    process = 2
};

/**
 * When C++ code is called from the Lua code we sometimes need to know
 * in what context this happens. These are the possible contexts.
 */
enum class calling_context
{
    main = 0, ///< In main context, i.e. the Lua script outside any callbacks
    process_node = 1,     ///< In the process_node() callback
    process_way = 2,      ///< In the process_way() callback
    process_relation = 3, ///< In the process_relation() callback
};

/**
 * The flex output calls several user-defined Lua functions. They are
 * "prepared" by putting the function pointers on the Lua stack. Objects
 * of the class prepared_lua_function_t are used to hold the stack position
 * of the function which allows them to be called later using a symbolic
 * name.
 */
class prepared_lua_function_t
{
public:
    prepared_lua_function_t() noexcept = default;

    /**
     * Get function with the name "osm2pgsql.name" from Lua and put pointer
     * to it on the Lua stack.
     *
     * \param lua_state Current Lua state.
     * \param name Name of the function.
     * \param nresults The number of results this function is supposed to have.
     */
    prepared_lua_function_t(lua_State *lua_state, calling_context context,
                            const char *name, int nresults = 0);

    /// Return the index of the function on the Lua stack.
    int index() const noexcept { return m_index; }

    /// The name of the function.
    char const *name() const noexcept { return m_name; }

    /// The number of results this function is expected to have.
    int nresults() const noexcept { return m_nresults; }

    calling_context context() const noexcept { return m_calling_context; }

    /// Is this function defined in the users Lua code?
    explicit operator bool() const noexcept { return m_index != 0; }

private:
    char const *m_name = nullptr;
    int m_index = 0;
    int m_nresults = 0;
    calling_context m_calling_context = calling_context::main;
}; // class prepared_lua_function_t

class Handler : public osmium::handler::Handler
{
public:
    Handler(std::string const &filename, std::string const &index_name,
            geom_proc_type geom_proc, untagged_mode untagged);

    void set_buffer(osmium::memory::Buffer *buffer) { m_out_buffer = buffer; }

    void node(osmium::Node const &node);
    void way(osmium::Way const &way);
    void relation(osmium::Relation const &relation);

    void output_memory_used(osmium::VerboseOutput *vout);

private:
    using node_index_type =
        osmium::index::map::Map<osmium::unsigned_object_id_type,
                                osmium::Location>;
    using way_index_type =
        osmium::index::map::FlexMem<osmium::unsigned_object_id_type,
                                    osmium::Box>;

    lua_State *lua_state() noexcept { return m_lua_state.get(); }

    void find_bounding_box(osmium::Box *box, osmium::Way const &way);
    void find_bounding_box(osmium::Box *box, osmium::Relation const &relation);

    void call_lua_function(prepared_lua_function_t func,
                           osmium::OSMObject const &object,
                           osmium::Box const &box);

    bool handle_boolean_return(osmium::OSMObject const &object);

    osmium::memory::Buffer *m_out_buffer = nullptr;
    std::shared_ptr<lua_State> m_lua_state;
    prepared_lua_function_t m_process_node;
    prepared_lua_function_t m_process_way;
    prepared_lua_function_t m_process_relation;
    calling_context m_calling_context = calling_context::main;

    std::unique_ptr<node_index_type> m_node_location_index;
    way_index_type m_way_location_index;
    osmium::Node const *m_context_node = nullptr;
    osmium::Way const *m_context_way = nullptr;
    osmium::Relation const *m_context_relation = nullptr;

    geom_proc_type m_geom_proc;
    untagged_mode m_untagged;
    bool m_must_sort_node_index = true;
    bool m_must_sort_way_index = true;

}; // class Handler

#endif // HANDLER_HPP
