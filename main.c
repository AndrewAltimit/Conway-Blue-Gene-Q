/***************************************************************************/
/* Andrew Kenneth Showers     **********************************************/
/***************************************************************************/

/***************************************************************************/
/* Includes ****************************************************************/
/***************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<math.h>
#include <pthread.h>

#include<clcg4.h>
#include<mpi.h>


/***************************************************************************/
/* Defines *****************************************************************/
/***************************************************************************/

#define ALIVE 1
#define DEAD  0

/***************************************************************************/
/* Global Vars *************************************************************/
/***************************************************************************/

// command line arguments
int world_width;
int world_height;
int num_ticks;
float threshold; // input is an int, divided by 100 to get as decimal
int num_threads;
// world height evenly divided amongst ranks
int rows_per_rank;
int starting_row;
int ending_row;
double start_execution_time;
double end_execution_time;
// Communication Variables
int exchange_rank_a; // rank with rows above you
int exchange_rank_b; // rank with rows beneath you
int mpi_myrank;

// World View -> 2 Copies, previous tick and current tick. Pointers swap in loop
int** data;
int** new_data;

pthread_barrier_t barrier; // barrier synchronization object

int get_digits_int(int number){
    int counter = number;
    int digits = 0;
    while(counter)
    {
        counter /= 10;
        digits++;
    }
    return digits;
}

// Swap the world view arrays
void swap_arrays(int** data, int** new_data){
  int *temp = *data;
  *data = *new_data;
  *new_data = temp;
}


// Simulation -> each thread will run this
void* simulation( void * arg ) {
    // Get passed thread ID number
    int* threadid = (int *)arg;

    //printf("[Rank %d] [Thread %d]: Running\n", mpi_myrank,*threadid);
    
    // Determine what rows to process
    int num_rows = rows_per_rank / num_threads;
    int row_offset = *threadid * num_rows;
    
    //printf("[Rank %d] [Thread %d]: Reading local rows %d to %d\n", mpi_myrank, *threadid, row_offset, row_offset + num_rows);
    
    
    // ========= SIMULATION BEGINS HERE =========

    int neighbors_alive;
    int i, r, c, x;
    for( i = 0; i < num_ticks; i++){
        
        // DATA EXCHANGE FOR ALL THREADS HANDLED BY THREAD 0
        if (*threadid == 0){
            // Swap pointers
            swap_arrays(data, new_data);
            
            // ========= DATA EXCHANGE =========
            // SEND
            MPI_Status status;
            MPI_Request request1, request2, request3, request4;
            MPI_Isend(data[1], world_width, MPI_INT, exchange_rank_a, 0, MPI_COMM_WORLD, &request1);
            MPI_Isend(data[rows_per_rank], world_width, MPI_INT, exchange_rank_b, 0, MPI_COMM_WORLD, &request2);
        
            // RECEIVE (save in ghost data rows)
            MPI_Irecv(data[0], world_width, MPI_INT, exchange_rank_a, 0, MPI_COMM_WORLD, &request3);
            MPI_Wait(&request3, &status);
            MPI_Irecv(data[rows_per_rank + 1], world_width, MPI_INT, exchange_rank_b, 0, MPI_COMM_WORLD, &request4);
            MPI_Wait(&request4, &status);
        }
        
        // Wait main thread (thread 0) to receive new data
        pthread_barrier_wait(&barrier);
        

        // PROCESS CELLS
        for (r = row_offset + 1; r < row_offset + num_rows + 1; r++) {  // Process each row
            for (c = 0; c < world_width; c++) {    // Process each column
                
                // Generate Random Value:  if above threshold, use 4 basic game rules  if below, randomly choose Dead / Alive
                x = GenVal(r - 1);
    
                if (x <= threshold) {
                    // Less than or equal to threshold, DO NOT APPLY GAME RULES. Randomly assign Dead / Alive
                    x = GenVal(r - 1);
                    if (x >= 0.5) {
                        new_data[r][c] = ALIVE;
                    } else {
                        new_data[r][c] = DEAD;
                    }
                    
                } else {
                    // Greater than threshold, APPLY GAME RULES
                    if (c == 0) {
                        // SPECIAL CASE: First Cell
                        neighbors_alive = data[r-1][world_width-1] + data[r][world_width-1] + data[r+1][world_width-1] + data[r-1][0] + data[r+1][0] + data[r-1][1] + data[r][1] + data[r+1][1];
                    } else if (c == world_width - 1) {
                        // SPECIAL CASE: Last Cell
                        neighbors_alive = data[r-1][world_width-2] + data[r][world_width-2] + data[r+1][world_width-2] + data[r-1][world_width-1] + data[r+1][world_width-1] + data[r-1][0] + data[r][0] + data[r+1][0];
                    } else {
                        // GENERAL CASE: inbetween cells
                        neighbors_alive = data[r-1][c-1] + data[r][c-1] + data[r+1][c-1] + data[r-1][c] + data[r+1][c] + data[r-1][c+1] + data[r][c+1] + data[r+1][c+1];
                    }
                    
                    // Enact 4 Game Rules
                    if (data[r][c] == ALIVE){
                        // Currently Alive Cell
                        if (neighbors_alive < 2) {
                            new_data[r][c] = DEAD;
                        } else if ((neighbors_alive == 2) ||  (neighbors_alive == 3)) {
                            new_data[r][c] = ALIVE;
                        } else {
                            new_data[r][c] = DEAD;
                        }
                    } else {
                        // Currently Dead Cell
                        if (neighbors_alive == 3){
                            new_data[r][c] = ALIVE;
                        }
                    }
                }
            }
        }
        
        // Wait for all threads to finish processing their respective rows
        pthread_barrier_wait(&barrier);
        
    }
    
    return NULL;
}


