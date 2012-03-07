#!/usr/bin/env python
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# SvnWcSub - Subscribe to a SvnPubSub stream, and keep a set of working copy
# paths in sync
#
# Example:
#  svnwcsub.py svnwcsub.conf
#
# On startup svnwcsub checks the working copy's path, runs a single svn update
# and then watches for changes to that path.
#
# See svnwcsub.conf for more information on its contents.
#

import subprocess
import threading
import sys
import os
import re
import ConfigParser
import time
import logging.handlers
import Queue
import optparse

from twisted.internet import reactor
from twisted.application import internet
from twisted.web.client import HTTPClientFactory, HTTPPageDownloader
from urlparse import urlparse
from xml.sax import handler, make_parser


# check_output() is only available in Python 2.7. Allow us to run with
# earlier versions
try:
    check_output = subprocess.check_output
except AttributeError:
    def check_output(args, env):  # note: we only use these two args
        pipe = subprocess.Popen(args, stdout=subprocess.PIPE, env=env)
        output, _ = pipe.communicate()
        if pipe.returncode:
            raise subprocess.CalledProcessError(pipe.returncode, args)
        return output


### note: this runs synchronously. within the current Twisted environment,
### it is called from ._get_match() which is run on a thread so it won't
### block the Twisted main loop.
def svn_info(svnbin, env, path):
    "Run 'svn info' on the target path, returning a dict of info data."
    args = [svnbin, "info", "--non-interactive", "--", path]
    output = check_output(args, env=env).strip()
    info = { }
    for line in output.split('\n'):
        idx = line.index(':')
        info[line[:idx]] = line[idx+1:].strip()
    return info


class WorkingCopy(object):
    def __init__(self, bdec, path, url):
        self.path = path
        self.url = url

        try:
            self.match, self.uuid = self._get_match(bdec.svnbin, bdec.env)
            bdec.wc_ready(self)
        except:
            logging.exception('problem with working copy: %s', path)

    def update_applies(self, uuid, path):
        if self.uuid != uuid:
            return False

        path = str(path)
        if path == self.match:
            #print "ua: Simple match"
            # easy case. woo.
            return True
        if len(path) < len(self.match):
            # path is potentially a parent directory of match?
            #print "ua: parent check"
            if self.match[0:len(path)] == path:
                return True
        if len(path) > len(self.match):
            # path is potentially a sub directory of match
            #print "ua: sub dir check"
            if path[0:len(self.match)] == self.match:
                return True
        return False

    def _get_match(self, svnbin, env):
        ### quick little hack to auto-checkout missing working copies
        if not os.path.isdir(self.path):
            logging.info("autopopulate %s from %s" % (self.path, self.url))
            subprocess.check_call([svnbin, 'co', '-q',
                                   '--non-interactive',
                                   '--config-dir',
                                   '/home/svnwc/.subversion',
                                   '--', self.url, self.path],
                                  env=env)

        # Fetch the info for matching dirs_changed against this WC
        info = svn_info(svnbin, env, self.path)
        root = info['Repository Root']
        url = info['URL']
        relpath = url[len(root):]  # also has leading '/'
        uuid = info['Repository UUID']
        return str(relpath), uuid


class HTTPStream(HTTPClientFactory):
    protocol = HTTPPageDownloader

    def __init__(self, url):
        self.url = url
        HTTPClientFactory.__init__(self, url, method="GET", agent="SvnWcSub/0.1.0")

    def pageStart(self, partial):
        pass

    def pagePart(self, data):
        pass

    def pageEnd(self):
        pass

class Revision:
    def __init__(self, repos, rev):
        self.repos = repos
        self.rev = rev
        self.dirs_changed = []

class StreamHandler(handler.ContentHandler):
    def __init__(self, stream, bdec):
        handler.ContentHandler.__init__(self)
        self.stream = stream
        self.bdec =  bdec
        self.rev = None
        self.text_value = None

    def startElement(self, name, attrs):
        """
        <commit revision="7">
                        <dirs_changed><path>/</path></dirs_changed>
                      </commit>
        """
        if name == "commit":
            self.rev = Revision(attrs['repository'], int(attrs['revision']))

    def characters(self, data):
        if self.text_value is not None:
            self.text_value = self.text_value + data
        else:
            self.text_value = data

    def endElement(self, name):
        if name == "commit":
            self.bdec.commit(self.stream, self.rev)
            self.rev = None
        if name == "path" and self.text_value is not None and self.rev is not None:
            self.rev.dirs_changed.append(self.text_value.strip())
        self.text_value = None


