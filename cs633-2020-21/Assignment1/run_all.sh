for iter in 1 2 3 4 5
do
	for P in 16
	do
		./hostFileGen.sh > garbage
		for N in 256 1024 4096 16384 65536 262144 1048576 
		do
			echo $iter"-"$P"-"$N
			mpirun -np $P -f hosts ./src $N 50 >> "./data/data_"$P
		done
	done
done