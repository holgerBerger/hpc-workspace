/*
 *  workspace++
 *
 *  ws_release
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
 *  (c) Holger Berger 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2023
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
#ifdef USE_BOOST_REGEXP
    #include <boost/regex.hpp>
    #define REGEX boost::regex
#else
    #include <regex>
    #define REGEX std::regex
#endif
#include <string>
#include <syslog.h>
#include <boost/program_options.hpp>


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
	if (getuid()==0) {
		cmd_options.add_options()
				("help,h", "produce help message")
				("version,V", "show version")
				("name,n", po::value<string>(&name), "workspace name")
				("filesystem,F", po::value<string>(&filesystem), "filesystem")
				("userworkspace", "release a user workspace")
		;
	} else {
		cmd_options.add_options()
				("help,h", "produce help message")
				("version,V", "show version")
				("name,n", po::value<string>(&name), "workspace name")
				("filesystem,F", po::value<string>(&filesystem), "filesystem")
				("delete-data", "delete all data, workspace can NOT BE RECOVERED")
		;
	}

    po::options_description secret_options("Secret");
    secret_options.add_options()
        ("debug", "show debugging information")
        ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opt);
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

    if (opt.count("version")) {
#ifdef IS_GIT_REPOSITORY
        cout << "workspace build from git commit hash " << GIT_COMMIT_HASH
             << " on top of release " << WS_VERSION << endl;
#else
        cout << "workspace version " << WS_VERSION << endl;
#endif

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
    // static const std::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$"); // #77
	// bugfix: as regexp parser are recursiv, split in two parts, complex match for start, simple search for remainder
	static const REGEX e1("^[[:alnum:]][[:alnum:]_.-]*$");
    if (!regex_match(name.substr(0,2) , e1)) {
            cerr << "Error: Illegal workspace name, use characters and numbers, -,. and _ only!" << endl;
            exit(1);
    }
	static const REGEX e2("[^[:alnum:]_.-]");
    if (regex_search(name, e2)) {
            cerr << "Error: Illegal workspace name, use characters and numbers, -,. and _ only!" << endl;
            exit(1);
    }
}


/*
 *  main logic here
 */

int main(int argc, char **argv) {
    int duration=0;
    bool extensionflag;
    string name;
    string filesystem;
    string acctcode, wsdir;
    string mailaddress;
    po::variables_map opt;
    YAML::Node config, userconfig;

    // we only support C locale, if the used local is not installed on the system
    // ws_release fails
    setenv("LANG","C",1);
	setenv("LC_CTYPE","C",1);
	setenv("LC_ALL","C",1);
    std::setlocale(LC_ALL, "C");
    std::locale::global(std::locale("C"));
	
    // read config
    try {
        config = YAML::LoadFile("/etc/ws.conf");
    } catch (const YAML::BadFile& e) {
        cerr << "Error: Could not read config file!" << endl;
        cerr << e.what() << endl;
        exit(-1);
    }

    int db_uid = config["dbuid"].as<int>();

    // lower capabilities to minimum
    Workspace::drop_cap(CAP_DAC_OVERRIDE, CAP_CHOWN, CAP_FOWNER, db_uid, __LINE__, __FILE__);

    // check commandline
    commandline(opt, name, duration, filesystem, extensionflag, argc, argv);

    openlog("ws_release", 0, LOG_USER); // SYSLOG

    // get workspace object
    Workspace ws(WS_Release, opt, duration, filesystem);
    
    // release workspace
    ws.release(name);
    
}


