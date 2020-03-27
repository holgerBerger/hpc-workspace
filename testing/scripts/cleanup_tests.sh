#!/bin/bash

# this is run as root and cleans up
echo cleaning tmp
rm -rf /tmp/ws
echo removing users and groups
userdel usera
userdel userb
userdel userc
groupdel groupa
groupdel groupb
groupdel groupc