/***************************************************************************/
/* Function: Main **********************************************************/
/***************************************************************************/

int main(int argc, char *argv[])
{
    
    // get additional arguments (world width, world height, number of ticks, threshold)
    if (argc == 6) {
        num_threads = atoi(argv[1]);
        world_width = atoi(argv[2]);
        world_height = atoi(argv[3]);
        num_ticks = atoi(argv[4]);
        threshold = (atoi(argv[5])) / 100.00;
    } else {
        // improper number of arguments
        fprintf(stderr, "ERROR: Invalid arguments\n");
        fprintf(stderr, "USAGE: mpirun -n <number of threads> <number of ranks> ./assignment3 <world width> <world height> <number of ticks> <threshold (25%% is 25>\n");
        fflush(NULL);
        return EXIT_FAILURE;
    }
    
    
    int mpi_commsize;
    // Example MPI startup and using CLCG4 RNG
    MPI_Init( &argc, &argv);
    MPI_Comm_size( MPI_COMM_WORLD, &mpi_commsize);
    MPI_Comm_rank( MPI_COMM_WORLD, &mpi_myrank);
    
    
    // Init 16,384 RNG streams - each rank has an independent stream
    InitDefault();

    //printf("Rank %d of %d has been started and a first Random Value of %lf\n", mpi_myrank, mpi_commsize, GenVal(mpi_myrank));
    MPI_Barrier( MPI_COMM_WORLD );
    
    // ========= BEGIN TIMER =========
    if (mpi_myrank == 0){
        start_execution_time = MPI_Wtime();
    }
    
    
    // evenly divide number of rows to each rank (assumes it is an even division!)
    rows_per_rank = world_height / mpi_commsize;
    starting_row = mpi_myrank * rows_per_rank;
    ending_row = (mpi_myrank + 1) * rows_per_rank - 1;
    
    // ========= ALLOCATE MEMORY FOR WORLD VIEW =========
    // rows_per_rank + 2 (extended by 2 due to ghost rows that will be imported from other ranks)
    //printf("Rows per rank: %d , World Width: %d\n", rows_per_rank, world_width);
    //int data[rows_per_rank + 2][world_width];  STATIC VERSION
    // DYNAMIC MEMORY ALLOCATION
    int i;
    data = malloc((rows_per_rank + 2) * sizeof(int*));
    for (i = 0; i < (rows_per_rank + 2); i++) {
        data[i] = malloc(world_width * sizeof(int));
    }
    
    // SECOND TABLE FOR VALUES. SWAPS WITH ORIGINAL IN LOOP
    new_data = malloc((rows_per_rank + 2) * sizeof(int*));
    for (i = 0; i < (rows_per_rank + 2); i++) {
        new_data[i] = malloc(world_width * sizeof(int));
    }
    
    
    // ========= DETERMINE COMMUNICATION PARTNERS =========
    if (mpi_myrank == 0){
        exchange_rank_a = mpi_commsize - 1;
        exchange_rank_b = 1;
    } else if (mpi_myrank == mpi_commsize - 1) {
        exchange_rank_a = mpi_myrank - 1;
        exchange_rank_b = 0;
    } else {
        // general case
        exchange_rank_a = mpi_myrank - 1;
        exchange_rank_b = mpi_myrank + 1;
    }

    // ========= POPULATE DATA ========= -> initially determined by 50/50 chance
    float x;
    int r, c = 0;
    for (r = 1; r < rows_per_rank + 1; r++) {
        // for each column
        for (c = 0; c < world_width; c++) {
            x = GenVal(starting_row + (r - 1));
            if (x >= 0.5) {
                data[r][c] = ALIVE;
            } else {
                data[r][c] = DEAD;
            }
        }
    }
    
    // ========= INITIALIZE PTHREADS =========
    // create a barrier object
    pthread_barrier_init (&barrier, NULL, num_threads);
    
    pthread_t t[num_threads];
    int* threadid;
    for (i = 1; i < num_threads; i++) {
        threadid = (int *)calloc(1, sizeof(int));
        *threadid = i;
        pthread_create(&t[i], NULL, simulation, threadid); // last argument is the thing you pass
    }
    
    // Parent also does the work of a thread, call the simulation function
    threadid = (int *)calloc(1, sizeof(int));
    *threadid = 0;
    simulation(threadid);

    // Join all remaining threads
    for (i = 1; i < num_threads; i++) {
        pthread_join(t[i], NULL);
    }

    //printf("[Rank %d]: ALL THREADS JOINED\n", mpi_myrank);
    
    // Wait for all ranks to finish processing
    MPI_Barrier( MPI_COMM_WORLD );

    // ========= END TIMER =========
    if (mpi_myrank == 0){
        end_execution_time = MPI_Wtime();
        double execution_time = end_execution_time - start_execution_time;
        //printf("TOTAL EXECUTION TIME:        %f Seconds\n", execution_time);
        
        // Write execution time into file
        char filename[6 + get_digits_int(mpi_commsize) + get_digits_int(num_threads)];
        sprintf(filename, "log_%d_%d", mpi_commsize, num_threads);
        FILE *file = fopen(filename, "w");
        char time[100];
        sprintf(time, "%f", execution_time);
        fputs(time, file);
        
    }
    
    
    // free memory allocations
    for (i = 0; i < rows_per_rank + 2; i++) {
        free(data[i]);
    }
    free(data);
    
    for (i = 0; i < rows_per_rank + 2; i++) {
        free(new_data[i]);
    }
    free(new_data);

    // END -Perform a barrier and then leave MPI
    MPI_Barrier( MPI_COMM_WORLD );
    MPI_Finalize();
    return EXIT_SUCCESS;
}

