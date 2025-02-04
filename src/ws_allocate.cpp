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


#include <iostream>
#include <fstream>
#include <string>
#ifdef USE_BOOST_REGEXP
	#include <boost/regex.hpp>
    #define REGEX boost::regex
#else
	#include <regex>
    #define REGEX std::regex
#endif
#include <syslog.h>

// YAML
#include <yaml-cpp/yaml.h>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>


#include "ws.h"

namespace po = boost::program_options;
using namespace std;


/* 
 *  parse the commandline and see if all required arguments are passed, and check the workspace name for 
 *  bad characters
 */
void commandline(po::variables_map &opt, string &name, int &duration, const int durationdefault, string &filesystem, 
                    bool &extension, int &reminder, const int reminderdefault, string &mailaddress, string &user, string &groupname, string &comment,
                    int argc, char**argv, std::stringstream &userconf) {
    // define all options

    po::options_description cmd_options( "\nOptions" );
    cmd_options.add_options()
            ("help,h", "produce help message")
            ("version,V", "show version")
            ("duration,d", po::value<int>(&duration), "duration in days")
            //("duration,d", po::value<int>(&duration)->default_value(durationdefault), "duration in days")
            ("name,n", po::value<string>(&name), "workspace name")
            ("filesystem,F", po::value<string>(&filesystem), "filesystem")
            ("reminder,r", po::value<int>(&reminder), "reminder to be sent n days before expiration")
            ("mailaddress,m", po::value<string>(&mailaddress), "mailaddress to send reminder to")
            ("extension,x", "extend workspace")
            ("username,u", po::value<string>(&user), "username")
            ("group,g", "group workspace")
            ("groupname,G", po::value<string>(&groupname)->default_value(""), "groupname")
            ("comment,c", po::value<string>(&comment), "comment")
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
#ifdef IS_GIT_REPOSITORY
        cout << "workspace build from git commit hash " << GIT_COMMIT_HASH
             << " on top of release " << WS_VERSION << endl;
#else
        cout << "workspace version " << WS_VERSION << endl;
#endif
        exit(1);
    }

    // this allows user to extend foreign workspaces
    if(opt.count("username") && !( opt.count("extension") || getuid()==0 ) ) {
        cerr << "Info: Ignoring username option." << endl;
		user="";
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
	
	// get some values from ~/.ws_user.conf if not on commandline
	//  mail:  mail address for reminder
	//  reminder: days before expiry to send reminder
	//  duration: duration of workspace 
	//  groupname: group for group workspace
	YAML::Node user_home_config;
	std::string firstline;
    getline(userconf, firstline);
    if ((firstline.find(":",0) == string::npos) && (firstline.find("@",0) != string::npos)) {
		cerr << "Info: ignoring ~/.ws_user.conf, seems to be in old, non yaml format and contain no mail: in front of mail address" << endl;
	} else {
        user_home_config = YAML::Load(userconf.str());
		if(opt.count("reminder")==0) {
			try {
				reminder = user_home_config["reminder"].as<int>();
				cerr << "Info: reminder <" << reminder << "> taken from ~/.ws_user.conf" << endl;
			} catch (...) {
				reminder = reminderdefault;
			}
		}
		if(groupname=="") {
			try {
				groupname = user_home_config["groupname"].as<std::string>();
				// ugly hack to get changed groupname into validator, we parse some pseudo commandline fake command line option
				auto tg = groupname;
				int largc=3; char* largv[] = {(char *)"",(char*)"-G",(char*)tg.c_str(),NULL};
				po::store(po::command_line_parser(largc, largv).options(all_options).positional(p).run(), opt);
				po::notify(opt);
				cerr << "Info: groupname <" << groupname << "> taken from ~/.ws_user.conf" << endl;
				cerr << "Warning: your workspace will be group writable, as your ~/.ws_user.conf contains a groupname directive!" << endl;
			} catch (...) {
				groupname = "";
			}
		}
		if(opt.count("duration")==0) {
			try {
				duration = user_home_config["duration"].as<int>();
				cerr << "Info: duration <" << duration << "> taken from ~/.ws_user.conf" << endl;
			} catch (...) {
				duration = durationdefault;
			}
		} 
	}

    // reminder check, if we have a reminder number, we need either a mailaddress argument or config file
    // with mailaddress in user home
    if(reminder!=0) {
        if (!opt.count("mailaddress")) {
            // check if file looks like yaml
            if (firstline.find(":",0) != string::npos) {
                try {
                    mailaddress = user_home_config["mail"].as<std::string>();
                } catch (...) {
                    mailaddress = "";
                }
            } else {
                mailaddress = firstline;
            }
            if(mailaddress.length()>0) {                        
                cerr << "Info: Took email address <" << mailaddress << "> from users config." << endl;
            } else {
                mailaddress = Workspace::getusername();
                cerr << "Info: could not read email from users config ~/.ws_user.conf." << endl;
                cerr << "Info: reminder email will be sent to local user account" << endl;
            }
        }
		// The same for the mailaddress (if set), because ws_restore python script leads to dataloss if non utf-8 characters are present in mailadress:
		//  UnicodeDecodeError: 'utf-8' codec can't decode byte 0xc5 in position 137: invalid continuation byte
		static const REGEX em("^[[:alnum:]][[:alnum:]_.@-]*$");
		if (!regex_match(mailaddress, em)) {
			cerr << "Error: Illegal non-utf8 mailadress, use characters and numbers, -,.,_ and @ only!" << endl;
			exit(1);
		}
        if (reminder>=duration) {
            cerr << "Warning: reminder is only sent after workspace expiry!" << endl;
        }
    } else {
        // check if mail address was set with -m but not -r
        if(opt.count("mailaddress") && !opt.count("extension")) {
            cerr << "Error: You can't use the mailaddress (-m) without the reminder (-r) option." << endl;
            exit(1);
        } 
    }


    // validate workspace name against nasty characters    
    //  static const std::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$");  // #77

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
    int duration, durationdefault;
    bool extensionflag;
    string name;
    string filesystem;
    string mailaddress("");
    string user_option, groupname;
	string comment;
    int reminder=0, reminderdefault;
    po::variables_map opt;

    // we only support C locale, if the used local is not installed on the system
    // ws_allocate fails
    setenv("LANG","C",1);
    setenv("LC_CTYPE","C",1);
	setenv("LC_ALL","C",1);
    std::setlocale(LC_ALL, "C");
    std::locale::global(std::locale("C"));
	boost::filesystem::path::imbue(std::locale());
    
    // read config (for dbuid, reminder and duration default only)
    YAML::Node config;
    try {
        config = YAML::LoadFile("/etc/ws.conf");
    } catch (const YAML::BadFile& e) {
        cerr << "Error: Could not read config file!" << endl;
        cerr << e.what() << endl;
        exit(-1);
    }

    reminderdefault = config["reminderdefault"].as<int>(0);
    durationdefault = config["durationdefault"].as<int>(1);

    // read user config before dropping privileges to DB user
    //
    // get user first, so user config is read as owner
    // of files, which is needed for root_squash homes
    Workspace::drop_cap(CAP_DAC_OVERRIDE, CAP_CHOWN, CAP_FOWNER, getuid(), __LINE__, __FILE__);

    std::stringstream user_conf;
    string user_conf_filename = Workspace::getuserhome()+"/.ws_user.conf";
    if (!boost::filesystem::is_symlink(user_conf_filename)) {
        std::ifstream t(user_conf_filename.c_str());
        user_conf << t.rdbuf();
    } else {
        cerr << "Error: ~/.ws_user.conf can not be symlink!" << endl;
        exit(-1);
    }

    // check commandline, get flags which are used to create ws object or for workspace allocation
    commandline(opt, name, duration, durationdefault , filesystem, extensionflag, 
				reminder, reminderdefault, mailaddress, user_option, groupname, comment, argc, argv, user_conf);

    openlog("ws_allocate", 0, LOG_USER); // SYSLOG

    // get workspace object
    Workspace ws(WS_Allocate, opt, duration, filesystem);
    
    // allocate workspace
    ws.allocate(name, extensionflag, reminder, mailaddress, user_option, groupname, comment);
    
}
