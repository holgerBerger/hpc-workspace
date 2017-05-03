# Workspace administrators guide

You can read this guide at
https://github.com/holgerBerger/hpc-workspace/blob/master/admin-guide.md 
with markup.

## motivation

The motivation for the workspace tools was the need to loadbalance a large number of users
over a medium size number of scratch/working filesystems in an HPC environment.

The basic idea is 

- a workspace is a directory created on behalf of the user on demand
- the lifetime is limited, the directory will be deleted automatically at some point in time
- the location is determined by the administrator and can contain a random component

This approach allows the administrator a flexible allocation of resources to users.
It offers a level of redirection to hide details from the user, it offers a stable
user interface to the user, and it allows e.g. migration of users between different filesystems,
although only over medium to large time scales.

Administrators can assign different filesystems to users based on user and group
and can loadbalance over several filesystems.

Typically a workspace will be created
- on a fast filesystem for temporay data, probably no backups, not intended for long time storage
- for the duration of a job or a job campaign

Typically a workspace will be deleted

- because the job or campaign ended, and the users releases the directory, or
- because the maximum lifetime of the workspace is reached

A friendly user or a user short on quota probably wants to remove the data before releasing 
the workspace to regain quota.

The workspace tool set offers the possibility to keep expired data for some time
in a restorable state, and users can restore the data without administrator intervention
using the ```ws_restore``` command.

Most operations are logged to syslog.


## basic components

The tool sets main components are user visible commands (*ws_allocate*, *ws_release*, *ws_list* and others),
the configuration file ```/etc/ws.conf``` and the administrator's tools like the cleaner removing
the workspaces and other helpers like a validation tool for the configuration file.


All configuration is in ```/etc/ws.conf```.

## installation

the workspace tools use CMake for configuration and building, make sure it is installed, you will
also need a C++ compiler, tested is GCC (on ubuntu and redhat).

Furthermore it uses the boost components ```system filesystem regex program_options```.
It also needs terminfo or ncurses, and libyaml-cpp. You can use ```getyamlcpp.sh``` to get and compile
a local libyaml-cpp into the source directory.


Complete list is
- c++ compiler (no c++1x needed so far) (g++ and clang++ tested)
- boost_system
- boost_filesystem
- boost_regex
- boost_program_options
- yaml-cpp
- python2.6
- pyyaml
- terminfo or ncurses
- libcap2 (if using capabilities)
- lua5.1 (if using LUA callouts)

The default is to use setuid executables for those parts of the tools needing it (*ws_allocate*, *ws_release*
and *ws_restore*). You can enable a capability aware variant using ccmake, but this is not encouraged, most network
filesystems and some linux distributions do not support capabilities. 


Only the setuid version is under regression testing.

Run ```cmake .``` and ```make -j 4``` to configure and compile the tool set.

Copy the executables from ```bin``` to e.g. ```/usr/local/bin``` and the
manpages from ```man``` to e.g. ```/usr/local/man/1```

## further preparation

You will need a uid and gid which will serve as owner of
the directories above the workspaces and formost for the DB entry directory.

You can reuse an existing user and group, but be aware that anybody able to 
use that user can manipulate other peoples DB entries, and that the setuid
tools spend most of their time with the privileges of that user.
So it makes sense to have a dedicated user and group id, but it is not
a hard requirement, you could also reuse a user and group of another daemon
or tool.

It is good practice to create the ```/etc/ws.conf``` and validate it with ```sbin/ws_validate_config```.

It is also good practice to use ```contribs/ws_prepare``` to create the filesystem structure according to the config file.


## getting started

A very simple ws.conf example:

```
admins: [root]			# users listed here can sees all workspaces with ws_list
clustername: My Green Cluster	# some name for the cluster
smtphost: mail.mydomain.com     # (my smtp server for sending mails)
dbgid: 85			# a user id, this is the owner of some directories
dbuid: 85			# a group id, this is the owner of some directories
default: ws1			# the workspace to use for everybody
duration: 10                    # (max duration in days, default for all workspaces )
maxextensions: 1                # (maximum number of times a user can ask for a extension)
workspaces:
  ws1:				# name of the workspace
    database: /tmp/ws/ws1-db	# location of the DB
    deleted: .removed		# name of the subdirectory used for expired workspaces
    duration: 30		# max lifetime of a workspace in days
    keeptime: 7			# days to keep deleted data after expiration
    maxextensions: 3		# maximum number of times a user can ask for a extension
    spaces: [/tmp/ws/ws1]	# pathes where workspaces are created, this is a list and path is picked randomly
```

(Note: the 3 lines with () around the comment are required by the validator, but are not really needed in an otherwise
correct and complete file.)

In this example, any user would get a directory below ```/tmp/ws/ws1``` if calling ```ws_allocate```, 
and he would be able to specify a lifetime of 30 days, not longer, and he would be able to extend the workspace
3 times before it expires.

For testing, as a user call ```ws_allocate BLA 1``` this should print a path to stdout and some info to stderr,
so it can be used in something like
```
SCR=$(ws_allocate BLA 1)
cd $SCR
```

