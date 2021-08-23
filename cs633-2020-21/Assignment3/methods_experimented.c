#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"
// #include "file_r&w.h"


//row normal communication method described in the report
float* row_comm_normal(float* row_data, long num_rows, long num_cols, long myrank, long numProc){
	// number of rows an cols of the original matrix
	long dims[2];
	if(myrank == 0 ){ dims[0] = num_rows; dims[1] = num_cols;}
	//broadcating the dimensions
	MPI_Bcast(&dims, 2, MPI_LONG, 0, MPI_COMM_WORLD);
	num_rows = dims[0];
	num_cols = dims[1];
	//preparing buffer to be recieved
	long rows_2be_recved = num_rows/numProc;
	long recv_count = rows_2be_recved*num_cols; 
	float* recv_buff = (float*)malloc(sizeof(float)*recv_count);
	//extra rows will be computed by the 0 rank
	long rows_left = num_rows - rows_2be_recved*numProc;
	long offset = rows_left*num_cols;
	// the scatter
	MPI_Request req_flag;
	MPI_Iscatter(row_data + offset, recv_count, MPI_FLOAT, recv_buff, recv_count, MPI_FLOAT, 0, MPI_COMM_WORLD, &req_flag);
	//preparing the buffer to be reduced;
	float* reduce_buff = (float*)malloc(sizeof(float)*(num_cols+1));
	for(long i=0; i<= num_cols; ++i) reduce_buff[i] = FLT_MAX;
	if(myrank == 0){
		for(long i=0; i<rows_left; ++i){
			for(long j=0; j<num_cols; ++j)
				reduce_buff[j] = min(reduce_buff[j], row_data[get_pos(i,j,num_cols)]);
		}
	}
	// getting the min of each col of the recieved buffer
	MPI_Wait(&req_flag, MPI_STATUS_IGNORE);
	for(long i=0; i<rows_2be_recved; ++i){
		for(long j=0; j<num_cols; ++j)
			reduce_buff[j] = min(reduce_buff[j], recv_buff[get_pos(i,j,num_cols)]);
	}
	reduce_buff[num_cols] = min_of_array(reduce_buff, 0, num_cols);
	// reducing the locally computed mins 
	float* final_ans = (float*)malloc(sizeof(float)*(num_cols+1));
	MPI_Reduce(reduce_buff, final_ans, num_cols+1,  MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);
	
	return final_ans;
}

