# mpi-word-count

Word counting utility written in C that makes use of the OpenMPI implementation of the Message Passing Interface standard. Developed for the Parallel and Concurrent Programming on Cloud course at UNISA.

## approach used

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

## code structure

### main executable

The main executable has a very simple structure, omitting the details regarding args parsing and MPI Initialization.

The first thing we do is commiting an MPI_Datatype, histogram_element_dt, which is used in order to transfer the local results to the master at the end of the program.

```c
 MPI_Datatype histogram_element_dt;
 MPI_Type_create_histogram(&histogram_element_dt);
```

Then, we proceed to analyze the files in the directory passed as argument, creating a list of file elements.

This list of files is used in order to generate the workload, which is done independently by each processor, in order to avoid wasting time waiting for the master to finish. This can be done assuming that the generated list of files maintains the same order, which is true if all processors run on the same OS.

To generalize this behaviour, we can simply sort the file list before generating the workload.

After generating the workload, each process accesses its list of chunks and proceed to count their words, saving for each special chunk their respective first and last word.
Results of the counting are stored in an hashtable called dict.

After counting words in every chunk, we simply need to synchronize the results with our peers, to avoid losing words in between chunks:

```c
for(int i = 0; i < 2; i++){
  if(special_chunks[i].chunk_type==FIRST){
    sync_with_prev(special_chunks[i].last_word, rank, dic);
  }
  else if(special_chunks[i].chunk_type == LAST){
    sync_with_next(special_chunks[i].first_word, rank, dic);
  }
  else if(special_chunks[i].chunk_type == REGULAR){
    sync_with_prev(special_chunks[i].last_word, rank, dic);
    sync_with_next(special_chunks[i].first_word, rank, dic);
  }
}
```

This is done after each processor counts words in his chunk list, to avoid having each process waiting the previous one before computing, effectively neutralizing the gains of parallelization.
More details on why there can only be 2 special chunks for chunk list in the following sections.

After this, the MASTER needs to know how many words each process has found in order to allocate the correct amount of space for each local histogram. I did this using a simple Gather.

Then, the MASTER proceeds to allocate space for each local histogram, while every other process sends his local histogram to the MASTER.

The MASTER merges his histogram with the ones that he received, and then writes the output to the file descriptor provided at the beginning.

### workload.h

This header and it's relative source file defines what a chunk is and how the workload is generated between each processor.

```c
typedef struct chunk{
    char*           file_name;
    long            start;
    long            end;
    int             special_position;
} File_chunk;
```

A chunk is simply a part of a file that gets processed by one of the processors involved in the computation. It has a start field that defines from where the counting starts, in bytes, and an end field that does exactly what you think it does.
The special_position field is used to denote if a certain chunk is the first of its file, the last, if it contains the whole file or if it's an intermediary one.

This is used to optimize computation, since clearly there's no need to synchronize anything with anyone if the current chunk contains the whole file (noone else is involved in the computation).
More on this on the next section.

When the workload is generated, I've used a greedy approach that reminds us of the knapsack problem.

We've got a certain total number of bytes to assign, and using this value and the number of processors we compute the maximum capacity of every single processor.

Then we iterate over the files, and we try to fill each "bin" until it's full with chunks of the current file. If the remaining capacity of the current process is enough to process a whole file, we create a chunk with special_position = UNIQUE, else it will be either FIRST, LAST or REGULAR, which means that either the chunk starts from 0 and ends before the file ends, or that it doesn't start from 0 but ends at EOF, or that it's neither at the beginning or the end of a file.

In this case obviously the "bin" is a list of chunks to be assigned to each process.

```c
while(file_remaining){
            if(remaining_capacities[cur_proc] >= file_remaining){
                chunk_push_back(&chunks_proc[cur_proc], strdup(cur_file.file_name), start, start+file_remaining, type);
                remaining_capacities[cur_proc] -= file_remaining;
                file_remaining = 0;
            }
            else if(remaining_capacities[cur_proc] > 0){
                chunk_push_back(&chunks_proc[cur_proc], strdup(cur_file.file_name), start, start+remaining_capacities[cur_proc], type);
                file_remaining -= remaining_capacities[cur_proc];
                start = start + remaining_capacities[cur_proc];
                remaining_capacities[cur_proc] = 0;
                cur_proc++;
            }
}
//Omitting type assignment for clarity, for more details look directly into the src code
```

