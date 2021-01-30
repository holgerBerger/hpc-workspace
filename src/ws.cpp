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
 *  (c) Holger Berger 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020
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
#include <vector>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <sys/wait.h>
#include <syslog.h>


#ifndef SETUID
#include <sys/capability.h>
#else
typedef int cap_value_t;
const int CAP_DAC_OVERRIDE = 0;
const int CAP_CHOWN = 1;
#endif

// YAML
#include <yaml-cpp/yaml.h>

// BOOST
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

// LUA
#ifdef LUACALLOUTS
#include <lua.hpp>
#endif

#include "ws.h"
#include "wsdb.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using boost::lexical_cast;

using namespace std;


/*
 * read global and user config and validate parameters
 */
Workspace::Workspace(const whichclient clientcode, const po::variables_map _opt, const int _duration,
                     string _filesystem)
    : opt(_opt), duration(_duration), filesystem(_filesystem)
{

    // set a umask so users can access db files
    umask(0002);

    // read config
    try {
        config = YAML::LoadFile("/etc/ws.conf");
    } catch (const YAML::BadFile& e) {
        cerr << "Error: Could not read config file!" << endl;
        cerr << e.what() << endl;
        exit(-1);
    }
    db_uid = config["dbuid"].as<int>();
    db_gid = config["dbgid"].as<int>();

    // lower capabilities to minimum
    drop_cap(CAP_DAC_OVERRIDE, CAP_CHOWN, db_uid);
    // read private config
    raise_cap(CAP_DAC_OVERRIDE);
    try {
        userconfig = YAML::LoadFile("/etc/ws_private.conf");
    } catch (const YAML::BadFile&) {
        // we do not care
    }
    // lower again, nothing needed
    lower_cap(CAP_DAC_OVERRIDE, db_uid);

    username = getusername(); // FIXME is this correct? what if username given on commandline?

    // valide the input  (opt contains name, duration and filesystem as well)
    validate(clientcode, config, userconfig, opt, filesystem, duration, maxextensions, acctcode);
}

/*
 *  create a workspace and its DB entry
 */
