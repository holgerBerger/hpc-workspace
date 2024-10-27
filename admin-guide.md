# Workspace Administrators Guide

You can read this guide at 
https://github.com/holgerBerger/hpc-workspace/blob/master/admin-guide.md with 
markup.

## Motivation

The motivation for these workspace tools was the need to loadbalance a large 
number of users over a medium size number of scratch/working filesystems in an 
HPC environment by the operations team without any manual interaction.

The basic idea is

- a workspace is a directory created on behalf of the user on demand
- the lifetime is limited, the directory will be deleted automatically at some 
  point in time
- the location is determined by the administrator and can contain a random 
  component

This approach allows the administrator a flexible allocation of resources to 
users. It offers a level of redirection to hide details from the user, it
offers a stable user interface to the user, it allows e.g. migration of users
between different filesystems, although only over medium to large time scales, 
and it offers a way to get a little grip on the lifetime of data. If no one 
takes care of the data anymore, it will get deleted.

Administrators can assign different filesystems to users based on user and 
group and can optionally loadbalance over several filesystems.

Typically, a workspace will be created
- on a fast filesystem for temporary data, probably no backups, not intended 
  for long time storage, probably faster and smaller than the typical home 
  filesystem, think of a parallel filesystem like Lustre or 
  GPFS/SpectrumScale/BeeGfs.
- for the duration of a job or a job campaign.

Typically, a workspace will be deleted

- because the job or campaign ended, and the user releases the directory, or
- because the maximum lifetime of the workspace is reached

A friendly user or a user short on quota probably wants to remove the data 
before releasing the workspace to regain quota.

The workspace tool set offers the possibility to keep expired data for some 
time in a restorable state, and users can restore the data without 
administrator intervention using the ```ws_restore``` command.

Most operations are logged to syslog.


## Basic components

The tool set's main components are user-visible commands (`ws_allocate`, 
`ws_release`, `ws_list` and others), the configuration file ```/etc/ws.conf``` 
and the administrator's tools like the cleaner removing the workspaces and 
other helpers like a validation tool for the configuration file.


All configuration is in ```/etc/ws.conf```.

## Installation

The workspace tools use CMake for configuration and building, make sure it is 
installed, you will also need a C++ compiler, compiling with GCC has been 
tested (on Ubuntu and Redhat).

Furthermore, it uses the boost components ```system filesystem regex
program_options```. It also needs terminfo or ncurses, and libyaml-cpp. You can 
use ```getyamlcpp.sh``` to get and compile a local libyaml-cpp into the source 
directory.


The complete list of dependencies is:
- C++ compiler (no c++1x needed so far) (g++ and clang++ tested)
- boost_system
- boost_filesystem
- boost_regex
- boost_program_options
- yaml-cpp
- python3
- pyyaml
- terminfo or ncurses
- libcap2 (if using capabilities)
- lua5.1 (if using LUA callouts)

The default is to use setuid executables for those parts of the tools that need 
it (`ws_allocate`, `ws_release` and `ws_restore`). You can enable a 
capability-aware variant using ccmake, but this is not encouraged, most network 
filesystems and some linux distributions do not support capabilities. 


Only the setuid version is under regression testing.

Run ```cmake .``` and ```make -j 4``` to configure and compile the tool set.

Copy the executables from ```bin``` to e.g. ```/usr/local/bin``` and the
manpages from ```man``` to e.g. ```/usr/local/man/man1```.

make `ws_allocate`, `ws_release`, `ws_restore` setuid root.

Finally, a cron job has to be set up that calls the `ws_expirer` script at
regular intervals, only then will old workspaces be cleaned up. The
`ws_expirer` setup is detailed below.

## Further preparation

You will need a uid and gid which will serve as owner of the directories above 
the workspaces and formost for the DB entry directory.

You can reuse an existing user and group, but be aware that anybody able to use 
that user can manipulate other peoples' DB entries, and that the setuid tools 
spend most of their time with the privileges of that user. Therefore, it makes
sense to have a dedicated user and group ID, but it is not a hard requirement, 
you could also reuse a user and group of another daemon or tool.

It is good practice to create the ```/etc/ws.conf``` and validate it with 
```sbin/ws_validate_config```.

