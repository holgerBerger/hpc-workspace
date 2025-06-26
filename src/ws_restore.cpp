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
#define _DEFAULT_SOURCE
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <syslog.h>

#include <algorithm>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#ifdef USE_BOOST_REGEXP
    #include <boost/regex.hpp>
    #define REGEX boost::regex
#else
    #include <regex>
    #define REGEX std::regex
#endif


// BOOST
#include <boost/program_options.hpp>
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

    po::options_description secret_options("Secret");
    secret_options.add_options()
        ("debug", "show debugging information")
        ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1);
    p.add("target", 2);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opt);
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
#ifdef IS_GIT_REPOSITORY
        cout << "workspace build from git commit hash " << GIT_COMMIT_HASH
             << " on top of release " << WS_VERSION << endl;
#else
        cout << "workspace version " << WS_VERSION << endl;
#endif
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
        // static const std::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$"); // #77
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
    // as username can contain -, splitting is not good here
    //  name has shape:    username-id-timestamp
    //                             ^ search for this
    auto lpos = name.rfind("-");
    if (lpos==string::npos) {
	    cerr << "Error: unexpected error in check_name, no - in name" << endl;
	    exit(-1);
    }
    auto pos = name.rfind("-", lpos-1);
    if (pos==string::npos) {
	    cerr << "Error: unexpected error in check_name, no second - in name" << endl;
	    exit(-1);
    }
    auto foundname = name.substr(0, pos+1);

    // we checked already that only root can use another username with -u, so here
    // we know we are either root or username == real_username
    if ((username != foundname) && (real_username != "root")) {
        cerr << "Error: only root can do this, or invalid workspace name! " << username << "," << foundname << endl;
        return false;
    } else {
        return true;
    }
}

// get list of valid filesystems for current user
std::vector<string> get_valid_fslist() {
  vector<string> fslist;

  YAML::Node config;
  // read config
  try {
      config = YAML::LoadFile("/etc/ws.conf");
  } catch (const YAML::BadFile& e) {
      cerr << "Error: Could not read config file!" << endl;
      cerr << e.what() << endl;
      exit(-1);
  }

  // get user name, group names etc
  vector<string> groupnames;

  string username = Workspace::getusername(); // FIXME is this correct? what if username given on commandline?

  struct group *grp;
  int ngroups = 128;
  gid_t gids[128];
  int nrgroups;
  string primarygroup;

  nrgroups = getgrouplist(username.c_str(), geteuid(), gids, &ngroups);
  if(nrgroups<=0) {
      cerr << "Error: user in too many groups!" << endl;
  }
  for(int i=0; i<nrgroups; i++) {
      grp=getgrgid(gids[i]);
      if(grp) groupnames.push_back(string(grp->gr_name));
  }
  // get current group
  grp=getgrgid(getegid());
  if(grp==NULL) {
      cerr << "Error: user has no group anymore!" << endl;
      exit(-1);
  }
  primarygroup=string(grp->gr_name);

  // iterate over all filesystems and search the ones allowed for current user
  YAML::Node node = config["workspaces"];
  for(YAML::const_iterator it = node.begin(); it!=node.end(); ++it ) {
      std::string cfilesystem = it->first.as<std::string>();
      // check ACLs
      vector<string>user_acl;
      vector<string>group_acl;

      // read ACL lists
      if ( config["workspaces"][cfilesystem]["user_acl"]) {
          for(string v: config["workspaces"][cfilesystem]["user_acl"].as<vector<string> >())
              user_acl.push_back(v);
      }
      if ( config["workspaces"][cfilesystem]["group_acl"]) {
          for(string v: config["workspaces"][cfilesystem]["group_acl"].as<vector<string> >())
              group_acl.push_back(v);
      }

      // check ACLs
      bool userok=true;
      if(user_acl.size()>0 || group_acl.size()>0) userok=false;

      if( find(group_acl.begin(), group_acl.end(), primarygroup) != group_acl.end() ) {
          userok=true;
      }
#ifdef CHECK_ALL_GROUPS
      for(string grp: groupnames) {
          if( find(group_acl.begin(), group_acl.end(), grp) != group_acl.end() ) {
              userok=true;
              break;
          }
      }
#endif
      if( find(user_acl.begin(), user_acl.end(), username) != user_acl.end() ) {
          userok=true;
      }
      if(userok || getuid()==0) {
          fslist.push_back(cfilesystem);
      }
  }
  return fslist;
}

// get restorable workspaces as names
vector<string> getRestorable(string filesystem, string username)
{
    YAML::Node config;
    // read config
    try {
        config = YAML::LoadFile("/etc/ws.conf");
    } catch (const YAML::BadFile& e) {
        cerr << "Error: Could not read config file!" << endl;
        cerr << e.what() << endl;
        exit(-1);
    }

    string dbprefix = config["workspaces"][filesystem]["database"].as<string>() + "/" +
                      config["workspaces"][filesystem]["deleted"].as<string>();

    vector<string> namelist;

    fs::directory_iterator end;
    for (fs::directory_iterator it(dbprefix); it!=end; ++it) {
#if BOOST_VERSION < 105000
        if (boost::starts_with(it->path().filename(), username + "-" )) {
            namelist.push_back(it->path().filename());
        }
#else
        if (boost::starts_with(it->path().filename().string(), username + "-" )) {
            namelist.push_back(it->path().filename().string());
        }
#endif
    }

    return namelist;
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
    Workspace::drop_cap(CAP_DAC_OVERRIDE, CAP_DAC_READ_SEARCH, db_uid);


    // check commandline, get flags which are used to create ws object or for workspace allocation
    commandline(opt, name, target, filesystem, listflag, terse, username, argc, argv);

    openlog("ws_restore", 0, LOG_USER); // SYSLOG

    if (listflag) {
        
        for(string fs: get_valid_fslist()) {
            std::cout << fs << ":" << std::endl;

            // construct db-entry username  name
            string real_username = Workspace::getusername();
            if (username == "") {
                username = real_username;
            } else if (real_username != username) {
                if (real_username != "root") {
                    cerr << "Error: only root can do that. 2" << endl;
                    username = real_username;
                    exit(-1);
                }
            }
            for(string dn: getRestorable(fs, username)) {
                cout << dn << endl;
                if (!terse) {
                    std::vector<std::string> splitted;
                    boost::split(splitted, dn, boost::is_any_of("-"));
                    time_t t = atol(splitted[splitted.size()-1].c_str());
                    cout << "\tunavailable since " << std::ctime(&t);
                }
            }

        }

    } else {
        // get workspace object
        Workspace ws(WS_Restore, opt, duration, filesystem);

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
        if (check_name(name, username, real_username)) {
            if (ruh()) {
                ws.restore(name, target, username);
            } else {
                syslog(LOG_INFO, "user <%s> failed ruh test.", username.c_str());
            }
        }
    }
}
