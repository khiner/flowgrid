# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# SoundIO_FOUND
# SoundIO_INCLUDE_DIR
# SoundIO_LIBRARY

find_path(SOUNDIO_INCLUDE_DIR NAMES soundio/soundio.h)

find_library(SOUNDIO_LIBRARY NAMES soundio)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SoundIO DEFAULT_MSG SOUNDIO_LIBRARY SOUNDIO_INCLUDE_DIR)

mark_as_advanced(SOUNDIO_INCLUDE_DIR SOUNDIO_LIBRARY)
