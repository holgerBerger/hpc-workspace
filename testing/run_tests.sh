#!/bin/bash

ls -1 tests/*/test.sh | sort -n | while read testname
do
	$testname
done
