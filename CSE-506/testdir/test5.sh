#!/bin/bash

echo "=============================================================="
echo "Validating user id-wise lookup in .stb"
echo "=============================================================="

cd /usr/src/hw2-mrai/CSE-506
echo "Currently in directory : $PWD"

sh install_modules.sh >> pre_loads
rm -rf pre_loads

setfacl -R -m u:ubuntu:rwx /usr/src/parent
setfacl -R -m u:ubuntu:rwx /usr/src/hw2-mrai/CSE-506

touch /usr/src/parent/.stb/2022-04-12-21-20-41-newFile-0
touch /usr/src/parent/.stb/2022-04-12-21-21-41-newFile-0
touch /usr/src/parent/.stb/2022-04-12-21-36-41-newFile-1000
touch /usr/src/parent/.stb/2022-04-12-21-36-45-newFile-1000

echo "Case1: Validating lookup for root user"
count=$(ls /mnt/stbfs/.stb | grep newFile- |  wc -l)
# echo "$count"
if [ $count -eq "4" ]
then
	echo "------------------ TEST CASE 1 PASSED ------------------"
else
	echo "------------------ TEST CASE 1 FAILED ------------------"
fi


rm /usr/src/parent/.stb/2022-04-12-21-20-41-newFile-0
rm /usr/src/parent/.stb/2022-04-12-21-21-41-newFile-0
rm /usr/src/parent/.stb/2022-04-12-21-36-41-newFile-1000
rm /usr/src/parent/.stb/2022-04-12-21-36-45-newFile-1000

# *******************************************************************
echo "**************************************************************"
echo "Case2: Validating lookup for non-root user"

sudo -i -u ubuntu bash << EOF
touch /usr/src/parent/.stb/2022-04-12-21-20-41-newFile-0
touch /usr/src/parent/.stb/2022-04-12-21-36-48-newFile1-1000
touch /usr/src/parent/.stb/2022-04-12-21-36-42-newFile1-1000
exec 5>&1
count1=$(ls /mnt/stbfs/.stb | grep newFile1 | wc -l) 
filecount=2

if [[ "$count1" -eq "$filecount" ]]
then
	echo "------------------ TEST CASE 2 PASSED ------------------"
else
	echo "------------------ TEST CASE 2 FAILED ------------------"
fi

EOF
echo "**************************************************************"

echo "$count1"
rm /usr/src/parent/.stb/2022-04-12-21-20-41-newFile-0
rm /usr/src/parent/.stb/2022-04-12-21-36-48-newFile1-1000
rm /usr/src/parent/.stb/2022-04-12-21-36-42-newFile1-1000
