#!/bin/bash

#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing,
#  software distributed under the License is distributed on an
#  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
#  specific language governing permissions and limitations
#  under the License.

set -e
set -x

scripts=$(cd $(dirname "$0") && pwd)/svnbot

. ${scripts}/setenv.sh

#
# Step 4: build swig-py
#

echo "============ make swig-py"
cd ${absbld}
make swig-py

echo "============ make swig-pl"
cd ${absbld}
make swig-pl

echo "============ make swig-rb"
cd ${absbld}
make swig-rb

echo "============ make javahl"
cd ${absbld}
make javahl
