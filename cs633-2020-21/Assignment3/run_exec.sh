mpirun -np $1 -f hostfile2 ./code $2
cat output.txt >> $3