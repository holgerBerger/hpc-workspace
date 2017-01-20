/*
 *  workspace++
 *
 *  ws_allocate
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and 
 *  configuration was changed to YAML files.
 * 
 *  differences to old workspace version
 *    - usage of YAML file format
 *    - using setuid or capabilities (needs support by filesystem!)
 *    - supports configuration of reminder emails
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


#include <iostream>
#include <fstream>
#include <string>
#include <syslog.h>

#include <boost/program_options.hpp>
#include <boost/regex.hpp>

#include "ws.h"

namespace po = boost::program_options;
using namespace std;


/* 
 *  parse the commandline and see if all required arguments are passed, and check the workspace name for 
 *  bad characters
 */
void commandline(po::variables_map &opt, string &name, int &duration, string &filesystem, 
                    bool &extension, int &reminder, string &mailaddress, string &user,
                    int argc, char**argv) {
    // define all options

    po::options_description cmd_options( "\nOptions" );
    cmd_options.add_options()
            ("help,h", "produce help message")
            ("version,V", "show version")
            ("duration,d", po::value<int>(&duration)->default_value(1), "duration in days")
            ("name,n", po::value<string>(&name), "workspace name")
            ("filesystem,F", po::value<string>(&filesystem), "filesystem")
            ("reminder,r", po::value<int>(&reminder), "reminder to be sent n days before expiration")
            ("mailaddress,m", po::value<string>(&mailaddress), "mailaddress to send reminder to")
            ("extension,x", "extend workspace")
            ("username,u", po::value<string>(&user), "username")
            ("group,g", "group workspace")
    ;

    po::options_description secret_options("Secret");
    secret_options.add_options()
        ("debug", "show debugging information")
        ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1).add("duration",2);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        cout << "Usage: " << argv[0] << ": [options] workspace_name duration" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    // see whats up

    if (opt.count("help")) {
        cout << "Usage: " << argv[0] << ": [options] workspace_name duration" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    if (opt.count("version")) {
        cout << "workspace version " << GIT_COMMIT_HASH << endl;
        exit(1);
    }

    if(opt.count("username") && !(opt.count("extension") || getuid()==0 )) {
        cerr << "Info: Ignoring username option." << endl;
    }

    if(opt.count("extension")) {
        extension = true;
    } else {
        extension = false;
    }

    if (opt.count("name"))
    {
        //cout << " name: " << name << "\n";
    } else {
        cout << argv[0] << ": [options] workspace_name duration" << endl;
        cout << cmd_options << "\n";
        exit(1);
    }

    // reminder check, if we have a reminder number, we need either a mailaddress argument or config file
    // with mailaddress in user home
    if(reminder!=0) {
        if (reminder > duration) {
            cerr << "Info: reminder in the past, ignored." << endl;
            reminder = 0;
        } else {
            if (!opt.count("mailaddress")) {
                ifstream infile;
                infile.open((Workspace::getuserhome()+"/.ws_user.conf").c_str());
                getline(infile, mailaddress);
                if(mailaddress.length()>0) {
                    cerr << "Info: Took email address <" << mailaddress << "> from users config." << endl;
                } else {
                    cerr << "Info: could not read email from users config ~/.ws_user.conf." << endl;
                    cerr << "Info: reminder will be ignored" << endl;
                    reminder = 0;
                }
            }
        }
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
    string mailaddress("");
    string user_option;
    int reminder = 0;
    po::variables_map opt;

    // check commandline, get flags which are used to create ws object or for workspace allocation
    commandline(opt, name, duration, filesystem, extensionflag, reminder, mailaddress, user_option, argc, argv);

    openlog("ws_allocate", 0, LOG_USER); // SYSLOG

    // get workspace object
    Workspace ws(WS_Allocate, opt, duration, filesystem);
    
    // allocate workspace
    ws.allocate(name, extensionflag, reminder, mailaddress, user_option);
    
}
