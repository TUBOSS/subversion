/* revprops-db.sql -- schema for use to store revprops
 *   This is intented for use with SQLite 3
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

pragma auto_vacuum = 1;

/* A table mapping representation hashes to locations in a rev file. */
create table revprop (revision integer UNIQUE not null,
                      properties BLOB not null);

create index i_revision on revprop (revision);

-- STMT_SET_REVPROP
insert or replace into revprop(revision, properties)
values (?1, ?2);

-- STMT_GET_REVPROP
select properties from revprop where revision = ?1;
