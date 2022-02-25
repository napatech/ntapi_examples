# - Try to find Napatech includes.
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
# NAPATECH_ROOT_DIR		set this variable to the root installation of
#				napatech if this module has problems finding
# 				the proper installation path.
#
# Variables defined by this module:
# NAPATECH_FOUND		system has napatech NTAPI files.
# NAPATECH_INCLUDE_DIR		the napatech include directory.
# NAPATECH_LIBRARY		the napatech library

find_path(NAPATECH_ROOT_DIR
	NAMES include/nt.h
	HINTS /opt/napatech3
)

find_path(NAPATECH_INCLUDE_DIR
	NAMES nt.h
	HINTS ${NAPATECH_ROOT_DIR}/include
)

find_library(NAPATECH_LIBRARY
	NAMES libntapi.so
	HINTS ${NAPATECH_ROOT_DIR}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NAPATECH DEFAULT_MSG
	NAPATECH_LIBRARY
	NAPATECH_INCLUDE_DIR
)

mark_as_advanced(
	NAPATECH_ROOT_DIR
	NAPATECH_INCLUDE_DIR
	NAPATECH_LIBRARY
)
