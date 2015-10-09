#ifndef WS_H
#define WS_H

/*
 *  workspace++
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and
 *    configuration was changed to YAML files.
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

// C++ stuff
#include <string>
#include <vector>

// YAML
#include <yaml-cpp/yaml.h>

// BOOST
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>

#include "wsdb.h"

#ifndef SETUID
#include <sys/capability.h>
#else
typedef int cap_value_t;
#endif

namespace po = boost::program_options;

using namespace std;


enum whichclient {
    WS_Allocate,
    WS_Release
};




class Workspace {

private:
    YAML::Node config, userconfig;
    int db_uid, db_gid;
    po::variables_map opt;
    int maxextensions, duration;
    string filesystem, acctcode, username;

    void validate(const whichclient wc, YAML::Node &config, YAML::Node &userconfig,
                  po::variables_map &opt, string &filesystem, int &duration, int &maxextensions, string &primarygroup);


    int mv(const char * source, const char *target);


public:

    // helpers
    static string getuserhome();
    static string getusername();
    static void drop_cap(cap_value_t cap_arg, int dbuid);
    static void drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid);
    static void raise_cap(int cap);
    static void lower_cap(int cap, int dbuid);

    // constructor reads config and userconfig
    Workspace(const whichclient clientcode, const po::variables_map opt, const int _duration, string filesystem);

    // allocate a new workspace, create workspace and DB entry
    void allocate(const string name, const bool extensionsflag, const int reminder, const string mailaddress, string user_option);

    // release an existing workspace, move workspace and DB entry
    void release(string name);

    string getfilesystem();

    // extend an existing workspace
    //bool extend();

    // return existing workspaces list from DB
    //vector<WsDB> getList();

    // return restorable workspaces list from DB
    vector<string> getRestorable(string username);

    // restore a workspace
    void restore(const string name, const string target, const string username);

};

#endif
