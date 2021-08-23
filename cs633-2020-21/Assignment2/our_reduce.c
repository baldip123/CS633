#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"

extern int  getMacNum(char* hostname);
extern int getGroupNumberFromHostname(char* hostname);

double MPI_Reduce_def(int count, int root, int iter){
	int myrank;
	double total_time = 0;
  	// initializing the buffers
	double *buff = (double*)malloc(sizeof(double)*count);
	double *buff_sum = (double*)malloc(sizeof(double)*count);
    
	MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
	for (int i=0; i<count; i++)
	    buff[i] = (double) rand();



	// has to be called by all processes
	for(int i=0; i < iter; ++i){
		double sTime = MPI_Wtime();
		MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, root, MPI_COMM_WORLD);
		double eTime = MPI_Wtime() - sTime;


		double maxtime;
		MPI_Reduce(&eTime , &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		if(!myrank){
	  		total_time += maxtime;
	  		// printf("%lf\n", maxtime);
		}
	  	MPI_Barrier(MPI_COMM_WORLD);
	}
	
	free(buff); free(buff_sum);
	return (total_time/(double)iter);
}

// Pseudo code for this part
// Node_group -> Node leader
// Node_leader_group -> Group_leader
// Group_leader_group -> Root

double MPI_Reduce_opt_node_group(int count, int root, int iter){
	
	char hostname[MPI_MAX_PROCESSOR_NAME];

	double *buff = (double*)malloc(sizeof(double)*count);
	double *buff_sum = (double*)malloc(sizeof(double)*count);
	
	double total_time = 0;

	double sTime = MPI_Wtime();
	// making the communicators 
	
	// getting the numproc and self rank
  	int numProc, rank, processorNameLen;
  	MPI_Comm_size (MPI_COMM_WORLD, &numProc);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	// get the name of the processor
    MPI_Get_processor_name (hostname, &processorNameLen);
    int machineNum = getMacNum(hostname);
    // this type of tag ensure that the given root will always be the 0th indexed element
    if(rank == root)
    	rank = (-1)*rank;

  	// nodes' communicator
  	int ncolor = machineNum;
  	int nodeGroupRank, nodeGroupSize;
  	MPI_Comm nodeGroupComm;

  	MPI_Comm_split (MPI_COMM_WORLD, ncolor, rank, &nodeGroupComm);
	MPI_Comm_rank( nodeGroupComm, &nodeGroupRank );
  	MPI_Comm_size( nodeGroupComm, &nodeGroupSize );
	
	 // Group's communicator ------------------------------------------------
  	int intraGroupRank, intraGroupSize;
  	MPI_Comm intraGroupComm;  	
  	intraGroupRank = -1; intraGroupSize = -1;
  	if( nodeGroupRank == 0){
  		int color = getGroupNumberFromHostname(hostname) + 100;	
		MPI_Comm_split (MPI_COMM_WORLD, color, rank, &intraGroupComm);
  	}
  	else
  		MPI_Comm_split (MPI_COMM_WORLD, MPI_UNDEFINED, rank, &intraGroupComm);

  	if(nodeGroupRank == 0){
	 	MPI_Comm_rank( intraGroupComm, &intraGroupRank );
  		MPI_Comm_size( intraGroupComm, &intraGroupSize );
  	}

  	// group's leader subgroup for the communication -------------------------
  	int interGroupSize; interGroupSize = -1;
  	MPI_Comm interGroupLeaderComm;
  	if(intraGroupRank == 0)
  	  	MPI_Comm_split (MPI_COMM_WORLD, 1000, rank, &interGroupLeaderComm);
  	else
    	MPI_Comm_split (MPI_COMM_WORLD, MPI_UNDEFINED, rank, &interGroupLeaderComm);

    if(intraGroupRank == 0)
    	MPI_Comm_size ( interGroupLeaderComm, &interGroupSize);
  	
  	double eTime = MPI_Wtime() - sTime;
  	double maxTime;
  	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    //-------------------------------------------
  	for (int i=0; i < iter; ++i){ 
	  	for(int i=0;i < count; ++i) (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	// reduction at node level -------------------------------------------------	
	  	sTime = MPI_Wtime();
	  	if( nodeGroupSize > 1){
	      MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, nodeGroupComm);
	  		// printf("%d-%d-%d\n",ncolor,rank,nodeGroupRank);
	      double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}
	  	// reduction at group level
	  	if( nodeGroupRank == 0 && intraGroupSize > 1 ){
	  		MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, intraGroupComm);
	  		double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}
	  	// reduction at inter group level
	  	if( intraGroupRank == 0 && interGroupSize>1){
	  		// printf("hi\n");
	      
	      MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, interGroupLeaderComm);
	  		double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }
	}


	free(buff); free(buff_sum);
	// if(!rank)
		// printf("Reduce Optimized Avg: %lf\n", (total_time/(double)iter));
	
	return (total_time/(double)iter);
}

