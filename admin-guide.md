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
It offers a level of redirection to hide details form the user, it offers a stable
user interface to the user, and it allows e.g. migration of users to the administrator,
altough only in large time scales.

Administrators can assign different filesystems to users based on user and group
and can loadbalance over several filesystems.

Typically a workspace will be created
- on a fast filesystem for temporay data, probably no backups
- for the duration of a job or a job campaign

Typically a workspace will be deleted
- because the job or campaign ended, and the users releases the directory
- because the maximum lifetime of the workspace is reached

A friendly user or a user short on quota probably removes the data before releasing 
the workspace.

The workspace toolset offers the possibility to keep expired data for some time
in a restorable state, and users can restore the data without help of the administrator
with ```ws_restore```.

Most operations are logged to syslog.


## basic componentents

The tools main components are user visible commands (ws_allocate, ws_release, ws_list, and others),
the configuration file /etc/ws.conf and the administrators tools like the cleaner removing
the workspaces.

All configuration is in /etc/ws.conf.

## installation

the workspace tools use CMake for configuration and building, make sure it is installed, you will
also need a C++ compiler, GCC is tested.
Furthermore it uses the boost components ```system filesystem regex program_options```.
It also needs terminfo or ncurses, and libyaml-cpp. You can use ```getyamlcpp.sh``` to get and compile
a local libyaml-cpp into the source directory.

The default is to use setuid executables for those parts of the tools needing it (ws_allocate, ws_release,
and ws_restore). You can enable a capability aware variant using ccmake, but this is not encouraged, most network
filesystems and some linux distributions do not support capabilites. 

Only the setuid version is under regression testing.

Run ```cmake .``` and ```make -j 4``` to configure and compile the toolset.

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
admins: [root]			# root sees all workspaces with ws_list
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


## full breakdown of all options

### global options

#### clustername

Name of the cluster, shows up in some outputs and in email warning before expiration.

#### smtphost

FQDN of SMTP server (no authentication supported), this is used to send reminder mails for
expiring workshops and to send calendar entries.

#### mail_from

Used as sender in any mails, should be of form ```user@domain```

#### default

Important option, this determines which workspace to use.

If there is several or one workspace, any user will get a workspace
for the one specified here, unless overwritten by a default clause
in a workspace.

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

a List of of users who can see all workspaces when calling ```ws_list```, not just their own.






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
This directory should be owned by dbuid and dbgid. 

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
If this list is empty, the entry is considered to be non-existing, and access restrictions apply.

As soon as the list exists and is not empty, this list joined with *group_acl* is matched against
the user and his group. If the user is not matching any of the two lists, access to the workspace
is denied and no workspace directory can be created.

**Attention**: the global *default* workspace can be used by any user, this overrides
any ACL! It is possible to have no global *default* directive, but make sure every user
shows up in the default user or group list of exactly one workspace!

#### group_acl

This is the list of groups being allowed to choose this workspace.
If this list is empty, the entry is considered to be non-existing, and access restrictions apply.

**Attention**: the global *default* workspace can be used by any user, this overrides
any ACL! It is possible to have no global *default* directive, but make sure every user
shows up in the default user or group list of exactly one workspace!

#### maxextensions

This specifies how often a user can extend a workspace, either with ```ws_extend``` or ```ws_allocate -x```.
An extension is consumed if the new duration ends later than the current duration (in other words, you can shorten
the lifetime even if you habe no extensions left) and if the user is not root.
Root can always extend any workspace.


#### allocatable

Option for migrations.
This allows to flag a workspace as being non-allocatable, by giving the value ```no```.
Default is ```yes```.
In that case, no new workspaces can be created, this can be used to phase out a workspace,
by moving the default of users to another one.

#### extendable

see above, same logic. It is possible to speed up on ongoing migration by forcing
a quicker expiration of workspaces by taking users the right to extend workspaces.

#### restorable

see above, same logic. Is is possible to prevent restoration from this workspace,
this also enhances migration speed of data.






TO BE CONTINUED
