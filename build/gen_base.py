#
# gen_base.py -- infrastructure for generating makefiles, dependencies, etc.
#

import os
import sys
import string
import glob
import re
import fileinput
import ConfigParser

import getversion


class GeneratorBase:

  #
  # Derived classes should define a class attribute named _extension_map.
  # This attribute should be a dictionary of the form:
  #     { (target-type, file-type): file-extension ...}
  #
  # where: target-type is 'exe', 'lib', ...
  #        file-type is 'target', 'object', ...
  #

  def __init__(self, fname, verfname):
    parser = ConfigParser.ConfigParser(_cfg_defaults)
    parser.read(fname)

    self.cfg = Config()
    self.cfg.swig_lang = string.split(parser.get('options', 'swig-languages'))

    # Version comes from a header file since it is used in the code.
    try:
      vsn_parser = getversion.Parser()
      vsn_parser.search('SVN_VER_LIBRARY', 'libver')
      self.cfg.version = vsn_parser.parse(verfname).libver
    except:
      raise GenError('Unable to extract version.')

    self.targets = { }
    self.includes = [ ]
    self.test_progs = [ ]
    self.test_deps = [ ]
    self.fs_test_progs = [ ]
    self.fs_test_deps = [ ]
    self.target_dirs = { }
    self.manpages = [ ]
    self.infopages = [ ]
    self.graph = DependencyGraph()

    # PASS 1: collect the targets and some basic info
    for target in _filter_targets(parser.sections()):
      install = parser.get(target, 'install')
      type = parser.get(target, 'type')

      target_class = _build_types.get(type)
      if not target_class:
        raise GenError('ERROR: unknown build type: ' + type)

      target_ob = target_class(target,
                               parser.get(target, 'path'),
                               install,
                               parser.get(target, 'custom'), ### bogus
                               self.cfg,
                               self._extension_map)

      self.targets[target] = target_ob

      ### I don't feel like passing these to the constructor right now,
      ### and I'm not sure how to really pass this stuff along. we
      ### certainly don't want the targets looking at the parser...
      target_ob.add_deps = parser.get(target, 'add-deps')

      # find all the sources involved in building this target
      target_ob.find_sources(parser.get(target, 'sources'))

      ### another hack for now. tell a SWIG target what libraries should
      ### be linked into each wrapper. this also depends on the fact that
      ### the swig libraries occur *after* the other targets in build.conf
      ### cuz of the test for "is this in self.targets?"
      if type == 'swig':
        target_ob.libs = self._find_libs(parser.get(target, 'libs'))

      # the target should add all relevant dependencies
      target_ob.add_dependencies(self.graph)

      self.manpages.extend(string.split(parser.get(target, 'manpages')))
      self.infopages.extend(string.split(parser.get(target, 'infopages')))

      if type != 'script':
        # collect test programs
        if type == 'exe':
          if install == 'test':
            self.test_deps.append(target_ob.output)
            if parser.get(target, 'testing') != 'skip':
              self.test_progs.append(target_ob.output)
          if install == 'fs-test':
            self.fs_test_deps.append(target_ob.output)
            if parser.get(target, 'testing') != 'skip':
              self.fs_test_progs.append(target_ob.output)

        # collect all the paths where stuff might get built
        self.target_dirs[target_ob.path] = None
        for pattern in string.split(parser.get(target, 'sources')):
          if string.find(pattern, os.sep) != -1:
            self.target_dirs[os.path.join(target_ob.path,
                                          os.path.dirname(pattern))] = None

    # compute intra-library dependencies
    for name, target in self.targets.items():
      if isinstance(target, TargetLinked):
        for lib in self._find_libs(parser.get(name, 'libs')):
          self.graph.add(DT_LINK, name, lib)

    # collect various files
    self.includes = _collect_paths(parser.get('options', 'includes'))
    self.apache_files = _collect_paths(parser.get('static-apache', 'paths'))

    # collect all the test scripts
    self.scripts = _collect_paths(parser.get('test-scripts', 'paths'))
    self.fs_scripts = _collect_paths(parser.get('fs-test-scripts', 'paths'))

    # get all the test scripts' directories
    script_dirs = map(os.path.dirname, self.scripts + self.fs_scripts)

    # remove duplicate directories between targets and tests
    build_dirs = self.target_dirs.copy()
    for d in script_dirs:
      build_dirs[d] = None
    self.build_dirs = build_dirs.keys()

  def _find_libs(self, libs_option):
    libs = [ ]
    for libname in string.split(libs_option):
      if self.targets.has_key(libname):
        libs.append(self.targets[libname])
      else:
        libs.append(ExternalLibrary(libname))
    return libs

  def compute_hdr_deps(self):
    #
    # Find all the available headers and what they depend upon. the
    # include_deps is a dictionary mapping a short header name to a tuple
    # of the full path to the header and a dictionary of dependent header
    # names (short) mapping to None.
    #
    # Example:
    #   { 'short.h' : ('/path/to/short.h',
    #                  { 'other.h' : None, 'foo.h' : None }) }
    #
    # Note that this structure does not allow for similarly named headers
    # in per-project directories. SVN doesn't have this at this time, so
    # this structure works quite fine. (the alternative would be to use
    # the full pathname for the key, but that is actually a bit harder to
    # work with since we only see short names when scanning, and keeping
    # a second variable around for mapping the short to long names is more
    # than I cared to do right now)
    #
    include_deps = _create_include_deps(self.includes)
    for d in self.target_dirs.keys():
      hdrs = glob.glob(os.path.join(d, '*.h'))
      if hdrs:
        more_deps = _create_include_deps(hdrs, include_deps)
        include_deps.update(more_deps)

    for objname, sources in self.graph.get_deps(DT_OBJECT):
      if isinstance(objname, SWIGObject):
        ### the .c file is generated, so we can't scan it. this isn't a
        ### very good test. ideally, the test would be to look for a
        ### dependency node for the source, meaning it is generated, and
        ### punt on it then.
        continue
      assert len(sources) == 1
      hdrs = [ ]
      for short in _find_includes(sources[0], include_deps):
        self.graph.add(DT_OBJECT, objname, include_deps[short][0])