This generation will generate at most 2 special chunks for every chunk list. The reason why this happens is, if a chunk is marked as "REGULAR" it means that it isn't the first of its file, it isn't the last and the current processor can't handle what remained of the file after the previous one added a "FIRST". This means, it's the only one the current process actually handles.
If a chunk is marked as "FIRST", it means that the remaining capacity of the current process isn't enough to process the whole file, but it could have processed some files before the current one. So the chunk marked as "FIRST" is always the last one of its batch.

Logic for chunks marked as "LAST" is the same, but reversed. "LAST" is always the first one of its batch.
A LAST can be potentially followed by a FIRST, and a FIRST can be preceded by a LAST. A REGULAR is always alone. Hence, the number of special chunks is at most 2.

### chnkcnt.h

This header defines the functions that get used to count words inside a file.
I won't go into details about the counting itself since you can image how it works, but this header also contains functions to synchronize results between chunks.
They are called sync_with_prev and synch_with_next, and as you can image they are used to synchronize with the previous process or the next in line.

sync_with_prev gets executed when the current chunk isn't the first of its file, so for example "LAST" or "REGULAR". If the current chunk begins with a word, not a space or other characters not considered to be alphanumeric, the word gets propagated backwards. The previous element will respond according to how its own chunk ends, so we can remove the partial word from the hasbtable.

sync_with_next has the same logic in reverse. It gets executed when the chunk is the "FIRST", or "REGULAR". That basically means that we need to check if the chunk ends with a word, and if it does, and we receive confirmation from the next process that its own chunk begins with a word, we can then proceed to merge them and update the hashtable with correct values.

This logic gets executed after the parallel counting of the words, in order to avoid losing too much performance. Alternative methods can obviously be used to improve performance further.

### histogram.h futils.h and hashdict.h

These are simple helper headers that define the hashtable used to store words and counts, the file dynamic array used to contain the list of files and the histogram structure to permit communication of local results.

Without going into details on the file array and the hashtable, which are as you would expect them to be, the histogram element is defined as follows:

```c
typedef struct{
    int count;
    char word[WORD_MAX];
} histogram_element;
```

histogram.h also contains the function that actually creates the histogram_element_dt MPI datatype, which is done by simply analyzing the structure and calling the appropriate MPI functions.

```c
int count = 2;

int block_length[2] = {
  1,
  256
};

MPI_Aint displacements[2] = {
  offsetof(histogram_element, count),
  offsetof(histogram_element, word)
};

MPI_Datatype types[2] = {
  MPI_INT,
  MPI_CHAR
};

MPI_Type_create_struct(count, block_length, displacements, types, histogram_element_dt);
```

## usage

Simply clone the rep, use make and then run with the desired number of processes.

You can specify -d, a directory and then -f and a file, where the results will be saved in CSV format.
Example of use:

```bash
git clone https://github.com/blazQ/mpi-word-count.git
cd mpi-word-count
make all
make clean
mpirun -np 3 ./word_count.out -d ./data/books -f output.csv
```

You can also use only -d and then specify a directory. It will print to stdout, so you'll have to redirect it to your desired output file.

```bash
mpirun -np 3 ./word_count -d ./data/books >output.csv
```

Passing "." as the input directory makes it scan the cwd. Executing word_count without any arguments simply makes it reading from the cwd and outputting to stdout.

ATTENTION:
Running it inside a container might require using the following launch options:

```bash
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np X ./word_count.out -d ./data/books > output_file.csv
```

Running it without these options doesn't constitute a problem, but may generate some random alerts (that do not alter the code's output in any way, but they derive from docker's own way of handling PIDs)

## correctness

The parsing algorithm is based con the definition of alphanumeric character.

Basically, this means that a word begins with an alphanumeric character and ends with an alphanumeric character.
Examples of words following this definition could be "house", "cat", "xiii", "154", "pag2" and so on.
Since this definition doesn't include characters like "-" for obvious reasons, words like "volupt-uousness" are supposed to be split into "volupt" and "uousness" and so on.

To easily test the correctness of the algorithm, you can simply compare it with the results of other utilities (like notepad++ or sublime text) that grant you the ability to search for whole words (not only substrings).
Since doing this by hand could require a lot of time, you can simply execute the program with 1 processor and then compare the output with the execution with n processors, like this:

```bash
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np 1 ./word_count.out -d -f ./data/books output1.csv
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np 2 ./word_count.out -d -f ./data/books output2.csv
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np 3 ./word_count.out -d -f ./data/books output3.csv

sort output1.csv > output1s
sort output2.csv > output2s
sort output3.csv > output3s

diff -s output1s output2s
diff -s output2s output3s
diff -s output1s output3s
```

