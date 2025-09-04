file_list=$(find . -type d -empty|grep -v .git|grep -v arch)
echo $file_list

echo "Continue?"

read

#for dirf in $(find . -type d -empty|grep -v .git|grep -v arch)
for dirf in $file_list 
do
        echo "Removing: " $dirf
       rm -rf $dirf
done
