
#include "ws.h"

// POSIX stuff
// #define _XOPEN_SOURCE
// #define _BSD_SOURCE
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifndef SETUID
#include <sys/capability.h>
#endif

// C++ stuff
#include <string>
#include <vector>

// YAML
#include <yaml-cpp/yaml.h>

// BOOST
#include <boost/regex.hpp>
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

// LUA
#ifdef LUACALLOUTS
#include <lua.hpp>
#endif

#include "ws_util.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

using namespace std;


Workspace::Workspace(const whichclient clientcode, const int _duration) : duration(_duration) {

    // set a umask so users can access db files
    umask(0002);

    // read config
    try {
        config = YAML::LoadFile("/etc/ws.conf");
    } catch (YAML::BadFile) {
        cerr << "Error: no config file!" << endl;
        exit(-1);
    }
    db_uid = config["dbuid"].as<int>();
    db_gid = config["dbgid"].as<int>();

    // lower capabilities to minimum
    drop_cap(CAP_DAC_OVERRIDE, CAP_CHOWN, db_uid);
    // read private config
    raise_cap(CAP_DAC_OVERRIDE);


    // read private config
    raise_cap(CAP_DAC_OVERRIDE);
    try {
        userconfig = YAML::LoadFile("ws_private.conf");
    } catch (YAML::BadFile) {
        // we do not care
    }

    // lower again, nothing needed
    lower_cap(CAP_DAC_OVERRIDE, db_uid);

    string username = getusername();
    
    // valide the input  (opt contains name, duration and filesystem as well)
    validate(clientcode, config, userconfig, opt, filesystem, duration, maxextensions, acctcode);
}

/*
 *  create a workspace and its DB entry
 *
 */

