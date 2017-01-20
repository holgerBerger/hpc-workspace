#ifndef WSDB_H
#define WSDB_H

/*
 *  workspace++
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and
 *  configuration was changed to YAML files.
 *
 *  differences to old workspace version
 *    - usage of YAML file format
 *    - using setuid or capabilities (needs support by filesystem!)
 *    - always moves released workspace away (this change is affecting the user!)
 *
 *  (c) Holger Berger 2013, 2014, 2015, 2016, 2017
 *
 *  workspace++ is based on workspace by Holger Berger, Thomas Beisel and Martin Hecht
 *
 *  workspace++ is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  workspace++ is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with workspace++.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <string>


using namespace std;

/*
 * new WsDB classe
 * represents a workspace database entry
 *
 * client does either create a new entry with constructor with arguments,
 * or wants to read it and just gives name to query other information.
 *
 */


class WsDB {

private:
    string dbfilename;
    string wsdir;
    long expiration;
    int extensions;
    string acctcode;
    int dbuid;
    int dbgid;
    int reminder;
    string mailaddress;
    string group;

    void write_dbfile();
    void read_dbfile();


public:
    // constructor to query a DB entry, this reads the database entry
    WsDB(const string filename, const int dbuid, const int dbgid);

    // constructor to create a new DB entry
    WsDB(const string filename, const string wsdir, const long expiration, const int extensions,
         const string acctcode, const int dbuid, const int dbgid,
         const int reminder, const string mailaddress, const string group);

    void use_extension(const long expiration);


    long int getexpiration() {
        return expiration;
    }

    int getextension() {
        return extensions;
    }

    string getwsdir() {
        return wsdir;
    }
};

#endif
