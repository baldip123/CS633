#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "mpi.h"
#include "file_r&w.h"

// Look at file_r&w.h for the funcitons on file reading and writing

float min(float num1, float num2) 
{
    return (num1 > num2 ) ? num2 : num1;
}

// gives the min of the array[start_index ---- start_index+size]
float min_of_array(float* arr, long start_index, long size);
// method where the coloumns are communicated to get the result
float* col_comm_normal(float* col_data, long num_rows, long num_cols, long myrank, long numProc);
// method where the rows are communicated to get teh results
float* row_comm_normal(float* row_data, long num_rows, long num_cols, long myrank, long numProc);
// row communication but with parallel sends to process with rank (myrank + numProc/2)
float* row_parallel_send(float* row_data, long num_rows, long num_cols, long myrank, long numProc);
// Simple send implementation where first data is tranfered to a rank at different node and then 
float* naive_send(float* row_data, long num_rows, long num_cols, long myrank, long numProc);

int main(int argc, char * argv[]){	
	//initialize MPI
    MPI_Init ( &argc, &argv);
    //get the rank and the number of processes
    int myrank;
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
    //get the number of processors
    int numProc;
    MPI_Comm_size( MPI_COMM_WORLD, &numProc );

    long num_rows = -1;
    long num_cols = -1;
    float* buffer_arr; float* row_data; float* col_data;
    float* one_proc_ans;float* row_ans;float* col_ans;
    double sTime, time, maxTime;
    if(myrank == 0){
        // read the file
        char filename[FILENAME_MAX];
        if(!(argc >= 2)){
            printf("Incorrect number of arguments. Exiting without doing anything...\n");
            return -1;
        }
        strcpy(filename, argv[1]);

        //The following segment of code does the following things:
        //1. count the number of lines(number of lines with data, the top row with just fields is not included) in the file
        //2. count the number of column in each line and check that each line has the same number
        long num_fields = get_num_fields_in_file(filename);
        // this does not include the line with the headinngs
        long num_lines = get_num_lines_in_file(filename);
        // dims = {row,col}
        num_rows = num_lines; 
        num_cols = num_fields-2;
        //allocate a buffer, to store the data
        buffer_arr = (float*)malloc(sizeof(float)*num_fields*num_lines);
        row_data = (float*)malloc(sizeof(float)*num_rows*num_cols);
        col_data = (float*)malloc(sizeof(float)*num_rows*num_cols);
        // reading the file in line by line fashion
        read_the_file_and_fill_the_buffer(filename, buffer_arr, col_data, row_data, num_lines, num_fields);
        //getting the actual answer for correctness check
        one_proc_ans =  (float*)malloc(sizeof(float)*(num_cols+1));
        for(long i=0; i<num_cols; ++i)
        	one_proc_ans[i] = min_of_array(col_data, i*num_rows, num_rows);
        one_proc_ans[num_cols] = min_of_array(one_proc_ans, 0, num_cols);


    }
    // the actual execution
  	double colTime = 0;

    
    MPI_Barrier(MPI_COMM_WORLD);
    sTime = MPI_Wtime();
	col_ans = col_comm_normal(col_data, num_rows, num_cols, myrank, numProc);
	time = MPI_Wtime() - sTime;
	MPI_Reduce(&time, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	
	if(myrank == 0){
		write_to_file(col_ans, num_cols, maxTime);
		for(int i=0; i<num_cols+1; ++i){
			if(one_proc_ans[i] != col_ans[i]){
				printf("PROBLEM\n");
				break;
			}
		}
	}
	

    MPI_Finalize();
	return 0;
}

float min_of_array(float* arr, long start_index, long size){
	float ans = FLT_MAX;
	for(long i=start_index; i<start_index+size; ++i)
		ans = min(arr[i], ans);
	return ans;
}

float* col_comm_normal(float* col_data, long num_rows, long num_cols, long myrank, long numProc){
	// number of rows an cols of the original matrix
	long dims[2];
	if(myrank == 0 ){ dims[0] = num_rows; dims[1] = num_cols;}
	//broadcating the dimensions
	MPI_Bcast(&dims, 2, MPI_LONG, 0, MPI_COMM_WORLD);
	num_rows = dims[0];
	num_cols = dims[1];
	//preparing buffer to be recieved
	long cols_2be_recved = num_cols/numProc;
	long recv_count =  (cols_2be_recved)*num_rows;
	float* recv_buff = (float*)malloc(sizeof(float)*recv_count);
	//the extra coloumns that will be just computed by the 0 rank
	long cols_left = num_cols - cols_2be_recved*numProc;
	long offset = cols_left*num_rows;
	// the scatter
	MPI_Request req_flag;
	MPI_Iscatter(col_data + offset, recv_count, MPI_FLOAT, recv_buff, recv_count, MPI_FLOAT, 0, MPI_COMM_WORLD, &req_flag);
	//computing the ans of extra cols
	float* final_ans;
	if(myrank == 0){
		final_ans = (float*)malloc(sizeof(float)*(num_cols+1));
		for(long i=0; i < cols_left; ++i)
			final_ans[i] = min_of_array(col_data, i*num_rows, num_rows);
	} 
	// computing the mins of the row for the cols recieved
	float* proc_mins = (float*)malloc(sizeof(float)*cols_2be_recved);
	MPI_Wait(&req_flag, MPI_STATUS_IGNORE);
	for(long i=0; i<cols_2be_recved; ++i){
		proc_mins[i] = min_of_array(recv_buff, i*num_rows, num_rows);
	};
	//gathering the mins that were computed by the other processes
	MPI_Gather(proc_mins, cols_2be_recved, MPI_FLOAT, final_ans+cols_left, cols_2be_recved, MPI_FLOAT, 0, MPI_COMM_WORLD);
	if(myrank == 0){
		final_ans[num_cols] = min_of_array(final_ans, 0, num_cols);
	}

	//computing global minima and prlonging
	return final_ans;
}	


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