```ws_list -t``` should now show something like
```
id: BLA
     workspace directory  : /tmp/ws/ws1/user-BLA
     remaining time       : 0 days 23 hours
     available extensions : 3
```

As you can see, the username is prefixed to the workspace ID in the path of the workspace.
Users should not rely on that, this could change over time.

```ls -ld /tmp/ws/ws1/user-BLA``` should reveal that the user is owner of the directory and allowed
to read and write, otherwise it sould be private.

**Note**: Make sure the ```database``` directory is owned by ```dbuid``` and ```dbguid``` !

## full breakdown of all options

### global options

#### clustername

Name of the cluster, shows up in some outputs and in email warning before expiration.

#### smtphost


FQDN of SMTP server (no authentification supported), this is used to send reminder mails for
expiring workspaces and to send calendar entries.


#### mail_from

Used as sender in any mails, should be of form ```user@domain```

#### default

Important option, this determines which workspace to use if not otherwise
specified.

If there is several or one workspaces, any user will get a workspace directory
from the one specified here, unless overwritten by a default clause
in a workspace or selected by user with ```-F```.

#### duration

maximum lifetime in days of a workspace, can be overwritten in 
each workspace.

#### maxextensions

maximum number of times a user can extend a workspace, can be overwritten
in each workspace.

#### pythonpath

this is a path that is prepended to the PYTHONPATH for the python tools.
Some of the workspace tools are written in python and use yaml.
This path is appended to the python module search path before yaml is loaded.
To read this line, a very simple YAML parser is embedded to find this line,
and the real yaml parser reads the rest of the file.
This option is handy in case your yaml installation is not in the default location.

#### dbuid

UID of the db directorty, and UID the setuid tools use as long as not requiring UID 0.
Can be a shared UID, but be aware the user using that UID can mess with the DB.
Best use a dedicated UID or a UID of another daemon.

#### dbgid

GID of the db directorty, and GID the setuid tools use as long as not requiring GID 0.
Can be a shared GID, but be aware the user using that GID can mess with the DB.
Best use a dedicated GID or a GID of another daemon.

#### admins

A list of of users who can see any workspace when calling ```ws_list```, not just their own.






### workspace options

#### keeptime

Time in days to keep data after it was expired. This is an option for the cleaner.
The cleaner will move expired workspace to a hidden location, but not delete it immediately.
Users or administrators can still recover the data.
After *keeptime* days, it will be removed and can not be recovered anymore.

#### spaces

A list of directories where the users directories will be created.
Among the members of the list, the actual directory will be picked randomly.

This can be used to distribute load and space over several filesystems or fileservers or 
metadata domains like DNE in recent Lustre versions.


#### deleted

the name of the subdirectory within the space and the DB directory where the expired data is kept.
This is always within the space to prevent copies of the data, but to allow rename operation to succeed
for most filesystems in most cases.

#### database

the directory where the DB is stored. The DB is simply a directory having one YAML file per workspace.

This directory should be owned by *dbuid* and *dbgid*. 


If your filesystem is slow for metadata, it might make sense
to put the DB on e.g. a NFS filesystem, but the DB is not accessed without any reason and
should not be performance relevant, only ```ws_list``` might feel faster if the filesystem
with the DB is fast in terms of iops and metadata

#### duration

maximum lifetime in days of a workspace a user can specify.

#### groupdefault

A user being member of the group within that list gets this workspace as default,
so it is picked if the user does not choose another workspace with ```ws_allocate -F``` option.
This overrides the global default named *default*.

Having a group in the groupdefault list of several workspaces is not defined, but it is
not subject of any tests, administrator has to take care to prevent that situation.


##### userdefault

A user in this list gets this workspace as default if not choosen otherwise with ```ws_allocate -F``` option.
This overrides the global default named *default*.

Having group and user default rules with conflicts or having users in several workspaces in the userdefault list
is not defined, it is upon the administrator to take care that such a situation does not occur.

#### user_acl

This is a list of users being allowed to choose this workspace.
If this list and *group_acl* are empty, the entry is considered to be non-existing, and access restrictions do not apply.

As soon as the list exists and is not empty, this list joined with *group_acl* is matched against
the user and his group. If the user is not matching any of the two lists, access to the workspace
is denied and no workspace directory can be created.

**Attention**: the global *default* workspace can be used by any user, this overrides
any ACL! It is possible to have no global *default* directive, but make sure every user
shows up in the default user or group list of exactly one workspace!

**Hint**: To enable access controll, one of *user_acl* or *group_acl* has to be existing
and non-empty! An invalid entry can be used to enable access controll, like a non-existing user or group.
An empty list does not enable access control, the workspace can still be accessed with an empty list by all
users!

#### group_acl

This is the list of groups being allowed to choose this workspace.
If this list and *user_acl* is empty, the entry is considered to be non-existing, and access restrictions do not apply.

See *user_acl* for further logic.

If the compile option *CHECK_ALL_GROUPS* is enabled, secondary groups are checked as well.
By default, only the primary group is considered.

