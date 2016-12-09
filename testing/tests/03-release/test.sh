# checks for
#  directory is moved to .removed and has correct permissions and ownsers
#  correct output of ws_release
#
testname=${0%%test.sh}
printf "%-60s " ${testname%%/}
sudo -u usera ../bin/ws_release workspace1 2> $testname/err.res > $testname/out.res
ret=$?

ls -l /tmp/ws/ws1/.removed | cut -d' ' -f 1,3,4 > $testname/ls.res
cmp --quiet $testname/ls.res $testname/ls.ref
cmp0=$?

cmp --quiet $testname/err.res $testname/err.ref
cmp1=$?
cmp --quiet $testname/out.res $testname/out.ref
cmp2=$?

if [ $ret != 0 -o $cmp0 != 0 -o $cmp1 != 0 -o $cmp2 != 0 ]
then
	echo -e "\e[1;31mfailed\e[0m $ret $cmp0 $cmp1 $cmp2"
else	
	echo -e "\e[1;32msuccess\e[0m"
fi
