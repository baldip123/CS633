#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"

extern int  getMacNum(char* hostname);
extern int getGroupNumberFromHostname(char* hostname);

double MPI_Gather_def(int count, int root, int iter){
	int myrank;
	int group_size;

	double total_time = 0;
  	// initializing the buffers
	
	MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
	MPI_Comm_size( MPI_COMM_WORLD, &group_size);

	double *send_buff = (double*)malloc(sizeof(double)*count);
	double *recv_buff = (double*)malloc(sizeof(double)*count*group_size);
    
    for (int i=0; i<count; i++)
	    send_buff[i] = (double) rand();
	
	// has to be called by all processes
	for(int i=0; i < iter; ++i){
		// printf("hi\n");
		double sTime = MPI_Wtime();
		MPI_Gather(send_buff, count, MPI_DOUBLE, recv_buff, count, MPI_DOUBLE, root, MPI_COMM_WORLD);
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
	free(recv_buff);
	return (total_time/(double)iter);
}

double MPI_Gather_opt_node_group(int count, int root, int iter){


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
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i)
  	{ 
	  	//---- INITIALIZING THE BUFFER SENT
	  	double *buff = (double*)malloc(sizeof(double)*count);
		int *ranks_arr = (int*)malloc(sizeof(int));

		ranks_arr[0] = rank;
		if(rank<0)
			ranks_arr[0] = (-1)*rank;
		for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	//---- GATHERING AT NODE LEVEL	
	  	sTime = MPI_Wtime();
	  	if( nodeGroupSize > 1){
	  		
	  		send_count = count; recv_count = nodeGroupSize*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gather(buff, send_count, MPI_DOUBLE, newbuff, send_count, MPI_DOUBLE, 0, nodeGroupComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1; recv_count = nodeGroupSize;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gather(ranks_arr, send_count, MPI_INT, nrank_arr, send_count, MPI_INT, 0, nodeGroupComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(newbuff); free(nrank_arr);
	    }
	  	//---- GATHERING AT GROUP LEVEL
	  	if( nodeGroupRank == 0 && intraGroupSize > 1 ){

	  		send_count = count*nodeGroupSize; recv_count = intraGroupSize*nodeGroupSize*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gather(buff, send_count, MPI_DOUBLE, newbuff, send_count, MPI_DOUBLE, 0, intraGroupComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1*nodeGroupSize ; recv_count = nodeGroupSize*intraGroupSize;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gather(ranks_arr, send_count, MPI_INT, nrank_arr, send_count, MPI_INT, 0, intraGroupComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(newbuff); free(nrank_arr);
	    }
	  	//---- GATHERING AT INTER-GROUP LEVEL
	  	if( intraGroupRank == 0 && interGroupSize>1){


	  	  	//---- GETTING THE SIZE OF THE SENDS
	  	  	int send_sizes[interGroupSize];
	  	  	int my_send_size = intraGroupSize*nodeGroupSize; 
	  	  	MPI_Gather(&my_send_size, 1, MPI_INT, send_sizes, 1, MPI_INT, 0, interGroupLeaderComm);

	  	  	//---- CREATING META INFORMATION
	  	  	int buff_disp[interGroupSize], buff_cnt[interGroupSize]; int cntrB = 0;
	  	  	int rank_buff_disp[interGroupSize], rank_buff_cnt[interGroupSize]; int cntrR = 0;
	  	  	for(int i=0; i < interGroupSize; ++i){
	  	  		buff_disp[i] = cntrB; rank_buff_disp[i] = cntrR;
	  	  		buff_cnt[i] = send_sizes[i]*count; rank_buff_cnt[i] = send_sizes[i];
	  	  		cntrB +=  send_sizes[i]*count; cntrR += send_sizes[i];
	  	  	}  

	  	  	//---- FINAL GATHERING
	  	  	send_count = count*nodeGroupSize*intraGroupSize; recv_count = numProc*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gatherv(buff, send_count, MPI_DOUBLE, newbuff, buff_cnt, buff_disp, MPI_DOUBLE, 0, interGroupLeaderComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1*nodeGroupSize*intraGroupSize ; recv_count = numProc;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gatherv(ranks_arr, send_count, MPI_INT, nrank_arr, rank_buff_cnt, rank_buff_disp, MPI_INT, 0, interGroupLeaderComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(nrank_arr); free(newbuff);
	  	}
	  	double* recv_buff = (double*)malloc(sizeof(double)*numProc*count);
		
		// ---- getting everything in proper place
	  	if(rank<0){
	  		for(int i=0; i<numProc; ++i){
	  			int sindex = ranks_arr[i]*count;
	  			for(int j=0; j<count; ++j)
	  				recv_buff[j+sindex] = buff[i*count+j];
	  		}
	  	}	  	

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    		

    	// ---- checking correctness
	  	// int tflag = 0;
	  	// if(rank<0){
	  	// 	for(int i=0; i<numProc*count; ++i){
	  	// 		if ((i/count) == (int)recv_buff[i] || (i/count) == (int)((-1)*recv_buff[i]) )
	  	// 			continue;
	  	// 		else
	  	// 			tflag = 1;
	  	// 	}
	  	// 	if(tflag)
	  	// 		printf("PROBLEM\n");
    	
	  	// }
	  	// free(buff); free(recv_buff); free(ranks_arr);

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }
	}

	// if(!rank)
		// printf("Gather Optimized Avg: %lf\n", (total_time/(double)iter));
	
	return (total_time/(double)iter);
}

double MPI_Gather_opt_node_only(int count, int root, int iter){


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
	//---- MAKING THE GROUP LEVEL COMMUNICATOR FOR THE NODE LEADERS
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
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i)
  	{ 
	  	//---- INITIALIZING THE BUFFER SENT
	  	double *buff = (double*)malloc(sizeof(double)*count);
		int *ranks_arr = (int*)malloc(sizeof(int));
		ranks_arr[0] = rank;
		if(rank<0)
			ranks_arr[0] = (-1)*rank;
		for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	//---- GATHERING AT NODE LEVEL	
	  	sTime = MPI_Wtime();
	  	if( nodeGroupSize > 1){
	  		
	  		send_count = count; recv_count = nodeGroupSize*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gather(buff, send_count, MPI_DOUBLE, newbuff, send_count, MPI_DOUBLE, 0, nodeGroupComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1; recv_count = nodeGroupSize;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gather(ranks_arr, send_count, MPI_INT, nrank_arr, send_count, MPI_INT, 0, nodeGroupComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;


	    	// free(newbuff); free(nrank_arr);
	    }
	  	//---- GATHERING AT NODE LEADER LEVEL
	  	if( nodeGroupRank == 0 && interGroupSize>1){

	  		//---- FINAL GATHERING
	  	  	send_count = count*nodeGroupSize; recv_count = numProc*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
			MPI_Gather(buff, send_count, MPI_DOUBLE, newbuff, send_count, MPI_DOUBLE, 0, interGroupLeaderComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1*nodeGroupSize; recv_count = numProc;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gather(ranks_arr, send_count, MPI_INT, nrank_arr, send_count, MPI_INT, 0, interGroupLeaderComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(newbuff); free(nrank_arr);
	  	}
	  	double* recv_buff = (double*)malloc(sizeof(double)*numProc*count);
		
		// ---- getting everything in proper place
	  	if(rank<0){
	  		for(int i=0; i<numProc; ++i){
	  			int sindex = ranks_arr[i]*count;
	  			for(int j=0; j<count; ++j)
	  				recv_buff[j+sindex] = buff[i*count+j];
	  		}
	  	}	  	

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    		

    	// ---- checking correctness
	  	// int tflag = 0;
	  	// if(rank<0){
	  	// 	for(int i=0; i<numProc*count; ++i){
	  	// 		if ((i/count) == (int)recv_buff[i] || (i/count) == (int)((-1)*recv_buff[i]) )
	  	// 			continue;
	  	// 		else
	  	// 			tflag = 1;
	  	// 	}
	  	// 	if(tflag)
	  	// 		printf("PROBLEM\n");
	  	// 	// else
	  	// 		// printf("OK\n");
    	
	  	// }
	  	// free(buff); free(recv_buff); free(ranks_arr);

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }

	}

	// if(!rank)
		// printf("Gather Optimized Node lvl only Avg: %lf\n", (total_time/(double)iter));
	
	return (total_time/(double)iter);
}

double MPI_Gather_opt_group_only(int count, int root, int iter){


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
    if(!rank){
    	total_time += maxTime;   
    	// printf("communicator init time: %lf\n", maxTime);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i)
  	{ 
	  	//---- INITIALIZING THE BUFFER SENT
	  	double *buff = (double*)malloc(sizeof(double)*count);
		int *ranks_arr = (int*)malloc(sizeof(int));
		ranks_arr[0] = rank;
		if(rank<0)
			ranks_arr[0] = (-1)*rank;
		for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	//---- GATHERING AT NODE LEVEL	
	  	sTime = MPI_Wtime();
	  	//---- GATHERING AT GROUP LEVEL
	  	if( intraGroupSize > 1 ){

	  		send_count = count; recv_count = intraGroupSize*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gather(buff, send_count, MPI_DOUBLE, newbuff, send_count, MPI_DOUBLE, 0, intraGroupComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1 ; recv_count = intraGroupSize;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gather(ranks_arr, send_count, MPI_INT, nrank_arr, send_count, MPI_INT, 0, intraGroupComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(newbuff); free(nrank_arr);
	    }
	  	//---- GATHERING AT INTER-GROUP LEVEL
	  	if( intraGroupRank == 0 && interGroupSize>1){


	  	  	//---- GETTING THE SIZE OF THE SENDS
	  	  	int send_sizes[interGroupSize];
	  	  	int my_send_size = intraGroupSize; 
	  	  	MPI_Gather(&my_send_size, 1, MPI_INT, send_sizes, 1, MPI_INT, 0, interGroupLeaderComm);

	  	  	//---- CREATING META INFORMATION
	  	  	int buff_disp[interGroupSize], buff_cnt[interGroupSize]; int cntrB = 0;
	  	  	int rank_buff_disp[interGroupSize], rank_buff_cnt[interGroupSize]; int cntrR = 0;
	  	  	for(int i=0; i < interGroupSize; ++i){
	  	  		buff_disp[i] = cntrB; rank_buff_disp[i] = cntrR;
	  	  		buff_cnt[i] = send_sizes[i]*count; rank_buff_cnt[i] = send_sizes[i];
	  	  		cntrB +=  send_sizes[i]*count; cntrR += send_sizes[i];
	  	  	}  

	  	  	//---- FINAL GATHERING
	  	  	send_count = count*intraGroupSize; recv_count = numProc*count;

	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gatherv(buff, send_count, MPI_DOUBLE, newbuff, buff_cnt, buff_disp, MPI_DOUBLE, 0, interGroupLeaderComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1*intraGroupSize ; recv_count = numProc;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gatherv(ranks_arr, send_count, MPI_INT, nrank_arr, rank_buff_cnt, rank_buff_disp, MPI_INT, 0, interGroupLeaderComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(newbuff); free(nrank_arr);
	  	}
	  	double* recv_buff = (double*)malloc(sizeof(double)*numProc*count);
		
		// ---- getting everything in proper place
	  	if(rank<0){
	  		for(int i=0; i<numProc; ++i){
	  			int sindex = ranks_arr[i]*count;
	  			for(int j=0; j<count; ++j)
	  				recv_buff[j+sindex] = buff[i*count+j];
	  		}
	  	}	  	

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    		

    	// ---- checking correctness
	  	// int tflag = 0;
	  	// if(rank<0){
	  	// 	for(int i=0; i<numProc*count; ++i){
	  	// 		if ((i/count) == (int)recv_buff[i] || (i/count) == (int)((-1)*recv_buff[i]) )
	  	// 			continue;
	  	// 		else
	  	// 			tflag = 1;
	  	// 	}
	  	// 	if(tflag)
	  	// 		printf("PROBLEM\n");
	  		
    	
	  	// }
	  	// free(buff); free(recv_buff); free(ranks_arr);

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }

	}

	// if(!rank)
		// printf("Gather Optimized group level only Avg: %lf\n", (total_time/(double)iter));
	
	return (total_time/(double)iter);
}

double MPI_Gather_opt_rank_only(int count, int root, int iter){

	char hostname[MPI_MAX_PROCESSOR_NAME];
	double total_time = 0;

	double sTime = MPI_Wtime();
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
    total_time += maxTime;   
    
    MPI_Barrier(MPI_COMM_WORLD);

    int send_count, recv_count;
    //---- ALL THE COMMUNICATORS ARE MADE NOW THE MAIN GATHERING AT DIFFERENT LEVELS
  	for (int i=0; i < iter; ++i){//---- INITIALIZING THE BUFFER SENT
	  	double *buff = (double*)malloc(sizeof(double)*count);
		int *ranks_arr = (int*)malloc(sizeof(int));
		ranks_arr[0] = rank;
		if(rank<0)
			ranks_arr[0] = (-1)*rank;
		for(int i=0;i < count; ++i) buff[i] = (double) rand();
		MPI_Barrier(MPI_COMM_WORLD);	  		
	  	
	  	sTime = MPI_Wtime();
	  	//---- GATHERING
	  		
	  		send_count = count; recv_count = numProc*count;
	    	double* newbuff = (double*)malloc(sizeof(double)*recv_count);
	    	MPI_Gather(buff, send_count, MPI_DOUBLE, newbuff, send_count, MPI_DOUBLE, 0, newComm);
	    	double *temp = buff; buff = newbuff; newbuff = temp;

	    	send_count = 1 ; recv_count = numProc;

	    	int* nrank_arr = (int*)malloc(sizeof(int)*recv_count);
	    	MPI_Gather(ranks_arr, send_count, MPI_INT, nrank_arr, send_count, MPI_INT, 0, newComm);
	    	int* ntemp = nrank_arr; nrank_arr = ranks_arr; ranks_arr = ntemp;

	    	// free(newbuff); free(nrank_arr);
	    
	  	double* recv_buff = (double*)malloc(sizeof(double)*numProc*count);
		
		// ---- getting everything in proper place
	  	if(rank==root){
	  		for(int i=0; i<numProc; ++i){
	  			int sindex = ranks_arr[i]*count;
	  			for(int j=0; j<count; ++j)
	  				recv_buff[j+sindex] = buff[i*count+j];
	  		}
	  	}	  	

    	eTime = MPI_Wtime() - sTime;
    	MPI_Reduce(&eTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    		

    	// ---- checking correctness
	  	// int tflag = 0;
	  	// if(rank==root){
	  	// 	// printf("hi\n");
	  	// 	for(int i=0; i<numProc*count; ++i){
	  	// 		if ((i/count) == (int)recv_buff[i] || (i/count) == (int)((-1)*recv_buff[i]) )
	  	// 			continue;
	  	// 		else
	  	// 			tflag = 1;
	  	// 	}
	  	// 	if(tflag)
	  	// 		printf("PROBLEM\n");
	  		
    	
	  	// }
	  	// free(buff); free(recv_buff); free(ranks_arr);

    	MPI_Barrier(MPI_COMM_WORLD);
	    if(!rank){
	    	total_time += maxTime;
    		// printf("%lf\n", maxTime);
	    }

	}

	
	return (total_time/(double)iter);
}

