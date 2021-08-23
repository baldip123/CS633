#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long  get_pos(long r, long c, long total_col){
    return (r*total_col + c);
}

void CSV_line_getfields(char* line, float* buffer_arr)
{
    const char* tok;
    int index = 0;
    for (tok = strtok(line, ","); tok && *tok; tok = strtok(NULL, ","))
    {
    	buffer_arr[index] = strtof(tok, NULL);
    	++index;
    }
    return;
}

int CSV_line_getnumfields(char* line)
{
    const char* tok;
    int index = 0;
    for (tok = strtok(line, ","); tok && *tok; tok = strtok(NULL, ","))
    {
    	++index;
    }
    return index;
}

int get_num_fields_in_file(char* filename){
    FILE *fptr;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fptr = fopen(filename,"r");
    // try to read the first line
    read = getline(&line, &len, fptr);
    // get the number of fields in the first line 
    int num_fields = CSV_line_getnumfields(line);
    if(read == -1){
        printf("First line does not exists. Exiting without doing anything...\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    free(line);
    fclose(fptr);

    return num_fields;
}

int get_num_lines_in_file(char* filename){
    FILE *fptr;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fptr = fopen(filename,"r");
    int num_lines = 0;
    // try to read the first line
    read = getline(&line, &len, fptr);
    // get the number of fields in the first line 
    int num_fields = CSV_line_getnumfields(line);
    if(read == -1){
        printf("First line does not exists. Exiting without doing anything...\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    free(line);
    line = NULL; 
    len = 0;
    while((read = getline(&line, &len, fptr)) != -1){
        // checking if this line has the same number of columns
        if(num_fields != CSV_line_getnumfields(line)){
            printf("Bad File. Inconsistent number of columns on line %d. Exiting without doing anything...\n", num_lines);
            free(line);
            exit(EXIT_FAILURE);
            return -1;
        }
        free(line);
        ++num_lines;
        line = NULL; 
        len = 0;
    }
    fclose(fptr);

    return num_lines;
}

void read_the_file_and_fill_the_buffer(char* filename, float * buffer_arr, float* col_data, float* row_data, int num_lines, int num_fields){
    FILE *fptr;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fptr = fopen(filename,"r");
    int line_num = 0;
    read = getline(&line, &len, fptr);
    if(read == -1){
        printf("First line does not exists. Exiting without doing anything...\n");
        exit(EXIT_FAILURE);
        return ;
    }
    free(line);
    line = NULL; 
    len = 0;
    while((read = getline(&line, &len, fptr)) != -1){
        CSV_line_getfields(line, buffer_arr+line_num*num_fields);
        //may need to free the line here
        free(line);
        ++line_num;
        line = NULL; 
        len = 0;
    }
    fclose(fptr);
    //row-wise
    long num_rows = num_lines; 
    long num_cols = num_fields-2;
    // data in row-wise format
        for(long i=0; i<num_lines; ++i){
            for(long j=0; j<num_fields; ++j){
                if(j==0 || j==1)
                    continue;
                row_data[get_pos(i,j-2,num_cols)] = buffer_arr[get_pos(i,j,num_fields)]; 
            }
        }
    //data in col-wise fomat
    for(long i=0; i<num_lines; ++i){
        for(long j=0; j<num_fields; ++j){
            if(j==0 || j==1)
                continue;
            col_data[get_pos(j-2,i,num_rows)] = buffer_arr[get_pos(i,j,num_fields)];
        }
    }
}


void write_to_file(float* final_ans, long num_cols, double time){
    
    FILE *fp = fopen("output.txt", "w");

    for(int i=0; i < num_cols; ++i){
        fprintf(fp, "%.2f", final_ans[i]);
        if( i != (num_cols-1) )
            fprintf(fp , ",");
        else
            fprintf(fp,"\n" );
    }

    fprintf(fp, "%.2f\n", final_ans[num_cols]);
    fprintf(fp, "%lf\n", time);

    return;
}