class DependencyGraph:
  """Record dependencies between build items.

  See the DT_* values for the different dependency types. For each type,
  the target and source objects recorded will be different. They could
  be file names, Target objects, install types, etc.
  """

  def __init__(self):
    self.deps = { }     # type -> { target -> [ source ... ] }
    for dt in dep_types:
      self.deps[dt] = { }

  def add(self, type, target, source):
    if self.deps[type].has_key(target):
      self.deps[type][target].append(source)
    else:
      self.deps[type][target] = [ source ]

  def get_sources(self, type, target, cls=None):
    sources = self.deps[type].get(target, [ ])
    if not cls:
      return sources
    filtered = [ ]
    for src in sources:
      if isinstance(src, cls):
        filtered.append(src)
    return filtered

  def get_all_sources(self, type):
    sources = [ ]
    for group in self.deps[type].values():
      sources.extend(group)
    sources.sort()  # ensures consistency between runs
    return sources

  def get_targets(self, type):
    targets = self.deps[type].keys()
    targets.sort()  # ensures consistency between runs
    return targets

  def get_deps(self, type):
    deps = self.deps[type].items()
    deps.sort()  # ensures consistency between runs
    return deps

# dependency types
dep_types = [
  'DT_INSTALL',  # install areas. e.g. 'lib', 'base-lib', 'fs-lib'
  'DT_OBJECT',   # an object filename, depending upon .c filenames
  'DT_SWIG_C',   # a swig-generated .c file, depending upon .i filename(s)
  'DT_LINK',     # a libtool-linked filename, depending upon object fnames
  'DT_INCLUDE',  # filename includes (depends) on sources (all basenames)
  ]

# create some variables for these
for _dt in dep_types:
  # e.g. DT_INSTALL = 'DT_INSTALL'
  globals()[_dt] = _dt

