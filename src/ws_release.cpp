
/*
 *	workspace++
 *
 *  ws_release
 *
 *	c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and 
 *	configuration was changed to JSON files.
 *  This version works without setuid bit, but capabilities need to be used.
 * 
 *  differences to old workspace version
 *	- usage if JSON file format
 *	- not using setuid, but needs capabilities
 *	- always moves released workspace away (this change is affecting the user!)
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
 * logic to find the corresponding delete directory for a workspace
 * in the config, the workspaces and delete directory are supposed to
 * have the same order!
 * that code should be hidden in a dark corner...
 */
string find_targetname(const ptree &pt, const string wsdir, const string filesystem) {
	ptree::const_iterator dit = pt.get_child(string("workspaces.")+filesystem+".deleted").begin();
	for(ptree::const_iterator wsit = pt.get_child(string("workspaces.")+filesystem+".spaces").begin();
			wsit != pt.get_child(string("workspaces.")+filesystem+".spaces").end(); ++wsit, ++dit) {
		if(wsdir.compare(0, wsit->second.data().length(), wsit->second.data())==0) {
			return dit->second.data();
		}
	}
	cerr << "Error: no deleted space found for the workspace " << wsdir << endl;
	cerr << "Error: That should not happen, tell the administrator!" << endl;
	return string("");
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
	ptree pt;
	po::variables_map opt;

	// check commandline
	commandline(opt, name, duration, filesystem, extensionflag, argc, argv);

	// read config 
	read_config(pt);

	// valide the input  (opt contains name, duration and filesystem as well)
	validate(ws_release, pt, opt, filesystem, duration, maxextensions, acctcode);

	// construct db-entry name
	string username = getusername();
	string dbfilename=pt.get<string>(string("workspaces.")+filesystem+".database")+"/"+username+"-"+name;

	// does db entry exist?
	if(fs::exists(dbfilename)) {
		read_dbfile(dbfilename, wsdir, expiration, extension, acctcode, reminder, mailaddress);
		if(unlink(dbfilename.c_str())) {
			cerr << "Error: database entry could not be deleted." << endl;
			exit(-1);
		}
		// rational: we move the workspace into deleted directory and append a timestamp to name
		// as a new workspace could have same name and releasing the new one would lead to a name
		// collision, so a timestamp is kind of generation label attached to a workspace
		string targetname = find_targetname(pt, wsdir, filesystem) + 
								"/" + username + "-" + name + "-" + lexical_cast<string>(time(NULL));
		if(rename(wsdir.c_str(), targetname.c_str())) {
			cerr << "Error: could not move workspace!" << endl;
			exit(-1);
		}
	} else {
		cerr << "Error: workspace does not exist!" << endl;
		exit(-1);
	}
}