class XMLHTTPStream(HTTPStream):
    def __init__(self, url, bdec):
        HTTPStream.__init__(self, url)
        self.parser = make_parser(['xml.sax.expatreader'])
        self.handler = StreamHandler(self, bdec)
        self.parser.setContentHandler(self.handler)

    def pagePart(self, data):
        self.parser.feed(data)


def connectTo(url, bdec):
    u = urlparse(url)
    port = u.port
    if not port:
        port = 80
    s = XMLHTTPStream(url, bdec)
    if bdec.service:
      conn = internet.TCPClient(u.hostname, u.port, s)
      conn.setServiceParent(bdec.service)
    else:
      conn = reactor.connectTCP(u.hostname, u.port, s)
    return [s, conn]


CHECKBEAT_TIME = 60
PRODUCTION_RE_FILTER = re.compile("/websites/production/[^/]+/")

class BigDoEverythingClasss(object):
    def __init__(self, config, service = None):
        self.urls = [s.strip() for s in config.get_value('streams').split()]
        self.svnbin = config.get_value('svnbin')
        self.env = config.get_env()
        self.tracking = config.get_track()
        self.worker = BackgroundWorker(self.svnbin, self.env)
        self.service = service
        self.transports = {}
        self.streams = {}
        for u in self.urls:
          self._restartStream(u)
        self.watch = []

    def start(self):
        for path, url in self.tracking.items():
            # working copies auto-register with the BDEC when they are ready.
            WorkingCopy(self, path, url)

    def _restartStream(self, url):
        (self.streams[url], self.transports[url]) = connectTo(url, self)

    def wc_ready(self, wc):
        # called when a working copy object has its basic info/url,
        # Add it to our watchers, and trigger an svn update.
        logging.info("Watching WC at %s <-> %s" % (wc.path, wc.url))
        self.watch.append(wc)
        self.worker.add_work(OP_UPDATE, wc)

    def _normalize_path(self, path):
        if path[0] != '/':
            return "/" + path
        return os.path.abspath(path)

    def commit(self, stream, rev):
        logging.info("COMMIT r%d (%d paths) via %s" % (rev.rev, len(rev.dirs_changed), stream.url))
        paths = map(self._normalize_path, rev.dirs_changed)
        if len(paths):
            pre = os.path.commonprefix(paths)
            if pre == "/websites/":
                # special case for svnmucc "dynamic content" buildbot commits
                # just take the first production path to avoid updating all cms working copies
                for p in paths:
                    m = PRODUCTION_RE_FILTER.match(p)
                    if m:
                        pre = m.group(0)
                        break

            #print "Common Prefix: %s" % (pre)
            wcs = [wc for wc in self.watch if wc.update_applies(rev.repos, pre)]
            logging.info("Updating %d WC for r%d" % (len(wcs), rev.rev))
            for wc in wcs:
                self.worker.add_work(OP_UPDATE, wc)


# Start logging warnings if the work backlog reaches this many items
BACKLOG_TOO_HIGH = 20
OP_UPDATE = 'update'
OP_CLEANUP = 'cleanup'

class BackgroundWorker(threading.Thread):
    def __init__(self, svnbin, env):
        threading.Thread.__init__(self)

        # The main thread/process should not wait for this thread to exit.
        ### compat with Python 2.5
        self.setDaemon(True)

        self.svnbin = svnbin
        self.env = env
        self.q = Queue.Queue()

        self.has_started = False

    def run(self):
        while True:
            if self.q.qsize() > BACKLOG_TOO_HIGH:
                logging.warn('worker backlog is at %d', self.q.qsize())

            # This will block until something arrives
            operation, wc = self.q.get()
            try:
                if operation == OP_UPDATE:
                    self._update(wc)
                elif operation == OP_CLEANUP:
                    self._cleanup(wc)
                else:
                    logging.critical('unknown operation: %s', operation)
            except:
                logging.exception('exception in worker')

            # In case we ever want to .join() against the work queue
            self.q.task_done()

    def add_work(self, operation, wc):
        # Start the thread when work first arrives. Thread-start needs to
        # be delayed in case the process forks itself to become a daemon.
        if not self.has_started:
            self.start()
            self.has_started = True

        self.q.put((operation, wc))

    def _update(self, wc):
        "Update the specified working copy."

        # For giggles, let's clean up the working copy in case something
        # happened earlier.
        self._cleanup(wc)

        logging.info("updating: %s", wc.path)

        ### we need to move some of these args into the config. these are
        ### still specific to the ASF setup.
        args = [self.svnbin, 'update',
                '--quiet',
                '--config-dir', '/home/svnwc/.subversion',
                '--non-interactive',
                '--trust-server-cert',
                '--ignore-externals',
                wc.path]
        subprocess.check_call(args, env=self.env)

        ### check the loglevel before running 'svn info'?
        info = svn_info(self.svnbin, self.env, wc.path)
        logging.info("updated: %s now at r%s", wc.path, info['Revision'])

    def _cleanup(self, wc):
        "Run a cleanup on the specified working copy."

        ### we need to move some of these args into the config. these are
        ### still specific to the ASF setup.
        args = [self.svnbin, 'cleanup',
                '--config-dir', '/home/svnwc/.subversion',
                '--non-interactive',
                '--trust-server-cert',
                wc.path]
        subprocess.check_call(args, env=self.env)