void Workspace::allocate(const string name, const bool extensionflag, const int reminder, const string mailaddress, string user_option, const string groupname, const string comment) {
    string wsdir, wsdir_nopostfix;
    int extension;
    long expiration;
#ifdef LUACALLOUTS
    // see if we have a prefix callout
    string prefixcallout;
    lua_State* L;
    if(config["workspaces"][filesystem]["prefix_callout"]) {
        prefixcallout = config["workspaces"][filesystem]["prefix_callout"].as<string>();
        L = lua_open();
        luaL_openlibs(L);
        if(luaL_dofile(L, prefixcallout.c_str())) {
            cerr << "Error: prefix callout script <"<< prefixcallout << "> does not exist or other lua error!"  << endl;
            cerr << lua_tostring(L, -1) << endl;
            prefixcallout = "";
        }
    }
#endif

    // TODO iterate over list of allowed filesystems here if user did not specify
    // a filesystem. if user did not specify, check all allowed once for existing db entry,
    // and reuse if it exists, otherwise create in <filesystem>

    string dbfilename;
    bool ws_exists = false;
    vector<string> searchlist;
    if(opt.count("filesystem")) {
        searchlist.push_back(opt["filesystem"].as<string>());
    } else {
        searchlist = get_valid_fslist();
		auto df = find(searchlist.begin(), searchlist.end(), filesystem);
        if(df!=searchlist.end()) {
			auto tmp=searchlist[0];
			searchlist[0]=*df;
			*df=tmp;
			if (opt.count("debug")) {
				cerr << "debug: moved default filesystem to front:" << filesystem << endl;
			}
		} else {
			cerr << "warn: you seem to have no access to your default workspace!?" << endl;
		}
    }

    BOOST_FOREACH(string cfilesystem, searchlist) {
      // construct db-entry name, special case if called by root with -x and -u, allows overwrite of maxextensions
      if (opt.count("debug")) {
		  cerr << "debug: searching valid filesystems " << cfilesystem << endl;
	  }

      if(extensionflag && user_option.length()>0) {
          dbfilename=config["workspaces"][cfilesystem]["database"].as<string>() + "/"+user_option+"-"+name;
          if(!fs::exists(dbfilename)) {
              cerr << "Error: workspace does not exist, can not be extended!" << endl;
              exit(-1);
			  // FIXME looks wrong? exit in loops?
          }
      } else { 
          if(user_option.length()>0 && (getuid()==0)) {
              dbfilename=config["workspaces"][cfilesystem]["database"].as<string>() + "/"+user_option+"-"+name;
          } else { 
              dbfilename=config["workspaces"][cfilesystem]["database"].as<string>() + "/"+username+"-"+name;
          }
      }

      // does db entry exist?
      if(fs::exists(dbfilename)) {
          WsDB dbentry(dbfilename,  config["dbuid"].as<int>(),  config["dbgid"].as<int>());
          wsdir = dbentry.getwsdir();
          extension = dbentry.getextension();
          expiration = dbentry.getexpiration();
          // if it exists, print it, if extension is required, extend it
          if(extensionflag) {
              if ( config["workspaces"][cfilesystem]["extendable"] ) {
                  if ( config["workspaces"][cfilesystem]["extendable"].as<bool>() == false ) {
                      cerr << "Error: workspaces can not be extended in this filesystem." << endl;
                      exit(1);
                  }
              }
              // we allow a user to specify -u -x together, and to extend a workspace if he has rights on the workspace
              if(user_option.length()>0 && (user_option != username) && (getuid() != 0)) {
                  cerr << "Info: you are not owner of the workspace." << endl;
                  if(access(wsdir.c_str(), R_OK|W_OK|X_OK)!=0) {
                      cerr << "Info: and you have no permissions to access the workspace, workspace will not be extended." << endl;
                      exit(-1);
                  }
              }
              cerr << "Info: extending workspace." << endl;
              syslog(LOG_INFO, "extending DB <%s> for user <%s>.", dbfilename.c_str(), username.c_str());

              if(mailaddress!="") {
                   cerr << "Info: changed mail address to " << mailaddress <<  endl;
              }
              if(reminder!=0) {
                   cerr << "Info: changed reminder setting." << endl;
              }
			  if(comment!="") {
				   cerr << "Info: changed comment." << endl;
			  }

              if (duration != 0) {
                expiration = time(NULL)+duration*24*3600;
                dbentry.use_extension(expiration, mailaddress, reminder, comment);
              } else {
                dbentry.use_extension(-1, mailaddress, reminder, comment);
              }
              extension = dbentry.getextension();
          } else {
              cerr << "Info: reusing workspace." << endl;
              syslog(LOG_INFO, "reusing DB <%s> for user <%s>.", dbfilename.c_str(), username.c_str());
          }
          ws_exists = true;
          break;
      }
    }

    if (!ws_exists) {
        if(extensionflag && user_option.length()>0) {
            dbfilename=config["workspaces"][filesystem]["database"].as<string>() + "/"+user_option+"-"+name;
            if(!fs::exists(dbfilename)) {
                cerr << "Error: workspace does not exist, can not be extended!" << endl;
                exit(-1);
            }
        } else {
            if(user_option.length()>0 && (getuid()==0)) {
                dbfilename=config["workspaces"][filesystem]["database"].as<string>() + "/"+user_option+"-"+name;
            } else {
                dbfilename=config["workspaces"][filesystem]["database"].as<string>() + "/"+username+"-"+name;
                if(extensionflag) {
                      if(!fs::exists(dbfilename)) {
                          cerr << "Error: workspace does not exist, can not be extended!" << endl;
                          exit(-1);
                      }
                }
            }
        }


        // workspace does not exist, we have to create one
        if( config["workspaces"][filesystem]["allocatable"] &&
            config["workspaces"][filesystem]["allocatable"].as<bool>() == false )  {
            cerr << "Error: this workspace can not be used for allocation." << endl;
            exit(1);
        }
        // if it does not exist, create it
        cerr << "Info: creating workspace." << endl;
        // read the possible spaces for the filesystem
        vector<string> spaces = config["workspaces"][filesystem]["spaces"].as<vector<string> >();
        string prefix = "";

        // the lua function "prefix" gets called as prefix(filesystem, username)
#ifdef LUACALLOUTS
        if(prefixcallout!="") {
            lua_getglobal(L, "prefix");
            lua_pushstring(L, filesystem.c_str() );
            lua_pushstring(L, username.c_str() );
            lua_call(L, 2, 1);
            prefix = string("/")+lua_tostring(L, -1);
            if (opt.count("debug")) {
                cerr << "Info: prefix=" << prefix << endl;
            }
            lua_pop(L,1);
        }
#endif

        // add some randomness
        srand(time(NULL));
        if (user_option.length()>0 && (user_option != username) && (getuid() != 0)) {
            wsdir = spaces[rand()%spaces.size()]+prefix+"/"+username+"-"+name;
        } else {  // we are root and can change owner!
            string randspace = spaces[rand()%spaces.size()];
            wsdir_nopostfix = randspace+prefix;
            if (user_option.length()>0 && (getuid()==0)) {
                wsdir = randspace+prefix+"/"+user_option+"-"+name;
            } else {
                wsdir = randspace+prefix+"/"+username+"-"+name;
            }
        }

        // make directory and change owner + permissions
        try {
            raise_cap(CAP_DAC_OVERRIDE);
            mode_t oldmask = umask( 077 );    // as we create intermediate directories, we better take care of umask!!
            fs::create_directories(wsdir);
            umask(oldmask);
            lower_cap(CAP_DAC_OVERRIDE, db_uid);
        } catch (...) {
            lower_cap(CAP_DAC_OVERRIDE, db_uid);
            cerr << "Error: could not create workspace directory!"  << endl;
            exit(-1);
        }

        uid_t tuid=getuid();
        gid_t tgid=getgid();

        if (user_option.length()>0) {
            struct passwd *pws = getpwnam(user_option.c_str());
            tuid = pws->pw_uid;
            tgid = pws->pw_gid;
        }

		if (groupname!="") {
			struct group *grp;
            grp=getgrnam(groupname.c_str());
			if (grp) {
				tgid=grp->gr_gid;
			}
		}

        raise_cap(CAP_CHOWN);
        /*
        // removed 3.6.2020, what was it good for??
        if(prefix.length()>0) {  // in case we have a prefix, we change owner of that one
            chown(wsdir_nopostfix.c_str(), tuid, tgid);
        }
        */

        if(chown(wsdir.c_str(), tuid, tgid)) {
            lower_cap(CAP_CHOWN, db_uid);
            cerr << "Error: could not change owner of workspace!" << endl;
            unlink(wsdir.c_str());
            exit(-1);
        }
        lower_cap(CAP_CHOWN, db_uid);

        raise_cap(CAP_DAC_OVERRIDE);
		mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;
        // group workspaces can be read and listed by group
		if (opt.count("group") || groupname!="") {
			mode |= S_IRGRP | S_IXGRP;
		}
		if (groupname!="") {
			mode |= S_IWGRP | S_ISGID;
		}
        if(chmod(wsdir.c_str(), mode)) {
            lower_cap(CAP_DAC_OVERRIDE, db_uid);
            cerr << "Error: could not change permissions of workspace!" << endl;
            unlink(wsdir.c_str());
            exit(-1);
        }
        lower_cap(CAP_DAC_OVERRIDE, db_uid);

        extension = maxextensions;
        expiration = time(NULL)+duration*24*3600;
        string primarygroup;
        if (opt.count("group")) {
            struct group *grp;
            grp=getgrgid(getegid());
            // grp should be ok here, was validated before
            primarygroup = string(grp->gr_name);
        }

		if (groupname!="") {
			primarygroup = groupname;
		}

        WsDB dbentry(dbfilename, wsdir, expiration, extension, acctcode, db_uid, db_gid, reminder, mailaddress, primarygroup, comment);

        syslog(LOG_INFO, "created for user <%s> DB <%s> with space <%s>.", username.c_str(), dbfilename.c_str(), wsdir.c_str());
    }
    cout << wsdir << endl;
    cerr << "remaining extensions  : " << extension << endl;
    cerr << "remaining time in days: " << (expiration-time(NULL))/(24*3600) << endl;

}

