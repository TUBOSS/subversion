#!/usr/bin/python
#
# This is a pretty-printer for subversion BDB repository databases.
#

import sys, os, re, codecs, textwrap

import skel, svnfs

from svnfs_bsddb.db import *

# Parse arguments
if len(sys.argv) == 2:
  dbhome = os.path.join(sys.argv[1], 'db')
  if not os.path.exists(dbhome):
    sys.stderr.write("%s: '%s' is not a valid svn repository\n" %
        (sys.argv[0], dbhome))
    sys.exit(1)
else:
  sys.stderr.write("Usage: %s <svn-repository>\n" % sys.argv[0])
  sys.exit(1)

# Helper Classes
class RepositoryProblem(Exception):
  pass

# Helper Functions
def ok(bool, comment):
  if not bool:
    raise RepositoryProblem(text)

# Helper Data
opmap = {
  'add': 'A',
  'modify': 'M',
  'delete': 'D',
  'replace': 'R',
  'reset': 'X',
}

# Analysis Modules
def am_uuid(ctx):
  "uuids"
  db = ctx.uuids_db
  ok(db.keys() == [1], 'uuid Table Structure')
  ok(re.match(r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$',
    db[1]), 'UUID format')
  print "Repos UUID: %s" % db[1]

def am_revisions(ctx):
  "revisions"
  cur = ctx.revs_db.cursor()
  try:
    rec = cur.first()
    ctx.txn2rev = txn2rev = {}
    prevrevnum = -1
    while rec:
      rev = skel.Rev(rec[1])
      revnum = rec[0] - 1
      print "r%d: txn %s%s" % (revnum, rev.txn,
          (not ctx.txns_db.has_key(rev.txn)) and "*** MISSING TXN ***" or "")
      ok(not txn2rev.has_key(rev.txn), 'Multiple revs bound to same txn')
      txn2rev[rev.txn] = revnum
      rec = cur.next()
  finally:
    cur.close()

def am_changes(ctx):
  "changes"
  cur = ctx.changes_db.cursor()
  try:
    rec = cur.first()
    prevtxn = None
    while rec:
      ch = skel.Change(rec[1])
      lead = "txn %s:" % rec[0]
      if prevtxn == rec[0]:
        lead = " " * len(lead)
      print "%s %s %s %s %s %s%s" % (lead, opmap[ch.kind], ch.path, ch.node,
          ch.textmod and "T" or "-", ch.propmod and "P" or "-",
          (not ctx.nodes_db.has_key(ch.node)) and "*** MISSING NODE ***" or "")
      prevtxn = rec[0]
      rec = cur.next()
  finally:
    cur.close()

def am_copies(ctx):
  "copies"
  cur = ctx.copies_db.cursor()
  try:
    print "next-key: %s" % ctx.copies_db['next-key']
    rec = cur.first()
    while rec:
      if rec[0] != 'next-key':
        cp = skel.Copy(rec[1])
        destnode = ctx.nodes_db.get(cp.destnode)
        if not destnode:
          destpath = "*** MISSING NODE ***"
        else:
          destpath = skel.Node(destnode).createpath
        print "cpy %s: %s %s @txn %s to %s (%s)" % (rec[0],
            {'copy':'C','soft-copy':'S'}[cp.kind], cp.srcpath or "-",
            cp.srctxn or "-", cp.destnode, destpath)
      rec = cur.next()
  finally:
    cur.close()

def am_txns(ctx):
  "transactions"
  cur = ctx.txns_db.cursor()
  try:
    print "next-key: %s" % ctx.txns_db['next-key']
    rec = cur.first()
    while rec:
      if rec[0] != 'next-key':
        txn = skel.Txn(rec[1])
        if txn.kind == "committed":
          label = "r%s" % txn.rev
          ok(ctx.txn2rev[rec[0]] == int(txn.rev), 'Txn->rev not <-txn')
        else:
          label = "%s based-on %s" % (txn.kind, txn.basenode)
        print "txn %s: %s root-node %s props %d copies %s" % (rec[0],
            label, txn.rootnode, len(txn.proplist) / 2, ",".join(txn.copies))
      rec = cur.next()
  finally:
    cur.close()