class ReloadableConfig(ConfigParser.SafeConfigParser):
    def __init__(self, fname):
        ConfigParser.SafeConfigParser.__init__(self)

        self.fname = fname
        self.read(fname)

        ### install a signal handler to set SHOULD_RELOAD. BDEC should
        ### poll this flag, and then adjust its internal structures after
        ### the reload.
        self.should_reload = False

    def reload(self):
        # Delete everything. Just re-reading would overlay, and would not
        # remove sections/options. Note that [DEFAULT] will not be removed.
        for section in self.sections():
            self.remove_section(section)

        # Now re-read the configuration file.
        self.read(fname)

    def get_value(self, which):
        return self.get(ConfigParser.DEFAULTSECT, which)

    def get_env(self):
        env = os.environ.copy()
        default_options = self.defaults().keys()
        for name, value in self.items('env'):
            if name not in default_options:
                env[name] = value
        return env

    def get_track(self):
        "Return the {PATH: URL} dictionary of working copies to track."
        track = dict(self.items('track'))
        for name in self.defaults().keys():
            del track[name]
        return track

    def optionxform(self, option):
        # Do not lowercase the option name.
        return str(option)


def prepare_logging(logfile):
    "Log to the specified file, or to stdout if None."

    if logfile:
        # Rotate logs daily, keeping 7 days worth.
        handler = logging.handlers.TimedRotatingFileHandler(
          logfile, when='midnight', backupCount=7,
          )
    else:
        handler = logging.StreamHandler(sys.stdout)

    # Add a timestamp to the log records
    formatter = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s',
                                  '%Y-%m-%d %H:%M:%S')
    handler.setFormatter(formatter)

    # Apply the handler to the root logger
    root = logging.getLogger()
    root.addHandler(handler)
    
    ### use logging.INFO for now. switch to cmdline option or a config?
    root.setLevel(logging.INFO)


def handle_options(options):
    # Set up the logging, then process the rest of the options.
    prepare_logging(options.logfile)

    if options.pidfile:
        pid = os.getpid()
        open(options.pidfile, 'w').write('%s\n' % pid)
        logging.info('pid %d written to %s', pid, options.pidfile)

    if options.uid:
        try:
            uid = int(options.uid)
        except ValueError:
            import pwd
            uid = pwd.getpwnam(options.uid)[2]
        logging.info('setting uid %d', uid)
        os.setuid(uid)

    if options.gid:
        try:
            gid = int(options.gid)
        except ValueError:
            import grp
            gid = grp.getgrnam(options.gid)[2]
        logging.info('setting gid %d', gid)
        os.setgid(gid)

    if options.umask:
        umask = int(options.umask, 8)
        os.umask(umask)
        logging.info('umask set to %03o', umask)


def main(args):
    parser = optparse.OptionParser(
        description='An SvnPubSub client to keep working copies synchronized '
                    'with a repository.',
        usage='Usage: %prog [options] CONFIG_FILE',
        )
    parser.add_option('--logfile',
                      help='filename for logging')
    parser.add_option('--pidfile',
                      help="the process' PID will be written to this file")
    parser.add_option('--uid',
                      help='switch to this UID before running')
    parser.add_option('--gid',
                      help='switch to this GID before running')
    parser.add_option('--umask',
                      help='set this (octal) umask before running')

    options, extra = parser.parse_args(args)

    if len(extra) != 1:
        parser.error('CONFIG_FILE is required')
    config_file = extra[0]

    # Process any provided options.
    handle_options(options)

    c = ReloadableConfig(config_file)
    bdec = BigDoEverythingClasss(c)

    # Start the BDEC on the main thread, then start up twisted
    bdec.start()
    reactor.run()


if __name__ == "__main__":
    main(sys.argv[1:])