class DependencyNode:
  def __init__(self, fname):
    self.fname = fname

  def __str__(self):
    return self.fname

  def __cmp__(self, ob):
    return cmp(self.fname, ob)

  def __hash__(self):
    return hash(self.fname)

class ObjectFile(DependencyNode):
  pass
class ApacheObject(ObjectFile):
  ### hmm. this is Makefile-specific
  build_cmd = '$(COMPILE_APACHE_MOD)'
class SWIGObject(ObjectFile):
  def __init__(self, fname, lang):
    ObjectFile.__init__(self, fname)
    self.lang = lang
    self.lang_abbrev = lang_abbrev[lang]
    ### hmm. this is Makefile-specific
    self.build_cmd = '$(COMPILE_%s_WRAPPER)' % string.upper(self.lang_abbrev)
    self.source_generated = 1

# the SWIG utility libraries
class SWIGUtilPython(ObjectFile):
  ### hmm. this is Makefile-specific
  build_cmd = '$(COMPILE_SWIG_PY)'
class SWIGUtilJava(ObjectFile):
  ### hmm. this is Makefile-specific
  build_cmd = '$(COMPILE_SWIG_JAVA)'

_custom_build = {
  'apache-mod' : ApacheObject,
  'swig-py' : SWIGUtilPython,
  'swig-java' : SWIGUtilJava,
  }

class SWIGLibrary(DependencyNode):
  ### stupid Target vs DependencyNode
  add_deps = ''

  def __init__(self, fname, lang):
    DependencyNode.__init__(self, fname)
    self.lang = lang
    self.lang_abbrev = lang_abbrev[lang]

    self.path = os.path.dirname(fname)

    self.name = lang + os.path.splitext(os.path.basename(fname))[0]

    ### maybe tweak to avoid these duplicate attrs
    self.output = fname

    ### hmm. this is Makefile-specific
    self.link_cmd = '$(LINK_%s_WRAPPER)' % string.upper(self.lang_abbrev)

class ExternalLibrary(DependencyNode):
  pass

lang_abbrev = {
  'python' : 'py',
  'java' : 'java',
  'perl' : 'pl',
  'ruby' : 'rb',
  'tcl' : 'tcl',
  ### what others?
  }


### we should turn these targets into DependencyNode subclasses...
class Target:
  def __init__(self, name, path, install, custom, cfg, extmap):
    self.name = name
    self.path = path
    self.cfg = cfg

    ### this should be a class attr and we should use different Target
    ### classes based on the "custom" value.
    self.object_cls = _custom_build.get(custom, ObjectFile)

    if not install:
      try:
        install = self.default_install
      except AttributeError:
        raise GenError('Class "%s" has no default install location'
                       % self.__class__.__name__)
    self.install = install

    # default output name; subclasses can/should change this
    self.output = os.path.join(path, name)

  def find_sources(self, patterns):
    if not patterns:
      try:
        patterns = self.default_sources
      except AttributeError:
        raise GenError('Class "%s" has no default sources'
                       % self.__class__.__name__)
    self._sources = _collect_paths(patterns, self.path)
    self._sources.sort()

  def add_dependencies(self, graph):
    # the specified install area depends upon this target
    graph.add(DT_INSTALL, self.install, self)

    for src in self._sources:
      if src[-2:] == '.c':
        objname = src[:-2] + self.objext

        ofile = self.object_cls(objname)

        # object depends upon source
        graph.add(DT_OBJECT, ofile, src)

        # target (a linked item) depends upon object
        graph.add(DT_LINK, self.name, ofile)
      else:
        raise GenError('ERROR: unknown file extension on ' + src)

  def __cmp__(self, ob):
    if isinstance(ob, Target):
      return cmp(self.name, ob.name)
    return cmp(self.name, ob)

class TargetLinked(Target):
  "The target is linked (by libtool) against other libraries."

  ### hmm. this is Makefile-specific
  link_cmd = '$(LINK)'

