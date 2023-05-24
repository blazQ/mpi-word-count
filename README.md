# mpi-word-count

Word counting utility written in C that makes use of the OpenMPI implementation of the Message Passing Interface standard. Developed for the Parallel and Concurrent Programming on Cloud course at UNISA.

- [mpi-word-count](#mpi-word-count)
  - [approach used](#approach-used)
  - [code structure](#code-structure)
    - [wordcount.c](#wordcountc)
    - [workload.h](#workloadh)
    - [chnkcnt.h](#chnkcnth)
    - [histogram.h futils.h and hashdict.h](#histogramh-futilsh-and-hashdicth)
  - [usage](#usage)
  - [correctness](#correctness)
  - [Performance](#performance)
    - [Strong Scalability](#strong-scalability)
    - [Weak Scalability](#weak-scalability)
  - [credits](#credits)

## approach used

I won't go into details about what Word Counting is since it is a well-known problem, or about MPI itself, but I'll generally only talk about my implementation.

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

There are certainly other important aspects of the general solution: the use of a hashtable for fast lookup of each word and their respective counts, the use of MPI-derived datatypes to optimize communications and so on. Each of these will be detailed in the following sections.

## code structure

### wordcount.c

The main executable has a very simple structure, omitting the details regarding args parsing and MPI Initialization.
I'll give you a bird-eye look at what the main does, and then go into details in the following sections.

The first thing we do is commit an MPI_Datatype, histogram_element_dt, which is used to transfer the local results to the master at the end of the program.

```c
 MPI_Datatype histogram_element_dt;
 MPI_Type_create_histogram(&histogram_element_dt);
```

Then, we proceed to analyze the files in the directory passed as an argument, creating a list of file elements.

This list of files is used to generate the workload, which is done independently by each processor, to avoid wasting time waiting for the master to finish. This can be done assuming that the generated list of files maintains the same order, which is true if all processors run on the same OS.

To generalize this behaviour, we can simply sort the file list before generating the workload.

Details on workload generation can be found in the following section.

After generating the workload, each process accesses its list of chunks and proceeds to count their words, saving for each special chunk their respective first and last word.
The results of the counting are stored in a hashtable called dict.

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

This is done after each processor counts words in his chunk list, to avoid having each process wait the previous one before computing, effectively neutralizing the gains of parallelization.
More details on why there can only be 2 special chunks for chunk list are in the following sections.

After this, the MASTER needs to know how many words each process has found to allocate the correct amount of space for each local histogram. I did this using a simple Gather.

Then, the MASTER proceeds to allocate space for each local histogram, while every other process sends its local histogram to the MASTER.

```c
  // Allocating space for wsize histogram_element[]
  histogram_element **process_histograms = malloc(sizeof(*process_histograms)*wsize);
  for(int i = 1; i < wsize; i++)
   process_histograms[i] = malloc(sizeof(histogram_element) * localszs[i]);

  // Receiving local histograms
  MPI_Status status;
  for(int i = 1; i < wsize; i++){
   MPI_Recv(process_histograms[i], localszs[i], histogram_element_dt, i, 0, MPI_COMM_WORLD, &status);
  }
```

Please note that every malloc in the project follows a very basic C idiom, to avoid having to redeclare the whole line if, in future updates, certain types change. For example, if I were to write:

```c
int list_size = 56 // A certain number
int* int_pointer = malloc(sizeof(int)*list_size);
```

And if in the future, I plan to swap int for uint32, 16 or 8 for specific implementation optimization, I have to rewrite the whole line, changing int* and sizeof(int).
Instead:

```c
int list_size = 56 // A certain number
int* int_pointer = malloc(sizeof(*int_pointer)*list_size); // Size of the content the pointer is referencing, in this case, it defaults to sizeof(int) like above.
```

With the idiom, I only have to change the variable's type, but not the argument of the sizeof operator.
This can seem something minor, but in a larger project with tons and tons of variables, it also makes it safer to edit, in my opinion.

Finally, MASTER merges his histogram with the ones he received with the merge_dict() function and then writes the output to the file descriptor provided at the beginning, and the program is over.

### workload.h

This header and its relative source file define what a chunk is and how the workload is generated between each processor.

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

This is used to optimize computation, since clearly there's no need to synchronize anything with anyone if the current chunk contains the whole file (no one else is involved in the computation).

When the workload is generated, I've used a greedy approach that reminds us of the knapsack problem.

We've got a certain total number of bytes to assign, and using this value and the number of processors we compute the maximum capacity of every single processor.

Then we iterate over the files, and we try to fill each "bin" until it's full with chunks of the current file. If the remaining capacity of the current process is enough to process a whole file, we create a chunk with special_position = UNIQUE, else it will be either FIRST, LAST or REGULAR, which means that either the chunk starts from 0 and ends before the file ends, or that it doesn't start from 0 but ends at EOF, or that it's neither at the beginning or the end of a file.

In this case, the "bin" is a list of chunks to be assigned to each process.

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
            }
            if(remaining_capacities[cur_proc] == 0)
                curr_proc++;
}
//Omitting type assignment for clarity, for more details, look directly into the src code
```

This process will generate at most 2 special chunks for every chunk list. 
We can be sure of this statement because the workload generation works by greedily scanning the list of files, which we assume is the same for every process.

This means that if a chunk is marked as FIRST, it MUST be the last element of a certain chunk list, because it means that the current process isn't able to make a whole chunk of the current file, thus it must create a chunk of said file that uses all of its remaining capacity.

Conversely, if a chunk is marked as LAST, it MUST be the first element of a certain chunk list, because it means that the file that the chunk refers to was processed before by one or more processes, that couldn't handle the whole size, and the current processor is the one that managed to do it.

Already we can see how there can only be at most one FIRST and one LAST per chunk list.

To complete the reasoning, if a chunk is marked as REGULAR it MUST be the only element in the chunk list. Because having it marked like so means that the previous process wasn't able to handle the whole file, but the current process isn't either. So its whole capacity is used by this so-called "intermediary" chunk.

### chnkcnt.h

This header defines the functions used to count words inside a file.
I won't go into details about the counting itself since you can imagine how it works, but this header also contains functions to synchronize results between chunks.
They are called sync_with_prev and synch_with_next, and as you can imagine they are used to synchronize with the previous process or the next in line.

sync_with_prev gets executed when the current chunk isn't the first of its file, for example, "LAST" or "REGULAR". If the current chunk begins with a word, not a space or other characters not considered to be alphanumeric, the word gets propagated backwards. The previous element will respond according to how its chunk ends, so we can remove the partial word from the hashtable.

sync_with_next has the same logic in reverse. It gets executed when the chunk is the "FIRST", or "REGULAR". That basically means that we need to check if the chunk ends with a word, and if it does, and we receive confirmation from the next process that its chunk begins with a word, we can then proceed to merge them and update the hashtable with correct values.

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

Where WORD_MAX is 256, assuming no english word is longer than this.

histogram.h also contains the function that creates the histogram_element_dt MPI datatype, which is done by simply analyzing the structure and calling the appropriate MPI functions.

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

Clone the repo, use make and then run with the desired number of processes.

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

Passing "." as the input directory makes it scan the cwd. Executing word_count without any arguments simply makes it read from the cwd and output to stdout.

ATTENTION:
Running it inside a docker container might require using the following launch options:

```bash
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np X ./word_count.out -d ./data/books > output_file.csv
```

Running it without these options doesn't constitute a problem, but may generate some random alerts (that do not alter the code's output in any way, but they derive from docker's way of handling PIDs)

## correctness

The parsing algorithm is based on the definition of alphanumeric character.

This means that a word begins with an alphanumeric character and ends with an alphanumeric character.
Examples of words following this definition could be "house", "cat", "xiii", "154", "pag2" and so on.
Since this definition doesn't include characters like "-" for obvious reasons, words like "volupt-uousness" are supposed to be split into "volupt" and "uousness" and so on.

To easily test the correctness of the algorithm, you can simply execute the program with 1 processor and then compare the output with the execution with n processors, like this:

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

Since this operation is tedious, I've included with the source code a simple bash script that you can execute like this:

```bash
./correctness_test.sh <max_number_of_processors>
```

It automates what I said before, executing the algorithm number_of_processors times, increasing the number of processors each time.
The outputs are saved to sorted files that get diff'd at the end and then removed to avoid cluttering.

## Performance

The utility has been tested using a cluster of machines, via Google Cloud.

I've used 3 e2-standard-8 machines, for a total of 24 vCPUs, to perform strong scalability tests with a fixed number of bytes as input and varying the number of processors.
Tests were done using a fair workload, automatically generated using the scripts found in the directory, to ensure reliable results.
The scripts supplied in the repo are meant for easy local testing. To adapt them in a clustered environment, make sure that every node has the same test directory. (For example, create it in the master and then send it to all the other nodes)

### Strong Scalability

During the test, I used a slightly modified version of the scripts supplied with the repository to test the program with increasing input sizes, evenly distributed, starting from 50 MB up to 1 GB. I've also conducted some tests using the full dataset of sparsely distributed files, that you can find in ./data/books, with varying file sizes, to see how the program handled uneven workloads. You can see full results in ./data/imgs.
In the credits, I've also included a source, where you can find other files, for personal testing.

First things first, let's analyze the results for 24 vCPUs, on all filesizes.
Here's a chart representing just that:

![Strong Scaling_vCPUs](data/imgs/vcpus_strong_scaling_dark.png#gh-dark-mode-only)
![Strong Scaling_vCPUs](data/imgs/vcpus_strong_scaling_light.pngg#gh-light-mode-only)

There's something strange happening to the algorithm, as soon as we go from 4 processes to 5.
It briefly worsens its performance, then starts scaling again but it's a bit slower than before.
After quite a bit of testing and fiddling with google cloud VMs, I've discovered that the number of vCPUs isn't the actual core number of the CPU.

In reality, vCPUs are computed as no. of Cores times no. of Threads per core.

This means that in e2-standard-8 VMs, there are actually 4 cores and 2 threads per CPU.
Based on this assumption, my guess, which is also backed up by further evidence, is that as soon as we go from 4 processes to 5, processes start to share a core, thus worsening relative performance compared to a situation where each process can work on a separate core.
I've performed similar tests on e2-standard-4 machines, and guess what? On those machines, the threshold after which the strange behaviour happens is no longer 4, but 2...just like the number of actual cores in that case!

Another thing is that, with an e2-standard-8 machine, simply running mpirun -np x with x > 4, without oversubscribing, results in the following error:

```sh
--------------------------------------------------------------------------
There are not enough slots available in the system to satisfy the 5 slots
that were requested by the application:
  ./word_count.out

Either request fewer slots for your application or make more slots available
for use.
--------------------------------------------------------------------------
```

This is what would happen running it locally, on a machine with less than the requested amount of cores. This means that if we use 8 processors on an e2-standard-8 machine, we are actually oversubscribing from 4, hence the reduced scaling.

Here's a table that summarizes results, in terms of speedup, when we use all 24 vCPUs, on an input of roughly 1 GB:

| PROCESSORS | TIME      | SPEEDUP   |
|------------|-----------|-----------|
| 1          | 18.371841 | 1.000000  |
| 2          | 9.515122  | 1.930805  |
| 3          | 6.491697  | 2.830052  |
| 4          | 4.717342  | 3.894533  |
| 5          | 5.299246  | 3.466879  |
| 6          | 4.454800  | 4.124055  |
| 7          | 3.871530  | 4.745370  |
| 8          | 3.469534  | 5.295190  |
| 9          | 3.138919  | 5.852920  |
| 10         | 2.837474  | 6.474717  |
| 11         | 2.617261  | 7.019491  |
| 12         | 2.483414  | 7.397818  |
| 13         | 2.317219  | 7.928401  |
| 14         | 2.161452  | 8.499768  |
| 15         | 2.052521  | 8.950866  |
| 16         | 1.942018  | 9.460183  |
| 17         | 1.853378  | 9.912625  |
| 18         | 1.731097  | 10.612831 |
| 19         | 1.687065  | 10.889827 |
| 20         | 1.623129  | 11.318784 |
| 21         | 1.590576  | 11.550436 |
| 22         | 1.536827  | 11.954401 |
| 23         | 1.493661  | 12.299873 |
| 24         | 1.434171  | 12.810077 |

Even with what we previously said, the algorithm doesn't show a bad performance at all. It manages to have a pretty decent and steady speedup. But it's clear how our considerations impact its performance. So I wanted to see what would happen If, instead of using all the vCPUs on the machines, I'd only use the actual cores.

This would mean that, with 3 e2-standard-8 machines, the total number of actual cores would be 12.
(Be aware of the fact that I only stopped at 24 vCPUs because there's a regional limit on how many vCPUs you can have active)

Guess what happens when you only use the actual cores, which is -np 4, in this case, for every machine? It literally flies.

![Strong Scaling](data/imgs/cores_strong_scaling_dark.png#gh-dark-mode-only)
![Strong Scaling](data/imgs/cores_strong_scaling_light.png#gh-light-mode-only)

As we can see, by varying the size of the input, we get stronger gains, relative to the size and to the same execution on a smaller size, which is something that also happened earlier, but in this case, it's scaling a lot better, as we can see from this table:

| PROCESSORS |  TIME      | Speedup    |
|------------|------------|------------|
| 1          | 18.2957978 | 1.0000000  |
| 2          | 9.3722064  | 1.9521335  |
| 3          | 6.3146182  | 2.8973720  |
| 4          | 4.7194074  | 3.8767151  |
| 5          | 3.8144026  | 4.7965041  |
| 6          | 3.3372394  | 5.4823151  |
| 7          | 2.8551636  | 6.4079683  |
| 8          | 2.4114638  | 7.5870091  |
| 9          | 2.2015970  | 8.3102392  |
| 10         | 1.9778902  | 9.2501585  |
| 11         | 1.8789884  | 9.7370467  |
| 12         | 1.7424422  | 10.5000888 |

This time, the speedup for 12 processors is 10.5, which is much closer to 12, against the 7.4 we obtained previously!

### Weak Scalability

To perform weak scalability testing I've created a script that automatically generates a workload of datasets with equally distributed filesizes to serve as input for each run. This makes it so 1 processor gets 100 mb as input, 2 processors get 200 MB and so on.
I've also conducted weak scalability testing bearing in mind what I said earlier, so without exceeding the number of actual cores in every machine.
The results of the weak scalability testing are summarised in the following table:

| PROCESSORS |  TIME     | Efficiency  |
|------------|-----------|-------------|
| 1          | 1,0294909 | 100,0000000 |
| 2          | 1,0707393 | 96,1476711  |
| 3          | 1,0867302 | 94,7328877  |
| 4          | 1,0892999 | 94,5094092  |
| 5          | 1,0985009 | 93,7178021  |
| 6          | 1,1132747 | 92,4741126  |
| 7          | 1,1176962 | 92,1082938  |
| 8          | 1,1435514 | 90,0257653  |
| 9          | 1,1468319 | 89,7682476  |
| 10         | 1,1502241 | 89,5035063  |
| 11         | 1,1570636 | 88,9744436  |
| 12         | 1,1605109 | 88,7101448  |

As we can see, with medium filesizes the efficiency tends to stay above 90%, although it steadily drops when we increase the number of processors, due to communication overhead, keeps steadily dropping the more we increase the number of processes.
Here's a chart to better visualize this result:

![Efficiency](data/imgs/efficiency_light_cores.png#gh-light-mode-only)
![Efficiency](data/imgs/efficiency_dark_cores.png#gh-dark-mode-only)


We can see how the algorithm might be improved, in terms of efficiency, to handle workloads without excessive communication between workers.

Right now, the efficiency is tolerable, but depending on the context of the application, there are certainly a lot of things we can fine-tune to reach optimal efficiency.

For example, we could implement a way to avoid making each process communicate to recover the split words, by implementing a way to make each process "peek" forward or backwards. Such a solution would reduce communication to what is strictly necessary (gathering of partial results) and possibly increase efficiency as the number of processors increases.

We could also rewrite some data structures to make better use of the cache, and be more efficient as the size inevitably grows.

Lastly, there's bound to be something not quite optimal with memory handling, since there are strings involved, so with a better understanding of the application domain there could be ways to optimize memory usage, for example regarding word lengths.

Additional charts regarding weak and strong scalability can be found at ./data/imgs.

## credits

Book files dataset courtesy of [TEXT FILES](http://textfiles.com).

Optimized hashdict courtesy of [exebook](https://github.com/exebook/hashdict.c).
