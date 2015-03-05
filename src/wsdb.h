#ifndef WSDB_H
#define WSDB_H

#include <string>

using namespace std;

class WsDB {

public:
static void write_dbfile(const string filename, const string wsdir, const long expiration, const int extensions, 
					const string acctcode, const int dbuid, const int dbgid,
					const int reminder, const string mailaddress);
static void read_dbfile(string filename, string &wsdir, long &expiration, int &extensions, string &acctcode,
					const int reminder, const string mailaddress);

};

#endif
