# checks for
#  correct output, including time and remaining extensiosn
#  correct permissions and owners
#  correct location of workspace

testname=${0%%test.sh}
printf "%-60s " ${testname%%/}
sudo -u usera ../bin/ws_allocate workspace1 10 2> $testname/err.res > $testname/out.res
ret=$?


cmp --quiet $testname/err.res $testname/err.ref
cmp1=$?
cmp --quiet $testname/out.res $testname/out.ref
cmp2=$?

ls -dl /tmp/ws/ws1/usera-workspace1 | cut -d' ' -f 1,3,4 > $testname/lsout1.res
cmp --quiet $testname/lsout1.res $testname/lsout1.ref
ls1=$?
ls -dl /tmp/ws/ws1-db/usera-workspace1 | cut -d' ' -f 1,3,4 > $testname/lsout2.res
cmp --quiet $testname/lsout2.res $testname/lsout2.ref
ls2=$?

if [ $ret != 0 -o $ls1 != 0 -o $ls2 != 0 -o $cmp1 != 0 -o $cmp2 != 0 ]
then
	echo -e "\e[1;31mfailed\e[0m"
else	
	echo -e "\e[1;32msuccess\e[0m"
fi
