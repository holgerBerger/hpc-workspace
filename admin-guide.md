# Workspace administrators guide


## motivation

The motivation for the workspace tools was the need to loadbalance a large number of users
over a medium size number of scratch/working filesystems in an HPC environment.

The basic idea is 

- a workspace is a directory created on behalf of the user on demand
- the lifetime is limited, the directory will be deleted automaticall at some point in time
- the location is determined by administrator and can contain a random component

This approach allows the administrator a flexible allocation of resources to users.
It offers a level of redirection to hide details form the user, it offers a stable
user interface to the user, and it allows e.g. migration of users to the administrator,
altough only in large time scales.

## basic componentents

The tools main components are user visible commands (ws_allocate, ws_release, ws_list and others),
the configuration file /etc/ws.conf and the administrators tools like the cleaner removing
the workspaces.

All configuration is in /etc/ws.conf

```
admins: [root]			# root sees all workspaces with ws_list
clustername: My Green Cluster	# some name for the cluster
dbgid: 85			# a user id, this is the owner of some directories
dbuid: 85			# a group id, this is the owner of some directories
default: ws1			# the workspace to use for everybody
workspaces:
  ws1:				# name of the workspace
    database: /tmp/ws/ws1-db	# location of the DB
    deleted: .removed		# name of the subdirectory used for expired workspaces
    duration: 30		# max lifetime of a workspace in days
    keeptime: 7			# days to keep deleted data after expiration
    maxextensions: 3		# maximum number of times a user can ask for a extension
    spaces: [/tmp/ws/ws1]	# pathes where workspaces are created, this is a list and path is picked randomly
```

## more complex examples
