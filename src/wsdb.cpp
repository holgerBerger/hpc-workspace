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
 *  (c) Holger Berger 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024
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
#include <vector>

// Posix
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

// YAML
#include <yaml-cpp/yaml.h>

// boost
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


#include "wsdb.h"
#include "ws.h"

using namespace std;


/*
 * write new db entry
 */
WsDB::WsDB(const string _filename, const string _wsdir, const long int _expiration,
           const int _extensions, const string _acctcode, const int _dbuid,
           const int _dbgid, const int _reminder, const string _mailaddress, const string _group, const string _comment)
    :
    dbfilename(_filename), wsdir(_wsdir), expiration(_expiration), extensions(_extensions),
    acctcode(_acctcode), dbuid(_dbuid), dbgid(_dbgid), reminder(_reminder), mailaddress(_mailaddress), group(_group), comment(_comment), released(0)
{
    write_dbfile();
}

/*
 *  open db entry for reading
 */
WsDB::WsDB(const string _filename, const int _dbuid, const int _dbgid) : dbfilename(_filename), dbuid(_dbuid), dbgid(_dbgid), released(0)
{
    read_dbfile();
}

/*
 * write db file after consuming an extension if not root
 */
void WsDB::use_extension(const long _expiration, const string _mailaddress, const int _reminder, const string _comment)
{
    if (_mailaddress!="") mailaddress=_mailaddress;
    if (_reminder!=0) reminder=_reminder;
    if (_comment!="") comment=_comment;
    // if root does this, we do not use an extension
    if((getuid()!=0) && (_expiration!=-1) && (_expiration > expiration)) extensions--;
    if((extensions<0) && (getuid()!=0)) {
        cerr << "Error: no more extensions." << endl;
        exit(-1);
    }
    if (_expiration!=-1) {
        expiration = _expiration;
    }
    write_dbfile();
}



// write data to file
void WsDB::write_dbfile()
{
    int perm;
    YAML::Node entry;
    entry["workspace"] = wsdir;
    entry["expiration"] = expiration;
    entry["extensions"] = extensions;
    entry["acctcode"] = acctcode;
    entry["reminder"] = reminder;
    entry["mailaddress"] = mailaddress;
    if (group.length()>0) {
        entry["group"] = group;
    }
    if (released > 0) {
        entry["released"] = released;
    }
    entry["comment"] = comment;
	
	// suppress ctrl-c to prevent broken DB entries when FS is hanging and user gets nervous
	signal(SIGINT,SIG_IGN);

    Workspace::raise_cap(CAP_DAC_OVERRIDE, __LINE__, __FILE__);
#ifdef SETUID
    // for filesystem with root_squash, we need to be DB user here
    if (setegid(dbgid)|| seteuid(dbuid)) {
			cerr << "Error: can not seteuid or setgid. Bad installation?" << endl;
			exit(-1);
	}
#endif
    ofstream fout(dbfilename.c_str());
    if(!(fout << entry)) cerr << "Error: could not write DB file! Please check if the outcome is as expected, you might have to make a backup of the workspace to prevent loss of data!"  << endl;
    fout.close();
    if (group.length()>0) {
        // for group workspaces, we set the x-bit
        perm = 0744;
    } else {
        perm = 0644;
    }
    Workspace::raise_cap(CAP_FOWNER, __LINE__, __FILE__);
    if (chmod(dbfilename.c_str(), perm) != 0) {
        cerr << "Error: could not change permissions of database entry" << endl;
    }
    Workspace::lower_cap(CAP_FOWNER, dbuid);
#ifdef SETUID
    if(seteuid(0)|| setegid(0)) {
			cerr << "Error: can not seteuid or setgid. Bad installation?" << endl;
			exit(-1);
	}
#endif
    Workspace::lower_cap(CAP_DAC_OVERRIDE, dbuid);

#ifndef SETUID
    Workspace::raise_cap(CAP_CHOWN, __LINE__, __FILE__);
    if (chown(dbfilename.c_str(), dbuid, dbgid)) {
        Workspace::lower_cap(CAP_CHOWN, dbuid);
        cerr << "Error: could not change owner of database entry" << endl;
    }
    Workspace::lower_cap(CAP_CHOWN, dbuid);
#endif

	// normal signal handling
	signal(SIGINT,SIG_DFL);
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
		// DBs created before b7aa2e64b63e616fece9a23f76807dabcd12f815
		// lack the comment field, and this falls through into old path
        comment = entry["comment"].as<string>("");
		// FIXME group missing here?
		group = entry["group"].as<string>("");
		// FIXME empty group or current group if no group in DB?
    } catch (const YAML::BadSubscript&) {
        // fallback to old db format, python version
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
	} catch (const YAML::Exception&) {
		cerr << "invalid db entry, aborting." << endl;
		exit(-2);
    }
}
