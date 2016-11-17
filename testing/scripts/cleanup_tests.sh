#!/bin/bash

# this is run as root and cleans up
echo cleaning tmp
rm -rf /tmp/ws
echo removing users and groups
deluser usera
deluser userb
deluser userc
delgroup groupa
delgroup groupb
delgroup groupc
