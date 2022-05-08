#!/bin/bash
echo "=============================================================="
echo "Unlink file check"
echo "=============================================================="

# *******************************************************************
cd /usr/src/hw2-mrai/CSE-506
echo "Currently in directory : $PWD"

sh install_modules.sh >> pre_loads
rm -rf pre_loads

setfacl -R -m u:ubuntu:rwx /usr/src/parent
setfacl -R -m u:ubuntu:rwx /usr/src/hw2-mrai/CSE-506

echo "Case1: Unlink file with root user"

touch /usr/src/parent/test1
before=$(ls /mnt/stbfs/.stb | wc -l)
rm /mnt/stbfs/test1
after=$(ls /mnt/stbfs/.stb | wc -l)
diff="$((after-before))"
check=$(ls /mnt/stbfs/.stb | grep test-0 | wc -l)


if [ $diff -eq $check ]
then
	echo "------------------ TEST CASE 1 PASSED ------------------"
else
	echo "------------------ TEST CASE 1 FAILED ------------------"
fi


echo "**************************************************************"


echo "Case1: Unlink file with non-root user"

sudo -i -u ubuntu bash << EOF
touch /usr/src/parent/test2
before=$(ls /mnt/stbfs/.stb | wc -l)
rm /mnt/stbfs/test2
after=$(ls /mnt/stbfs/.stb | wc -l)
diff="$((after-before))"
check=$(ls /mnt/stbfs/.stb | grep test-1000 | wc -l)

if [ $diff -eq $check ]
then
	echo "------------------ TEST CASE 2 PASSED ------------------"
else
	echo "------------------ TEST CASE 2 FAILED ------------------"
fi

EOF
echo "**************************************************************"