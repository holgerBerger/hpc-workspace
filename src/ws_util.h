#ifndef WS_UTIL_H
#define WS_UTIL_H
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>

#include <sys/capability.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <yaml-cpp/yaml.h>

#define BOOST_FILESYSTEM_VERSION 3 
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;

enum whichcode {ws_allocate, ws_release};

void write_dbfile(const string filename, const string wsdir, const long expiration, const int extensions, 
					const string acctcode, const int dbuid, const int dbgid,
					const int reminder, const string mailaddress);
void read_dbfile(string filename, string &wsdir, long &expiration, int &extensions, string &acctcode,
					const int reminder, const string mailaddress);
void validate(const whichcode wc, YAML::Node &config, YAML::Node &userconfig, po::variables_map &opt, 
					string &filesystem, int &duration, int &maxextensions, string &primarygroup);
string getuserhome();
string getusername();
void drop_cap(cap_value_t cap_arg, int dbuid);
void drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid);
void raise_cap(int cap);
void lower_cap(int cap, int dbuid);
int mv(const char * source, const char *target);

#endif 