It is also good practice to use ```contribs/ws_prepare``` to create the 
filesystem structure according to the config file.


## Getting started

A very simple example `ws.conf` file:

```yaml
admins: [root]			# users listed here can see all workspaces with ws_list
adminmail: [root]      # (add somethingmeaningfull here, it is used to alarm of bad confitions)
clustername: My Green Cluster	# some name for the cluster
smtphost: mail.mydomain.com     # (my smtp server for sending mails)
dbuid: 85			# a user id, this is the owner of some directories
dbgid: 85			# a group id, this is the group of some directories
default: ws1			# the default workspace location to use for everybody
duration: 10                    # (maximum duration in days, default for all workspaces)
maxextensions: 1                # (maximum number of times a user can ask for an extension)
workspaces:
  ws1:				# name of the workspace location
    database: /tmp/ws/ws1-db	# DB directory
    deleted: .removed		# name of the subdirectory used for expired workspaces
    duration: 30		# max lifetime of a workspace in days
    keeptime: 7			# days to keep deleted data after expiration
    maxextensions: 3		# maximum number of times a user can ask for an extension
    spaces: [/tmp/ws/ws1]	# paths where workspaces are created, this is a list and path is picked randomly or based on uid or guid
```

**Note:** the 3 lines with () around the comment are required by the validator, 
but are not really needed in an otherwise correct and complete file.

In this example, any workspace would be created in a directory in 
```/tmp/ws/ws1``` whenever a user calls ```ws_allocate```, and he would be able 
to specify a lifetime of 30 days, not longer, and he would be able to extend 
the workspace 3 times before it expires.

When the `ws_allocate` command is called, for example with ```ws_allocate BLA 
1```, it will print the path to the newly created workspace to `stdout` and 
some additional info to `stderr`. This allows using `ws_allocate` in scripts 
like the following example:
```bash
SCR=$(ws_allocate BLA 1)
cd $SCR
```

and ```ws_list -t``` should then show something like

```yaml
id: BLA
     workspace directory  : /tmp/ws/ws1/user-BLA
     remaining time       : 0 days 23 hours
     available extensions : 3
```

As you can see, the username is prefixed to the workspace ID in the path of the 
workspace. Users should not rely on that, this could change over time.

```ls -ld /tmp/ws/ws1/user-BLA``` will reveal that the user who created the 
workspace is the owner of the directory and has read and write permissions, 
otherwise it should be private.

**Note**: Make sure the ```database``` directory is owned by ```dbuid``` and 
```dbgid``` !

## Full breakdown of all options

### Global options

#### `clustername`

Name of the cluster, shows up in some outputs and in email warning before 
expiration.

#### `smtphost`

FQDN of SMTP server (no authentification supported), this is used to send 
reminder mails for
expiring workspaces and to send calendar entries.


#### `mail_from`

Used as sender in any mail, should be of the form ```user@domain```.

#### `default`

Important option, this determines which workspace location to use if not 
otherwise specified.

If there is more than one workspace location (i.e. more than one entry in 
`workspaces`), then the location specified here will be used for all workspaces 
by all users. A user may still manually choose the location with ```-F```.

#### `duration`

Maximum lifetime in days of a workspace, can be overwritten in each workspace 
location specific section.

#### `durationdefault`

Lifetimes in days attached to a workspace if user does not specify it.
Defaults to 1 day.

#### `maxextensions`

Maximum number of times a user can extend a workspace, can be overwritten in 
each workspace location specific section.

#### `pythonpath`

This is a path that is prepended to the PYTHONPATH for the Python tools. Some 
of the workspace tools are written in Python and use the PyYAML package. This 
path is appended to the Python module search path before `yaml` is imported. To 
read this line, a very simple YAML parser is embedded to find this line, and 
the real YAML parser reads the rest of the file.

This option is handy in case your YAML installation is not in the default 
location.

#### `dbuid`

UID of the database directory, and the UID that will be used by all setuid 
tools (as long as UID 0 is not required). Can be a shared UID, but be aware
the user using that UID can mess with the DB.
It is strongly suggested to use a dedicated UID or an UID of another daemon.

#### `dbgid`

