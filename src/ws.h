#ifndef WS_H
#define WS_H

// C++ stuff
#include <string>
#include <vector>

// YAML
#include <yaml-cpp/yaml.h>

// BOOST
#include <boost/program_options.hpp>

namespace po = boost::program_options;

using namespace std;


enum whichclient {
    WS_Allocate, 
    WS_Release
};


class DBEntry {

};



class Workspace {

private:
    YAML::Node config, userconfig;
    int db_uid, db_gid; 
    po::variables_map opt;
    int maxextensions, duration;
    string filesystem, acctcode, username;
    
    void validate(const whichclient wc, YAML::Node &config, YAML::Node &userconfig,
                po::variables_map &opt, string &filesystem, int &duration, int &maxextensions, string &primarygroup);

public:
    // constructor reads config and userconfig
    Workspace(const whichclient clientcode, const po::variables_map opt, const int _duration, string filesystem);

    // allocate a new workspace, create workspace and DB entry
    void allocate(const string name, const bool extensionsflag, const int reminder, const string mailaddress, string user_option);

    // release an existing workspace, move workspace and DB entry
    void release(string name);

    // extend an existing workspace
    bool extend();

    // return existing workspaces list from DB
    vector<DBEntry> getList();
};

#endif