**Attention**: the global *default* workspace can be used by any user, this overrides
any ACL! It is possible to have no global *default* directive, but make sure every user
shows up in the default user or group list of exactly one workspace!

**Hint**: To enable access controll, one of *user_acl* or *group_acl* has to be existing
and non-empty! An invalid entry can be used to enable access controll, like a non-existing user or group.
An empty list does not enable access control, the workspace can still be accessed with an empty list by all
users!


#### maxextensions

This specifies how often a user can extend a workspace, either with ```ws_extend``` or ```ws_allocate -x```.
An extension is consumed if the new duration ends later than the current duration (in other words, you can shorten
the lifetime even if you habe no extensions left) and if the user is not root.
Root can always extend any workspace.


#### allocatable

Option for migrations.
This allows to flag a workspace as being non-allocatable, by giving the value ```no```.
Default is ```yes```.

In that case, no new workspaces can be created, this can be used to phase a workspace out,
by moving the default of users to e.g. another filesystem.


#### extendable

see above, same logic. It is possible to speed up on ongoing migration by forcing
a quicker expiration of workspaces by taking users the right to extend workspaces.

#### restorable

see above, same logic. Is is possible to prevent restoration from this workspace,
this also enhances migration speed of data.


## compile options

You can use ```ccmake .``` to change options.

options are

### SETUID

this is default, and enables code to use ```setuid()``` in the code.
If this is not set, code uses ```libcap``` to change capabilities.
This allows a finer privilege controll, but does not work on all filesystems, e.g.
NFS and LUSTRE are known not to work.
It works on local filesystems like ext4, so it could be used for single hosts.

GPFS/SpectrumScaleFs is not yet tested, so not yet known if it works or not.

### STATIC

disabled by default, allows static linking if enabled. This can be handy for systems where the compute
nodes do not have all libraries available.

### CHECK_ALL_GROUPS

disabled by default, checks secondary groups as well when going though *group_acl* and *groupdefault*
lists.

### LUACALLOUT

disabled by default, *not fully working*. Allows to call a LUA script to change the path in ```ws_allocate```.
This allows to insert components into the path, luke putting all users into a common directory for a group.
there is some issues with this option with most tools, *do not use*, it is not in use.

## internals

The rewrite in C++ (old codebase was python) allowed to get rid of some dirty setuid hack,
and offered the chance to enhance the DB format for new functionality.
A DB file is now a YAML file, all entries are tagged, basically it is a multiline key-value
file, using YAML syntax.

This allows easy extension of the DB format, which was a bit harder with the old format,
with a fixed order of lines in the file format.
Nevertheless, the new tools can read the old files, which is handy for migration from
the old tools to the new tools.

There is three tools needing priviliges, this is ```ws_allocate```, ```ws_release``` and
```ws_restore```.

All three have to change owners and permissions of files.

All other tools are either for root only (in ```sbin```) or do not need privilges (```ws_list```, 
```ws_extend```, ```ws_find```, ```ws_register```, ```ws_send_ical```).

Basic setup is having at leats two directory trees, one for the DB and one for the data.
They can reside on different filesystems, but do not have to.
They should not be included one in the other.

A typical setup could look like

```
/tmp/ws -+- ws1-db -+              (owned by dbuid:dbguid, permissions drwxr-xr-x)
         |          +- .removed    (owned by dbuid:dbguid, permissions drwx------)
         |
         +- ws1-----+              (owned by anybody, permissions drwxr-xr-x)
                    +- .removed    (owned by anybody, permissions drwx------)
```

this is the structure as in the previous example config file.

```ws1-db``` is be the ```database``` entry in the config file,
```ws1``` would appear in the ```spaces``` list, and ```deleted: .removed```
configures the location of expired entries for both the spaces and the DB.

If a workspace is created, an empty user owned and writable directory is created in 
```ws1```, and a file with the DB entry will be created in ```ws1-db```, owned
by ```dbuid:dbgid``` but world readable. Both the directory and the file have the naming
convention of ```username-workspacename```, so several users can have a workspace with the same name.


If a workspace is expired or released, both are moved into the ```deleted``` directory
(called ```.removed``` in this example) and get a timestamp appended to the name.
So there can be several generations of a workspace with the same name from the same user
exist in the restorable location.

As the moved data is still owned by the user, but in a non accessible location, it still
is accounting for the users quota. Users who want to free the space have to restore the
data with ```ws_restore```, delete it, and release it again.

It is the cleaners task to iterate through the spaces to find if there is anything looking
like a workspace not having a valid DB entry, and iterate through the deleted workspace to check
how old they are, and if they should still be kept or be deleted.
Furthermore it checks the DB entries if any of them are expired, and moved the entry and the directory
to the deleted directory if needed.

All the tools fall back from ```rename()``` if needed, and can operate across filesystem-boundaries,
but this is ofc a lot slower and should be avoided. Some filesystems like NEC ScaTeFS or Lustre with DNE can have failing
```rename()``` within a filesystem, this is covered.










TO BE CONTINUED
