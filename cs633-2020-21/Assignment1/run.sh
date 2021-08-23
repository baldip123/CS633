#!/bin/bash
# 1. compile using make
make
echo "the file is compiled successfully"

#1.5. ovewriting previously made data files
for P in 16 36 49 64
do
        echo "" > "./data_"$P
done
   

# 2. run the job for each N, P 5 times
for iter in 1 2 3 4 5
do
	for P in 16 36 49 64
	do
		~/UGP/allocator/src/allocator.out "$P" 8 > garbage
		for N in 256 1024 4096 16384 65536 262144 1048576 
		do
			echo $iter"-"$P"-"$N
			TIMES=$(mpirun -np $P -f hosts ./src $N 50)
                        i=1
                        for TIME in $TIMES
                        do
                            echo $i,$P,$N,$TIME >> "./data_"$P
                            ((i=i+1))
                        done
		done
	done
done
# 3. plot the data
DIRECTORY=$(pwd)
ssh -X csews1 "cd ${DIRECTORY} ; python plot.py"

# 4. cleanup
rm allocator.log  garbage hostsimproved hosts comphosts
