#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"

extern int  getMacNum(char* hostname);
extern int getGroupNumberFromHostname(char* hostname);

double MPI_Bcast_def(int count, int root, int iter){
	int myrank;
	int group_size;

	double total_time = 0;
  	// initializing the buffers
	
	MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
	MPI_Comm_size( MPI_COMM_WORLD, &group_size);

	double *send_buff = (double*)malloc(sizeof(double)*count);
    
    if(myrank==root)
    	for (int i=0; i<count; i++)
	    	send_buff[i] = (double) rand();
	
	for(int i=0; i < iter; ++i){
		double sTime = MPI_Wtime();
		MPI_Bcast(send_buff, count, MPI_DOUBLE , root, MPI_COMM_WORLD);
		double eTime = MPI_Wtime() - sTime;


		double maxtime;
		MPI_Reduce(&eTime , &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		if(!myrank){
	  		total_time += maxtime;
	  		// printf("%lf\n", maxtime);
		}
	  	MPI_Barrier(MPI_COMM_WORLD);
	}
	
  free(send_buff); 
	return (total_time/(double)iter);
}

// Pseudo code for this part
// Group_leader_group <- Root
// Node_leader_group <- Group_leader
// Node_group <- Node leader

double MPI_Bcast_opt_node_group(int count, int root, int iter){
    
    char hostname[MPI_MAX_PROCESSOR_NAME];
    double total_time = 0;

    double sTime = MPI_Wtime();
    //---- INFORMATION COLLECTION
    int numProc, rank, processorNameLen;
  	MPI_Comm_size (MPI_COMM_WORLD, &numProc);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank); 
    MPI_Get_processor_name (hostname, &processorNameLen);
    int machineNum = getMacNum(hostname);
    
    // this type of tag ensure that the given root will always be the 0th indexed element in any of the subsequent group formed
    if(rank == root)
    	rank = (-1)*rank;
    //---- MAKING THE NODE LEVEL COMMUNICATOR
    int ncolor = machineNum;
  	int nodeGroupRank, nodeGroupSize;
  	MPI_Comm nodeGroupComm;
  	MPI_Comm_split (MPI_COMM_WORLD, ncolor, rank, &nodeGroupComm);
    MPI_Comm_rank( nodeGroupComm, &nodeGroupRank );
  	MPI_Comm_size( nodeGroupComm, &nodeGroupSize );
    //---- MAKING THE GROUP LEVEL COMMUNICATOR FOR THE NODE LEADERS
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
  	//---- MAKING THE GROUP LEVEL COMMUNICATOR FOR THE GROUP LEADERS
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
    if(!rank)
    	total_time += maxTime;   
    
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i){ 
	  	//---- INITIALIZING THE BUFFER SENT
      double *send_buff = (double*)malloc(sizeof(double)*count);
		
      if(rank<0) // ie the root
        for(int i=0;i < count; ++i) send_buff[i] = (double) rand();
		  MPI_Barrier(MPI_COMM_WORLD);	  		
	  	//---- BROADCASTING AT INTER GROUP LEVEL	
	  	sTime = MPI_Wtime();
	  	if( interGroupSize > 1)
	  		MPI_Bcast(send_buff, count, MPI_DOUBLE , 0, interGroupLeaderComm);
	  	//---- BROADCASTING IN GROUP 
	  	if( intraGroupSize > 1 )
	  		MPI_Bcast(send_buff, count, MPI_DOUBLE , 0, intraGroupComm);
	  	//---- BROADCASTING IN NODE
	  	if( nodeGroupSize > 1)
	  		MPI_Bcast(send_buff, count, MPI_DOUBLE, 0, nodeGroupComm);
		  eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    		

    	// ---- checking correctness
	  	// int tflag = 0;
  	
  		// for(int i=0; i<count; ++i){
  		// 	if ((int)send_buff[i] == (-1)*root)
  		// 		continue;
  		// 	else
  		// 		tflag = 1;
  		// }
  		// if(tflag)
  		// 	printf("PROBLEM\n");

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }

      free(send_buff);
	}
	return (total_time/(double)iter);
}

// Pseudo code for this part
// Node_leader_group <- Root
// Node_group <- Node leader

