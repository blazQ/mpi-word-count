# mpi-word-count
Word counting utility written in C that makes use of the OpenMPI implementation of the Message Passing Interface standard. Developed for the Parallel and Concurrent Programming on Cloud course at UNISA. 
# Usage
Simply clone the rep, use make and then run with the desired number of processes.

You can use -d and then specify a directory. It will print to stdout, so you'll have to redirect it to your desired output file.

You can also specify -d, a directory and then -f and a file to directly print output to the file instead of redirecting stdout.

```
>git clone https://github.com/blazQ/mpi-word-count.git
>cd mpi-word-count
>make all
>make clean
>mpirun -np X ./word_count -d ./data/books >file_log_name.csv
OR
>mpirun -np X ./word_count -d input_directory -f outputfile.csv
```
Passing "." as the input directory makes it scan the cwd. Executing word_count without any arguments simply makes it reading from the cwd and outputting to stdout.

A simple workload recap is printed on the stderr, regardless.

In data/books there's a sample dataset of txt books courtesy of [TEXT FILES](http://textfiles.com). It can be used for quick local tests.
