cmake_minimum_required(VERSION 2.6.3)

project(Plugin)

include(BroPlugin)

find_package(Napatech)

if ( NAPATECH_FOUND )
    include_directories(BEFORE ${NAPATECH_INCLUDE_DIR})
    bro_plugin_begin(Zeek Napatech)
    bro_plugin_cc(src/Plugin.cc)
    bro_plugin_cc(src/Napatech.cc)
    bro_plugin_bif(src/Napatech.bif)
    bro_plugin_link_library(${NAPATECH_LIBRARY})
    bro_plugin_dist_files(broctl/Napatech.py)
    bro_plugin_end()
    message(STATUS "Napatech prefix      : ${NAPATECH_ROOT_DIR}")
else ()
    message(FATAL_ERROR "Napatech library and/or headers not found.")
endif ()
