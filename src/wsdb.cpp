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
 *  (c) Holger Berger 2013, 2014, 2015
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

// C++
#include <string>
#include <fstream>
#include <iostream>

// Posix
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <grp.h>
#include <time.h>

// YAML
#include <yaml-cpp/yaml.h>

// boost
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#ifndef SETUID
#include <sys/capability.h>
#else
typedef int cap_value_t;
const int CAP_DAC_OVERRIDE = 0;
const int CAP_CHOWN = 1;
#endif

#include "wsdb.h"
#include "ws.h"

using namespace std;


/*
 * write new db entry
 */
WsDB::WsDB(const string _filename, const string _wsdir, const long int _expiration,
           const int _extensions, const string _acctcode, const int _dbuid,
           const int _dbgid, const int _reminder, const string _mailaddress)
    :
    dbfilename(_filename), wsdir(_wsdir), expiration(_expiration), extensions(_extensions),
    acctcode(_acctcode), dbuid(_dbuid), dbgid(_dbgid), reminder(_reminder), mailaddress(_mailaddress)
{
    write_dbfile();
}

/*
 *  open db entry for reading
 */
WsDB::WsDB(const string _filename, const int _dbuid, const int _dbgid) : dbfilename(_filename), dbuid(_dbuid), dbgid(_dbgid)
{
    read_dbfile();
}

/*
 * write db file after consuming an extension if not root
 */
void WsDB::use_extension(const long _expiration)
{
    expiration = _expiration;
    // if root does this, we do not use an extension
    if(getuid()!=0) extensions--;
    if(extensions<0) {
        cerr << "Error: no more extensions." << endl;
        exit(-1);
    }
    write_dbfile();
}




// write data to file
void WsDB::write_dbfile()
{
    YAML::Node entry;
    entry["workspace"] = wsdir;
    entry["expiration"] = expiration;
    entry["extensions"] = extensions;
    entry["acctcode"] = acctcode;
    entry["reminder"] = reminder;
    entry["mailaddress"] = mailaddress;
    Workspace::raise_cap(CAP_DAC_OVERRIDE);
#ifdef SETUID
    // for filesystem with root_squash, we need to be DB user here
    seteuid(dbuid); setegid(dbgid);
#endif
    ofstream fout(dbfilename.c_str());
    fout << entry;
    fout.close();
#ifdef SETUID
    seteuid(0); setegid(0);
#endif
    Workspace::lower_cap(CAP_DAC_OVERRIDE, dbuid);
    Workspace::raise_cap(CAP_CHOWN);
    if(chown(dbfilename.c_str(), dbuid, dbgid)) {
        Workspace::lower_cap(CAP_CHOWN, dbuid);
        cerr << "Error: could not change owner of database entry" << endl;
    }
    Workspace::lower_cap(CAP_CHOWN, dbuid);
}

// read data from file
void WsDB::read_dbfile()
{
    YAML::Node entry = YAML::LoadFile(dbfilename);
    try {
        wsdir = entry["workspace"].as<string>();
        expiration = entry["expiration"].as<long>();
        extensions = entry["extensions"].as<int>();
        acctcode = entry["acctcode"].as<string>();
        reminder = entry["reminder"].as<int>();
        mailaddress = entry["mailaddress"].as<string>();
    } catch (YAML::BadSubscript) {
        // fallback to old db format
        ifstream entry (dbfilename.c_str());
        entry >> expiration;
        entry >> wsdir;
        // get acctcode and extensions, need some splitting
        string line;
        std::vector<std::string> sp;
        getline(entry, line); // newline
        getline(entry, line); // acctcode
        boost::split(sp, line, boost::is_any_of(":"));
        acctcode = sp[1];
        getline(entry, line); // extension
        boost::split(sp, line, boost::is_any_of(":"));
        extensions = boost::lexical_cast<int>(sp[1]);
        entry.close();
        mailaddress = "";
        reminder = 0;
    }
}
