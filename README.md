A **workspace** is a directory, with an associated expiration date, created on behalf of a user, to prevent disks from uncontrolled filling.
The project provides user and admin tools to manage those directories.

Example for usage:

In a batch job, you write

```
SCR=$(ws_allocate MyData 10)
cd $SCR
```

This will set you into a temporary directory that will last 10 days.

You can check which **workspaces** you have using

```
$ ws_list 
id: MyData
     workspace directory  : /tmp/username-MyData
     remaining time       : 9 days 23 hours
     creation time        : Wed Mar 13 23:51:57 2013
     expiration date      : Sat Mar 23 23:51:57 2013
     available extensions : 15
```

and you remove the directory if you do not need it anymore with

```
$ ws_release MyData
```

For the administrator, the **workspace** is a way to make sure users do not store their data
forever on your fast storage, but have to stage it to slower and cheaper storage.


For admin guide see
https://github.com/holgerBerger/hpc-workspace/blob/master/admin-guide.md

For user guide see
https://github.com/holgerBerger/hpc-workspace/blob/master/user-guide.md
