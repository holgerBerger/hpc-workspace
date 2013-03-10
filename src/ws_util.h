#ifndef WS_UTIL_H
#define WS_UTIL_H
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <string>

#define BOOST_FILESYSTEM_VERSION 3 
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using boost::property_tree::ptree;
using namespace std;

enum whichcode {ws_allocate, ws_release};

void read_config(ptree &pt);
void write_dbfile(const string filename, const string wsdir, const long expiration, const int extensions, 
					const string acctcode, const int dbuid, const int dbgid,
					const int reminder, const string mailaddress);
void read_dbfile(string filename, string &wsdir, long &expiration, int &extensions, string &acctcode,
					const int reminder, const string mailaddress);
void validate(const whichcode wc, ptree &pt, po::variables_map &opt, string &filesystem, int &duration, 
					int &maxextensions, string &primarygroup);
string getuserhome();
string getusername();

#endif 
