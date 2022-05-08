#!/bin/bash

echo "=============================================================="
echo "Check if file is getting undeleted"
echo "=============================================================="

cd /usr/src/hw2-mrai/CSE-506
make >> pre_loads
sh install_modules.sh >> pre_loads
rm -rf pre_loads
echo "Currently in directory : $PWD"
setfacl -R -m u:ubuntu:rwx /usr/src/parent
setfacl -R -m u:ubuntu:rwx /usr/src/hw2-mrai/CSE-506
touch /usr/src/parent/.stb/2022-04-12-21-36-41-newFile-0


echo "Case1: Undelete file with root user"

before=$(ls /mnt/stbfs/.stb | wc -l)
./stbfsctl -u 2022-04-12-21-36-41-newFile-0
after=$(ls /mnt/stbfs/.stb | wc -l)
diff="$((before-after))"
# echo "$diff" 
check=$(ls | grep newFile-0 | wc -l)
# echo "$check"
rm newFile-0

if [ $diff -eq $check ]
then
	echo "------------------ TEST CASE 1 PASSED ------------------"
else
	echo "------------------ TEST CASE 1 FAILED ------------------"
fi


# *******************************************************************
echo "**************************************************************"
echo "Case2: Undelete file with non-root user"

sudo -i -u ubuntu bash << EOF
touch /usr/src/parent/.stb/2022-04-12-21-36-41-newFile-1000
before=$(ls /mnt/stbfs/.stb | wc -l)
sudo -u ubuntu /usr/src/hw2-mrai/CSE-506/stbfsctl -u 2022-04-12-21-36-41-newFile-1000
after=$(ls /mnt/stbfs/.stb | wc -l)
diff="$((before-after))"
# echo "$diff"
check=$(ls | grep newFile-1000 | wc -l)
# echo "$check"
rm newFile-1000

if [ $diff -eq $check ]
then
	echo "------------------ TEST CASE 2 PASSED ------------------"
else
	echo "------------------ TEST CASE 2 FAILED ------------------"
fi

EOF
echo "**************************************************************"