#!/bin/bash
echo "=============================================================="
echo "Rename files in .stb check"
echo "=============================================================="

# *******************************************************************

cd /usr/src/hw2-mrai/CSE-506/
sh install_modules.sh >> pre_run
rm -rf test_result test_result1
rm pre_run
cd testdir

setfacl -R -m u:ubuntu:rwx /usr/src/parent
setfacl -R -m u:ubuntu:rwx /usr/src/hw2-mrai/CSE-506/

echo "Currently in directory : $PWD"

echo "Case 1 : Rename file in .stb with root user"
touch /usr/src/parent/.stb/test1;
mv /mnt/stbfs/.stb/test1 /mnt/stbfs/.stb/test2 >> test_result 2>&1
if grep "cannot move" test_result; 
then 
	echo "------------------ TEST CASE 1 PASSED ------------------"
else
	echo "------------------ TEST CASE 1 FAILED ------------------"
fi


echo "**************************************************************"


echo "Case 2 : Rename file in .stb with non-root user"
sudo -i -u ubuntu bash << EOF
echo "Currently in directory : $PWD"
cd /usr/src/hw2-mrai/CSE-506/testdir
touch test_result1
if mv /mnt/stbfs/.stb/test3 /mnt/stbfs/.stb/test4 2>&1 | grep "cannot move"
# if grep "cannot move" test_result1;
then
	echo "------------------ TEST CASE 2 PASSED ------------------"
else
	echo "------------------ TEST CASE 2 FAILED ------------------"
fi
# rm -rf /usr/src/parent/.stb/test1 /usr/src/parent/.stb/test3 

echo "**************************************************************"
EOF