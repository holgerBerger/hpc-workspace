#!/bin/bash

# this script is running as root and prepares the test environment

echo "cleaning tmp"
rm -rf /tmp/ws
echo "copying ws.conf"
cp input/ws.conf.1 /etc/ws.conf
echo "creating users and groups"

# Create user and groups in a portable way
groupadd groupa
groupadd groupb
groupadd groupc
useradd --groups groupa usera
useradd --groups groupb userb
useradd --groups groupc userc
usermod -a -G groupa userc

echo create workspaces etc
../contribs/ws_prepare

chown root ../bin/ws_allocate ../bin/ws_release ../bin/ws_restore
chmod u+s ../bin/ws_allocate ../bin/ws_release ../bin/ws_restore