GID of the database directory, and the GID that will be used by all setuid 
tools (as long as GID 0 is not required). Can be a shared GID, but be aware
users assigned to that GID can mess with the DB. It is strongly suggested
to use a dedicated GID or a GID of another daemon.

#### `admins`

A list of users who can see any workspace when calling ```ws_list```, not
just their own.


#### `adminmail`

A list of email addresses to inform when a bad condition is discovered by ws_expirer
which needs intervention.

### Workspace-location-specific options

In the config entry `workspaces`, multiple workspace location entries may be 
specified, each with its own set of options. The following options may be 
specified on a per-workspace-location basis:

#### `keeptime`

Time in days to keep data after it was expired. This is an option for the 
cleaner. The cleaner will move the expired workspace to a hidden location 
(specified by the `deleted` entry below), but does not delete it immediately.
Users or administrators can still recover the data. After `keeptime` days,
it will be removed and can not be recovered anymore.

#### `spaces`

A list of directories that make up the workspace location. The directory for 
new workspaces will be picked randomly from the directories in this list by default, see ```spaceselection```

This can be used to distribute load and storage space over several filesystems 
or fileservers or metadata domains like DNE in Lustre.

### `spaceselection`

can be `random` which is default, or `uid` or `gid` to select space based on modulo operation with uid or gid.

#### `deleted`

The name of the subdirectory, both inside the workspace location and inside the 
DB directory, where the expired data is kept. This is always inside the space 
to prevent copies of the data, but to allow rename operation to succeed for 
most filesystems in most cases.

#### `database`

The directory where the DB is stored. The DB is simply a directory having one 
YAML file per workspace.

This directory should be owned by `dbuid` and `dbgid`, see the corresponding 
entries in the global configuration.


If your filesystem is slow for metadata, it might make sense to put the DB on 
e.g. a NFS filesystem, but the DB is not accessed without any reason and should 
not be performance-relevant, only ```ws_list``` might feel faster if the 
filesystem with the DB is fast in terms of iops and metadata.

#### `duration`

Maximum allowed lifetime of a workspace in days. User may not specify a longer 
duration for his workspaces than this value.

#### `groupdefault`

Lists which groups use this location by default. Any user that is a member of 
one of the groups in this list will have their workspaces allocated in this 
workspace location. This overrides the `default` in the global config. A user 
may still manually pick a different workspace location with the ```ws_allocate 
-F``` option.

**Caution:** if a group is listed in the `groupdefault` list of several 
workspace locations, this results in undefined behavior. This condition is not 
tested for, the administrator has to ensure that this does not happen.


##### `userdefault`

Lists users which use this location by default. Any user in this list will have 
their workspaces allocated in this workspace location. This overrides the 
`default` in the global config. A user may still manually pick a different 
workspace location with the ```ws_allocate -F``` option.

**Caution:** if a user is listed in the `userdefault` list of several workspace 
locations, this results in undefined behavior. This condition is not tested 
for, the administrator has to ensure that this does not happen.

#### `user_acl`

List of users who are allowed to choose this workspace location. If this list 
and `group_acl` are both empty, all users may choose this location.

As soon as the list exists and is not empty, this list joined with `group_acl` 
is matched against the user and his group. If the user is not in either of the 
two lists, he may not create a workspace in this location.

**Caution:** the global `default` workspace can be used by any user, this 
overrides any ACL! It is possible to have no global `default` directive, but in 
that case the administrator needs to ensure that every user shows up in the
`userdefault` or `group default` list of exactly one workspace!

**Hint**: To enable access control, at least one of `user_acl` or `group_acl` 
has to be existing and non-empty! An invalid entry can be used to enable access 
control, like a non-existing user or group. An empty list does not enable 
access control, the workspace can still be accessed with an empty list by all 
users!

#### `group_acl`

List of groups who are allowed to choose this workspace location.  If this list 
and `user_acl` are both empty, all users may choose this location.

See `user_acl` for further logic.

If the compile option `CHECK_ALL_GROUPS` is enabled, secondary groups are 
checked as well. By default, only the primary group is considered.

**Caution:** if the global `default` option is set, the location specified 
there may be used by any user, this overrides any ACL! Also, if no global 
`default` directive is set, then the administrator **has to** ensure that every 
user show up in the `userdefault` or `group default` list of exactly one 
workspace!