double MPI_Bcast_opt_node_only(int count, int root, int iter){

    char hostname[MPI_MAX_PROCESSOR_NAME];
    double total_time = 0;

    double sTime = MPI_Wtime();
    //---- INFORMATION COLLECTION
    int numProc, rank, processorNameLen;
    MPI_Comm_size (MPI_COMM_WORLD, &numProc);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank); 
    MPI_Get_processor_name (hostname, &processorNameLen);
    int machineNum = getMacNum(hostname);
    // this type of tag ensure that the given root will always be the 0th indexed element
    if(rank == root)
    	rank = (-1)*rank;
    //---- MAKING THE NODE LEVEL COMMUNICATOR
    int ncolor = machineNum;
  	int nodeGroupRank, nodeGroupSize;
  	MPI_Comm nodeGroupComm;
  	MPI_Comm_split (MPI_COMM_WORLD, ncolor, rank, &nodeGroupComm);
    MPI_Comm_rank( nodeGroupComm, &nodeGroupRank );
  	MPI_Comm_size( nodeGroupComm, &nodeGroupSize );
    //---- MAKING THE GROUP LEVEL COMMUNICATOR FOR THE GROUP LEADERS
  	int interGroupSize; interGroupSize = -1;
  	MPI_Comm interGroupLeaderComm;
  	if(nodeGroupRank == 0)
  	  MPI_Comm_split (MPI_COMM_WORLD, 1000, rank, &interGroupLeaderComm);
  	else
    	MPI_Comm_split (MPI_COMM_WORLD, MPI_UNDEFINED, rank, &interGroupLeaderComm);

    if(nodeGroupRank == 0)
    	MPI_Comm_size ( interGroupLeaderComm, &interGroupSize);
  	
  	double eTime = MPI_Wtime() - sTime;
  	double maxTime;
  	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(!rank)
    	total_time += maxTime;   
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i){ 
	  	//---- INITIALIZING THE BUFFER SENT
	  	double *send_buff = (double*)malloc(sizeof(double)*count);
		
  		if(rank<0)
  			for(int i=0;i < count; ++i) send_buff[i] = (double) rand();
  		MPI_Barrier(MPI_COMM_WORLD);	  		
  	  	//---- BROADCASTING AT INTER GROUP LEVEL	
  	  	sTime = MPI_Wtime();
  	  	if( interGroupSize > 1)
  	  		MPI_Bcast(send_buff, count, MPI_DOUBLE , 0, interGroupLeaderComm);
  	  	//---- BROADCASTING IN NODE
  	  	if( nodeGroupSize > 1)
  	  		MPI_Bcast(send_buff, count, MPI_DOUBLE, 0, nodeGroupComm);
  		eTime = MPI_Wtime() - sTime;
      	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
      		

    	// ---- checking correctness
	  	// int tflag = 0;
  	
  		// for(int i=0; i<count; ++i){
  		// 	if ((int)send_buff[i] == (-1)*root)
  		// 		continue;
  		// 	else
  		// 		tflag = 1;
  		// }
  		// if(tflag)
  		// 	printf("PROBLEM\n");

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }

      free(send_buff);
	}

	
	return (total_time/(double)iter);
}

// Pseudo code for this part
// Group_leader_group <- Root
// Group_group <- Group leader

double MPI_Bcast_opt_group_only(int count, int root, int iter){

    char hostname[MPI_MAX_PROCESSOR_NAME];
    double total_time = 0;

    double sTime = MPI_Wtime();
    //---- INFORMATION COLLECTION
    int numProc, rank, processorNameLen;
  	MPI_Comm_size (MPI_COMM_WORLD, &numProc);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank); 
    MPI_Get_processor_name (hostname, &processorNameLen);
    int machineNum = getMacNum(hostname);
    // this type of tag ensure that the given root will always be the 0th indexed element
    if(rank == root)
    	rank = (-1)*rank;
    //---- MAKING THE GROUP LEVEL COMMUNICATOR FOR THE NODE LEADERS
    int intraGroupRank, intraGroupSize;
  	MPI_Comm intraGroupComm;  	
  	intraGroupRank = -1; intraGroupSize = -1;
  	
    int color = getGroupNumberFromHostname(hostname) + 100;	

    MPI_Comm_split (MPI_COMM_WORLD, color, rank, &intraGroupComm);
    MPI_Comm_rank( intraGroupComm, &intraGroupRank );
    MPI_Comm_size( intraGroupComm, &intraGroupSize );
  	//---- MAKING THE GROUP LEVEL COMMUNICATOR FOR THE GROUP LEADERS
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
    if(!rank)
    	total_time += maxTime;   
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i){ 
	  	//---- INITIALIZING THE BUFFER SENT
	  	double *send_buff = (double*)malloc(sizeof(double)*count);
		
      if(rank<0)
      	for(int i=0;i < count; ++i) send_buff[i] = (double) rand();
      MPI_Barrier(MPI_COMM_WORLD);	  		
    	//---- BROADCASTING AT INTER GROUP LEVEL	
    	sTime = MPI_Wtime();
    	if( interGroupSize > 1)
    		MPI_Bcast(send_buff, count, MPI_DOUBLE , 0, interGroupLeaderComm);
    	//---- BROADCASTING IN GROUP 
    	if( intraGroupSize > 1 )
    		MPI_Bcast(send_buff, count, MPI_DOUBLE , 0, intraGroupComm);
    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	

    	// ---- checking correctness
	  	// int tflag = 0;
  	
  		// for(int i=0; i<count; ++i){
  		// 	if ((int)send_buff[i] == (-1)*root)
  		// 		continue;
  		// 	else
  		// 		tflag = 1;
  		// }
  		// if(tflag)
  		// 	printf("PROBLEM\n");

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }

      free(send_buff);
	}

	
    return (total_time/(double)iter);
}


// Pseudo code
// rank -> (groupnum*10000 + machinenum*100 - rank)
// near process toplogically have contigious ranks
// newCommunicator used for Bcast

double MPI_Bcast_opt_rank_only(int count, int root, int iter){

	char hostname[MPI_MAX_PROCESSOR_NAME];

  double *buff = (double*)malloc(sizeof(double)*count);

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
      MPI_Bcast(buff, count, MPI_DOUBLE , 0, newComm);
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
  free(buff);
  return (total_time/(double)iter);
}