/*
 * release a workspace by moving workspace and DB entry into trash
 *
 */
void Workspace::release(string name) {
    string wsdir;

    int dbuid = config["dbuid"].as<int>();
    int dbgid = config["dbgid"].as<int>();

    string userprefix;

    if (opt.count("userworkspace") && (getuid()==0)) {
        cerr << "not prefixing workspace name" << endl;
        userprefix="";
    } else {
        userprefix=username+"-";
    }

    string dbfilename=config["workspaces"][filesystem]["database"].as<string>()+"/"+userprefix+name;

    // does db entry exist?
    // cout << "file: " << dbfilename << endl;
    if(fs::exists(dbfilename)) {
        WsDB dbentry(dbfilename,  config["dbuid"].as<int>(),  config["dbgid"].as<int>());
        wsdir = dbentry.getwsdir();

        string timestamp = lexical_cast<string>(time(NULL));

	// set expiration to now so it gets deleted earlier after beeing released
	dbentry.setexpiration(time(NULL));
	// set released flag so released workspaces can be distinguished from expired ones
    	dbentry.setreleased(time(NULL));
	dbentry.write_dbfile();

        string dbtargetname = fs::path(dbfilename).parent_path().string() + "/" +
                              config["workspaces"][filesystem]["deleted"].as<string>() +
                              "/" + userprefix + name + "-" + timestamp;
        // cout << dbfilename.c_str() << "-" << dbtargetname.c_str() << endl;
        raise_cap(CAP_DAC_OVERRIDE);
#ifdef SETUID
        // for filesystem with root_squash, we need to be DB user here
        setegid(dbgid); seteuid(dbuid); 
#endif
        if(rename(dbfilename.c_str(), dbtargetname.c_str())) {
            // cerr << "rename " << dbfilename.c_str() << " -> " << dbtargetname.c_str() << " failed" << endl;
            lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());
            cerr << "Error: database entry could not be deleted." << endl;
            exit(-1);
        }
        lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());

        // rational: we move the workspace into deleted directory and append a timestamp to name
        // as a new workspace could have same name and releasing the new one would lead to a name
        // collision, so the timestamp is kind of generation label attached to a workspace


        string wstargetname = fs::path(wsdir).parent_path().string() + "/" +
                              config["workspaces"][filesystem]["deleted"].as<string>() +
                              "/" + userprefix + name + "-" + timestamp;
       
        // FIXME when a prefix is used, this is the wrong place!!!
        // may be check in case of prefix callout .. and ../.. for config["workspaces"][filesystem]["deleted"] ??