**Hint**: to enable access control, at least one of `user_acl` or `group_acl` 
has to be existing and non-empty! An invalid entry can be used to enable access 
control, like a non-existing user or group. An empty list does not enable 
access control, the workspace can still be accessed with an empty list by all 
users!


#### `maxextensions`

This specifies how often a user can extend a workspace, either with 
```ws_extend``` or ```ws_allocate -x```. An extension is consumed if the new 
duration ends later than the current duration (in other words, you can shorten 
the lifetime even if you have no extensions left) and if the user is not root. 
Root can always extend any workspace.


#### `allocatable`

Default is ```yes```. If set to ```no```, the location is non-allocatable, 
meaning no new workspaces can be created in this location.

This option, together with the `extendable` and `restorable` options below, is 
intended to facilitate migration and maintenance, i.e. to phase out a 
workspace, or when moving the default of users, e.g. to another filesystem.


#### `extendable`

Analog to `allocatable` option above. If set to `no`, existing workspaces in 
this location cannot be extended anymore.

#### `restorable`

Analog to `allocatable` option above. If set to `no`, workspaces cannot be
restored to this location anymore.


## Compile options

You can use ```ccmake .``` to change options.

The following options are possible:

### SETUID

This is default, and enables code to use ```setuid()``` in the code. If this is 
not set, code uses ```libcap``` to change capabilities. This allows a finer 
privilege control.

**Caution:** does not work on all filesystems:

* NFS and Lustre are known **not** to work.
* It works on local filesystems like ext4, so it could be used for single hosts.
* GPFS/SpectrumScaleFs is not yet tested, so not yet known if it works or not.

### STATIC

Disabled by default. Allows static linking if enabled. This can be handy for 
systems where the compute nodes do not have all libraries available.

### CHECK_ALL_GROUPS

Disabled by default. Checks secondary groups as well when going through
`group_acl` and `groupdefault` lists.

### LUACALLOUT

**Caution:** does not work fully. Do not use for productive setups!

Disabled by default. Allows calling a LUA script to change the path in 
```ws_allocate```. This allows inserting components into the path, like
putting all users into a common directory for a group. There are some issues 
with this option with most tools.

## Internals

The rewrite of some parts in C++ (the old codebase was Python) allowed getting 
rid of some dirty setuid hacks, and offered the chance to enhance the DB format 
for new functionality. A DB file is now a YAML file, all entries are tagged, 
basically it is a multiline key-value file, using YAML syntax.

This allows easy extension of the DB format, which was a bit harder with the 
old format, with a fixed order of lines in the file format. Nevertheless, the 
new tools can read the old files, which is handy for migration from the old 
tools to the new tools.

There are three tools that need privileges, these are ```ws_allocate```, 
```ws_release``` and ```ws_restore```.

All three have to change owners and permissions of files.

All other tools are either for root only (in ```sbin```) or do not need 
privileges (```ws_list```, ```ws_extend```, ```ws_find```, ```ws_register```, 
```ws_send_ical```).

The basic setup consists of at least two directory trees, one for the DB and 
one for the data. These trees have to be separate and neither may be a 
subdirectory of the other. They may reside on different filesystems, but do not 
have to. The database directory has to contain a file .ws_db_magic with the name
of the workspace in it, this is used by the ws_expirer to verify that the DB is present 
and valid, to avoid e.g. problems with not mounted filesystems.

A typical setup could look like this:

```
/tmp/ws -+- ws1-db -+              (owned by dbuid:dbgid, permissions drwxr-xr-x)
		 |          +- .ws_db_magic (containing name of ws, ws1 in the example)
         |          +- .removed    (owned by dbuid:dbgid, permissions drwx------)
         |
         +- ws1-----+              (owned by anybody, permissions drwxr-xr-x)
                    +- .removed    (owned by anybody, permissions drwx------)
```

This is the structure that would result from the example config file shown 
above.

In this case, ```ws1-db``` is the database location, corresponding to the 
```database``` entry in the config file, ```ws1``` corresponds to the single 
entry in the ```spaces``` list, and the `.removed` directories are the 
locations of expired entries for both the spaces and the DB, corresponding to 
the ```deleted: .removed``` config file entry.

