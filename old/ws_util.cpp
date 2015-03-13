#define _XOPEN_SOURCE
#define _BSD_SOURCE
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <grp.h>
#include <time.h>

#include <sys/capability.h>

#include "ws_util.h"

using namespace std;

const string configname="/etc/ws.conf";

// FIXME: THIS SHOULD ALL GO AWAY INTO ws.cpp and wsdb.cpp

/*
 * get user name
 * we have this to avoud cuserid
 */
string getusername() 
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return string(pw->pw_name);
}

/*
 * get home of current user, we have this to avoid $HOME
 */
string getuserhome()
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return string(pw->pw_dir);
}

/*
 * drop effective capabilities, except CAP_DAC_OVERRIDE | CAP_CHOWN 
 */
void drop_cap(cap_value_t cap_arg, int dbuid) 
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
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem dropping capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
    }

    cap_free(caps);
#else
    // seteuid(0);
    seteuid(dbuid);
#endif
}

void drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid) 
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
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem dropping capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
    }

    cap_free(caps);
#else
    // seteuid(0);
    seteuid(dbuid);
#endif

}

/*
 * remove a capability from the effective set
 */
void lower_cap(int cap, int dbuid)
{
#ifndef SETUID
    cap_t caps;
    cap_value_t cap_list[1];

    caps = cap_get_proc();

    cap_list[0] = cap;
    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_CLEAR) == -1) {
        cerr << "Error: problem with capabilities." << endl;
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem lowering capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
    }

    cap_free(caps);
#else
    // seteuid(0);
    seteuid(dbuid);
#endif
}

/*
 * add a capability to the effective set
 */
void raise_cap(int cap)
{
#ifndef SETUID
    cap_t caps;
    cap_value_t cap_list[1];

    caps = cap_get_proc();

    cap_list[0] = cap;
    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET) == -1) {
        cerr << "Error: problem with capabilities." << endl;
    }

    if (cap_set_proc(caps) == -1) {
        cerr << "Error: problem raising capabilities." << endl;
        cap_t cap = cap_get_proc();
        cerr << "Running with capabilities: " << cap_to_text(cap, NULL) << endl;
        cap_free(cap);
    }

    cap_free(caps);
#else
    seteuid(0);
#endif
}

/*
 * write db file and change owner
 */
void write_dbfile(const string filename, const string wsdir, const long expiration, const int extensions, 
                    const string acctcode, const int dbuid, const int dbgid, 
                    const int reminder, const string mailaddress ) 
{
    YAML::Node entry;
    entry["workspace"] = wsdir;
    entry["expiration"] = expiration;
    entry["extensions"] = extensions;
    entry["acctcode"] = acctcode;
    entry["reminder"] = reminder;
    entry["mailaddress"] = mailaddress;
    raise_cap(CAP_DAC_OVERRIDE);
    ofstream fout(filename.c_str());
    fout << entry;
    fout.close();
    lower_cap(CAP_DAC_OVERRIDE, dbuid);
    raise_cap(CAP_CHOWN);
    if(chown(filename.c_str(), dbuid, dbgid)) {
        lower_cap(CAP_CHOWN, dbuid);
        cerr << "Error: could not change owner of database entry" << endl;
    }
    lower_cap(CAP_CHOWN, dbuid);
}

/*
 * read dbfile and return contents
 */
void read_dbfile(string filename, string &wsdir, long &expiration, int &extensions, string &acctcode,
                        int reminder, string mailaddress) 
{
    YAML::Node entry = YAML::LoadFile(filename);
    wsdir = entry["workspace"].as<string>();
    expiration = entry["expiration"].as<long>();
    extensions = entry["extensions"].as<int>();
    acctcode = entry["acctcode"].as<string>();
    reminder = entry["reminder"].as<int>();
    mailaddress = entry["mailaddress"].as<string>();
}


/*
 *  validate the commandline versus the configuration file, to see if the user 
 *  is allowed to do what he asks for.
 */
// FIXME: THIS IS REDUNDANT WITH WS.CPP, SHOULD BE REMOVED AFTER MIGRATION TO NEW CODE
void validate(const whichcode wc, YAML::Node &config, YAML::Node &userconfig, 
                po::variables_map &opt, string &filesystem, int &duration, int &maxextensions, string &primarygroup) 
{

    // get user name, group names etc
    string username = getusername();
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

    if(wc==ws_allocate) {
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

/*
 * fallback for rename in case of EXDEV
 * we do not use system() as we are in setuid
 * and it would fail, and it sucks anyhow,
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