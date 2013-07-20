#
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
#
#
# gen_win_dependencies.py 
#
#   base class for generating windows projects, containing the
#   dependency locator code shared between the test runner and
#   the make file generators
#

import os
import sys
import fnmatch
import re
import subprocess
import string

if sys.version_info[0] >= 3:
  # Python >=3.0
  from io import StringIO
else:
  # Python <3.0
  try:
    from cStringIO import StringIO
  except ImportError:
    from StringIO import StringIO

import gen_base
import ezt

class SVNCommonLibrary:

  def __init__(self, name, include_dirs, lib_dir, lib_name, version=None,
               debug_lib_dir=None, debug_lib_name=None, dll_dir=None,
               dll_name=None, debug_dll_dir=None, debug_dll_name=None,
               is_src=False, defines=[]):
    self.name = name
    if include_dirs:
      self.include_dirs = include_dirs if isinstance(include_dirs, list) \
                                       else [include_dirs]
    else:
      self.include_dirs = []
    self.defines = defines if not defines or isinstance(defines, list) else [defines]
    self.lib_dir = lib_dir
    self.lib_name = lib_name
    self.version = version
    self.dll_dir = dll_dir
    self.dll_name = dll_name
    self.is_src = is_src

    if debug_lib_dir:
      self.debug_lib_dir = debug_lib_dir
    else:
      self.debug_lib_dir = lib_dir
      
    if debug_lib_name:
      self.debug_lib_name = debug_lib_name
    else:
      self.debug_lib_name = lib_name
      
    if debug_dll_dir:
      self.debug_dll_dir = debug_dll_dir
    else:
      self.debug_dll_dir = dll_dir
      
    if debug_dll_name:
      self.debug_dll_name = debug_dll_name
    else:
      self.debug_dll_name = dll_name

