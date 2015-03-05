
/*
 *    workspace++
 *
 *  ws_release
 *
 *    c++ version of workspace utility
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

#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>

#include "ws_util.h"
#include "ws.h"

namespace po = boost::program_options;
using namespace std;


/* 
 *  parse the commandline and see if all required arguments are passed, and check the workspace name for 
 *  bad characters
 */
void commandline(po::variables_map &opt, string &name, int &duration, 
                    string &filesystem, bool &extension,  int argc, char**argv) {
    // define all options

    po::options_description cmd_options( "\nOptions" );
    cmd_options.add_options()
            ("help,h", "produce help message")
            ("name,n", po::value<string>(&name), "workspace name")
            ("filesystem,F", po::value<string>(&filesystem), "filesystem")
    ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(cmd_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        cout << "Usage:" << argv[0] << ": [options] workspace_name" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    // see whats up

    if (opt.count("help")) {
        cout << "Usage:" << argv[0] << ": [options] workspace_name" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    if (opt.count("name"))
    {
        //cout << " name: " << name << "\n";
    } else {
        cout << argv[0] << ": [options] workspace_name" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    // validate workspace name against nasty characters    
    static const boost::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$");
    if (!regex_match(name, e)) {
            cerr << "Error: Illegal workspace name, use characters and numbers, -,. and - only!" << endl;
            exit(1);
    }
}


/*
 *  main logic here
 */

int main(int argc, char **argv) {
    int duration;
    bool extensionflag;
    string name;
    string filesystem;
    string acctcode, wsdir;
    string mailaddress;
    po::variables_map opt;
    YAML::Node config, userconfig;

    // check commandline
    commandline(opt, name, duration, filesystem, extensionflag, argc, argv);

    // get workspace object
    Workspace ws(WS_Release, opt, duration, filesystem);
    
    // allocate workspace
    ws.release(name);
    
}


