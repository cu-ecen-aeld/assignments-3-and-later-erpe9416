#!/bin/sh
# Finder script for AESD assignment 1
# Author: Eric Percin, 1/19/2025

# Check arguments for validity
if [ "$#" -ne 2 ]; then
	echo "Error: Invalid parameters. Usage: $0 <filesdir> <searchstr>"
	exit 1
fi

filesdir=$1
searchstr=$2

# Check that directory exists
if [ ! -d "$filesdir" ]; then
	echo "Error: $filesdir does not represent a directory on the filesystem"
	exit 1
fi

# Count the number of files in the directory
filecount=$(find "$filesdir" -type f | wc -l)

# Find number of lines contianing the search string. Discard warnings
linecount=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

# Print results
echo "The number of files are $filecount and the number of matching lines are $linecount"




