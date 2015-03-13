#ifndef WSDB_H
#define WSDB_H

#include <string>
#include <boost/concept_check.hpp>

using namespace std;

/*
 * new WsDB classe
 * represents a workspace database entry
 * 
 * client does either create a new entry with constructor with arguments,
 * or wants to read it and just gives name to query other information.
 * 
 */


class WsDB {

private:
    string dbfilename;
    string wsdir;
    long expiration;
    int extensions;
    string acctcode;
    int dbuid;
    int dbgid;
    int reminder;
    string mailaddress;
  
    void write_dbfile();
    void read_dbfile();
   
    
public:
    // constructor to query a DB entry, this reads the database entry
    WsDB(const string filename);
  
    // constructor to create a new DB entry
    WsDB(const string filename, const string wsdir, const long expiration, const int extensions,
                             const string acctcode, const int dbuid, const int dbgid,
                             const int reminder, const string mailaddress);
  
    void use_extension(const long expiration);
    

    long int getexpiration() { return expiration; }

    int getextension() { return extensions; }

    string getwsdir() { return wsdir; }
};

#endif