def am_nodes(ctx):
  "nodes"
  cur = ctx.nodes_db.cursor()
  try:
    print "next-key: %s" % ctx.txns_db['next-key']
    rec = cur.first()
    data = {}
    while rec:
      if rec[0] == 'next-key':
        rec = cur.next()
        continue
      nd = skel.Node(rec[1])
      nid,cid,tid = rec[0].split(".")
      data[tid.rjust(20)+nd.createpath] = (rec[0], nd)
      rec = cur.next()
    k = data.keys()
    k.sort()
    reptype = {"fulltext":"F", "delta":"D"}
    for i in k:
      nd = data[i][1]
      prkind = drkind = " "
      if nd.proprep:
        try:
          rep = ctx.reps_db[nd.proprep]
          prkind = reptype[skel.Rep(rep).kind]
        except KeyError:
          prkind = "*** MISSING ***"
      if nd.datarep:
        try:
          rep = ctx.reps_db[nd.datarep]
          drkind = reptype[skel.Rep(rep).kind]
        except KeyError:
          drkind = "*** MISSING ***"
      stringdata = "%s: %s %s pred %s count %s prop %s %s data %s %s edit %s" \
          % ( data[i][0], {"file":"F", "dir":"D"}[nd.kind], nd.createpath,
          nd.prednode or "-", nd.predcount, prkind, nd.proprep or "-",
          drkind, nd.datarep or "-", nd.editrep or "-")
      if nd.createpath == "/":
        print
      print stringdata
  finally:
    cur.close()

def get_string(ctx, id):
  try:
    return ctx.get_whole_string(id)
  except DbNotFoundError:
    return "*** MISSING STRING ***"

def am_reps(ctx):
  "representations"
  cur = ctx.reps_db.cursor()
  try:
    print "next-key: %s" % ctx.txns_db['next-key']
    rec = cur.first()
    while rec:
      if rec[0] != 'next-key':
        rep = skel.Rep(rec[1])
        lead = "rep %s: txn %s: %s %s " % (rec[0], rep.txn, rep.cksumtype,
            codecs.getencoder('hex_codec')(rep.cksum)[0])
        if rep.kind == "fulltext":
          note = ""
          if not ctx.strings_db.has_key(rep.str):
            note = " *MISS*"
          print lead+("fulltext str %s%s" % (rep.str, note))
          if ctx.verbose:
            print textwrap.fill(get_string(ctx, rep.str), initial_indent="  ",
                subsequent_indent="  ", width=78)
        elif rep.kind == "delta":
          print lead+("delta of %s window%s" % (len(rep.windows),
            len(rep.windows) != 1 and "s" or ""))
          for window in rep.windows:
            noterep = notestr = ""
            if not ctx.reps_db.has_key(window[1][0][2]):
              noterep = " *MISS*"
            if not ctx.strings_db.has_key(window[1][2]):
              notestr = " *MISS*"
            print "\toff %s len %s vs-rep %s%s str %s%s" % (window[0],
                window[1][1], window[1][0][2], noterep, window[1][2], notestr)
        else:
          print lead+"*** UNKNOWN REPRESENTATION TYPE ***"
      rec = cur.next()
  finally:
    cur.close()


def am_stringsize(ctx):
  "string size"
  if not ctx.verbose:
    return
  cur = ctx.strings_db.cursor()
  try:
    rec = cur.first()
    size = 0
    while rec:
      size = size + len(rec[1] or "")
      rec = cur.next()
    print size, size/1024.0, size/1024.0/1024.0
  finally:
    cur.close()

modules = (
    am_uuid,
    am_revisions,
    am_changes,
    am_copies,
    am_txns,
    am_reps,
    am_nodes,
    # Takes too long: am_stringsize,
    )

def main():
  print "Repository View for '%s'" % dbhome
  print
  ctx = svnfs.Ctx()
  # Stash process state in a library data structure. Yuck!
  ctx.verbose = 0
  try:
    ctx.open(DBEnv(), dbhome, readonly=1)

    for am in modules:
      print "MODULE: %s" % am.__doc__
      am(ctx)
      print

  finally:
    ctx.close()

if __name__ == '__main__':
  main()