bool Workspace::allocate(string name, bool extensionflag, string user_option) {
    string wsdir;
    long expiration;
    int extension;
    string acctcode, mailaddress;
    int reminder = 0;

#ifdef LUACALLOUTS
    // see if we have a prefix callout
    string prefixcallout;
    lua_State* L;
    if(config["workspaces"][filesystem]["prefix_callout"]) {
        prefixcallout = config["workspaces"][filesystem]["prefix_callout"].as<string>();
        L = lua_open();
        luaL_openlibs(L);
        if(luaL_dofile(L, prefixcallout.c_str())) {
            cerr << "Error: prefix callout script does not exist!" << endl;
            prefixcallout = "";
        }
    }
#endif

    // construct db-entry name, special case if called by root with -x and -u, allows overwrite of maxextensions
    string dbfilename;
    if(extensionflag && user_option.length()>0) {
        dbfilename=config["workspaces"][filesystem]["database"].as<string>() + "/"+user_option+"-"+name;
        if(!fs::exists(dbfilename)) {
            cerr << "Error: workspace does not exist, can not be extended!" << endl;
            exit(-1);
        }
    } else {
        dbfilename=config["workspaces"][filesystem]["database"].as<string>() + "/"+username+"-"+name;
    }

    // does db entry exist?
    if(fs::exists(dbfilename)) {
        read_dbfile(dbfilename, wsdir, expiration, extension, acctcode, reminder, mailaddress);
        // if it exists, print it, if extension is required, extend it
        if(extensionflag) {
            // we allow a user to specify -u -x together, and to extend a workspace if the has rights on the workspace
            if(user_option.length()>0 && (user_option != username) && (getuid() != 0)) {
                cerr << "Info: you are not owner of the workspace." << endl;
                if(access(wsdir.c_str(), R_OK|W_OK|X_OK)!=0) {
                    cerr << "Info: and you have no permissions to access the workspace, workspace will not be extended." << endl;
                    exit(-1);
                }
            }
            cerr << "Info: extending workspace." << endl;
            // if root does this, we do not use an extension
            if(getuid()!=0) extension--;
            if(extension<0) {
                cerr << "Error: no more extensions." << endl;
                exit(-1);
            }
            expiration = time(NULL)+duration*24*3600;
            write_dbfile(dbfilename, wsdir, expiration, extension, acctcode, db_uid, db_gid, reminder, mailaddress);
        } else {
            cerr << "Info: reusing workspace." << endl;
        }
    } else {
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
            cerr << "Info: prefix=" << prefix << endl;
            lua_pop(L,1);
        }
#endif 

        // add some random
        srand(time(NULL));
        wsdir = spaces[rand()%spaces.size()]+prefix+"/"+username+"-"+name;

        // make directory and change owner + permissions
        try{
            raise_cap(CAP_DAC_OVERRIDE);
            fs::create_directories(wsdir);
            lower_cap(CAP_DAC_OVERRIDE, db_uid);
        } catch (...) {
            lower_cap(CAP_DAC_OVERRIDE, db_uid);
            cerr << "Error: could not create workspace directory!"  << endl;
            exit(-1);
        }

        raise_cap(CAP_CHOWN);
        if(chown(wsdir.c_str(), getuid(), getgid())) {
            lower_cap(CAP_CHOWN, db_uid);
            cerr << "Error: could not change owner of workspace!" << endl;
            unlink(wsdir.c_str());
            exit(-1);
        }
        lower_cap(CAP_CHOWN, db_uid);

        raise_cap(CAP_DAC_OVERRIDE);
        if(chmod(wsdir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR)) {
            lower_cap(CAP_DAC_OVERRIDE, db_uid);
            cerr << "Error: could not change permissions of workspace!" << endl;
            unlink(wsdir.c_str());
            exit(-1);
        }
        lower_cap(CAP_DAC_OVERRIDE, db_uid);

        extension = maxextensions;
        expiration = time(NULL)+duration*24*3600;
        write_dbfile(dbfilename, wsdir, expiration, extension, acctcode, db_uid, db_gid, reminder, mailaddress);
    }
    cout << wsdir << endl;
    cerr << "remaining extensions  : " << extension << endl;
    cerr << "remaining time in days: " << (expiration-time(NULL))/(24*3600) << endl;
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
    if(nrgroups<=0) {
        cerr << "Error: user in too many groups!" << endl;
    }
    for(int i=0; i<nrgroups; i++) {
        grp=getgrgid(gids[i]);
        if(grp) groupnames.push_back(string(grp->gr_name));    
    }
    grp=getgrgid(getegid());
    primarygroup=string(grp->gr_name);

    // if the user specifies a filesystem, he must be allowed to use it
    if(opt.count("filesystem")) {
        // check ACLs
        vector<string>user_acl;
        vector<string>group_acl;

        // read ACL lists
        try{
            BOOST_FOREACH(string v,
                config["workspaces"][opt["filesystem"].as<string>()]["user_acl"].as<vector<string> >())
                user_acl.push_back(v);
        } catch (...) {};
        try{
            BOOST_FOREACH(string v,
                    config["workspaces"][opt["filesystem"].as<string>()]["group_acl"].as<vector<string> >())
                group_acl.push_back(v);
        } catch (...) {};

        // check ACLs
        bool userok=true;
        if(user_acl.size()>0 || group_acl.size()>0) userok=false;
        BOOST_FOREACH(string grp, groupnames) {
            if( find(group_acl.begin(), group_acl.end(), grp) != group_acl.end() ) {
                userok=true;
                break;
            }
        }
        if( find(user_acl.begin(), user_acl.end(), username) != user_acl.end() ) {
            userok=true;
        }
        if(!userok) {
            cerr << "Error: You are not allowed to use the specified workspace!" << endl;
            exit(4);
        }
    } else {  
        // no filesystem specified, figure out which to use
        map<string, string>groups_defaults;
        map<string, string>user_defaults;
        BOOST_FOREACH(const YAML::Node &v, config["workspaces"]) {
            try{
                BOOST_FOREACH(string u, config["workspaces."][v.as<string>()]["groupdefault"].as<vector<string> >())
                    groups_defaults[u]=v.as<string>();
            } catch (...) {};
            try{
                BOOST_FOREACH(string u, config["workspaces."][v.as<string>()]["userdefault"].as<vector<string> >())
                    user_defaults[u]=v.as<string>();
            } catch (...) {};
        }    
        if( user_defaults.count(username) > 0 ) {
            filesystem=user_defaults[username];
            goto found;
        }
        if( groups_defaults.count(primarygroup) > 0 ) {
            filesystem=groups_defaults[primarygroup];
            goto found;
        }
        BOOST_FOREACH(string grp, groupnames) {
            if( groups_defaults.count(grp)>0 ) {
                filesystem=groups_defaults[grp];
                goto found;
            }
        }
        // fallback, if no per user or group default, we use the config default
        filesystem=config["default"].as<string>();
        found:;
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

        // if we are root, we ignore the limits
        if ( getuid()!=0 && opt["duration"].as<int>() > configduration ) {
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