/*
		cout << "RELEASE:" <<
			"\n  filesystem:" << filesystem <<
			"\n  wstargetname:" << wstargetname << endl;
*/

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

        syslog(LOG_INFO, "release for user <%s> from <%s> to <%s> done, moved DB entry from <%s> to <%s>.", username.c_str(), wsdir.c_str(), wstargetname.c_str(), dbfilename.c_str(), dbtargetname.c_str());

    } else {
        cerr << "Error: workspace does not exist!" << endl;
        exit(-1);
    }

}


/*
 *  validate the commandline versus the configuration file, to see if the user
 *  is allowed to do what he asks for.
 */
void Workspace::validate(const whichclient wc, YAML::Node &config, YAML::Node &userconfig,
                         po::variables_map &opt, string &filesystem, int &duration, int &maxextensions, string &primarygroup)
{

    // get user name, group names etc
    vector<string> groupnames;

    struct group *grp;
    int ngroups = 128;
    gid_t gids[128];
    int nrgroups;

    nrgroups = getgrouplist(username.c_str(), geteuid(), gids, &ngroups);
    if(nrgroups == -1) {
        cerr << "Error: user in too many groups!" << endl;
        exit(-1);
    }
    for(int i=0; i<nrgroups; i++) {
        grp=getgrgid(gids[i]);
        if(grp) {
			groupnames.push_back(string(grp->gr_name));
			if (opt.count("debug")) {
				cerr << "Debug: secondary group " << string(grp->gr_name) << endl; 
			}
		}
    }
    // get current group
    grp=getgrgid(getegid());
    if (grp==NULL) {
        cerr << "Error: user has no group anymore!" << endl;
        exit(-1);
    }
    primarygroup=string(grp->gr_name);

    if (opt.count("debug")) {
        cerr << "debug: primarygroup=" << primarygroup << endl;
    }

    if(wc==WS_Allocate && opt["groupname"].as<string>()!="") {
        if ( find(groupnames.begin(), groupnames.end(), opt["groupname"].as<string>()) == groupnames.end() ) {
            cerr << "Error: invalid group specified!" << endl;
            exit(-1);
        }
    }

    // if the user specifies a filesystem, he must be allowed to use it
    if(opt.count("filesystem")) {

        if (opt.count("debug")) {
            cerr << "debug: filesystem given: " << opt["filesystem"].as<string>() << endl;
        }
        
        // check ACLs
        vector<string>user_acl;
        vector<string>group_acl;

        // check if filesystem is valid
        if (!config["workspaces"][opt["filesystem"].as<string>()]) {
			cerr << "Error: please specify an existing filesystem with -F!" << endl;
            exit(1);
        }

        // read ACL lists
        if ( config["workspaces"][opt["filesystem"].as<string>()]["user_acl"]) {
            BOOST_FOREACH(string v,
                          config["workspaces"][opt["filesystem"].as<string>()]["user_acl"].as<vector<string> >())
                user_acl.push_back(v);
        }

        if ( config["workspaces"][opt["filesystem"].as<string>()]["group_acl"]) {
            BOOST_FOREACH(string v,
                          config["workspaces"][opt["filesystem"].as<string>()]["group_acl"].as<vector<string> >())
                group_acl.push_back(v);
        }

        // check ACLs
        bool userok=true;
        if(user_acl.size()>0 || group_acl.size()>0) {
            userok=false;
            if (opt.count("debug")) {
                cerr << "debug: acls non-empty, all user access denied before check." << endl;
            }
        }

        if( find(group_acl.begin(), group_acl.end(), primarygroup) != group_acl.end() ) {
            userok=true;
            if (opt.count("debug")) {
                cerr << "debug: group found in group acl, access granted." << endl;
            }
        }
#ifdef CHECK_ALL_GROUPS
        BOOST_FOREACH(string grp, groupnames) {
            if( find(group_acl.begin(), group_acl.end(), grp) != group_acl.end() ) {
                userok=true;
                if (opt.count("debug")) {
                    cerr << "debug: secondary group found in group acl, access granted." << endl;
                }
                break;
            }
        }
#endif
        if( find(user_acl.begin(), user_acl.end(), username) != user_acl.end() ) {
            userok=true;
            if (opt.count("debug")) {
                cerr << "debug: user found in user acl, access granted." << endl;
            }
        }
        if(!userok && getuid()!=0) {
            cerr << "Error: You are not allowed to use the specified workspace!" << endl;
            exit(4);
        }
    } else {
        // no filesystem specified, figure out which to use
        if (opt.count("debug")) {
            cerr << "debug: no filesystem given, searching..." << endl;
        }
        map<string, string>groups_defaults;
        map<string, string>user_defaults;
        YAML::Node node = config["workspaces"];
        for(YAML::const_iterator it = node.begin(); it!=node.end(); ++it ) {
            // cout << "v=" <<  it->first << endl;
            std::string v = it->first.as<std::string>();
            if (opt.count("debug")) {
                cerr << "debug: searching " << v << endl;
            }
            // check permissions during search, has to be repeated later in case
            // no search performed
            if(wc==WS_Allocate && !opt.count("extension")) {
                if( config["workspaces"] [it->first] ["allocatable"] &&
                    config["workspaces"][it->first]["allocatable"].as<bool>() == false ) {
                    if (opt.count("debug")) {
                        cerr << "debug: not allocatable, skipping " << endl;
                    }
                    continue;
                }
            }
            if(config["workspaces"][it->first]["groupdefault"]) {
                BOOST_FOREACH(string u, config["workspaces"][it->first]["groupdefault"].as<vector<string> >())
                    groups_defaults[u]=v;
            }
            if(config["workspaces"][it->first]["userdefault"]) {
                BOOST_FOREACH(string u, config["workspaces"][it->first]["userdefault"].as<vector<string> >())
                    user_defaults[u]=v;
            }
        }

        if( user_defaults.count(username) > 0 ) {
            filesystem=user_defaults[username];
            if (opt.count("debug")) {
                cerr << "debug: user default, ending search" << endl;
            }
            goto found;
        }
        // name is misleading, this is current group, not primary group
        if( groups_defaults.count(primarygroup) > 0 ) {
            filesystem=groups_defaults[primarygroup];
            if (opt.count("debug")) {
                cerr << "debug: group default, ending search" << endl;
            }
            goto found;
        }
#ifdef CHECK_ALL_GROUPS
        BOOST_FOREACH(string grp, groupnames) {
            if( groups_defaults.count(grp)>0 ) {
                filesystem=groups_defaults[grp];
                goto found;
            }
        }
#endif
        // fallback, if no per user or group default, we use the config default
		try {
        	filesystem=config["default"].as<string>();
            if (opt.count("debug")) {
                cerr << "debug: fallback, using global default, ending search" << endl;
            }
          goto found;
		} catch (...) {
			cerr << "Error: please specify a valid filesystem with -F!" << endl;
            exit(1);
		}
    // fallback, we end here if user does not specify a filesystem and does
    // not have any default
    cerr << "Error: please specify a valid filesystem with -F." << endl;
    cerr << "The administrator did not configure a default filesystem for you." << endl;
    exit(1);
found:
        ;
    }

    if(wc==WS_Allocate) {
        // check durations - userexception in workspace/workspace/global
        int configduration;
        if(userconfig["workspaces"][filesystem]["userexceptions"][username]["duration"]) {
            configduration = userconfig["workspaces"][filesystem]["userexceptions"][username]["duration"].as<int>();
        } else {
            if(config["workspaces"][filesystem]["duration"]) {
                configduration = config["workspaces"][filesystem]["duration"].as<int>();
            } else {
                configduration = config["duration"].as<int>();
            }
        }

        // if we are root, we ignore the limits, check for negativ duration as well
        if ( getuid()!=0 && ( (opt["duration"].as<int>() > configduration) || (opt["duration"].as<int>() < 0) ) ) {
            duration = configduration;
            cerr << "Error: Duration longer than allowed for this workspace" << endl;
            cerr << "       setting to allowed maximum of " << duration << endl;
        }

        // get extensions from workspace or default  - userexception in workspace/workspace/global
        if(userconfig["workspaces"][filesystem]["userexceptions"][username]["maxextensions"]) {
            maxextensions = userconfig["workspaces"][filesystem]["userexceptions"][username]["maxextensions"].as<int>();
        } else {
            if(config["workspaces"][filesystem]["maxextensions"]) {
                maxextensions = config["workspaces"][filesystem]["maxextensions"].as<int>();
            } else {
                maxextensions = config["maxextensions"].as<int>();
            }
        }
    }
}

