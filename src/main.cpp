/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm-tags-transform.
 *
 * Copyright (C) 2022 by Jochen Topf <jochen@topf.org>.
 */

#include "handler.hpp"

#include <osmium/index/map/all.hpp>
#include <osmium/index/node_locations_map.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/io/writer.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm.hpp>
#include <osmium/util/memory.hpp>
#include <osmium/util/verbose_output.hpp>

#include <array>
#include <cassert>
#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <string>

void show_help()
{
    std::cout << "Usage: osm-tags-transform [OPTIONS] INPUT_FILE\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config-file=CONFIG.lua  Set config file\n";
    std::cout << "  -f, --output-format=FORMAT    Set output file format\n";
    std::cout << "  -g, --geom-proc=TYPE          Geometry processing ('none' "
                 "(default) or 'bbox')\n";
    std::cout << "  -h, --help                    Show this help\n";
    std::cout << "  -i, --index-type=INDEX        Set index type (default: "
                 "'flex_mem')\n";
    std::cout << "  -I, --show-index-types        Show available index types\n";
    std::cout << "  -o, --output=OUTPUT_FILE      Set output file name\n";
    std::cout << "  -O, --overwrite               Allow an existing output file to be overwritten.\n";
    std::cout << "  -v, --verbose                 Enable verbose mode\n";
    std::cout << "  -V, --version                 Show version\n";
}

static void show_index_types()
{
    const auto &map_factory =
        osmium::index::MapFactory<osmium::unsigned_object_id_type,
                                  osmium::Location>::instance();
    for (const auto &map_type : map_factory.map_types()) {
        std::cout << map_type << '\n';
    }
}

static std::string check_index_type(const std::string &index_type_name)
{
    std::string type{index_type_name};
    const auto pos = type.find(',');
    if (pos != std::string::npos) {
        type.resize(pos);
    }

    const auto &map_factory =
        osmium::index::MapFactory<osmium::unsigned_object_id_type,
                                  osmium::Location>::instance();
    if (!map_factory.has_map_type(type)) {
        throw std::runtime_error{
            "Unknown index type '" + index_type_name +
            "'. Use --show-index-types or -I to get a list."};
    }

    return index_type_name;
}

static geom_proc_type check_geom_proc(std::string const &geom_proc_name)
{
    if (geom_proc_name == "none") {
        return geom_proc_type::none;
    }

    if (geom_proc_name == "bbox") {
        return geom_proc_type::bbox;
    }

    throw std::runtime_error{"Unknown geometry processing '" + geom_proc_name +
                             "'. Use 'none' or 'bbox'."};
}

int main(int argc, char *argv[])
{
    char const *const short_options = "c:f:g:hi:Io:OvV";

    std::array<option, 11> const long_options = {
        {{"config-file", required_argument, nullptr, 'c'},
         {"output-format", required_argument, nullptr, 'f'},
         {"geom-proc", required_argument, nullptr, 'g'},
         {"help", no_argument, nullptr, 'h'},
         {"index-type", required_argument, nullptr, 'i'},
         {"show-index-types", no_argument, nullptr, 'I'},
         {"output", required_argument, nullptr, 'o'},
         {"overwrite", no_argument, nullptr, 'O'},
         {"verbose", no_argument, nullptr, 'v'},
         {"version", no_argument, nullptr, 'V'},
         {nullptr, 0, nullptr, 0}}};

    std::string config_filename;
    std::string input_filename;
    std::string output_filename;
    std::string output_format;
    std::string index_name{"flex_mem"};
    geom_proc_type geom_proc = geom_proc_type::none;
    osmium::io::overwrite overwrite = osmium::io::overwrite::no;

    bool verbose = false;

    try {
        optind = 0;
        int c = 0;
        while (-1 != (c = getopt_long(argc, argv, short_options,
                                      long_options.data(), nullptr))) {
            switch (c) {
            case 'c':
                config_filename = optarg;
                break;
            case 'f':
                output_format = optarg;
                break;
            case 'g':
                geom_proc = check_geom_proc(optarg);
                break;
            case 'h':
                show_help();
                return 0;
            case 'i':
                index_name = check_index_type(optarg);
                break;
            case 'I':
                show_index_types();
                return 0;
            case 'o':
                output_filename = optarg;
                break;
            case 'O':
                overwrite = osmium::io::overwrite::allow;
                break;
            case 'v':
                verbose = true;
                break;
            case 'V':
                std::cout << "osm-tags-transform " << PROJECT_VERSION << "\n";
                return 0;
            default:
                std::cerr << "Usage error. Try with --help.\n";
                return 2;
            }
        }

        if (config_filename.empty()) {
            std::cerr << "Missing config file. Try with --help.\n";
            return 2;
        }

        if (output_filename.empty() && output_format.empty()) {
            std::cerr << "Missing output file or format. Try with --help.\n";
            return 2;
        }

        if (optind >= argc) {
            std::cerr << "Missing input file. Try with --help.\n";
            return 2;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        input_filename = argv[optind];
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }

    try {
        osmium::VerboseOutput vout{verbose};
        vout << "osm-tags-transform " << PROJECT_VERSION << " started\n";
        if (geom_proc == geom_proc_type::none) {
            vout << "No geometry processing. bbox will not be available\n";
        } else {
            vout << "Geometry processing enabled. bbox will be available\n";
            vout << "Using index type '" << index_name << "'\n";
        }

        Handler handler{config_filename, index_name, geom_proc};

        vout << "Writing into '" << output_filename << "'.\n";
        osmium::io::File output_file{output_filename, output_format};
        osmium::io::Writer writer{output_file, overwrite};

        vout << "Start processing '" << input_filename << "'...\n";
        osmium::io::Reader reader{input_filename};
        writer.set_header(reader.header());
        while (osmium::memory::Buffer buffer = reader.read()) {
            osmium::memory::Buffer out_buffer{
                buffer.committed(), osmium::memory::Buffer::auto_grow::yes};
            handler.set_buffer(&out_buffer);
            osmium::apply(buffer, handler);
            writer(std::move(out_buffer));
        }
        reader.close();
        writer.close();
        vout << "Done processing.\n";

        handler.output_memory_used(&vout);

        osmium::MemoryUsage mem;
        if (mem.peak() != 0) {
            vout << "Overall memory usage: peak=" << mem.peak()
                 << "MByte current=" << mem.current() << "MByte\n";
        }
    } catch (std::exception const &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