Since this operation is tedious, I've inclued with the source code a simple bash script that you can execute, to test the correctness of the algorithm up to N processors.

You can use it like this:

```bash
./correctness_test.sh <max_number_of_processors>
```

It basically automates what I said before, executing the algorithm number_of_processors times, increasing the number of processors each time.
The outputs are saved to sorted files that get diff'd at the end and then removed to avoid cluttering.

## Performance

The utility has been tested using a cluster of machines, via Google Cloud.

I've used 4 e2-standard-2 machines, for a total of 8 cores, to perform strong scalability tests with a fixed number of bytes as an input and varying the number of processors.
Tests were done using a fair workload, automatically generated using the scripts found in the directory, to ensure reliable results.

### Strong Scalability

During the test, I've used the scripts supplied with the repository to basically automatically test the program with increasing input sizes, evenly distributed, up to 1 GB. I've also conducted some tests using the full dataset of sparsely distributed files, that you can find in ./data/books, with varying filesizes, to see how the program handled uneven workloads. You can see full results in ./data/imgs.
In the credits I've also inclued a source, where you can find other files, for personal testing.

Here's a table representing the results of the algorithm, when the input size is roughly 1 GB.

| PROCESSORS | TIME(s)   | Speedup |
|------------|-----------|---------|
| 1          | 10,119054 | 1       |
| 2          | 5,2377792 | 1,932   |
| 3          | 3,6411202 | 2,779   |
| 4          | 2,6933014 | 3,757   |
| 5          | 2,1974652 | 4,605   |
| 6          | 1,8708760 | 5,409   |
| 7          | 1,6055676 | 6,302   |
| 8          | 1,4172594 | 7,140   |

Strong scalability testing results show very good performance by the algorithm. It successfully manages to handle even very big workloads, in the order of GBs, with ease and with adequate scaling, although the presence of a large sequential portion, mainly the final merging, done only by process 0, does limit the gains we get from increasing the number of processors after a while.

I've also conducted the tests with inferior input sizes, and the algorithm tends to behave better the more the size increases. The reason for this behaviour is that the communication overhead becomes more negligible the more the input size increases. Since we only need to check for at most p split words, where p is the number of processors, when we compare p with filesizes in the order of gigabytes, it obviously becomes negligible. That's the reason why apparently the speedup is so high. But as soon as we drop to more "realistic" filesizes, such as 20-80 mb, the speedup for 2 processors hovers around 1.6, which is to be expected, since the comunication overhead and the sequential part become more prominent in the overall computation.

Here's some data to further motivate my conclusions, input sizes ranging from 50 mb to 1000 mb:

![Strong Scaling](data/imgs/strong_scaling_dark.png#gh-dark-mode-only)
![Strong Scaling](data/imgs/strong_scaling_light.png#gh-light-mode-only)

As we can see, varying the size of the input, we get stronger gains, relatively to the size and to the same execution on a smaller size.


### Weak Scalability

To perform weak scalability testing I've created a script that automatically generates a workload of datasets with equally distributed filesizes to serve as input for each run. This basically makes it so 1 processor gets 100 mb as input, 2 processors get 200 mb and so on.
The results of the weak scalability testing are summarised in the following table:

| PROCESSORS | INPUT SIZE (mb) |  TIME(s)  | Efficiency  |
|------------|-----------------|-----------|-------------|
| 1          | 100             | 1,0294909 | 100,0000000 |
| 2          | 200             | 1,0707393 | 96,1476711  |
| 3          | 300             | 1,0867302 | 94,7328877  |
| 4          | 400             | 1,0892999 | 94,5094092  |
| 5          | 500             | 1,0985009 | 93,7178021  |
| 6          | 600             | 1,1132747 | 92,4741126  |
| 7          | 700             | 1,1176962 | 92,1082938  |
| 8          | 800             | 1,1435514 | 90,0257653  |

As we can see, with medium filesizes the efficiency tends to stay above 90%, although it steadily drops when we increase the number of processors, due to communication overhead.
Here's a chart to better visualize this result:

![Weak Scaling](data/imgs/weak_scaling_light.png#gh-light-mode-only)
![Weak Scaling](data/imgs/weak_scaling_dark.png#gh-dark-mode-only)

## credits

Book files dataset courtesy of [TEXT FILES](http://textfiles.com).

Optimized hashdict courtesy of [exebook](https://github.com/exebook/hashdict.c).
