/*
 *    workspace++
 *
 *  ws_restore
 *
 *    c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and 
 *    configuration was changed to YAML files.
 *  This version works without setuid bit, but capabilities need to be used.
 * 
 *  differences to old workspace version
 *    - usage of YAML file format
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
#define _XOPEN_SOURCE
#define _BSD_SOURCE
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/capability.h>
#include <time.h>

#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define BOOST_FILESYSTEM_VERSION 3 
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

#include "ws_util.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;
using boost::lexical_cast;


void commandline(po::variables_map &opt, string &name, 
                    string &filesystem, bool &listflag, string &username,  int argc, char**argv) {
    // define all options

    po::options_description cmd_options( "\nOptions" );
    cmd_options.add_options()
            ("help,h", "produce help message")
            ("list,l", "list restorable workspaces")
            ("name,n", po::value<string>(&name), "workspace name")
            ("filesystem,F", po::value<string>(&filesystem), "filesystem")
            ("username,u", po::value<string>(&username), "username")
    ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(cmd_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        cout << "Usage:" << argv[0] << ": [options] workspace_name | -l" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    // see whats up

    if (opt.count("help")) {
        cout << "Usage:" << argv[0] << ": [options] workspace_name | -l" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    if (opt.count("list")) {
	listflag = true;
    } else {
	listflag = false;
    }

    if (opt.count("name"))
    {
        // validate workspace name against nasty characters    
        static const boost::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$");
        if (!regex_match(name, e)) {
            cerr << "Error: Illegal workspace name, use characters and numbers, -,. and - only!" << endl;
            exit(1);
        }
    } else if (!opt.count("list")) {
        cout << argv[0] << ": [options] workspace_name" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

}

int main(int argc, char **argv) {
    po::variables_map opt;
    string name, filesystem, acctcode, username;
    bool listflag;
    int duration, maxextensions;
    YAML::Node config, userconfig;

    commandline(opt, name, filesystem, listflag, username, argc, argv);

    // read config 
    try {
        config = YAML::LoadFile("/etc/ws.conf");
    } catch (YAML::BadFile) {
        cerr << "Error: no config file!" << endl;
        exit(-1);
    }

    // drop capabilites
    drop_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());

    // read private config
    raise_cap(CAP_DAC_OVERRIDE);
    try {
        userconfig = YAML::LoadFile("ws_private.conf");
    } catch (YAML::BadFile) {
        // we do not care
    }
    lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());

    // valide the input  (opt contains name, duration and filesystem as well)
    validate(ws_release, config, userconfig, opt, filesystem, duration, maxextensions, acctcode);

    // construct db-entry name
    string real_username = getusername();
    if (username == "") {
        username = real_username;
    } else if (real_username != username) {
        if (real_username != "root") {
            cerr << "Error: only root can do that." << endl;
            username = real_username;
            exit(-1);
        }
    }

    if (listflag) {
        cerr << "Info: available workspaces for recovery:" << endl;
        string dbfilename=config["workspaces"][filesystem]["database"].as<string>()+"/"+username+"-"+name;
        string dbprefix = fs::path(dbfilename).parent_path().string() + "/" + 
                          config["workspaces"][filesystem]["deleted"].as<string>();
        
        fs::directory_iterator end;
        for (fs::directory_iterator it(dbprefix); it!=end; ++it) {
            if (boost::starts_with(it->path().filename(), username + "-" )) {
                cout << it->path().filename() << endl;
            }
        }
    } else {
        string dbfilename=fs::path(config["workspaces"][filesystem]["database"].as<string>()).string() 
                        + "/" + config["workspaces"][filesystem]["deleted"].as<string>()+"/"+name;
        if (fs::exists(dbfilename)) {
            string wsdir, mailaddress;
            long expiration;
            int reminder, extension;
            read_dbfile(dbfilename, wsdir, expiration, extension, acctcode, reminder, mailaddress);
            cout << fs::path(wsdir).parent_path().string() + "/" +
                    config["workspaces"][filesystem]["deleted"].as<string>() + "/" +name
                    << " -> " << wsdir << endl;
            cout << dbfilename << " -> "    << endl;
 // TODO move to where?
        } else {
            cerr << "Error: workspace does not exist." << endl;
        }
    }

}