/*
 * fallback for rename in case of EXDEV
 * we do not use system() as we are in setuid
 * and it would fail, and it sucks anyhow,
 */
int Workspace::mv(const char * source, const char *target) {
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


/*
 * get user name
 * we have this to avoid cuserid
 */
string Workspace::getusername()
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return string(pw->pw_name);
}

/*
 * get home of current user, we have this to avoid $HOME
 */
string Workspace::getuserhome()
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return string(pw->pw_dir);
}


/*
 * get filesystem
 */
string Workspace::getfilesystem()
{
    return filesystem;
}


/*
 * restore a workspace, argument is name of workspace DB entry including username and timestamp, form user-name-timestamp
 */
void Workspace::restore(const string name, const string target, const string username) {
    string dbfilename = fs::path(config["workspaces"][filesystem]["database"].as<string>()).string()
                         + "/" + config["workspaces"][filesystem]["deleted"].as<string>()+"/"+name;

    string targetdbfilename = fs::path(config["workspaces"][filesystem]["database"].as<string>()).string()
                            + "/" + username + "-" + target;

    string targetwsdir;

    // FIXME should root be able to override this?
    if (config["workspaces"][filesystem]["restorable"]) {
        if (config["workspaces"][filesystem]["restorable"].as<bool>() == false) {
            cerr << "Error: it is not possible to restore workspaces in this filesystem." << endl;
            exit(1);
        }
    }


    // check for target existance and get directory name of workspace, which will be target of mv operations
    if(fs::exists(targetdbfilename)) {
        WsDB targetdbentry(targetdbfilename, config["dbuid"].as<int>(),  config["dbgid"].as<int>());
        targetwsdir = targetdbentry.getwsdir();
    } else {
        cerr << "Error: target workspace does not exist!" << endl;
      	if (opt.count("debug")) {
			cerr << "Debug: target=" << targetdbfilename << endl;
		}
        exit(1);
    }

    if(fs::exists(dbfilename)) {
        WsDB dbentry(dbfilename,  config["dbuid"].as<int>(),  config["dbgid"].as<int>());
        // this is path of original workspace, from this we derive the deleted name
        string wsdir = dbentry.getwsdir();

        // go one up, add deleted subdirectory and add workspace name
        string wssourcename = fs::path(wsdir).parent_path().string() + "/" +
                              config["workspaces"][filesystem]["deleted"].as<string>() +
                              "/" + name;

        // log restore request
        // syslog(LOG_INFO, "restore for user <%s> from <%s> to <%s>.", username, wssourcename.c_str(), targetwsdir.c_str());

        raise_cap(CAP_DAC_OVERRIDE);

        int ret = mv(wssourcename.c_str(), targetwsdir.c_str());
#ifdef SETUID
        // get db user to be able to unlink db entry from root_squash filesystems
        setegid(config["dbgid"].as<int>()); seteuid(config["dbuid"].as<int>());
#endif
        if (ret == 0) {
            unlink(dbfilename.c_str());
            syslog(LOG_INFO, "restore for user <%s> from <%s> to <%s> done, removed DB entry <%s>.", username.c_str(), wssourcename.c_str(), targetwsdir.c_str(), dbfilename.c_str());
            cerr << "Info: restore successful, database entry removed." << endl;
        } else {
            syslog(LOG_INFO, "restore for user <%s> from <%s> to <%s> failed, kept DB entry <%s>.", username.c_str(), wssourcename.c_str(), targetwsdir.c_str(), dbfilename.c_str());
            cerr << "Error: moving data failed, database entry kept!" << endl;
        }
#ifdef SETUID
        seteuid(0); setegid(0);
#endif
        lower_cap(CAP_DAC_OVERRIDE, config["dbuid"].as<int>());


    } else {
        cerr << "Error: workspace does not exist." << endl;
    }
}


