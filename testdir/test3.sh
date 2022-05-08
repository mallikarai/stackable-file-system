#!/bin/bash
echo "========================================================================="
echo "Writing/Creating/Changing permission of files directly in .stb directory"
echo "========================================================================="

# *******************************************************************
cd /usr/src/hw2-mrai/CSE-506
echo "Currently in directory : $PWD"

# echo "Test file to check if kernel is working as expected" >> test04_input;

sh install_mods.sh >> pre_run
rm pre_run

setfacl -R -m u:ubuntu:rwx /usr/src/parent
setfacl -R -m u:ubuntu:rwx /usr/src/hw2-mrai/CSE-506


echo "Case 1 : Creating file in .stb with root user"
touch /mnt/stbfs/.stb/file.txt >> test_result 2>&1
if grep --q "Operation not permitted" test_result;
then
	echo "------------------ TEST CASE 1 PASSED ------------------"
else
	echo "------------------ TEST CASE 1 FAILED ------------------"
fi
rm -rf test_result

echo "**************************************************************"


echo "Case 1 : Creating file in .stb with non-root user"
sudo -i -u ubuntu bash << EOF
if touch /mnt/stbfs/.stb/file.txt 2>&1 | grep "Operation not permitted"
then
	echo "------------------ TEST CASE 2 PASSED ------------------"
else
	echo "------------------ TEST CASE 2 FAILED ------------------"
fi
# rm -rf test_result

EOF
echo "**************************************************************"