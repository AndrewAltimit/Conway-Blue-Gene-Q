# Overview
Implementation of Conway's Game of Life designed for the Blue Gene/Q. See uploaded PDF for the performance results.

## Contributors

<a href="https://github.com/AndrewAltimit/Distributed-Twitter-Service//graphs/contributors">
  <img src="contributors.png" />
</a>


## Usage

Command-Line Arguments:

	mpirun -n <number of ranks> ./main <number of threads (serial is 1)> <world width> <world height> <number of ticks> <threshold (25% is 25)>

Running locally using MPI:

	mpirun -n 8 ./assignment5 8 8192 8192 100 25

Compiling for the Blue Gene/Q Supercomputer

	rename MakeBGQ to Makefile
	run module load xl
	run make

Running on Blue Gene/Q: 

	srun --ntasks 64 --overcommit -o output.log main.xl 8 8192 8192 100 25
