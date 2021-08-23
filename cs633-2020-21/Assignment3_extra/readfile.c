#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// gives the ith field of the csv line
// NOTE: copied from stack-overflow
void CSV_line_getfields(char* line, double* buffer_arr)
{
    const char* tok;
    int index = 0;
    for (tok = strtok(line, ","); tok && *tok; tok = strtok(NULL, ","))
    {
    	buffer_arr[index] = strtod(tok, NULL);
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


int main(int argc, char * argv[]){

	FILE *fptr;
	char *line = NULL;
    char filename[FILENAME_MAX];
	size_t len = 0;
	ssize_t read;

    if(argc != 2){
        printf("Incorrect number of arguments. Exiting without doing anything...\n");
        return -1;
    }
    strcpy(filename, argv[1]);

	//The following segment of code does the following things:
    //1. count the number of lines(number of lines with data, the top row with just fields is not included) in the file
    //2. count the number of column in each line and check that each line has the same number
	fptr = fopen(filename,"r");
	int num_lines = 0;
    // try to read the first line
    read = getline(&line, &len, fptr);
    // get the number of fields in the first line 
    int num_fields = CSV_line_getnumfields(line);
    if(read == -1){
        printf("First line does not exists. Exiting without doing anything...\n");
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
            return -1;
        }
        free(line);
		++num_lines;
        line = NULL; 
        len = 0;
	}
    fclose(fptr);

    //allocate a buffer, to store the data
    double* buffer_arr = (double*)malloc(sizeof(double)*num_fields*num_lines);

    // reading the file in line by line fashion
	fptr = fopen(filename,"r");
	int line_num = 0;
    read = getline(&line, &len, fptr);
    if(read == -1){
        printf("First line does not exists. Exiting without doing anything...\n");
        return -1;
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

	for(int i=0;i<num_lines; ++i){
        printf("%d\n", i);
		for(int j=0;j<num_fields;++j)
			printf("%lf,",*(buffer_arr+i*num_fields+j));
        printf("\n");
	}
	return 0;
}
