#!/bin/bash

# run as root from testing directory

ls -1 tests/*/test.sh | sort -n | while read testname
do
	$testname
done