Whenever a workspace is created, an empty directory is created in ```ws1```, 
this directory is owned by and writable for the user who created the workspace. 
Aditionally, a file with the DB entry will be created in ```ws1-db```, owned by 
```dbuid:dbgid``` but readable by all users. Both the directory and the file 
have the naming convention of ```username-workspacename```, so several users 
can have a workspace with the same name.

If a workspace is expired or released, both its workspace directory and the DB 
entry file are moved into the corresponding ```deleted``` directories (called 
```.removed``` in this example) and get a timestamp with the time of deletion 
appended to the name. This ensures that there can be several generations of a 
workspace with the same name from the same user that exist in parallel in the 
restorable location.

**Caution:** since the moved data is still owned by the user, only in a 
non-accessible location, it is still counted towards the user's quota. Users 
who want to free the space have to restore the data with ```ws_restore```, 
delete it, and release it again.

**Caution:** make sure that the DB and the workspace directory are available
when the expirer is running, a missing DB (due e.g. a missing mount if in a different
filesystem) can be fatal. It is advisable to avoid this by having both DB and data
in same fs. For performance reasons, in Lustre the DB can be a DOM directory.

It is the task of the cleaner, a part of the `ws_expirer` script, to iterate 
through the spaces to find if there is anything looking like a workspace not 
having a valid DB entry, and iterate through the deleted workspaces to check 
how old they are, and whether they should still be kept or be deleted. 
Furthermore, it checks the DB entries if any of them are expired, and moves the
entry and the directory to the deleted directory if needed. The cleaner is only 
enabled if the `--cleaner` (or `-c`) option is specified when calling 
`ws_expirer`.

Some filesystems like NEC ScaTeFS or Lustre with DNE can cause the Python 
builtin ```os.rename()``` to fail. Therefore, all the tools fall back from
```os.rename()``` to the Linux command `mv` if needed, and can operate across 
filesystem-boundaries, but this is of course a lot slower and should be avoided.


## Setting up the ws_expirer

The `ws_expirer` is the script which takes care of expired Workspaces. To set 
it up, create a daily cron job that runs the `ws_expirer` script:

```
10 1 * * * /usr/sbin/ws_expirer -c
```

Note the required `-c` option. This option enables the cleaner. If it were left 
out, `ws_expirer` would be running in "dry-run" mode, which is a testing 
feature, and would not perform any file operations.

However, it might be better to create a dedicated script for the cron job. That 
script, in addition to calling `ws_expirer`, may contain any additional steps 
like creating log files. An example for this is shown below.

### Example with logging and cleanup

You can of course add logging of the `ws_expirer` outputs simply by writing the 
outputs in a log file:

```
10 1 * * * /usr/sbin/ws_expirer -c > /var/log/workspace/expirer-`date +%d.%m.%y`
```
However this will create a lot of log files over time.

The following example consists of a script that logs the `ws_expirer` output 
and cleans up old log files.

#### Content of crontab:

```
10 1 * * * /usr/sbin/ws-expirer.date
```

#### Content of `ws-expirer.date` script:

```
#!/bin/bash
/usr/sbin/ws_expirer -c > /var/log/workspace/expirer-`date +%d.%m.%y`

find /var/log/workspace -type f -ctime +80 -exec rm {} \;
```

## Contributing

Is highly welcome. Please refer to the 
[issue tracker](https://github.com/holgerBerger/hpc-workspace/issues) of this project.

It is good practice to apply the linter and formatter tools (see below) to the changed files before pushing any commit.
### Code Style, Linter and Formatter

#### Python Code

We use [black](https://github.com/psf/black) for formatting Python code. It is configured via the file `pyproject.toml`
which is found automatically, if `black` is run from the hpc-workspace directory or any subdirectory.

```
$ black <python-file>
reformatted <python-file>
All done! ✨ 🍰 ✨
1 file reformatted.
```

For linting, we rely on [flake8](https://flake8.pycqa.org). It helps to check the style and quality of Python code.
Flake8 is configured via the file `setup.cfg` and invoked as follows:

```
flake8 <python-file>
```

