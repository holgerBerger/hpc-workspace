#define _XOPEN_SOURCE
#define _BSD_SOURCE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <grp.h>
#include <time.h>
#include "ws_util.h"

using boost::property_tree::ptree;
using namespace std;

const string configname="ws.conf";

/*
 * read the config file
 */
void read_config(ptree &pt) {
	read_json(configname, pt);
}

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
 * write db file and change owner
 */
void write_dbfile(const string filename, const string wsdir, const long expiration, const int extensions, 
					const string acctcode, const int dbuid, const int dbgid, 
					const int reminder, const string mailaddress ) 
{
	ptree pt;
	pt.put("workspace", wsdir);
	pt.put("expiration", expiration);
	pt.put("extensions", extensions);
	pt.put("acctcode", acctcode);
	pt.put("reminder", reminder);
	pt.put("mailaddress", mailaddress);
	write_json(filename, pt);
	if(chown(filename.c_str(), dbuid, dbgid)) {
		cerr << "Error: could not change owner of database entry" << endl;
	}
}

/*
 * read dbfile and return contents
 */
void read_dbfile(string filename, string &wsdir, long &expiration, int &extensions, string &acctcode,
						int reminder, string mailaddress) 
{
	ptree pt;
	read_json(filename, pt);
	wsdir = pt.get<string>("workspace");
	expiration = pt.get<long>("expiration");
	extensions = pt.get<int>("extensions");
	acctcode = pt.get<string>("acctcode");
	reminder = pt.get<int>("reminder");
	mailaddress = pt.get<string>("mailaddress");
}


/*
 *  validate the commandline versus the configuration file, to see if the user 
 *  is allowed to do what he asks for.
 */
void validate(const whichcode wc, ptree &pt, po::variables_map &opt, string &filesystem, int &duration, 
				int &maxextensions, string &primarygroup) 
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
		groupnames.push_back(string(grp->gr_name));	
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
			BOOST_FOREACH(ptree::value_type &v, 
					pt.get_child(string("workspaces.")+opt["filesystem"].as<string>()+".user_acl")) {
				user_acl.push_back(v.second.data());
			}
		} catch (...) {};
		try{
			BOOST_FOREACH(ptree::value_type &v, 
					pt.get_child(string("workspaces.")+opt["filesystem"].as<string>()+".group_acl")) {
				group_acl.push_back(v.second.data());
			}
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
		BOOST_FOREACH(ptree::value_type &v, pt.get_child(string("workspaces"))) {
			try{
				BOOST_FOREACH(ptree::value_type &u, 
						pt.get_child(string("workspaces.")+v.first.data()+".groupdefault")) 
					groups_defaults[u.second.data()]=v.first.data();
			} catch (...) {};
			try{
				BOOST_FOREACH(ptree::value_type &u, 
						pt.get_child(string("workspaces.")+v.first.data()+".userdefault")) 
					user_defaults[u.second.data()]=v.first.data();
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
		filesystem=pt.get<string>("default");
		found:;
	}

	if(wc==ws_allocate) {
		// check durations - userexception in workspace/workspace/global
		boost::optional<int> configduration;
		configduration = pt.get_optional<int>(string("workspaces.")+filesystem+".userexceptions."+username+".duration");
		if(!configduration) {
			configduration = pt.get_optional<int>(string("workspaces.")+filesystem+".duration");
			if(!configduration) {
				configduration = pt.get_optional<int>("duration");
			}
		}
		if ( opt["duration"].as<int>() > configduration.get() ) {
			duration = configduration.get();
			cerr << "Error: Duration longer than allowed for this workspace" << endl;
			cerr << "       setting to allowed maximum of " << duration << endl;
		}

		// get extensions from workspace or default  - userexception in workspace/workspace/global
		boost::optional<int> maxextensions_opt;
		maxextensions_opt = pt.get_optional<int>(string("workspaces.")+filesystem+".userexceptions."+username+".maxextensions");
		if(!maxextensions_opt) {
			maxextensions_opt = pt.get_optional<int>(string("workspaces.")+filesystem+".maxextensions");
			if(!maxextensions_opt) {
				maxextensions_opt = pt.get_optional<int>("maxextensions");
			}
		}
		maxextensions=maxextensions_opt.get();
	}

}
