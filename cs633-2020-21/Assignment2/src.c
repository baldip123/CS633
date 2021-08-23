#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"
#include "our_gather.h"
#include "our_reduce.h"
#include "our_bcast.h"
#include "our_alltoallv.h"
#include "time.h"


int nodeToGroupMap[93] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0, 3, 0, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

// takes the starting of the hostname string and returns the groups to which the nodes belongs to.
int getGroupNumberFromHostname(char* hostname){
  int csewsNumber = atoi(hostname+5);
  return nodeToGroupMap[csewsNumber];
}

int getMacNum(char* hostname){
	int csewsNumber = atoi(hostname + 5);
	return csewsNumber;
}

void get_best(double ng, double g, double n, double r){
	if(ng <= ng && ng <= g && ng <= n && ng <= r)
		printf("ng\n");
	if(n <= ng && n <= g && n <= n && n <= r)
		printf("n\n");
	if(g <= ng && g <= g && g <= n && g <= r)
		printf("g\n");
	if(r <= ng && r <= g && r <= n && r <= r)
		printf("r\n");
	return;
}


// for the different MPI_Reduce_opt look at the file "our_reduce.c"
double MPI_Reduce_optimized(int count, int root, int iter, int ppn){
	double avg_time;
	if(ppn > 1){
		if( count < (1024*2048)/sizeof(double))
			avg_time = MPI_Reduce_opt_node_only(count, root, iter);
		else
			avg_time = MPI_Reduce_opt_node_group(count, root, iter);
	}
	else{
		if( count < (1024*2048)/sizeof(double))
			avg_time = MPI_Reduce_opt_rank_only(count, root, iter);
		else
			avg_time = MPI_Reduce_opt_group_only(count, root, iter);
	}
	return avg_time;
}

// for the different MPI_Bcast_opt functions look at the file "our_bcast.c"
double MPI_Bcast_optimized(int count, int root, int iter, int ppn){
	double avg_time;
	int numProcs;
	MPI_Comm_size( MPI_COMM_WORLD, &numProcs );

	if(ppn==1){
		if(numProcs/ppn > 13 && count >= (2048*1024)/sizeof(double) )
			avg_time = MPI_Bcast_opt_group_only(count, root, iter);
		else
			avg_time = MPI_Bcast_opt_rank_only(count, root, iter);
	}
	else{
		if(count < (2048*1024)/sizeof(double))
			avg_time = MPI_Bcast_opt_node_only(count, root, iter);
		else
			avg_time = MPI_Bcast_opt_node_group(count, root, iter);
	}
	
	return avg_time;
}

// for the different MPI_Gather_opt functions look at the file "our_gather.c"

double MPI_Gather_optimized(int count, int root, int iter, int ppn){
	double avg_time;
	int numProcs;
	MPI_Comm_size( MPI_COMM_WORLD, &numProcs );

	if(numProcs/ppn > 13)
		avg_time = MPI_Gather_opt_group_only(count, root, iter);
	else
		avg_time = MPI_Gather_opt_rank_only(count, root, iter);
	
	return avg_time;
}

// Have a look at "our_alltoallv.c"
double MPI_Alltoallv_optimized(double*sendBuf, int* sendCounts, int*sendDisplacement, double* recvBuf, int* recvCounts, int* recvDisplacement, int count, int iter, int ppn ){
    double avg_time;
    int numProcs;
	MPI_Comm_size( MPI_COMM_WORLD, &numProcs );

    if(ppn < 4){
        avg_time = standard_alltoallv(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement, iter);
    }
    else{
        if(count* numProcs < 2000000){
            //use the otimized all to all other wise use the default
            avg_time = optimized_alltoallv(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement, iter);
        }
        else{
            avg_time = standard_alltoallv(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement, iter);
        }
    }
}

void Run_All_To_All_Def_And_Opt(int count, int iter, int ppn){
  char hostname[MPI_MAX_PROCESSOR_NAME];

  int numProc, rank, processorNameLen;

  // get number of processes
  MPI_Comm_size (MPI_COMM_WORLD, &numProc);

  // get my rank
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);

  //so that every process has unequal data being sent
  srand(time(0)+rank);
  int sendCounts[numProc];
  int totalCounts = 0;
  for(int i = 0; i < numProc; i++){
      sendCounts[i] = getRandom(0, count);
      totalCounts += sendCounts[i];
  }

  //find the displacements
  int sendDisplacement[numProc];
  int displ = 0;
  for(int i = 0; i < numProc; i++){
      sendDisplacement[i] = displ;
      displ += sendCounts[i];
  }

  //initialize the array
  double* sendBuf = (double*)malloc(sizeof(double)*totalCounts);
  for(int i = 0; i < totalCounts; i++){
      sendBuf[i] = rand()%10;
  }

  //get the receiveCounts from all the process
  int recvCounts[numProc];
  
  //get the counts of doubles being sent by each pro 
  MPI_Alltoall(sendCounts, 1, MPI_INT, recvCounts, 1, MPI_INT, MPI_COMM_WORLD);
  

  //get the total double being sent
  int totalRecvCount = 0;
  for(int i = 0; i < numProc; i++){
      totalRecvCount += recvCounts[i];
  }

  //define the receive buffer
  double* recvBuf = (double*)malloc(sizeof(double)*totalRecvCount);

  //define the receiveDisplacement
  int recvDisplacement[numProc];
  displ = 0;
  for(int i = 0; i < numProc; i++){
      recvDisplacement[i] = displ;
      displ += recvCounts[i];
  }

  //here each process has the counts
  //now we can call the functions for the standard alltoallv and our optimized alltoallv
  double standardTime =  standard_alltoallv(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement, iter);
  if(!rank){
    printf("%lf\n", standardTime);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double optimizedTime = MPI_Alltoallv_optimized(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement,count,iter,ppn);
  if(!rank){
    printf("%lf\n", optimizedTime);
  }

  //dont forget to free the buffers
  free(recvBuf);
  free(sendBuf);

}


int main (int argc, char *argv[]){
	
	// number of doubles
	int count =  atoi(argv[1]) / sizeof(double);
	int ppn =  atoi(argv[2]) ;
	
	int root = 0;
	// the mpi part
	MPI_Init ( &argc, &argv);
	
	int myrank;
	MPI_Comm_rank( MPI_COMM_WORLD, &myrank );

    int numProc;
    MPI_Comm_size( MPI_COMM_WORLD, &numProc );

    // the 5 iterations and count ant the root is given as the input
    // you can change the root by changing the line at 184
	double bcast_def =  MPI_Bcast_def(count, root, 5);
	double bcast_best =  MPI_Bcast_optimized(count, root, 5, ppn);
	if(!myrank){
		printf("%lf\n", bcast_def);
		printf("%lf\n", bcast_best);
	}
	//the reduce results genereating part
	double reduce_def =  MPI_Reduce_def(count, root, 5);
	double reduce_best =  MPI_Reduce_optimized(count, root, 5, ppn);
	if(!myrank){
		printf("%lf\n", reduce_def);
		printf("%lf\n", reduce_best);
	}
	//the gather results genereating part
	double gather_def =  MPI_Gather_def(count, root, 5);
	double gather_best = MPI_Gather_optimized(count, root, 5, ppn); 
	if(!myrank){
		printf("%lf\n", gather_def);
		printf("%lf\n", gather_best);
	}
	// the all to all results generating part
	Run_All_To_All_Def_And_Opt(count, 5, ppn);

	MPI_Finalize();
}



