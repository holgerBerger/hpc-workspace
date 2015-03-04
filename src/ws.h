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
    string filesystem, acctcode, username;
    int duration, maxextensions;
    void validate(const whichclient wc, YAML::Node &config, YAML::Node &userconfig,
                po::variables_map &opt, string &filesystem, int &duration, int &maxextensions, string &primarygroup);

public:
    // constructor reads config and userconfig
    Workspace(const whichclient clientcode, const int _duration);

    // allocate a new workspace, create workspace and DB entry
    bool allocate(string name, bool extensionsflag, string user_option);

    // release an existing workspace, move workspace and DB entry
    bool release();

    // extend an existing workspace
    bool extend();

    // return existing workspaces list from DB
    vector<DBEntry> getList();
};

#endif