// Pseudo code for this part
// Node_group -> Node leader
// Node_leader_group -> Root

double MPI_Reduce_opt_node_only(int count, int root, int iter){
	char hostname[MPI_MAX_PROCESSOR_NAME];

	double *buff = (double*)malloc(sizeof(double)*count);
	double *buff_sum = (double*)malloc(sizeof(double)*count);
  


	double total_time = 0;

	double sTime = MPI_Wtime();
	// making the communicators 
	
	// getting the numproc and self rank
  	int numProc, rank, processorNameLen;
  	MPI_Comm_size (MPI_COMM_WORLD, &numProc);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	// get the name of the processor
    MPI_Get_processor_name (hostname, &processorNameLen);
    int machineNum = getMacNum(hostname);
    // this type of tag ensure that the given root will always be the 0th indexed element
    if(rank == root)
    	rank = (-1)*rank;

  	// nodes' communicator
  	int ncolor = machineNum;
  	int nodeGroupRank, nodeGroupSize;
  	MPI_Comm nodeGroupComm;

  	MPI_Comm_split (MPI_COMM_WORLD, ncolor, rank, &nodeGroupComm);
	MPI_Comm_rank( nodeGroupComm, &nodeGroupRank );
  	MPI_Comm_size( nodeGroupComm, &nodeGroupSize );
	

  	// node's leader subgroup for the communication -------------------------
  	int interNodeSize; interNodeSize = -1;
  	MPI_Comm interNodeLeaderComm;
  	if(nodeGroupRank == 0)
  	  	MPI_Comm_split (MPI_COMM_WORLD, 1000, rank, &interNodeLeaderComm);
  	else
    	MPI_Comm_split (MPI_COMM_WORLD, MPI_UNDEFINED, rank, &interNodeLeaderComm);

    if(nodeGroupRank == 0)
    	MPI_Comm_size ( interNodeLeaderComm, &interNodeSize);
  	
  	double eTime = MPI_Wtime() - sTime;
  	double maxTime;
  	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);

  	for (int i=0; i < iter; ++i){ 
	  	for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	// reduction at node level -------------------------------------------------	
	  	sTime = MPI_Wtime();
	  	if( nodeGroupSize > 1){
	      MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, nodeGroupComm);
	  		// printf("%d-%d-%d\n",ncolor,rank,nodeGroupRank);
	      double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}
	  	
	  	// reduction at inter node level
	  	if( nodeGroupRank == 0 && interNodeSize>1){
	  		// printf("hi\n");
	      
	      MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, interNodeLeaderComm);
	  		double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }
	}

	// if(!rank)
		// printf("Reduce Optimized Node Lvl Avg: %lf\n", (total_time/(double)iter));
	free(buff); free(buff_sum);
	
	return (total_time/(double)iter);
}

// Pseudo code for this part
// Group_group -> Group leader
// Group_leader_group -> Root

