
/*
 *    workspace++
 *
 *  ws_release
 *
 *    c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and 
 *    configuration was changed to YAML files.
 *  This version works without setuid bit, but capabilities need to be used.
 * 
 *  differences to old workspace version
 *    - usage of YAML file format
 *    - not using setuid, but needs capabilities
 *    - always moves released workspace away (this change is affecting the user!)
 *
 *  (c) Holger Berger 2013
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
// #include <sys/capability.h>
#include <time.h>

#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

#define BOOST_FILESYSTEM_VERSION 3 
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

#include "ws_util.h"
int mv(const char * source, const char *target);

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;
using boost::lexical_cast;

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
    int duration, maxextensions;
    long expiration;
    int extension;
    bool extensionflag;
    string name;
    string filesystem;
    string acctcode, wsdir;
    int reminder=0;
    string mailaddress;
    po::variables_map opt;
    YAML::Node config, userconfig;

    // check commandline
    commandline(opt, name, duration, filesystem, extensionflag, argc, argv);

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
    string username = getusername();
    string dbfilename=config["workspaces"][filesystem]["database"].as<string>()+"/"+username+"-"+name;

    // does db entry exist?
    // cout << "file: " << dbfilename << endl;
    if(fs::exists(dbfilename)) {
        read_dbfile(dbfilename, wsdir, expiration, extension, acctcode, reminder, mailaddress);

        string timestamp = lexical_cast<string>(time(NULL)); 

        string dbtargetname = fs::path(dbfilename).parent_path().string() + "/" + 
                                config["workspaces"][filesystem]["deleted"].as<string>() +
                                "/" + username + "-" + name + "-" + timestamp;
        // cout << dbfilename.c_str() << "-" << dbtargetname.c_str() << endl;
        raise_cap(CAP_DAC_OVERRIDE);
        if(rename(dbfilename.c_str(), dbtargetname.c_str())) {
            // cerr << "rename " << dbfilename.c_str() << " -> " << dbtargetname.c_str() << " failed" << endl;
            lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());
            cerr << "Error: database entry could not be deleted." << endl;
            exit(-1);
        }
        lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());

        // rational: we move the workspace into deleted directory and append a timestamp to name
        // as a new workspace could have same name and releasing the new one would lead to a name
        // collision, so a timestamp is kind of generation label attached to a workspace

        string wstargetname = fs::path(wsdir).parent_path().string() + "/" + 
                                config["workspaces"][filesystem]["deleted"].as<string>() +
                                "/" + username + "-" + name + "-" + timestamp;

        // cout << wsdir.c_str() << " - " << wstargetname.c_str() << endl;
        raise_cap(CAP_DAC_OVERRIDE);
        if(rename(wsdir.c_str(), wstargetname.c_str())) {
            // cerr << "rename " << wsdir.c_str() << " -> " << wstargetname.c_str() << " failed " << geteuid() << " " << getuid() << endl;
            
            // fallback to mv for filesystems where rename() of directories returns EXDEV 
            int r = mv(wsdir.c_str(), wstargetname.c_str());
            if(r!=0) {
                lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());
                cerr << "Error: could not remove workspace!" << endl;
                exit(-1);
            }
        }
        lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());

    } else {
        cerr << "Error: workspace does not exist!" << endl;
        exit(-1);
    }
}

/*
 * fallback for rename in case of EXDEV
 * we do not use system() as we are in setuid
 * and it would fail.
 */
int mv(const char * source, const char *target) {
    pid_t pid;
    int status;
    pid = fork();
    if (pid==0) {
        execl("/bin/mv", "mv", source, target, NULL);
    } else if (pid<0) {
        //
    } else {
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
    return 0;
}
