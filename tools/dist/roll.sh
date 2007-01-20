#!/bin/sh
set -e

VERSION=$1
REV=$2
EXTRA=$3
BRANCH=${VERSION%.*}.x

rs=http://svn.collab.net/repos/svn

if [ -n "`svn diff $rs/branches/$BRANCH/CHANGES $rs/trunk/CHANGES`" ]; then
  echo "CHANGES not synced between trunk and branch, aborting!" >&2
  exit 1
fi

SVNRM_BIN="`pwd`/prefix/bin"
if [ ! -f "$SVNRM_BIN/autoconf" ] || [ ! -f "$SVNRM_BIN/libtoolize" ] \
  || [ ! -f "$SVNRM_BIN/swig" ]; then
  echo "You do not appear to have an appropriate prefix directory" >&2
  exit 1
fi
export PATH="$SVNRM_BIN:$PATH"

rm -f dist.sh
svn export -r $REV $rs/branches/$BRANCH/dist.sh

mkdir deploy

(cd unix-dependencies &&
  ../dist.sh -v $VERSION -pr branches/$BRANCH -r $REV $EXTRA &&
  mv subversion-* ../deploy/ &&
  mv svn_version.h.dist ../deploy/)

(cd win32-dependencies &&
  ../dist.sh -v $VERSION -pr branches/$BRANCH -r $REV -zip $EXTRA &&
  mv subversion-* ../deploy/ &&
  rm svn_version.h.dist)

(cd deploy &&
  md5sum subversion-* svn_version.h.dist > md5sums &&
  sha1sum subversion-* svn_version.h.dist > sha1sums &&
  mkdir to-tigris &&
  cd to-tigris &&
  for i in ../subversion-*; do ln -s "$i"; done)

rm -f dist.sh