class TargetExe(TargetLinked):
  default_install = 'bin'
  default_sources = '*.c'

  def __init__(self, name, path, install, custom, cfg, extmap):
    Target.__init__(self, name, path, install, custom, cfg, extmap)

    self.objext = extmap['exe', 'object']
    self.output = os.path.join(path, name + extmap['exe', 'target'])

class TargetScript(Target):
  default_install = 'bin'
  # no default_sources

  def find_sources(self, patterns):
    # Script "sources" are actually final targets, which means they may be
    # generated, which means they are not available the time this program
    # is run. Therefore, we have no work to do in find_sources().
    pass

  def add_dependencies(self, graph):
    # we don't need to "compile" the sources, so there are no dependencies
    # to add here, except to get the script installed in the proper area.
    graph.add(DT_INSTALL, self.install, self)

class TargetLib(TargetLinked):
  default_install = 'lib'
  default_sources = '*.c'

  def __init__(self, name, path, install, custom, cfg, extmap):
    Target.__init__(self, name, path, install, custom, cfg, extmap)

    self.objext = extmap['lib', 'object']

    if install != 'apache-mod':
      # the target file is the name, version, and appropriate extension
      tfile = '%s-%s%s' % (name, cfg.version, extmap['lib', 'target'])
    else:
      tfile = name + extmap['lib', 'target']

      # we have a custom linking rule
      ### hmm. this is Makefile-specific
      ### kind of hacky anyways. we should use a different Target subclass
      self.link_cmd = '$(LINK_APACHE_MOD)'

    self.output = os.path.join(path, tfile)

    ### eek. this is pretty ugly. we should have a new Target subclass.
    if custom == 'ra-module':
      self.is_ra_module = 1

class TargetDoc(Target):
  # no default_install
  default_sources = '*.texi'

class TargetSWIG(Target):
  default_install = 'swig'
  # no default_sources

  def __init__(self, name, path, install, custom, cfg, extmap):
    Target.__init__(self, name, path, install, custom, cfg, extmap)

    self.objext = extmap['lib', 'object']
    self.libext = extmap['lib', 'target']

  def add_dependencies(self, graph):
    ### simple assertions for now
    assert len(self._sources) == 1

    ifile = self._sources[0]
    assert ifile[-2:] == '.i'

    dir, iname = os.path.split(ifile)
    cname = iname[:-2] + '.c'
    oname = iname[:-2] + self.objext

    ### we should really extract the %module line
    if iname[:4] == 'svn_':
      libname = iname[3:-2] + self.libext
    else:
      libname = '_' + iname[:-2] + self.libext

    for lang in self.cfg.swig_lang:
      abbrev = lang_abbrev[lang]

      # the .c file depends upon the .i file
      cfile = os.path.join(dir, lang, cname)
      graph.add(DT_SWIG_C, SWIGObject(cfile, lang), ifile)

      # the object depends upon the .c file
      ofile = SWIGObject(os.path.join(dir, lang, oname), lang)
      graph.add(DT_OBJECT, ofile, cfile)

      # the library depends upon the object
      library = SWIGLibrary(os.path.join(dir, lang, libname), lang)
      graph.add(DT_LINK, library, ofile)

      # add some more libraries
      for lib in self.libs:
        graph.add(DT_LINK, library, lib)

      # add some language-specific libraries for languages other than
      # Java (SWIG doesn't seem to provide a libswigjava.so)
      if abbrev != 'java':
        ### fix this. get these from the .conf file
        graph.add(DT_LINK, library, ExternalLibrary('-lswig' + abbrev))
      ### fix this, too. find the right Target swigutil lib. we know there
      ### will be only one.
      util = graph.get_sources(DT_INSTALL, 'swig-%s-lib' % abbrev)[0]
      graph.add(DT_LINK, library, util)

      # the specified install area depends upon the library
      graph.add(DT_INSTALL, self.install + '-' + abbrev, library)

_build_types = {
  'exe' : TargetExe,
  'script' : TargetScript,
  'lib' : TargetLib,
  'doc' : TargetDoc,
  'swig' : TargetSWIG,
  }


class Config:
  pass


