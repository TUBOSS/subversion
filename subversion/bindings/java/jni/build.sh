#!/bin/bash
#
# this is a pretty simple script, just with one simple use,
# to build the native library libsvn_jni.so which is used
# in the class "org.tigris.subversion.ClientImpl"
#
# I used this script on my linux box.
# In the future this will be integrated into the central makefile
#
LIBS="-lapr -ldl -lmm -lcrypt -lpthread -lsvn_subr -lneon -lsvn_repos -lsvn_fs -lsvn_delta -lexpat"
CC_ARGS="-I/usr/lib/java/include -I/usr/lib/java/include/linux -L/usr/local/lib $LIBS -pthread -shared"
NATIVE_CLASS_NAME="svn_jni"
OBJS="main.c date.c misc.c status.c item.c hashtable.c string.c j.c"
CC_ARGS="$CC_ARGS -o lib$NATIVE_CLASS_NAME.so $OBJS"
cc $CC_ARGS
