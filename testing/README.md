# Testing

Tests require to

- add some users and groups to your system
- modify `/etc/ws.conf`
- create some files in `/tmp`

## Running Tests
If this is ok, you can run tests on your system, like

```bash
$ cd testing
$ sudo ./scripts/prepare_tests.sh
$ sudo ./run_tests.sh
$ sudo ./scripts/cleanup_tests.sh
```

or if that is proven to work you can run

```bash
$ cd testing
$ sudo ./prepare_and_run.sh
```

## Virtual Maschine

If modifying local machine is not ok, use virtual machine:

* Select a vagrant file and rename it to `Vagrantfile`.
* For testing, you start a virtual machine in this directory here using the provided Vagrantfile

```bash
$ vagrant up
```

to create a Ubuntu based virtual machine (2GB RAM, change in file if too much).
* install prerequisits like compillers and libraries, and download latest hpc-workspace
from github and configure and compile it.
* log into virtual machine with

```bash
$ vagrant ssh
$ cd hpc-workspace/testing
$ sudo ./scripts/prepare_tests.sh
```
and run tests with

```bash
$ sudo ./run_tests.sh
```

to repeat, do in doubt

```bash
$ sudo ./scripts/cleanup_tests.sh
$ sudo ./scripts/prepare_tests.sh
```

before running tests another time, to be sure you have a clean setup.

* To stop or destroy the VM, use

```
$ vagrant halt
# or
$ vagrant destroy
```
