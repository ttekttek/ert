#  Copyright (C) 2011  Statoil ASA, Norway. 
#   
#  The file '__init__.py' is part of ERT - Ensemble based Reservoir Tool. 
#   
#  ERT is free software: you can redistribute it and/or modify 
#  it under the terms of the GNU General Public License as published by 
#  the Free Software Foundation, either version 3 of the License, or 
#  (at your option) any later version. 
#   
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or 
#  FITNESS FOR A PARTICULAR PURPOSE.   
#   
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
#  for more details. 
"""
ert - Ensemble Reservoir Tool - a package for reservoir modeling.

The ert package itself has no code, but contains several subpackages:

ert.ecl: Package for working with ECLIPSE files. The far most mature
   package in ert.

ert.job_queue: 

ert.util:

The ert package is based on wrapping the libriaries from the ERT C
code with ctypes; an essential part of ctypes approach is to load the
shared libraries with the ctypes.CDLL() function. The ctypes.CDLL()
function uses the standard methods of the operating system,
i.e. standard locations configured with ld.so.conf and the environment
variable LD_LIBRARY_PATH. 

To avoid conflict with other application using the ert libraries the
Python code should be able to locate the shared libraries without
(necessarily) using the LD_LIBRARY_PATH variable. The default
behaviour is to try to load from the library ../../lib64, but by using
the enviornment variable ERT_LIBRARY_PATH you can alter how ert looks
for shared libraries. This module will set the ert_lib_path of the
ert.cwrap.clib module; the actual loading will take place in that
module.

   1. By default the code will try to load the shared libraries from
      '../../lib64' relative to the location of this file.

   2. Depending on the value of ERT_LIBRARY_PATH two different
      behaviours can be imposed:

         Existing path: the package will look in the path pointed to
            by ERT_LIBRARY_PATH for shared libraries.

         Arbitrary value: the package will use standard load order for
         the operating system.

If the fixed path, given by the default ../../lib64 or ERT_LIBRARY_PATH
alternative fails, the loader will try the default load behaviour
before giving up completely.
"""
import os.path
import cwrap.clib
import sys
import warnings



try:
    import ert_site_init
except ImportError:
    pass


required_version_hex = 0x02060000


# 1. Start by initialing the ert_lib_path variable to None
ert_lib_path = None


# 2. Try to load the __ert_lib_path module; this module has been
#    configured by cmake during the build configuration process. The
#    module should contain the variable lib_path pointing to the
#    directory with shared object files.
try:
    import __ert_lib_path
    ert_lib_path = __ert_lib_path.lib_path
except ImportError:
    pass


# 3. Using the environment variable ERT_LIBRARY_PATH it is possible to
#    override the default algorithms. If the ERT_LIBRARY_PATH is set
#    to a non existing directory a warning will go to stderr and the
#    setting will be ignored.
env_lib_path = os.getenv("ERT_LIBRARY_PATH")
if env_lib_path:
    if os.path.isdir( env_lib_path ):
        ert_lib_path = os.getenv("ERT_LIBRARY_PATH")
    else:
        sys.stderr.write("Warning: Environment variable ERT_LIBRARY_PATH points to nonexisting directory:%s - ignored" % env_lib_path)
        

# Check that the final ert_lib_path setting corresponds to an existing
# directory.
if ert_lib_path:
    if not os.path.exists( ert_lib_path ):
        ert_lib_path = None
        


# Set the module variable ert_lib_path of the ert.cwrap.clib module;
# this is where the actual loading will be performed.
cwrap.clib.ert_lib_path = ert_lib_path

if sys.hexversion < required_version_hex:
    raise Exception("ERT Python requires at least version 2.6 of Python")

