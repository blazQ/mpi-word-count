# mpi-word-count
Word counting utility written in C that makes use of the OpenMPI implementation of the Message Passing Interface standard. Developed for the Parallel and Concurrent Programming on Cloud course at UNISA. 

# approach used
I won't go into details about what Word Counting is, since it is a well known problem, or about MPI itself, but I'll generally only talk about my implementation.

The most important aspect of the solution is the workload generation and the criteria behind the splitting of the files between each processor. 
I've considered three general ways of dividing the files between the processors:
  - Splitting based purely on the number of files
  - Splitting based on the number of lines
  - Splitting based on file sizes

The first one is not efficient enough when the filesize varies greatly between files in the directory, causing an uneven workload distribution, while the second one has problems when the line length is not the same between the files.
The third one, while being slightly more difficult to implement, ensures a perfectly balanced workload distribution between each process.

The problem this approach poses is that a single file could, theoretically, be processed by more than one process. So a word could be split between 2 processes, and thus, the final count would be wrong.

To avoid this, synchronization logic must be inserted in the final solution, in order to recover the "missing word".

This synchronization logic must be implement in such a way as to avoid unnecessary communication overhead and lose all of the gains of parallelization.

There are certainly other important aspects of the general solution: the use of an hashtable for fast lookup of each word and their respective counts, the use of MPI derived datatypes to optimize comunications and so on. Each of this will be detailed in the following sections.

# code structure
## main executable

## workload.h

## chnkcnt.h

## histogram.h

## futils.h and hashdict.h

# usage
Simply clone the rep, use make and then run with the desired number of processes.

You can specify -d, a directory and then -f and a file, where the results will be saved in CSV format.
```
>git clone https://github.com/blazQ/mpi-word-count.git
>cd mpi-word-count
>make all
>make clean
>mpirun -np X ./word_count -d input_directory -f outputfile.csv
```
You can also use -d and then specify a directory. It will print to stdout, so you'll have to redirect it to your desired output file.
```
>mpirun -np X ./word_count -d ./data/books >file_log_name.csv
```
Passing "." as the input directory makes it scan the cwd. Executing word_count without any arguments simply makes it reading from the cwd and outputting to stdout.

A simple workload recap is printed on the stderr, regardless.

# credits 
Book files dataset courtesy of [TEXT FILES](http://textfiles.com).