double MPI_Reduce_opt_group_only(int count, int root, int iter){
	char hostname[MPI_MAX_PROCESSOR_NAME];

	double *buff = (double*)malloc(sizeof(double)*count);
	double *buff_sum = (double*)malloc(sizeof(double)*count);
	
	
  


	double total_time = 0;

	double sTime = MPI_Wtime();
	// making the communicators 
	
	// getting the numproc and self rank
  	int numProc, rank, processorNameLen;
  	MPI_Comm_size (MPI_COMM_WORLD, &numProc);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	// get the name of the processor
    MPI_Get_processor_name (hostname, &processorNameLen);
    
    // Group's communicator ------------------------------------------------
  	int intraGroupRank, intraGroupSize;
  	MPI_Comm intraGroupComm;  	
  	int color = getGroupNumberFromHostname(hostname);	
	
	MPI_Comm_split (MPI_COMM_WORLD, color, rank, &intraGroupComm);
  	MPI_Comm_rank( intraGroupComm, &intraGroupRank );
  	MPI_Comm_size( intraGroupComm, &intraGroupSize );
  	

  	// group's leader subgroup for the communication -------------------------
  	int interGroupSize; interGroupSize = -1;
  	MPI_Comm interGroupLeaderComm;
  	if(intraGroupRank == 0)
  	  	MPI_Comm_split (MPI_COMM_WORLD, 1000, rank, &interGroupLeaderComm);
  	else
    	MPI_Comm_split (MPI_COMM_WORLD, MPI_UNDEFINED, rank, &interGroupLeaderComm);

    if(intraGroupRank == 0)
    	MPI_Comm_size ( interGroupLeaderComm, &interGroupSize);
  	
  	double eTime = MPI_Wtime() - sTime;
  	double maxTime;
  	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    //-------------------------------------------
  	for (int i=0; i < iter; ++i){ 
	  	for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	// reduction at node level -------------------------------------------------	
	  	sTime = MPI_Wtime();
	  	// reduction at group level
	  	if( intraGroupSize > 1 ){
	  		MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, intraGroupComm);
	  		double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}
	  	// reduction at inter group level
	  	if( intraGroupRank == 0 && interGroupSize>1){
	  		// printf("hi\n");
	      
	      MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, interGroupLeaderComm);
	  		double *temp = buff;
	      buff = buff_sum;
	      buff_sum = temp;
	  	}

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }
	}

	// if(!rank)
		// printf("Reduce Optimized Group Avg: %lf\n", (total_time/(double)iter));
	free(buff); free(buff_sum);
	
	return (total_time/(double)iter);
}

// Pseudo code
// rank -> (groupnum*10000 + machinenum*100 - rank)
// near process toplogically have contigious ranks
// newCommunicator used for reduce

double MPI_Reduce_opt_rank_only(int count, int root, int iter){
	char hostname[MPI_MAX_PROCESSOR_NAME];

	double *buff = (double*)malloc(sizeof(double)*count);
	double *buff_sum = (double*)malloc(sizeof(double)*count);
	
	
  


	double total_time = 0;

	double sTime = MPI_Wtime();
	// making the communicators 
	
	// getting the numproc and self rank
  	int numProc, rank, processorNameLen;
  	MPI_Comm_size (MPI_COMM_WORLD, &numProc);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	// get the name of the processor
    MPI_Get_processor_name (hostname, &processorNameLen);
    int machineNum = getMacNum(hostname);
    int nrank = getGroupNumberFromHostname(hostname)*10000 + machineNum*100 - rank;

	if(rank==root)
      nrank = 0;
    // this type of tag ensure that the given root will always be the 0th indexed element
    MPI_Comm newComm;
    MPI_Comm_split (MPI_COMM_WORLD, 0, nrank, &newComm);  	

  	
  	double eTime = MPI_Wtime() - sTime;
  	double maxTime;
  	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    //-------------------------------------------
  	for (int i=0; i < iter; ++i){ 
	  	for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	// reduction at node level -------------------------------------------------	
	  	sTime = MPI_Wtime();
	  	MPI_Reduce(buff, buff_sum, count, MPI_DOUBLE, MPI_SUM, 0, newComm);
    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }
	}

	// if(!rank)
		// printf("Reduce Optimized new Rank Avg: %lf\n", (total_time/(double)iter));
	free(buff); free(buff_sum);
	return (total_time/(double)iter);
}