/*
 * drop effective capabilities, except CAP_DAC_OVERRIDE | CAP_CHOWN
 */
void Workspace::drop_cap(cap_value_t cap_arg, int dbuid)
{
#ifndef SETUID
    cap_t caps;
    cap_value_t cap_list[1];

    cap_list[0] = cap_arg;

    caps = cap_init();

    // cap_list[0] = CAP_DAC_OVERRIDE;
    // cap_list[1] = CAP_CHOWN;

    if (cap_set_flag(caps, CAP_PERMITTED, 1, cap_list, CAP_SET) == -1) {
        cerr << "Error: problem with capabilities." << endl;
        exit(1);
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem dropping capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
        exit(1);
    }

    cap_free(caps);
#else
    // seteuid(0);
    if(seteuid(dbuid)) {
        cerr << "Error: can not change uid." << endl;
        exit(1);
    }
#endif
}

void Workspace::drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid)
{
#ifndef SETUID
    cap_t caps;
    cap_value_t cap_list[2];

    cap_list[0] = cap_arg1;
    cap_list[1] = cap_arg2;

    caps = cap_init();

    // cap_list[0] = CAP_DAC_OVERRIDE;
    // cap_list[1] = CAP_CHOWN;

    if (cap_set_flag(caps, CAP_PERMITTED, 2, cap_list, CAP_SET) == -1) {
        cerr << "Error: problem with capabilities." << endl;
        exit(1);
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem dropping capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
        exit(1);
    }

    cap_free(caps);
#else
    // seteuid(0);
    if(seteuid(dbuid)) {
        cerr << "Error: can not change uid." << endl;
        exit(1);
    }
#endif

}

