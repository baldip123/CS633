#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"
#include "file_r&w.h"




float min(float num1, float num2) 
{
    return (num1 > num2 ) ? num2 : num1;
}

void find_the_min_for_all_years(float* buffer_arr_for_this_proc, 
    int num_fields, int num_lines_for_this_proc, float* min_for_every_year){
    // talk to someone how to optimize this loop
    // printf("entered min function\n");
    for(int i = 0; i < num_fields-2; i++){
        min_for_every_year[i] = buffer_arr_for_this_proc[(i+2)];
        // printf("%lf\n", min_for_every_year[i]);
        for(int j = 0; j < num_lines_for_this_proc; j++){
            min_for_every_year[i] = min(min_for_every_year[i], buffer_arr_for_this_proc[j*num_fields + (i+2)]);
            // printf("%lf\n", min_for_every_year[i]);
        }
    }
    // printf("exit min function\n");
}

int main(int argc, char * argv[]){

    //initialize MPI
    MPI_Init ( &argc, &argv);

    //get the rank and the number of processes
    int myrank;
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank );

    int numProc;
    MPI_Comm_size( MPI_COMM_WORLD, &numProc );


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
        int num_fields = get_num_fields_in_file(filename);
        // this does not include the line with the headinngs
        int num_lines = get_num_lines_in_file(filename);
        // printf("num_fields: %d, num_lines: %d\n", num_fields, num_lines);

        //allocate a buffer, to store the data
        float* buffer_arr = (float*)malloc(sizeof(float)*num_fields*num_lines);
        // reading the file in line by line fashion
        read_the_file_and_fill_the_buffer(filename, buffer_arr, num_lines, num_fields);
        // printf("printinf buffer....\n");
        // for(int i = 0;  i< num_lines; i++){
        //     for(int j = 0; j < num_fields; j++){
        //         printf("%lf,", *(buffer_arr+i*num_fields+j));
        //     }
        //     printf("\n");
        // }
        // printf("printed buffer....\n");

        //call barrier to synchronize the timing
        MPI_Barrier(MPI_COMM_WORLD);

        double sTime = MPI_Wtime();

        //1. broadcast the number of columns
        MPI_Bcast(&num_fields, 1, MPI_INT, 0, MPI_COMM_WORLD);
        //2. scatter the number of rows to be received by each process so that they can initialize the 
        //      buffers accordingly
        int num_lines_for_this_proc = num_lines / numProc;
        MPI_Bcast(&num_lines_for_this_proc, 1, MPI_INT, 0, MPI_COMM_WORLD);
        // put the remaining data in the first process
        // printf("num_lines_for_this_proc : %d\n", num_lines_for_this_proc);

        //3. distribute the data with scatter, lets say that there are 15 line, and 5 proc then the all
        // will have 15 /4 =  3  lines, and the mod is 3 remaining lines, and the first 3 lines are put 
        //into the first proc's buffer
        num_lines_for_this_proc += (num_lines%numProc);
        float *buffer_arr_for_this_proc = (float*)malloc(sizeof(float)*num_fields*num_lines_for_this_proc);
        //copy the data in this buffer
        for(int i =0 ; i < num_fields*((num_lines%numProc)); i++){
            buffer_arr_for_this_proc[i] = buffer_arr[i];
        }

        if(num_lines / numProc > 0){
            MPI_Scatter(buffer_arr + num_fields*((num_lines%numProc)), (num_lines/numProc)*num_fields, MPI_FLOAT,
                buffer_arr_for_this_proc + num_fields*((num_lines%numProc)), 
                (num_lines/numProc)*num_fields, MPI_FLOAT, 0, MPI_COMM_WORLD);

            // for(int i =0; i < num_lines_for_this_proc; i ++){
            //     for(int j = 0; j < num_fields; j++){
            //         printf("%lf,", *(buffer_arr_for_this_proc+i*num_fields+j));
            //     }
            //     printf("\n");
            // }


            //4. find the minimum of the buffers
            float min_for_every_year[num_fields - 2];
            find_the_min_for_all_years(buffer_arr_for_this_proc, num_fields, num_lines_for_this_proc, min_for_every_year);
            // printf("printing min_for_this_proc..\n");
            // for(int i = 0; i < num_fields -2; i ++){
            //     printf("%lf,", min_for_every_year[i]);
            // }
            // printf("printed min_for_this_proc..\n");

            //5. reduce the data
            float min_for_every_year_accross_all_proc[num_fields-2];
            MPI_Reduce(min_for_every_year, min_for_every_year_accross_all_proc, num_fields-2, 
                MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);

            //6. print the answers
            float global_min_temp = min_for_every_year_accross_all_proc[0];
            for(int i = 0; i < num_fields -2; i ++){
                printf("%.2f", min_for_every_year_accross_all_proc[i]);
                global_min_temp = min(global_min_temp, min_for_every_year_accross_all_proc[i]);
                if(i != num_fields -3){
                    printf(",");
                }
            }
            printf("\n");
            printf("%.2f\n", global_min_temp);
        }
        else{
            //4. find the minimum of the buffers
            float min_for_every_year[num_fields - 2];
            find_the_min_for_all_years(buffer_arr_for_this_proc, num_fields, num_lines_for_this_proc, min_for_every_year);
            // printf("printing min_for_this_proc..\n");
            // for(int i = 0; i < num_fields -2; i ++){
            //     printf("%lf,", min_for_every_year[i]);
            // }
            // printf("printed min_for_this_proc..\n");

            //6. print the answers, and reduce is not required as this is the only process which has data
            float global_min_temp = min_for_every_year[0];
            for(int i = 0; i < num_fields -2; i ++){
                printf("%lf", min_for_every_year[i]);
                global_min_temp = min(global_min_temp, min_for_every_year[i]);
                if(i != num_fields -3){
                    printf(",");
                }
            }
            //prinf overall minimun
            printf("\n");
            printf("%lf\n", global_min_temp);
        }

        double eTime = MPI_Wtime();
        double time = eTime - sTime;
        double maxTime;
		MPI_Reduce(&time, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        printf("%f\n", maxTime);
    }
    else{
        //not the root process we will have receive the data here
        
        //call barrier to synchronize the timing
        MPI_Barrier(MPI_COMM_WORLD);

        double sTime = MPI_Wtime();

        //1. broadcast the number of columns
        int num_fields;
        MPI_Bcast(&num_fields, 1, MPI_INT, 0, MPI_COMM_WORLD);
        //2. scatter the number of rows to be received by each process so that they can initialize the 
        //      buffers accordingly
        int num_lines_for_this_proc;
        MPI_Bcast(&num_lines_for_this_proc, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        if(num_lines_for_this_proc > 0){
            //3. initialize the buffer to receive the data
            float *buffer_arr_for_this_proc = (float*)malloc(sizeof(float)*num_fields*num_lines_for_this_proc);
            MPI_Scatter(NULL, 0, MPI_FLOAT,
                    buffer_arr_for_this_proc , 
                    num_lines_for_this_proc*num_fields, MPI_FLOAT, 0, MPI_COMM_WORLD);
            // for(int i =0; i < num_lines_for_this_proc; i ++){
            //     for(int j = 0; j < num_fields; j++){
            //         printf("%lf,", *(buffer_arr_for_this_proc+i*num_fields+j));
            //     }
            //     printf("\n");
            // }
            
            //4. find the minimum of the buffers
            float min_for_every_year[num_fields - 2];
            find_the_min_for_all_years(buffer_arr_for_this_proc, num_fields, num_lines_for_this_proc, min_for_every_year);
            // printf("printing min_for_this_proc..\n");
            // for(int i = 0; i < num_fields -2; i ++){
            //     printf("%lf,", min_for_every_year[i]);
            // }
            // printf("printed min_for_this_proc..\n");

            //5. reduce the data
            MPI_Reduce(min_for_every_year, NULL, num_fields-2, MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);
        }

        double eTime = MPI_Wtime();
        double time = eTime - sTime;
		MPI_Reduce(&time, NULL, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        
    }

    //done with MPI
    MPI_Finalize();


	return 0;
}
