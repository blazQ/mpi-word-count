#!/bin/bash

# Check if the number of arguments is correct
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <number_of_processors>"
  exit 1
fi

no_of_processors=$1
files=()

if [ "$no_of_processors" -le 0 ]; then
    echo "The number of processors must be greather or equal than 1."
    exit 1
fi

# Executing the program with processors ranging from 1 to number of processors.
for ((i=1; i<="$no_of_processors"; i++)); do
    echo "=========================================="
    echo -e "Executing with $i processors...\n"
    mpirun \
        --allow-run-as-root \
        --mca btl_vader_single_copy_mechanism none \
        -np $i "./word_count.out" \
        -d "./data/books" > "output$i.csv" 2> "logfile$i"
    echo -e "Output saved to output$i.csv\n"
    echo -e "Sorting current output...\n"
    sort "output$i.csv" > "sorted_$i.csv"
    echo -e "Sorted output saved to sorted_$i.csv\n"
    files+=("sorted_$i.csv")
done

echo "=========================================="

# Iterate over each pair of files
for ((i=0; i<${#files[@]}; i++)); do
  for ((j=i+1; j<${#files[@]}; j++)); do
    file1="${files[i]}"
    file2="${files[j]}"
    echo "Comparing $file1 and $file2:"
    diff -s "$file1" "$file2"
    echo
  done
done

echo "Merging logfiles..."
echo "RECAP OF THE EXECUTIONS FOR $i PROCESSORS" > final_logfile
for ((i=1; i<=no_of_processors; i++)); do
    echo "EXECUTION NO. $i" >> final_logfile
    echo "========================================" >> final_logfile
    cat logfile$i >> final_logfile
    echo >> final_logfile
    rm -f logfile$i
done

cp sorted_1.csv results.csv
rm -f sorted*
rm -f output*

echo "Done!"
echo "You can view a recap of all executions inside the logfile."
echo "All output.csv and sorted.csv files have been removed to avoid cluttering. You can view the actual word count in results.csv"
