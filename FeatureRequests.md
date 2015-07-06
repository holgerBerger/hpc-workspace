# user restore tool #

  * if a workspace is expired but was not empty, the files are not accessible for user but still consume quote.

  * two options

  1. let user wipe the trash
  1. let user restore the data to delete it

  * second option seems better, will be done

# quota #

  * if a workspace is created, an optional quota lookup could take place, and the quota (user+group) could be set on the workspace filesystem

looks ok, could be done

# group workspaces #

  * new ws\_allocate option -u open for all
  * if worspace has group write permissions and calling user is part of the group, the user is allowed to extend the workspace

looks ok, could be done

**DONE**

# restore tool #

  * admin interface to restore workspaces

looks ok, will be done

**DONE**

# database entry saving #

  * move the database entry away on release/expire to be able to recover non-trivial contents

**DONE**

# separation of userconfig from general config #

  * make user configs private in extra file, to prevents users from seeing it
  * general concept of public and private file, to allow userspace tools accessing normal config, and some options to be hidden sounds like good idea

**DONE**