//send to a rank of different node and then normal inter node send
float* naive_send(float* row_data, long num_rows, long num_cols, long myrank, long numProc){
	/// number of rows an cols of the original matrix
	long dims[2];
	if(myrank == 0 ){ dims[0] = num_rows; dims[1] = num_cols;}
	//broadcating the dimensions
	MPI_Bcast(&dims, 2, MPI_LONG, 0, MPI_COMM_WORLD);
	num_rows = dims[0];
	num_cols = dims[1];
	//preparing the final buffer to be recieved
	long rows_2be_recved = num_rows/numProc;
	long recv_count = rows_2be_recved*num_cols;
	float* recv_buff = (float*)malloc(sizeof(float)*recv_count);
	//implementaiton 
	if(myrank == numProc/2)
		recv_buff = (float*)malloc(sizeof(float)*recv_count*numProc/2);
	//extra rows will be computed by the 0 rank
	long rows_left = num_rows - (rows_2be_recved)*numProc;
	long offset = rows_left*num_cols;
	if(myrank == 0)
		recv_buff = row_data + offset; // just for ease
	//internode 0->numProc/2 send
	MPI_Request req_flag;
	double sTime = MPI_Wtime();
	if(myrank == 0){
		MPI_Send(recv_buff + recv_count*numProc/2, recv_count*numProc/2, MPI_FLOAT, numProc/2, 1, MPI_COMM_WORLD );
	}
	if(myrank == (numProc/2))
		MPI_Recv(recv_buff, recv_count*numProc/2, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	double eTime = MPI_Wtime() - sTime;
	// The sends from 0 and numProc/2
	if(myrank == 0 || myrank ==(numProc/2)){
		for(int i=1; i < numProc/2; ++i){
			offset = i*recv_count;
			MPI_Send(recv_buff+i*recv_count, recv_count, MPI_FLOAT, myrank+i, 1, MPI_COMM_WORLD);
		}
	}
	else if(myrank < numProc/2)
		MPI_Recv(recv_buff, recv_count, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	else
		MPI_Recv(recv_buff, recv_count, MPI_FLOAT, numProc/2, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	eTime = MPI_Wtime() - sTime;

	//final computation
	float* reduce_buff = (float*)malloc(sizeof(float)*(num_cols+1));
	for(long i=0; i<= num_cols; ++i) reduce_buff[i] = FLT_MAX;
	if(myrank == 0){
		for(long i=0; i<rows_left; ++i){
			for(long j=0; j<num_cols; ++j)
				reduce_buff[j] = min(reduce_buff[j], row_data[get_pos(i,j,num_cols)]);
		}
	}
	for(long i=0; i<rows_2be_recved; ++i){
		for(long j=0; j<num_cols; ++j)
			reduce_buff[j] = min(reduce_buff[j], recv_buff[get_pos(i,j,num_cols)]);
	}
	reduce_buff[num_cols] = min_of_array(reduce_buff, 0, num_cols);
	// reducing the locally computed mins 
	float* final_ans = (float*)malloc(sizeof(float)*(num_cols+1));
	MPI_Reduce(reduce_buff, final_ans, num_cols+1,  MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);
	return final_ans;
}

//the parallel send method described in the report
float* row_2_node(float* row_data, long num_rows, long num_cols, long myrank, long numProc){
	/// number of rows an cols of the original matrix
	long dims[2];
	if(myrank == 0 ){ dims[0] = num_rows; dims[1] = num_cols;}
	//broadcating the dimensions
	MPI_Bcast(&dims, 2, MPI_LONG, 0, MPI_COMM_WORLD);
	num_rows = dims[0];
	num_cols = dims[1];
	//preparing the final buffer to be recieved
	long rows_2be_recved = num_rows/numProc;
	long recv_count = rows_2be_recved*num_cols;
	if(myrank < numProc/2)
		recv_count *= 2;
	float* recv_buff = (float*)malloc(sizeof(float)*recv_count);
	//extra rows will be computed by the 0 rank
	long rows_left = num_rows - (rows_2be_recved)*numProc;
	long offset = rows_left*num_cols;
	// the first scattering
	MPI_Request req_flag;
	int send_count[numProc];
	int disp[numProc];
	if(myrank == 0){
		int dummy = 0;
		for(int i=0;i<numProc;++i){
			if(i<numProc/2)
				send_count[i] = recv_count;
			else
				send_count[i] = 0;
			disp[i] = dummy;
			dummy += send_count[i];
		}
		MPI_Iscatterv(row_data+offset, send_count, disp, MPI_FLOAT, recv_buff, recv_count, MPI_FLOAT, 0, MPI_COMM_WORLD, &req_flag);
	}
	else if(myrank<numProc/2)
		MPI_Iscatterv(row_data+offset, send_count, disp, MPI_FLOAT, recv_buff, recv_count, MPI_FLOAT, 0, MPI_COMM_WORLD, &req_flag);	
	else
		MPI_Iscatterv(row_data+offset, send_count, disp, MPI_FLOAT, recv_buff, 0, MPI_FLOAT, 0, MPI_COMM_WORLD, &req_flag);
	//preparing the buffer to be reduced;
	float* reduce_buff = (float*)malloc(sizeof(float)*(num_cols+1));
	for(long i=0; i<= num_cols; ++i) reduce_buff[i] = FLT_MAX;
	if(myrank == 0){
		for(long i=0; i<rows_left; ++i){
			for(long j=0; j<num_cols; ++j)
				reduce_buff[j] = min(reduce_buff[j], row_data[get_pos(i,j,num_cols)]);
		}
	}
	// if(myrank == 0){
	// 	for(int i=0; i<numProc; ++i)
	// 		printf("%d\n",send_count[i]);
	// }
	MPI_Wait(&req_flag, MPI_STATUS_IGNORE);
	MPI_Request req_flag_2;
	if(myrank < numProc/2)
		MPI_Isend(recv_buff + recv_count/2, recv_count/2, MPI_FLOAT, myrank+(numProc/2), 0, MPI_COMM_WORLD, &req_flag_2);
	else{
		MPI_Irecv(recv_buff, recv_count, MPI_FLOAT, myrank-(numProc/2), 0, MPI_COMM_WORLD, &req_flag_2);
		MPI_Wait(&req_flag_2, MPI_STATUS_IGNORE);
	}
	// getting the min of each col of the recieved buffer
	for(long i=0; i<rows_2be_recved; ++i){
		for(long j=0; j<num_cols; ++j)
			reduce_buff[j] = min(reduce_buff[j], recv_buff[get_pos(i,j,num_cols)]);
	}

	reduce_buff[num_cols] = min_of_array(reduce_buff, 0, num_cols);
	// reducing the locally computed mins 
	float* final_ans = (float*)malloc(sizeof(float)*(num_cols+1));
	MPI_Reduce(reduce_buff, final_ans, num_cols+1,  MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);

	return final_ans;
}