#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "mpi.h"
#include "file_r&w.h"

float min(float num1, float num2) 
{
    return (num1 > num2 ) ? num2 : num1;
}

void experiment(int size, int myrank, int numProc){
	float* send_buff = (float*)malloc(sizeof(float)*size); 
	float* recv_buff = (float*)malloc(sizeof(float)*size/2);

	float dummy = 0;
	double sTime, time, maxTime, compTime, commTime;
	compTime = 0;
	commTime = 0;
	for(int iter=0; iter<10; ++iter){

		MPI_Barrier(MPI_COMM_WORLD);
		sTime = MPI_Wtime();
		if(myrank == 0){
			MPI_Send(send_buff, size/2, MPI_FLOAT, numProc/2, 1, MPI_COMM_WORLD );
			// printf("%d\n",myrank + numProc/2);
		}
		else if(myrank==numProc/2){
			MPI_Recv(recv_buff, size/2, MPI_FLOAT, 0, 1, MPI_COMM_WORLD,  MPI_STATUS_IGNORE);
			// printf("%d\n",numProc/2);
		}
		time = MPI_Wtime() - sTime;
		MPI_Reduce(&time, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		if(myrank == 0)
			commTime += maxTime;
		//---------------
		MPI_Barrier(MPI_COMM_WORLD);
		sTime = MPI_Wtime();
		if(myrank < numProc/2){
			MPI_Send(send_buff, size/(numProc), MPI_FLOAT, myrank + numProc/2, 1, MPI_COMM_WORLD );
			// printf("%d\n",myrank + numProc/2);
		}
		else{
			MPI_Recv(recv_buff, size/(numProc), MPI_FLOAT, myrank - numProc/2, 1, MPI_COMM_WORLD,  MPI_STATUS_IGNORE);
			// printf("%d\n",myrank - numProc/2);
		}
		time = MPI_Wtime() - sTime;
		MPI_Reduce(&time, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		if(myrank == 0)
			compTime += (MPI_Wtime() - sTime);
	}
	if(myrank == 0)
		printf("size = %d %lf , %lf \n", size, compTime, commTime);
	return;
}

int main(int argc, char * argv[]){	
	//initialize MPI
    MPI_Init ( &argc, &argv);
    //get the rank and the number of processes
    int myrank;
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
    //get the number of processors
    int numProc;
    MPI_Comm_size( MPI_COMM_WORLD, &numProc );

    for(int size = 1000000; size<=20000000; size = size+1000000){
    	experiment(size, myrank,numProc);
    }

   	MPI_Finalize();
	return 0;
}

//---------------------------------------------1/2
// size = 10000 0.001285 , 0.002901 
// size = 100000 0.007751 , 0.019752 
// size = 1000000 0.070702 , 0.173154 
// size = 10000000 0.705102 , 1.710467 
// size = 100000000 7.047669 , 17.090403 

// size = 10000 0.005124 , 0.002056 
// size = 100000 0.024649 , 0.021514 
// size = 1000000 0.176296 , 0.173782 
// size = 10000000 1.713116 , 1.711512 
// size = 100000000 17.092975 , 17.087829 
