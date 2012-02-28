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

import sys
import os
from twisted.application import service, internet

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from svnpubsub.server import svnpubsub_server

application = service.Application("SvnPubSub")

def get_service():
    return internet.TCPServer(2069, svnpubsub_server())

service = get_service()
service.setServiceParent(application)
