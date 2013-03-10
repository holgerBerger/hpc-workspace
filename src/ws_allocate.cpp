
/*
 *	workspace++
 *
 *  ws_allocate
 *
 *	c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *  This version is not DB and configuration compatible with the older version, the DB and 
 *	configuration was changed to JSON files.
 *  This version works without setuid bit, but capabilities need to be used.
 * 
 *  differences to old workspace version
 *  - usage if JSON file format
 *  - not using setuid, but needs capabilities
 *  - supports configuration of reminder emails
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
#include <fstream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>

#define BOOST_FILESYSTEM_VERSION 3 
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

#include "ws_util.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;

/* 
 *  parse the commandline and see if all required arguments are passed, and check the workspace name for 
 *  bad characters
 */
void commandline(po::variables_map &opt, string &name, int &duration, 
					string &filesystem, bool &extension, int &reminder, string &mailaddress, int argc, char**argv) {
	// define all options

	po::options_description cmd_options( "\nOptions" );
	cmd_options.add_options()
			("help,h", "produce help message")
			("duration,d", po::value<int>(&duration)->default_value(1), "duration in days")
			("name,n", po::value<string>(&name), "workspace name")
			("filesystem,F", po::value<string>(&filesystem), "filesystem")
			("reminder,r", po::value<int>(&reminder), "reminder to be sent n days before expiration")
			("mailaddress,m", po::value<string>(&mailaddress), "mailaddress to send reminder to")
			("extension,x", "extend workspace")
	;

	// define options without names
	po::positional_options_description p;
	p.add("name", 1).add("duration",2);

	// parse commandline
	try{
		po::store(po::command_line_parser(argc, argv).options(cmd_options).positional(p).run(), opt);
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
			goto noreminder;
		}
		if (!opt.count("mailaddress")) {
			ifstream infile;
			infile.open((getuserhome()+"/.ws_user.conf").c_str());
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
	noreminder:

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
	int db_uid, db_gid;
	long expiration;
	int extension;
	bool extensionflag;
	string name;
	string filesystem;
	string acctcode, wsdir;
	string mailaddress("");
	int reminder=0;
	ptree pt;
	po::variables_map opt;

	// check commandline
	commandline(opt, name, duration, filesystem, extensionflag, reminder, mailaddress, argc, argv);

	// read config 
	read_config(pt);
	db_uid = pt.get<int>("dbuid");
	db_gid = pt.get<int>("dbgid");

	// valide the input  (opt contains name, duration and filesystem as well)
	validate(ws_allocate, pt, opt, filesystem, duration, maxextensions, acctcode);


	// construct db-entry name
	string username = getusername();
	string dbfilename=pt.get<string>(string("workspaces.")+filesystem+".database")+"/"+username+"-"+name;

	// does db entry exist?
	if(fs::exists(dbfilename)) {
		read_dbfile(dbfilename, wsdir, expiration, extension, acctcode, reminder, mailaddress);
		// if it exists, print it, if extension is required, extend it
		if(extensionflag) {
			cerr << "Info: extending workspace." << endl;
			extension--;
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
		vector<string> spaces;
		cerr << "Info: creating workspace." << endl;

		// read the possible spaces for the filesystem
		BOOST_FOREACH(ptree::value_type &v, pt.get_child(string("workspaces.")+filesystem+".spaces")) {
			spaces.push_back(v.second.data());
		}
	
		// add some random
		srand(time(NULL));
		wsdir = spaces[rand()%spaces.size()]+"/"+username+"-"+name;

		// make directory and change owner + permissions
		try{
			fs::create_directories(wsdir);
		} catch (...) {
			cerr << "Error: could not create workspace directory!" << endl;
			exit(-1);
		}
		if(chown(wsdir.c_str(), getuid(), getgid())) {
			cerr << "Error: could not change owner of workspace!" << endl;
			unlink(wsdir.c_str());
			exit(-1);
		}
		if(chmod(wsdir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR)) {
			cerr << "Error: could not change permissions of workspace!" << endl;
			unlink(wsdir.c_str());
			exit(-1);
		}

		extension = maxextensions;
	    expiration = time(NULL)+duration*24*3600;
		write_dbfile(dbfilename, wsdir, expiration, extension, acctcode, db_uid, db_gid, reminder, mailaddress);
	}
	cout << wsdir << endl;
	cerr << "remaining extensions  : " << extension << endl;
	cerr << "remaining time in days: " << (expiration-time(NULL))/(24*3600) << endl;

}