class GenDependenciesBase(gen_base.GeneratorBase):
  """This intermediate base class exists to be instantiated by win-tests.py,
  in order to obtain information from build.conf and library paths without
  actually doing any generation."""
  _extension_map = {
    ('exe', 'target'): '.exe',
    ('exe', 'object'): '.obj',
    ('lib', 'target'): '.dll',
    ('lib', 'object'): '.obj',
    ('pyd', 'target'): '.pyd',
    ('pyd', 'object'): '.obj',
    }

  _libraries = {}     # Dict of SVNCommonLibrary instances of found libraries

  _optional_libraries = [  # List of optional libraries to suppress warnings
        'db',
        'intl',
        'serf',
        'sasl',
        'perl',
        'python',
        'ruby',
        'java_sdk',

        # So optional, we don't even have any code to detect them on Windows
        'apr_memcache',
        'magic',
  ]

  def parse_options(self, options):
    self.apr_path = 'apr'
    self.apr_util_path = 'apr-util'
    self.apr_iconv_path = 'apr-iconv'
    self.serf_path = None
    self.bdb_path = 'db4-win32'
    self.httpd_path = None
    self.libintl_path = None
    self.zlib_path = 'zlib'
    self.openssl_path = None
    self.jdk_path = None
    self.junit_path = None
    self.swig_path = None
    self.vs_version = '2002'
    self.sln_version = '7.00'
    self.vcproj_version = '7.00'
    self.vcproj_extension = '.vcproj'
    self.sqlite_path = 'sqlite-amalgamation'
    self.skip_sections = { 'mod_dav_svn': None,
                           'mod_authz_svn': None,
                           'mod_dontdothat' : None,
                           'libsvn_auth_kwallet': None,
                           'libsvn_auth_gnome_keyring': None }

    # Instrumentation options
    self.disable_shared = None
    self.static_apr = None
    self.static_openssl = None
    self.instrument_apr_pools = None
    self.instrument_purify_quantify = None
    self.configure_apr_util = None
    self.sasl_path = None

    # NLS options
    self.enable_nls = None

    # ML (assembler) is disabled by default; use --enable-ml to detect
    self.enable_ml = None

    for opt, val in options:
      if opt == '--with-berkeley-db':
        self.bdb_path = val
      elif opt == '--with-apr':
        self.apr_path = val
      elif opt == '--with-apr-util':
        self.apr_util_path = val
      elif opt == '--with-apr-iconv':
        self.apr_iconv_path = val
      elif opt == '--with-serf':
        self.serf_path = val
      elif opt == '--with-httpd':
        self.httpd_path = val
        del self.skip_sections['mod_dav_svn']
        del self.skip_sections['mod_authz_svn']
        del self.skip_sections['mod_dontdothat']
      elif opt == '--with-libintl':
        self.libintl_path = val
        self.enable_nls = 1
      elif opt == '--with-jdk':
        self.jdk_path = val
      elif opt == '--with-junit':
        self.junit_path = val
      elif opt == '--with-zlib':
        self.zlib_path = val
      elif opt == '--with-swig':
        self.swig_path = val
      elif opt == '--with-sqlite':
        self.sqlite_path = val
      elif opt == '--with-sasl':
        self.sasl_path = val
      elif opt == '--with-openssl':
        self.openssl_path = val
      elif opt == '--enable-purify':
        self.instrument_purify_quantify = 1
        self.instrument_apr_pools = 1
      elif opt == '--enable-quantify':
        self.instrument_purify_quantify = 1
      elif opt == '--enable-pool-debug':
        self.instrument_apr_pools = 1
      elif opt == '--enable-nls':
        self.enable_nls = 1
      elif opt == '--enable-bdb-in-apr-util':
        self.configure_apr_util = 1
      elif opt == '--enable-ml':
        self.enable_ml = 1
      elif opt == '--disable-shared':
        self.disable_shared = 1
      elif opt == '--with-static-apr':
        self.static_apr = 1
      elif opt == '--with-static-openssl':
        self.static_openssl = 1
      elif opt == '--vsnet-version':
        if val == '2002' or re.match('7(\.\d+)?$', val):
          self.vs_version = '2002'
          self.sln_version = '7.00'
          self.vcproj_version = '7.00'
          self.vcproj_extension = '.vcproj'
        elif val == '2003' or re.match('8(\.\d+)?$', val):
          self.vs_version = '2003'
          self.sln_version = '8.00'
          self.vcproj_version = '7.10'
          self.vcproj_extension = '.vcproj'
        elif val == '2005' or re.match('9(\.\d+)?$', val):
          self.vs_version = '2005'
          self.sln_version = '9.00'
          self.vcproj_version = '8.00'
          self.vcproj_extension = '.vcproj'
        elif val == '2008' or re.match('10(\.\d+)?$', val):
          self.vs_version = '2008'
          self.sln_version = '10.00'
          self.vcproj_version = '9.00'
          self.vcproj_extension = '.vcproj'
        elif val == '2010':
          self.vs_version = '2010'
          self.sln_version = '11.00'
          self.vcproj_version = '10.0'
          self.vcproj_extension = '.vcxproj'
        elif val == '2012' or val == '11':
          self.vs_version = '2012'
          self.sln_version = '12.00'
          self.vcproj_version = '11.0'
          self.vcproj_extension = '.vcxproj'
        elif re.match('^1\d+$', val):
          self.vsversion = val
          self.sln_version = '12.00'
          self.vcproj_version = val + '.0'
          self.vcproj_extension = '.vcxproj'
        else:
          print('WARNING: Unknown VS.NET version "%s",'
                 ' assuming "%s"\n' % (val, '7.00'))


  def __init__(self, fname, verfname, options, find_libs=True):

    # parse (and save) the options that were passed to us
    self.parse_options(options)

    # Initialize parent
    gen_base.GeneratorBase.__init__(self, fname, verfname, options)

    # These files will be excluded from the build when they're not
    # explicitly listed as project sources.
    self._excluded_from_build = frozenset(self.private_includes
                                          + self.private_built_includes)

    if find_libs:
      self.find_libraries(False)
      
  def find_libraries(self, show_warnings):

    # Required dependencies
    self._find_apr()
    self._find_apr_util_and_expat()
    self._find_zlib()
    self._find_sqlite(show_warnings)

    # Optional dependencies
    self._find_bdb(show_warnings)
    self._find_openssl(show_warnings)
    self._find_serf(show_warnings)
    self._find_sasl(show_warnings)
    self._find_libintl(show_warnings)

    # Swig (optional) dependencies
    self._find_perl(show_warnings)
    self._find_python(show_warnings)
    self._find_ruby(show_warnings)
    self._find_jdk(show_warnings)

    if show_warnings:
      # Find the installed SWIG version to adjust swig options
      self._find_swig()


  def _find_apr(self):
    "Find the APR library and version"

    minimal_apr_version = (0, 9, 0)
    
    if not self.apr_path:
      sys.stderr.write("ERROR: Use '--with-apr' option to configure APR " + \
                       "location.\n")
      sys.exit(1)                       

    inc_base = os.path.join(self.apr_path, 'include')

    if os.path.isfile(os.path.join(inc_base, 'apr-1', 'apr_version.h')):
      inc_path = os.path.join(inc_base, 'apr-1')
    elif os.path.isfile(os.path.join(inc_base, 'apr_version.h')):
      inc_path = inc_base
    else:
      sys.stderr.write("ERROR: '%s' not found.\n" % version_file_path)
      sys.stderr.write("Use '--with-apr' option to configure APR location.\n")
      sys.exit(1)

    version_file_path = os.path.join(inc_path, 'apr_version.h')
    txt = open(version_file_path).read()

    vermatch = re.search(r'^\s*#define\s+APR_MAJOR_VERSION\s+(\d+)', txt, re.M)
    major = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+APR_MINOR_VERSION\s+(\d+)', txt, re.M)
    minor = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+APR_PATCH_VERSION\s+(\d+)', txt, re.M)
    patch = int(vermatch.group(1))

    version = (major, minor, patch)
    self.apr_version = apr_version = '%d.%d.%d' % version

    if version < minimal_apr_version:
      sys.stderr.write("ERROR: apr %s or higher is required "
                       "(%s found)\n" % (
                          '.'.join(str(v) for v in minimal_apr_version),
                          self.apr_version))
      sys.exit(1)

    suffix = ''
    if major > 0:
        suffix = '-%d' % major

    defines = []
    if self.static_apr:
      lib_name = 'apr%s.lib' % suffix
      lib_dir = os.path.join(self.apr_path, 'LibR')
      dll_dir = None
      debug_dll_dir = None
      defines.extend(["APR_DECLARE_STATIC"])

      if not os.path.isdir(lib_dir) and \
         os.path.isfile(os.path.join(self.apr_path, 'lib', lib_name)):
        # Installed APR instead of APR-Source
        lib_dir = os.path.join(self.apr_path, 'lib')
        debug_lib_dir = None
      else:
        debug_lib_dir = os.path.join(self.apr_path, 'LibD')
    else:
      lib_name = 'libapr%s.lib' % suffix
      lib_dir = os.path.join(self.apr_path, 'Release')
      
      if not os.path.isdir(lib_dir) and \
         os.path.isfile(os.path.join(self.apr_path, 'lib', lib_name)):
        # Installed APR instead of APR-Source
        lib_dir = os.path.join(self.apr_path, 'lib')
        debug_lib_dir = lib_dir
      else:
        debug_lib_dir = os.path.join(self.apr_path, 'Debug')
        
      dll_name = 'libapr%s.dll' % suffix
      if os.path.isfile(os.path.join(lib_dir, dll_name)):
        dll_dir = lib_dir
        debug_dll_dir = debug_lib_dir
      else:
        dll_dir = os.path.join(self.apr_path, 'bin')
        debug_dll_dir = None
      
    self._libraries['apr'] = SVNCommonLibrary('apr', inc_path, lib_dir, lib_name,
                                              apr_version,
                                              debug_lib_dir=debug_lib_dir,
                                              dll_dir=dll_dir,
                                              dll_name=dll_name,
                                              debug_dll_dir=debug_dll_dir,
                                              defines=defines)

  def _find_apr_util_and_expat(self):
    "Find the APR-util library and version"

    minimal_aprutil_version = (0, 9, 0)

    inc_base = os.path.join(self.apr_util_path, 'include')

    if os.path.isfile(os.path.join(inc_base, 'apr-1', 'apu_version.h')):
      inc_path = os.path.join(inc_base, 'apr-1')
    elif os.path.isfile(os.path.join(inc_base, 'apu_version.h')):
      inc_path = inc_base
    else:
      sys.stderr.write("ERROR: 'apu_version' not found.\n")
      sys.stderr.write("Use '--with-apr-util' option to configure APR-Util location.\n")
      sys.exit(1)

    version_file_path = os.path.join(inc_path, 'apu_version.h')

    if not os.path.exists(version_file_path):
      sys.stderr.write("ERROR: '%s' not found.\n" % version_file_path);
      sys.stderr.write("Use '--with-apr-util' option to configure APR-Util location.\n");
      sys.exit(1)

    txt = open(version_file_path).read()

    vermatch = re.search(r'^\s*#define\s+APU_MAJOR_VERSION\s+(\d+)', txt, re.M)
    major = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+APU_MINOR_VERSION\s+(\d+)', txt, re.M)
    minor = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+APU_PATCH_VERSION\s+(\d+)', txt, re.M)
    patch = int(vermatch.group(1))

    version = (major, minor, patch)
    self.aprutil_version = aprutil_version = '%d.%d.%d' % version
    
    if version < minimal_aprutil_version:
      sys.stderr.write("ERROR: apr-util %s or higher is required "
                       "(%s found)\n" % (
                          '.'.join(str(v) for v in minimal_aprutil_version),
                          aprutil_version))
      sys.exit(1)

    suffix = ''
    if major > 0:
        suffix = '-%d' % major

    defines = []
    if self.static_apr:
      lib_name = 'aprutil%s.lib' % suffix
      lib_dir = os.path.join(self.apr_util_path, 'LibR')
      dll_dir = None
      debug_dll_dir = None
      defines.extend(["APR_DECLARE_STATIC"])
      
      if not os.path.isdir(lib_dir) and \
         os.path.isfile(os.path.join(self.apr_util_path, 'lib', lib_name)):
        # Installed APR-Util instead of APR-Util-Source
        lib_dir = os.path.join(self.apr_util_path, 'lib')
        debug_lib_dir = None
      else:
        debug_lib_dir = os.path.join(self.apr_util_path, 'LibD')
    else:
      lib_name = 'libaprutil%s.lib' % suffix
      lib_dir = os.path.join(self.apr_util_path, 'Release')
      
      if not os.path.isdir(lib_dir) and \
         os.path.isfile(os.path.join(self.apr_util_path, 'lib', lib_name)):
        # Installed APR-Util instead of APR-Util-Source
        lib_dir = os.path.join(self.apr_util_path, 'lib')
        debug_lib_dir = lib_dir
      else:
        debug_lib_dir = os.path.join(self.apr_util_path, 'Debug')
        
      dll_name = 'libaprutil%s.dll' % suffix
      if os.path.isfile(os.path.join(lib_dir, dll_name)):
        dll_dir = lib_dir
        debug_dll_dir = debug_lib_dir
      else:
        dll_dir = os.path.join(self.apr_util_path, 'bin')
        debug_dll_dir = None

    self._libraries['aprutil'] = SVNCommonLibrary('apr-util', inc_path, lib_dir,
                                                   lib_name,
                                                   aprutil_version,
                                                   debug_lib_dir=debug_lib_dir,
                                                   dll_dir=dll_dir,
                                                   dll_name=dll_name,
                                                   debug_dll_dir=debug_dll_dir,
                                                   defines=defines)

    # And now find expat
    # If we have apr-util as a source location, it is in a subdir.
    # If we have an install package it is in the lib subdir
    if os.path.exists(os.path.join(self.apr_util_path, 'xml/expat')):
      inc_path = os.path.join(self.apr_util_path, 'xml/expat/lib')
      lib_dir = os.path.join(self.apr_util_path, 'xml/expat/lib/LibR')
      debug_lib_dir = os.path.join(self.apr_util_path, 'xml/expat/lib/LibD')
    else:
      inc_path = os.path.join(self.apr_util_path, 'include')
      lib_dir = os.path.join(self.apr_util_path, 'lib')
      debug_lib_dir = None
      
    version_file_path = os.path.join(inc_path, 'expat.h')

    if not os.path.exists(version_file_path):
      sys.stderr.write("ERROR: '%s' not found.\n" % version_file_path);
      sys.stderr.write("Use '--with-apr-util' option to configure APR-Util's XML location.\n");
      sys.exit(1)
      
    txt = open(version_file_path).read()

    vermatch = re.search(r'^\s*#define\s+XML_MAJOR_VERSION\s+(\d+)', txt, re.M)
    major = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+XML_MINOR_VERSION\s+(\d+)', txt, re.M)
    minor = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+XML_MICRO_VERSION\s+(\d+)', txt, re.M)
    patch = int(vermatch.group(1))

    version = (major, minor, patch)
    xml_version = '%d.%d.%d' % version

    self._libraries['xml'] = SVNCommonLibrary('expat', inc_path, lib_dir,
                                               'xml.lib', xml_version,
                                               debug_lib_dir = debug_lib_dir,
                                               defines=['XML_STATIC'])

  def _find_zlib(self):
    "Find the ZLib library and version"

    minimal_zlib_version = (1, 2, 5)

    if not self.zlib_path or not os.path.isdir(self.zlib_path):
      sys.stderr.write("ERROR: '%s' not found.\n" % self.zlib_path);
      sys.stderr.write("Use '--with-zlib' option to configure ZLib location.\n");
      sys.exit(1)

    if os.path.isdir(os.path.join(self.zlib_path, 'include')):
      # We have an install location
      inc_path = os.path.join(self.zlib_path, 'include')
      lib_path = os.path.join(self.zlib_path, 'lib')
      is_src = False
      
      # Different build options produce different library names :(
      if os.path.exists(os.path.join(lib_path, 'zlibstatic.lib')):
        # CMake default: zlibstatic.lib (static) and zlib.lib (dll)
        lib_name = 'zlibstatic.lib'
      else:
        # Standard makefile produces zlib.lib (static) and zdll.lib (dll)      
        lib_name = 'zlib.lib'
      debug_lib_name = None
    else:
      # We have a source location
      inc_path = lib_path = self.zlib_path
      is_src = True
      lib_name = 'zlibstat.lib'
      debug_lib_name = 'zlibstatD.lib'
      
    version_file_path = os.path.join(inc_path, 'zlib.h')
    
    if not os.path.exists(version_file_path):
      sys.stderr.write("ERROR: '%s' not found.\n" % version_file_path);
      sys.stderr.write("Use '--with-zlib' option to configure ZLib location.\n");
      sys.exit(1)
      
    txt = open(version_file_path).read()
    vermatch = re.search(
                r'^\s*#define\s+ZLIB_VERSION\s+"(\d+)\.(\d+)\.(\d+)(?:\.\d)?"',
                 txt, re.M)

    version = tuple(map(int, vermatch.groups()))
    self.zlib_version = '%d.%d.%d' % version
    
    if version < minimal_zlib_version:
      sys.stderr.write("ERROR: ZLib %s or higher is required "
                       "(%s found)\n" % (
                          '.'.join(str(v) for v in minimal_zlib_version),
                          self.zlib_version))
      sys.exit(1)

    self._libraries['zlib'] = SVNCommonLibrary('zlib', inc_path, lib_path, lib_name,
                                                self.zlib_version,
                                                debug_lib_name=debug_lib_name,
                                                is_src=is_src)
    if is_src:
      self._find_ml()

  def _find_bdb(self, show_warnings):
    "Find the Berkeley DB library and version"

    # Default to not found
    self.bdb_lib = None

    inc_path = os.path.join(self.bdb_path, 'include')
    db_h_path = os.path.join(inc_path, 'db.h')

    if not self.bdb_path or not os.path.isfile(db_h_path):
      if show_warnings and self.bdb_path:
        print('WARNING: \'%s\' not found' % (db_h_path,))
        print("Use '--with-berkeley-db' to configure BDB location.");
      return

    # Obtain bdb version from db.h
    txt = open(db_h_path).read()

    maj_match = re.search(r'DB_VERSION_MAJOR\s+(\d+)', txt)
    min_match = re.search(r'DB_VERSION_MINOR\s+(\d+)', txt)
    patch_match = re.search(r'DB_VERSION_PATCH\s+(\d+)', txt)

    if maj_match and min_match and patch_match:
      ver = (int(maj_match.group(1)),
             int(min_match.group(1)),
             int(patch_match.group(1)))
    else:
      return

    version = '%d.%d.%d' % ver
    versuffix = '%d%d' % (ver[0], ver[1])

    # Before adding "60" to this list, see build/ac-macros/berkeley-db.m4.
    if versuffix not in (
            '50', '51', '52', '53',
            '40', '41', '42', '43', '44', '45', '46', '47', '48',
       ):
      return

    lib_dir = os.path.join(self.bdb_path, 'lib')
    lib_name = 'libdb%s.lib' % (versuffix,)

    if not os.path.exists(os.path.join(lib_dir, lib_name)):
      return

    # Do we have a debug version?
    debug_lib_name = 'libdb%sd.lib' % (versuffix,)
    if not os.path.isfile(os.path.join(lib_dir, debug_lib_name)):
      debug_lib_name = None

    dll_dir = os.path.join(self.bdb_path, 'bin')

    # Are there binaries we should copy for testing?
    dll_name = os.path.splitext(lib_name)[0] + '.dll'
    if not os.path.isfile(os.path.join(dll_dir, dll_name)):
      dll_name = None

    if debug_lib_name:
      debug_dll_name = os.path.splitext(debug_lib_name)[0] + '.dll'
      if not os.path.isfile(os.path.join(dll_dir, debug_dll_name)):
        debug_dll_name = None
    else:
      debug_dll_name = None

    # Usually apr-util doesn't find BDB on Windows, so we help apr-util
    # by defining the value ourselves (Legacy behavior)
    defines = ['APU_HAVE_DB=1', 'SVN_LIBSVN_FS_LINKS_FS_BASE']

    self._libraries['db'] = SVNCommonLibrary('db', inc_path, lib_dir, lib_name,
                                              version,
                                              debug_lib_name=debug_lib_name,
                                              dll_dir=dll_dir,
                                              dll_name=dll_name,
                                              debug_dll_name=debug_dll_name,
                                              defines=defines)

    # For compatibility with old code
    self.bdb_lib = self._libraries['db'].lib_name

  def _find_openssl(self, show_warnings):
    "Find openssl"
    
    if not self.openssl_path:
      return
      
    version_path = os.path.join(self.openssl_path, 'inc32/openssl/opensslv.h')
    if os.path.isfile(version_path):
      # We have an OpenSSL Source location
      # For legacy reason
      inc_dir = os.path.join(self.openssl_path, 'inc32')
      if self.static_openssl:
        lib_dir = os.path.join(self.openssl_path, 'out32')
        bin_dir = None
      else:
        lib_dir = os.path.join(self.openssl_path, 'out32dll')
        bin_dir = lib_dir
    elif os.path.isfile(os.path.join(self.openssl_path,
                        'include/openssl/opensslv.h')):
      version_path = os.path.join(self.openssl_path,
                                  'include/openssl/opensslv.h')
      inc_dir = os.path.join(self.openssl_path, 'include')
      lib_dir = os.path.join(self.openssl_path, 'lib')
      if self.static_openssl:
        bin_dir = None
      else:
        bin_dir = os.path.join(self.openssl_path, 'bin')
    else:
      if show_warning:
        print('WARNING: \'opensslv.h\' not found')
        print("Use '--with-openssl' to configure openssl location.");
      return

    txt = open(version_path).read()

    vermatch = re.search(
      r'#define OPENSSL_VERSION_TEXT\s+"OpenSSL\s+((\d+)\.(\d+).(\d+)([^ -]*))',
      txt)
  
    version = (int(vermatch.group(2)), 
               int(vermatch.group(3)),
               int(vermatch.group(4)))
    openssl_version = vermatch.group(1)
  
    self._libraries['openssl'] = SVNCommonLibrary('openssl', inc_dir, lib_dir,
                                                  'ssleay32.lib',
                                                  openssl_version,
                                                  dll_name='ssleay32.dll',
                                                  dll_dir=bin_dir)

    self._libraries['libeay32'] = SVNCommonLibrary('openssl', inc_dir, lib_dir,
                                                    'libeay32.lib',
                                                    openssl_version,
                                                    dll_name='libeay32.dll',
                                                    dll_dir=bin_dir)

  def _find_perl(self, show_warnings):
    "Find the right perl library name to link swig bindings with"

    fp = os.popen('perl -MConfig -e ' + escape_shell_arg(
                  'print "$Config{PERL_REVISION}.$Config{PERL_VERSION}.'
                          '$Config{PERL_SUBVERSION}\\n"; '
                  'print "$Config{archlib}\\n"'), 'r')
    try:
      line = fp.readline()
      if line:
        perl_version = line.strip()
        perlv = perl_version.split('.')
        perl_lib = 'perl%s%s.lib' % (perlv[0], perlv[1])
      else:
        return

      line = fp.readline()
      if line:
        lib_dir = os.path.join(line.strip(), 'CORE')
        inc_dir = lib_dir
    finally:
      fp.close()

    self._libraries['perl'] = SVNCommonLibrary('perl', inc_dir, lib_dir,
                                               perl_lib, perl_version)

  def _find_ruby(self, show_warnings):
    "Find the right Ruby library name to link swig bindings with"

    lib_dir = None

    # Pass -W0 to stifle the "-e:1: Use RbConfig instead of obsolete
    # and deprecated Config." warning if we are using Ruby 1.9.
    fp = os.popen('ruby -rrbconfig -W0 -e ' + escape_shell_arg(
                  "puts Config::CONFIG['ruby_version'];"
                  "puts Config::CONFIG['LIBRUBY'];"
                  "puts Config::CONFIG['archdir'];"
                  "puts Config::CONFIG['libdir'];"), 'r')
    try:
      line = fp.readline()
      if line:
        ruby_version = line.strip()

      line = fp.readline()
      if line:
        ruby_lib = line.strip()

      line = fp.readline()
      if line:
        inc_dir = line.strip()

      line = fp.readline()
      if line:
        lib_dir = line.strip()
    finally:
      fp.close()

    if not lib_dir:
      return

    # Visual C++ doesn't have a standard compliant snprintf yet
    # (Will probably be added in VS2013 + 1)
    defines = ['snprintf=_snprintf']

    ver = ruby_version.split('.')
    ver = tuple(map(int, ver))
    if ver > (1, 8, 0):
      defines.extend(["HAVE_RB_ERRINFO"])

    self._libraries['ruby'] = SVNCommonLibrary('ruby', inc_dir, lib_dir,
                                               ruby_lib, ruby_version,
                                               defines=defines)

  def _find_python(self, show_warnings):
    "Find the appropriate options for creating SWIG-based Python modules"

    try:
      from distutils import sysconfig

      inc_dir = sysconfig.get_python_inc()
      lib_dir = os.path.join(sysconfig.PREFIX, "libs")
    except ImportError:
      return

    self._libraries['python'] = SVNCommonLibrary('python', inc_dir, lib_dir, None,
                                                 sys.version.split(' ')[0])

  def _find_jdk(self, show_warnings):
    "Find details about an installed jdk"

    jdk_path = self.jdk_path
    self.jdk_path = None # No jdk on errors

    minimal_jdk_version = (1, 0, 0) # ### Provide sane default

    if not jdk_path:
      jdk_ver = None
      try:
        try:
          # Python >=3.0
          import winreg
        except ImportError:
          # Python <3.0
          import _winreg as winreg
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                           r"SOFTWARE\JavaSoft\Java Development Kit")
        # Find the newest JDK version.
        num_values = winreg.QueryInfoKey(key)[1]
        for i in range(num_values):
          (name, value, key_type) = winreg.EnumValue(key, i)
          if name == "CurrentVersion":
            jdk_ver = value
            break

        # Find the JDK path.
        if jdk_ver is not None:
          key = winreg.OpenKey(key, jdk_ver)
          num_values = winreg.QueryInfoKey(key)[1]
          for i in range(num_values):
            (name, value, key_type) = winreg.EnumValue(key, i)
            if name == "JavaHome":
              jdk_path = value
              break
        winreg.CloseKey(key)
      except (ImportError, EnvironmentError):
        pass

    if not jdk_path or not os.path.isdir(jdk_path):
      return

    try:
      outfp = subprocess.Popen([os.path.join(jdk_path, 'bin', 'javah.exe'),
                               '-version'], stdout=subprocess.PIPE).stdout
      line = outfp.read()
      if line:
        vermatch = re.compile(r'"(([0-9]+(\.[0-9]+)+)(_[._0-9]+)?)"', re.M) \
                   .search(line)
      else:
        vermatch = None

      if vermatch:
        version = tuple(map(int, vermatch.groups()[1].split('.')))
        versionstr = vermatch.groups()[0]
      else:
        if show_warnings:
          print('Could not find installed JDK,')
        return
      outfp.close()
    except OSError:
      if show_warnings:
        print('Could not find installed JDK,')
      return

    if version < minimal_jdk_version:
      if show_warning:
        print('Found java jdk %s, but >= %s is required. '
              'javahl will not be built.\n' % \
              (versionstr, '.'.join(str(v) for v in minimal_jdk_version)))
      return

    self.jdk_path = jdk_path
    inc_dirs = [
        os.path.join(jdk_path, 'include'),
        os.path.join(jdk_path, 'include', 'win32'),
      ]

    lib_dir = os.path.join(jdk_path, 'lib')

    # The JDK provides .lib files, but we currently don't use these.
    self._libraries['java_sdk'] = SVNCommonLibrary('java-skd', inc_dirs,
                                                   lib_dir, None,
                                                   versionstr)

  def _find_swig(self):
    # Require 1.3.24. If not found, assume 1.3.25.
    default_version = '1.3.25'
    minimum_version = '1.3.24'
    vernum = 103025
    minimum_vernum = 103024
    libdir = ''

    if self.swig_path is not None:
      self.swig_exe = os.path.abspath(os.path.join(self.swig_path, 'swig'))
    else:
      self.swig_exe = 'swig'

    try:
      outfp = subprocess.Popen([self.swig_exe, '-version'], stdout=subprocess.PIPE, universal_newlines=True).stdout
      txt = outfp.read()
      if txt:
        vermatch = re.compile(r'^SWIG\ Version\ (\d+)\.(\d+)\.(\d+)$', re.M) \
                   .search(txt)
      else:
        vermatch = None

      if vermatch:
        version = tuple(map(int, vermatch.groups()))
        # build/ac-macros/swig.m4 explains the next incantation
        vernum = int('%d%02d%03d' % version)
        print('Found installed SWIG version %d.%d.%d\n' % version)
        if vernum < minimum_vernum:
          print('WARNING: Subversion requires version %s\n'
                 % minimum_version)

        libdir = self._find_swig_libdir()
      else:
        print('Could not find installed SWIG,'
               ' assuming version %s\n' % default_version)
        self.swig_libdir = ''
      outfp.close()
    except OSError:
      print('Could not find installed SWIG,'
             ' assuming version %s\n' % default_version)
      self.swig_libdir = ''

    self.swig_vernum = vernum
    self.swig_libdir = libdir

  def _find_swig_libdir(self):
    fp = os.popen(self.swig_exe + ' -swiglib', 'r')
    try:
      libdir = fp.readline().rstrip()
      if libdir:
        print('Using SWIG library directory %s\n' % libdir)
        return libdir
      else:
        print('WARNING: could not find SWIG library directory\n')
    finally:
      fp.close()
    return ''

  def _find_ml(self):
    "Check if the ML assembler is in the path"
    if not self.enable_ml:
      self.have_ml = 0
      return
    fp = os.popen('ml /help', 'r')
    try:
      line = fp.readline()
      if line:
        msg = 'Found ML, ZLib build will use ASM sources'
        self.have_ml = 1
      else:
        msg = 'Could not find ML, ZLib build will not use ASM sources'
        self.have_ml = 0
      print('%s\n' % (msg,))
    finally:
      fp.close()

  def _get_serf_version(self, inc_dir):
    "Retrieves the serf version from serf.h"

    # shouldn't be called unless serf is there
    assert inc_dir and os.path.exists(inc_dir)

    serf_ver_maj = None
    serf_ver_min = None
    serf_ver_patch = None

    # serf.h should be present
    if not os.path.exists(os.path.join(inc_dir, 'serf.h')):
      return None, None, None

    txt = open(os.path.join(inc_dir, 'serf.h')).read()

    maj_match = re.search(r'SERF_MAJOR_VERSION\s+(\d+)', txt)
    min_match = re.search(r'SERF_MINOR_VERSION\s+(\d+)', txt)
    patch_match = re.search(r'SERF_PATCH_VERSION\s+(\d+)', txt)
    if maj_match:
      serf_ver_maj = int(maj_match.group(1))
    if min_match:
      serf_ver_min = int(min_match.group(1))
    if patch_match:
      serf_ver_patch = int(patch_match.group(1))

    return serf_ver_maj, serf_ver_min, serf_ver_patch

  def _find_serf(self, show_warning):
    "Check if serf and its dependencies are available"

    minimal_serf_version = (1, 2, 1)

    if not self.serf_path:
      return

    inc_dir = self.serf_path

    if os.path.isfile(os.path.join(inc_dir, 'serf.h')):
      # Source layout
      version = self._get_serf_version(inc_dir)

      if version < (1, 3, 0):
        lib_dir = os.path.join(self.serf_path, 'Release')
        debug_lib_dir = os.path.join(self.serf_path, 'Debug')
      else:
        lib_dir = self.serf_path
        debug_lib_dir = None
      is_src = True
    elif os.path.isfile(os.path.join(self.serf_path, 'include/serf-1/serf.h')):
      # Install layout
      inc_dir = os.path.join(self.serf_path, 'include/serf-1')
      version = self._get_serf_version(inc_dir)
      lib_dir = os.path.join(self.serf_path, 'lib')
      debug_lib_dir = None
      is_src = False
    elif os.path.isfile(os.path.join(self.serf_path, 'include/serf-2/serf.h')):
      # Install layout
      inc_dir = os.path.join(self.serf_path, 'include/serf-2')
      version = self._get_serf_version(inc_dir)
      lib_dir = os.path.join(self.serf_path, 'lib')
      debug_lib_dir = None
      is_src = False
    else:
      if show_warning:
        print('WARNING: \'serf.h\' not found')
        print("Use '--with-serf' to configure serf location.");
      return

    if is_src and 'openssl' not in self._libraries:
      if show_warning:
        print('openssl not found, serf and ra_serf will not be built')
      return
    serf_version = '.'.join(str(v) for v in version)

    if version < minimal_serf_version:
      if show_warning:
        print('Found serf %s, but >= %s is required. '
              'ra_serf will not be built.\n' %
              (serf_version, '.'.join(str(v) for v in minimal_serf_version)))
      return

    serf_ver_maj = version[0]

    if serf_ver_maj > 0:
      lib_name = 'serf-%d.lib' % (serf_ver_maj,)
    else:
      lib_name = 'serf.lib'

    defines = ['SVN_HAVE_SERF', 'SVN_LIBSVN_CLIENT_LINKS_RA_SERF']

    self._libraries['serf'] = SVNCommonLibrary('serf', inc_dir, lib_dir,
                                                lib_name, serf_version,
                                                debug_lib_dir=debug_lib_dir,
                                                is_src=is_src,
                                                defines=defines)

  def _find_sasl(self, show_warning):
    "Check if sals is available"

    minimal_sasl_version = (2, 0, 0)

    if not self.sasl_path:
      return

    inc_dir = os.path.join(self.sasl_path, 'include')

    version_file_path = os.path.join(inc_dir, 'sasl.h')

    if not os.path.isfile(version_file_path):
      if show_warning:
        print('WARNING: \'%s\' not found' % (version_file_path,))
        print("Use '--with-sasl' to configure sasl location.");
      return

    txt = open(version_file_path).read()

    vermatch = re.search(r'^\s*#define\s+SASL_VERSION_MAJOR\s+(\d+)', txt, re.M)
    major = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+SASL_VERSION_MINOR\s+(\d+)', txt, re.M)
    minor = int(vermatch.group(1))

    vermatch = re.search(r'^\s*#define\s+SASL_VERSION_STEP\s+(\d+)', txt, re.M)
    patch = int(vermatch.group(1))

    version = (major, minor, patch)
    sasl_version = '.'.join(str(v) for v in version)

    if version < minimal_sasl_version:
      if show_warning:
        print('Found sasl %s, but >= %s is required. '
              'sals support will not be built.\n' %
              (sasl_version, '.'.join(str(v) for v in minimal_serf_version)))
      return

    lib_dir = os.path.join(self.sasl_path, 'lib')

    if os.path.isfile(os.path.join(lib_dir, 'libsasl.dll')):
      dll_dir = lib_dir
      dll_name = 'libsasl.dll'
    elif os.path.isfile(os.path.join(self.sasl_path, 'bin', 'libsasl.dll')):
      dll_dir = os.path.join(self.sasl_path, 'bin')
      dll_name = 'libsasl.dll'
    else:
      # Probably a static compilation
      dll_dir = None
      dll_name = None

    self._libraries['sasl'] = SVNCommonLibrary('sasl', inc_dir, lib_dir,
                                               'libsasl.lib', sasl_version,
                                               dll_dir=dll_dir,
                                               dll_name=dll_name,
                                               defines=['SVN_HAVE_SASL'])

  def _find_libintl(self, show_warning):
    "Find gettext support"
    minimal_libintl_version = (0, 14, 1)

    if not self.enable_nls or not self.libintl_path:
      return;

    # We support 2 scenarios.
    if os.path.isfile(os.path.join(self.libintl_path, 'inc', 'libintl.h')) and\
       os.path.isfile(os.path.join(self.libintl_path, 'lib', 'intl3_svn.lib')):

      # 1. Subversion's custom libintl based on gettext 0.14.1
      inc_dir = os.path.join(self.libintl_path, 'inc')
      lib_dir = os.path.join(self.libintl_path, 'lib')
      dll_dir = os.path.join(self.libintl_path, 'bin')

      lib_name = 'intl3_svn.lib'
      dll_name = 'intl3_svn.dll'
    elif os.path.isfile(os.path.join(self.libintl_path, \
                                     'include', 'libintl.h')):
      # 2. A gettext install
      inc_dir = os.path.join(self.libintl_path, 'include')
      lib_dir = os.path.join(self.libintl_path, 'lib')
      dll_dir = os.path.join(self.libintl_path, 'bin')

      lib_name = 'intl.lib'
      dll_name = 'intl.dll'
    else:
      if (show_warning):
        print('WARNING: \'libintl.h\' not found')
        print("Use '--with-libintl' to configure libintl location.")
      return

    version_file_path = os.path.join(inc_dir, 'libintl.h')
    txt = open(version_file_path).read()

    vermatch = re.search(r'^\s*#define\s+LIBINTL_VERSION\s+((0x)?[0-9A-Fa-f]+)', txt, re.M)

    ver = int(vermatch.group(1), 0)
    version = (ver >> 16, (ver >> 8) & 0xFF, ver & 0xFF)

    libintl_version = '.'.join(str(v) for v in version)

    if version < minimal_libintl_version:
      if show_warning:
        print('Found libintl %s, but >= %s is required.\n' % \
              (libintl_version,
               '.'.join(str(v) for v in minimal_libintl_version)))
      return

    self._libraries['intl'] = SVNCommonLibrary('libintl', inc_dir, lib_dir,
                                               lib_name, libintl_version,
                                               dll_dir=dll_dir,
                                               dll_name=dll_name)

  def _find_sqlite(self, show_warnings):
    "Find the Sqlite library and version"

    minimal_sqlite_version = (3, 7, 12)

    # For SQLite we support 3 scenarios:
    # - Installed in standard directory layout
    # - Installed in legacy directory layout
    # - Amalgamation compiled directly into our libraries

    sqlite_base = self.sqlite_path

    lib_dir = None
    dll_dir = None
    dll_name = None
    defines = []

    lib_name = 'sqlite3.lib'

    if os.path.isfile(os.path.join(sqlite_base, 'include/sqlite3.h')):
      # Standard layout
      inc_dir = os.path.join(sqlite_base, 'include')
      lib_dir = os.path.join(sqlite_base, 'lib')

      # We assume a static library, but let's support shared in this case
      if os.path.isfile(os.path.join(sqlite_base, 'bin/sqlite3.dll')):
        dll_dir = os.path.join(sqlite_base, 'bin')
        dll_name = 'sqlite3.dll'
    elif os.path.isfile(os.path.join(sqlite_base, 'inc/sqlite3.h')):
      # Standard layout
      inc_dir = os.path.join(sqlite_base, 'inc')
      lib_dir = os.path.join(sqlite_base, 'lib')

      # We assume a static library, but let's support shared in this case
      if os.path.isfile(os.path.join(sqlite_base, 'bin/sqlite3.dll')):
        dll_dir = os.path.join(sqlite_base, 'bin')
        dll_name = 'sqlite3.dll'
    elif (os.path.isfile(os.path.join(sqlite_base, 'sqlite3.h'))
          and os.path.isfile(os.path.join(sqlite_base, 'sqlite3.c'))):
      # Amalgamation
      inc_dir = sqlite_base
      lib_dir = None
      lib_name = None 
      defines.append('SVN_SQLITE_INLINE')
    else:
      sys.stderr.write("ERROR: SQLite not found\n" % self.sqlite_path)
      sys.stderr.write("Use '--with-sqlite' option to configure sqlite location.\n");
      sys.exit(1)

    version_file_path = os.path.join(inc_dir, 'sqlite3.h')

    txt = open(version_file_path).read()

    vermatch = re.search(r'^\s*#define\s+SQLITE_VERSION\s+"(\d+)\.(\d+)\.(\d+)(?:\.(\d))?"', txt, re.M)

    version = vermatch.groups()

    # Sqlite doesn't add patch numbers for their ordinary releases
    if not version[3]:
      version = version[0:3]

    version = tuple(map(int, version))

    sqlite_version = '.'.join(str(v) for v in version)

    if version < minimal_sqlite_version:
      sys.stderr.write("ERROR: sqlite %s or higher is required "
                       "(%s found)\n" % (
                          '.'.join(str(v) for v in minimal_sqlite_version),
                          sqlite_version))
      sys.exit(1)

    self._libraries['sqlite'] = SVNCommonLibrary('sqlite', inc_dir, lib_dir,
                                                 lib_name, sqlite_version,
                                                 dll_dir=dll_dir,
                                                 dll_name=dll_name,
                                                 defines=defines)

# ============================================================================
# This is a cut-down and modified version of code from:
#   subversion/subversion/bindings/swig/python/svn/core.py
#
if sys.platform == "win32":
  _escape_shell_arg_re = re.compile(r'(\\+)(\"|$)')

  def escape_shell_arg(arg):
    # The (very strange) parsing rules used by the C runtime library are
    # described at:
    # http://msdn.microsoft.com/library/en-us/vclang/html/_pluslang_Parsing_C.2b2b_.Command.2d.Line_Arguments.asp

    # double up slashes, but only if they are followed by a quote character
    arg = re.sub(_escape_shell_arg_re, r'\1\1\2', arg)

    # surround by quotes and escape quotes inside
    arg = '"' + arg.replace('"', '"^""') + '"'
    return arg

else:
  def escape_shell_arg(str):
    return "'" + str.replace("'", "'\\''") + "'"
