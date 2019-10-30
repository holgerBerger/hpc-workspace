/*
 *  workspace++
 *
 *  ws_restore
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and 
 *  configuration was changed to YAML files.
 *  This version works without setuid bit, but capabilities need to be used.
 * 
 *  differences to old workspace version
 *    - usage of YAML file format
 *    - always moves released workspace away (this change is affecting the user!)
 *
 *  (c) Holger Berger 2013, 2014, 2015, 2016, 2017, 2018, 2019
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
#include <time.h>
#include <syslog.h>

#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#define BOOST_FILESYSTEM_VERSION 3 
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

#include "ws.h"
#include "ruh.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;
using boost::lexical_cast;


void commandline(po::variables_map &opt, string &name, string &target,
                    string &filesystem, bool &listflag, bool &terse, string &username,  int argc, char**argv) {
    // define all options

    po::options_description cmd_options( "\nOptions" );
    cmd_options.add_options()
            ("help,h", "produce help message")
            ("version,V", "show version")
            ("list,l", "list restorable workspaces")
            ("brief,b", "do not show unavailability date in list")
            ("name,n", po::value<string>(&name), "workspace name")
            ("target,t", po::value<string>(&target), "existing target workspace name")
            ("filesystem,F", po::value<string>(&filesystem), "filesystem")
            ("username,u", po::value<string>(&username), "username")
    ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1);
    p.add("target", 2);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(cmd_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        cout << "Usage:" << argv[0] << ": [options] workspace_name target_name | -l" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    // see whats up

    if (opt.count("help")) {
        cout << "Usage:" << argv[0] << ": [options] workspace_name target_name | -l" << endl;
        cout << cmd_options << "\n";
		cout << "attention: the workspace_name argument is as printed by " << argv[0] << " -l not as printed by ws_list!" << endl;
        exit(1);
    }

    if (opt.count("version")) {
        cout << "workspace version " << GIT_COMMIT_HASH << endl;
        exit(1);
    }

    if (opt.count("list")) {
        listflag = true;
    } else {
        listflag = false;
    }

    if (opt.count("brief")) {
        terse = true;
    } else {
        terse = false;
    }

    if (opt.count("name"))
    {
        if (!opt.count("target")) {
            cout << "Error: no target given." << endl;
            cout << argv[0] << ": [options] workspace_name target_name | -l" << endl;
            cout << cmd_options << "\n";
            exit(1);
        }
        // validate workspace name against nasty characters    
        static const boost::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$");
        if (!regex_match(name, e)) {
            cerr << "Error: Illegal workspace name, use characters and numbers, -,. and _ only!" << endl;
            exit(1);
        }
    } else if (!opt.count("list")) {
        cout << "Error: neither workspace nor -l specified." << endl;
        cout << argv[0] << ": [options] workspace_name target_name | -l" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

}

/*
 * check that either username matches the name of the workspace, or we are root
 */
bool check_name(const string name, const string username, const string real_username) {
    // split the name in username, name and timestamp
    vector<string> sp;
    boost::split(sp, name, boost::is_any_of("-"));

    // we checked already that only root can use another username with -u, so here
    // we know we are either root or username == real_username
    if ((username != sp[0]) && (real_username != "root")) {
        cerr << "Error: only root can do this 1" << username << sp[0] << endl;
        return false;
    } else {
        return true;
    }
}

int main(int argc, char **argv) {
    po::variables_map opt;
    string name, target, filesystem, acctcode, username;
    bool listflag, terse;
    int duration=0;
    YAML::Node config, userconfig;

    // we only support C locale, if the used local is not installed on the system
    // ws_restore fails
    setenv("LANG","C",1);
    std::setlocale(LC_ALL, "C");
	std::locale::global(std::locale("C"));

    // check commandline, get flags which are used to create ws object or for workspace allocation
    commandline(opt, name, target, filesystem, listflag, terse, username, argc, argv);

    // get workspace object
    Workspace ws(WS_Release, opt, duration, filesystem);

    // construct db-entry username  name
    string real_username = ws.getusername();
    if (username == "") {
        username = real_username;
    } else if (real_username != username) {
        if (real_username != "root") {
            cerr << "Error: only root can do that. 2" << endl;
            username = real_username;
            exit(-1);
        }
    }

    openlog("ws_restore", 0, LOG_USER); // SYSLOG

    if (listflag) {
        BOOST_FOREACH(string dn, ws.getRestorable(username)) {
            cout << dn << endl;
            if (!terse) {
                std::vector<std::string> splitted;
                boost::split(splitted, dn, boost::is_any_of("-"));
                time_t t = atol(splitted[splitted.size()-1].c_str());
                cout << "\tunavailable since " << std::ctime(&t);
            }
        }
    } else {
        if (check_name(name, username, real_username)) {
            if (ruh()) {
                ws.restore(name, target, username);
            } else {
                syslog(LOG_INFO, "user <%s> failed ruh test.", username.c_str());
            }
        }
    }
}
