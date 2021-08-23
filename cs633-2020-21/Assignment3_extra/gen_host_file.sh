echo $1,$2,$3
cd helper_scripts/
python script.py $1 $2 $3
cp hostfile2 ../
cd ../
