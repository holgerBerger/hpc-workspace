#!/bin/bash

# run this after compilation for some testing

mkdir /tmp/lustre-db /tmp/lustre1 /tmp/lustre2 /tmp/nfs-db /tmp/nfs1
mkdir /tmp/lustre1/.removed /tmp/lustre2/.removed /tmp/nfs1/.trash

chown ws /tmp/lustre-db /tmp/lustre1 /tmp/lustre2
chmod a+rx /tmp/lustre-db /tmp/lustre1 /tmp/lustre2
chmod g=rx /tmp/lustre-db /tmp/lustre1 /tmp/lustre2


setcap "CAP_DAC_OVERRIDE=p CAP_CHOWN=p" ./bin/ws_allocate
setcap "CAP_DAC_OVERRIDE=p" ./bin/ws_release