/*
 * remove a capability from the effective set
 */
void Workspace::lower_cap(int cap, int dbuid)
{
#ifndef SETUID
    cap_t caps;
    cap_value_t cap_list[1];

    caps = cap_get_proc();

    cap_list[0] = cap;
    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_CLEAR) == -1) {
        cerr << "Error: problem with capabilities." << endl;
        exit(1);
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem lowering capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
        exit(1);
    }

    cap_free(caps);
#else
    // seteuid(0);

    if(seteuid(dbuid)) {
        cerr << "Error: can not change uid." << endl;
        exit(1);
    }
#endif
}

/*
 * add a capability to the effective set
 */
void Workspace::raise_cap(int cap)
{
#ifndef SETUID
    cap_t caps;
    cap_value_t cap_list[1];

    caps = cap_get_proc();

    cap_list[0] = cap;
    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET) == -1) {
        cerr << "Error: problem with capabilities." << endl;
        exit(1);
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem raising capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
        exit(1);
    }

    cap_free(caps);
#else
    if (seteuid(0)) {
        cerr << "Error: can not change uid." << endl;
        exit(1);
    }
#endif
}

std::vector<string> Workspace::get_valid_fslist() {
  vector<string> fslist;

  // get user name, group names etc
  vector<string> groupnames;

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

      if (opt.count("debug")) {
          cerr << "debug: find_valid_fs:" << cfilesystem << endl;
      }

      // read ACL lists
      if ( config["workspaces"][cfilesystem]["user_acl"]) {
          BOOST_FOREACH(string v,
                        config["workspaces"][cfilesystem]["user_acl"].as<vector<string> >())
              user_acl.push_back(v);
      }
      if ( config["workspaces"][cfilesystem]["group_acl"]) {
          BOOST_FOREACH(string v,
                        config["workspaces"][cfilesystem]["group_acl"].as<vector<string> >())
              group_acl.push_back(v);
      }

      // check ACLs
      bool userok=true;
      if(user_acl.size()>0 || group_acl.size()>0) {
          if (opt.count("debug")) {
              cerr << "debug: find_valid_fs, has ACL, access denied." << endl;
          }
          userok=false;
      }

      if( find(group_acl.begin(), group_acl.end(), primarygroup) != group_acl.end() ) {
          userok=true;
          if (opt.count("debug")) {
              cerr << "debug: find_valid_fs, in group ACL, access granted." << endl;
          }
      }
#ifdef CHECK_ALL_GROUPS
      BOOST_FOREACH(string grp, groupnames) {
          if( find(group_acl.begin(), group_acl.end(), grp) != group_acl.end() ) {
			  if (opt.count("debug")) {
				  cerr << "debug: find_valid_fs, in group ACL, access granted (secondary)." << endl;
			  }
              userok=true;
              break;
          }
      }
#endif
      if( find(user_acl.begin(), user_acl.end(), username) != user_acl.end() ) {
          userok=true;
          if (opt.count("debug")) {
              cerr << "debug: find_valid_fs, in user ACL, access granted." << endl;
          }
      }
      if(userok || getuid()==0) {
          if (opt.count("debug")) {
              cerr << "debug: find_valid_fs, granted" << endl;
          }
          fslist.push_back(cfilesystem);
      } else {
          if (opt.count("debug")) {
              cerr << "debug: find_valid_fs, denied" << endl;
          }
      }
  }
  return fslist;
}