class GenError(Exception):
  pass

_cfg_defaults = {
  'sources' : '',
  'libs' : '',
  'manpages' : '',
  'infopages' : '',
  'custom' : '',
  'install' : '',
  'testing' : '',
  'add-deps' : '',
  }

_predef_sections = [
  'options',
  'static-apache',
  'test-scripts',
  'fs-test-scripts',
  ]

def _filter_targets(t):
  t = t[:]
  for s in _predef_sections:
    if s in t:
      t.remove(s)
  t.sort()
  return t

def _collect_paths(pats, path=None):
  result = [ ]
  for pat in string.split(pats):
    if path:
      pat = os.path.join(path, pat)
    files = glob.glob(pat)
    if not files:
      raise GenError('ERROR: "%s" found no files.' % pat)
    result.extend(files)
  return result

def _strip_path(path, files):
  "Strip the given path from each file."
  if path[-1] not in (os.sep, os.altsep):
    path = path + os.sep
  l = len(path)
  result = [ ]
  for file in files:
    assert file[:l] == path
    result.append(file[l:])
  return result

def _retreat_dots(path):
  "Given a relative directory, return ../ paths to retreat to the origin."
  parts = string.split(path, os.sep)
  return (os.pardir + os.sep) * len(parts)

def _find_includes(fname, include_deps):
  hdrs = _scan_for_includes(fname, include_deps.keys())
  return _include_closure(hdrs, include_deps).keys()

def _create_include_deps(includes, prev_deps={}):
  shorts = map(os.path.basename, includes)

  # limit intra-header dependencies to just these headers, and what we
  # may have found before
  limit = shorts + prev_deps.keys()

  deps = prev_deps.copy()
  for inc in includes:
    short = os.path.basename(inc)
    deps[short] = (inc, _scan_for_includes(inc, limit))

  # keep recomputing closures until we see no more changes
  while 1:
    changes = 0
    for short in shorts:
      old = deps[short]
      deps[short] = (old[0], _include_closure(old[1], deps))
      if not changes:
        ok = old[1].keys()
        ok.sort()
        nk = deps[short][1].keys()
        nk.sort()
        changes = ok != nk
    if not changes:
      return deps

def _include_closure(hdrs, deps):
  new = hdrs.copy()
  for h in hdrs.keys():
    new.update(deps[h][1])
  return new

_re_include = re.compile(r'^#\s*include\s*[<"]([^<"]+)[>"]')
def _scan_for_includes(fname, limit):
  "Return a dictionary of headers found (fnames as keys, None as values)."
  # note: we don't worry about duplicates in the return list
  hdrs = { }
  for line in fileinput.input(fname):
    match = _re_include.match(line)
    if match:
      h = match.group(1)
      if h in limit:
        hdrs[h] = None
  return hdrs

def _sorted_files(graph, area):
  "Given a list of targets, sort them based on their dependencies."

  # we're going to just go with a naive algorithm here. these lists are
  # going to be so short, that we can use O(n^2) or whatever this is.

  inst_targets = graph.get_sources(DT_INSTALL, area)

  # first we need our own copy of the target list since we're going to
  # munge it.
  targets = inst_targets[:]

  # the output list of the targets' files
  files = [ ]

  # loop while we have targets remaining:
  while targets:
    # find a target that has no dependencies in our current targets list.
    for t in targets:
      for d in graph.get_sources(DT_LINK, t.name, Target):
        if d in targets:
          break
      else:
        # no dependencies found in the targets list. this is a good "base"
        # to add to the files list now.
        files.append(t.output)

        # don't consider this target any more
        targets.remove(t)

        # break out of search through targets
        break
    else:
      # we went through the entire target list and everything had at least
      # one dependency on another target. thus, we have a circular dependency
      # tree. somebody messed up the .conf file, or the app truly does have
      # a loop (and if so, they're screwed; libtool can't relink a lib at
      # install time if the dependent libs haven't been installed yet)
      raise CircularDependencies()

  return files

class CircularDependencies(Exception):
  pass


### End of file.
