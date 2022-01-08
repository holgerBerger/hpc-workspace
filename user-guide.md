# Workspace user guide

For the latest version (which might not fit your installation) see
    https://github.com/holgerBerger/hpc-workspace/blob/master/user-guide.md


## motivation

*workspaces* are a concept allowing the operations team of an HPC resource to offload some
tasks to the user, and allow the user to keep easier track of job directories.
It also allows the operations team to manage and load balance several filesystems,
and hiding this fact from the users.

A *workspace* is formost a directory created on behalf of the user with some properties
- it has an *ID* selected by the user
- it has a lifetime selected by the user, but limited by the operations team
- it has permissions the user can influence or change

The *workspace* is the place where big data remains during a job or during a job campaign.
It is probably not the place to store source files, and it is not the place to archive data at.

Any *workspace* will be deleted at some point in time, so better keep track of them.
But this property makes sure the nasty user from the other side of the floor does not use
all the space for ages.

## creation of a workspace

A workspace can be created with *ws_allocate*.

Example:

```
SCRDIR=$(ws_allocate MySpace 10)
```

will create a *workspace* with the ID *MySpace* existing for 10 days.

The command will return the path to that new directory into *SCRDIR*,
and will print some more information on stderr.

The maximum lifetime for a workspace may be limited by the operations team.  If
you specify a longer lifetime, it will be capped to the maxium, and you will see
a message that it was changed.  If you do not specify a lifetime, a default
lifetime will be used (typcially 1 day).

There might be several filesystems prepared by the operations team where a workspace
can be created, you can find out those filesystems with ```ws_list -l```.
This will give a list of filesystems, and one of them might be marked with ```(default)```.
The default one will be chosen if you do not specify a filesystem. You can otherwise
choose the filesystem using ```ws_allocate -F <location> <ID> <DURATION>```.

**Important:** Creating a workspace a second time with any of above lines
is a no-operation, it always returns the same path, so it is safe and encourage
to use such a line in batch jobs which are part of a series of jobs working
on the same data, no matter if the job was running before or not.

You can use ```ws_find <ID>``` instead as well, if you feel more comfortable.

See ```man ws_allocate``` for a description of all options.

## listing workspaces

```ws_list``` will list all your owned workspaces. This has many options for verbosity
(*-s* only names, *-t* less than default, *-v* more than default) and sorting
(*-N* for name, *-C* date of creation, *-R* remaining time, *-r* reversed)

*-l* shows the available locations, and *-F* limits the locations of the listing.

See ```man ws_list``` for a description of all options.

## releasing workspaces

```ws_release <ID>``` releases a workspace.
Releasing means that the ID can be reused and the directory is not accessible any more,
but it does not delete the data immediately.
The data is probably kept for a while if there is enough space and can be recovered using
the ```ws_restore``` command as long as it is not finally deleted.

The real deletion will probably take place during the nighttime.

**Please note:** data in a released workspace can still account for the quota usage!
In case the data is limiting you, delete the data before releasing the workspace, or if already
released, restore it using ```ws_restore```, delete it and release the workspace again.

## extending workspaces

As each workspace has an expiration date, its lifetime is limited.
The operations team can allow you a certain number of extensions of a workspace,
you can see the amount of available extensions with ```ws_list```.

You can extend a workspace using ```ws_allocate -x <ID> <DAYS>```,
each call will consume an extension, unless the new expiration date is shorter
than the previous one. You can also shorten the lifetime if no extensions
are available anymore.

## getting reminder

TBD

## cooperative usage (group workspaces and sharing with other users)

TBD
