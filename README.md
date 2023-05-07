# mpi-word-count
Word counting utility written in C that makes use of the OpenMPI implementation of the Message Passing Interface standard. Developed for the Parallel and Concurrent Programming on Cloud course at UNISA. 

# approach used
I won't go into details about what Word Counting is, since it is a well known problem, or about MPI itself, but I'll generally only talk about my implementation.

The most important aspect of the solution is the workload generation and the criteria behind the splitting of the files between each processor. 
I've considered three general ways of dividing the files between the processors:
  - Splitting based purely on the number of files
  - Splitting based on the number of lines
  - Splitting based on file sizes

The first one is not efficient enough when the filesize, which is the actual measure of input complexity, varies greatly between files in the directory, causing an uneven workload distribution, while the second one has problems when the line length is not the same between the files.
The third one, while being slightly more difficult to implement, ensures a perfectly balanced workload distribution between each process.

The problem this approach poses is that a single file could, theoretically, be processed by more than one process. So a word could be split between 2 processes, and thus, the final count would be wrong.

To avoid this, synchronization logic must be inserted in the final solution, in order to recover the "missing word".

This synchronization logic must be implemented in such a way as to avoid unnecessary communication overhead and loss of all the gains of parallelization.

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

ATTENTION:
Running it inside a container might require using the following launch options in order to avoid errors:
```
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np X ./word_count.out -d -f ./data/books output_file.csv
```

# correctness
The parsing algorithm is based con the definition of alphanumeric character. 

Basically, this means that a word begins with an alphanumeric character and ends with an alphanumeric character.
Examples of words following this definition could be "house", "cat", "xiii", "154", "pag2" and so on.
Since this definition doesn't include characters like "-" for obvious reasons, words like "volupt-uousness" are supposed to be split into "volupt" and "uousness" and so on.

To easily test the correctness of the algorithm, you can simply compare it with the results of other utilities (like notepad++ or sublime text) that grant you the ability to search for whole words (not only substrings).
Since doing this by hand could require a lot of time, you can simply execute the program with 1 processor and then compare the output with the execution with n processors, like this:

```
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np 1 ./word_count.out -d -f ./data/books output1.csv
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np 2 ./word_count.out -d -f ./data/books output2.csv
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np 3 ./word_count.out -d -f ./data/books output3.csv
...
sort output1.csv > output1s
sort output2.csv > output2s
sort output3.csv > output3s
...
diff -s output1s output2s
diff -s output2s output3s
diff -s output1s output3s
...
```
Clearly the use of sort and diff is reminiscent towards Linux or other UNIX operating systems, but you can swap it for the equivalent command on your OS.

# credits 
Book files dataset courtesy of [TEXT FILES](http://textfiles.com).
