#!/bin/bash

# Check if the number of arguments is correct
if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <number_of_processors> <number_of_iterations> <desired_filesize>"
  exit 1
fi

no_of_processors=$1
no_of_iterations=$2
file_size=$3


if [ "$no_of_processors" -le 0 ]; then
    echo "The number of processors must be greather or equal than 1."
    exit 1
fi

if [ "$no_of_iterations" -le 0 ]; then
    echo "The number of iterations must be greather or equal than 1."
    exit 1
fi

if [ "$file_size" -le 0 ]; then
    echo "The desired filesize must be greather or equal than 1."
    exit 1
fi

let possible=$((file_size % 5))
if [ "$possible" -ne 0 ]; then
    echo "Filesize must be multiple of 5!"
    exit 1
fi

# Source file to replicate for testing
source_file="./data/books/bible10.txt"  
# The bible is roughly 5 MBs...
source_file_size=5
# Create test directory
test_directory="./data/test"
mkdir -p $test_directory

let times_to_copy=$((file_size/source_file_size))
for ((z=1; z<=times_to_copy; z++)); do
    cp "$source_file" "$test_directory/bible10$z.txt"
done

# Executing the program with processors ranging from 1 to number of processors.
echo "PROCESSORS, TIME" >timings.csv
for ((i=1; i<="$no_of_processors"; i++)); do
    echo
    echo "=========================================="
    echo -e "Executing with $i processors...\n"
    current_time=0
    # Each configuration runs for no_of_iterations times.
   for ((j=1; j<="$no_of_iterations"; j++)); do 
        echo "Executing run number $j with $i processor(s)..."
        mpirun \
            --allow-run-as-root \
            --oversubscribe \
            -np $i "./word_count.out" \
            -d $test_directory > "output$i.csv" 2> "logfile$i-$j"
        last_line=$(tail -1 "logfile$i-$j")
        current_line=$(echo "$last_line" | grep -Eo '[0-9]+\.[0-9]+')
        current_time=$(echo "$current_time + $current_line" | bc -l)
    done
    current_time_mean=$(echo "$current_time/$no_of_iterations" | bc -l)
    echo "$i, 0$current_time_mean" >>timings.csv
done

cp output1.csv results.csv
rm -dr $test_directory
rm -f output*
rm -f logfile*

echo "Done!"
echo "Recap of timings at timings.csv, results at results.csv"