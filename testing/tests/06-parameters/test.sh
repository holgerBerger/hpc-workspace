# checks for
#  illegal names
#  illegal FS
#  non-existing FS
#  illegal duration
#  change user

testname=${0%%test.sh}
printf "%-60s " ${testname%%/}

sudo -u usera ../bin/ws_allocate workspace/1 10 2> $testname/err1.res > $testname/out1.res
ret1=$?
cmp --quiet $testname/err1.res $testname/err1.ref
cmp1_1=$?
cmp --quiet $testname/out1.res $testname/out1.ref
cmp1_2=$?

sudo -u usera ../bin/ws_allocate -F ws10 workspace1 10 2> $testname/err2.res > $testname/out2.res
ret2=$?
cmp --quiet $testname/err2.res $testname/err2.ref
cmp2_1=$?
cmp --quiet $testname/out2.res $testname/out2.ref
cmp2_2=$?

sudo -u usera ../bin/ws_allocate -F ws12 workspace1 10 2> $testname/err3.res > $testname/out3.res
ret3=$?
cmp --quiet $testname/err3.res $testname/err3.ref
cmp3_1=$?
cmp --quiet $testname/out3.res $testname/out3.ref
cmp3_2=$?

sudo -u usera ../bin/ws_allocate  workspace1 100 2> $testname/err4.res > $testname/out4.res
ret4=$?
cmp --quiet $testname/err4.res $testname/err4.ref
cmp4_1=$?
cmp --quiet $testname/out4.res $testname/out4.ref
cmp4_2=$?

sudo -u usera ../bin/ws_allocate -u userb  workspace1 10 2> $testname/err5.res > $testname/out5.res
ret5=$?
cmp --quiet $testname/err5.res $testname/err5.ref
cmp5_1=$?
cmp --quiet $testname/out5.res $testname/out5.ref
cmp5_2=$?

# echo $ret1 $ret2 $ret3 $ret4 $ret5
# 1 0 1 0 0

if [ $ret1 != 1 -o $ret2 != 4 -o $ret3 != 1 -o $ret4 != 0 -o $ret5 != 0 -o $cmp1_1 != 0 -o $cmp1_2 != 0 -o $cmp2_1 != 0 -o $cmp2_2 != 0 -o $cmp3_1 != 0 -o $cmp3_2 != 0 -o $cmp4_1 != 0 -o $cmp4_2 != 0 -o $cmp5_1 != 0 -o $cmp5_2 != 0 ]
then
	echo -e "\e[1;31mfailed\e[0m $ret1 $ret2 $ret3 $ret4 $ret5 $cmp1_1 $cmp1_2 $cmp2_1 $cmp2_2 $cmp3_1 $cmp3_2 $cmp4_1 $cmp4_2 $cmp5_1 $cmp5_2 "
else	
	echo -e "\e[1;32msuccess\e[0m"
fi
