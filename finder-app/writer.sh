#!/bin/sh
# Writer script for AESD assignment 1
# Author: Eric Percin, 1/19/2025

# Check arguments for validity
if [ "$#" -ne 2 ]; then
	echo "Error: Invalid parameters. Usage: $0 <writefile> <writestr>"
	exit 1
fi

writefile=$1
writestr=$2

# Create the specified directory
mkdir -p "$(dirname "$writefile")"

# Write the file to the specified directory
echo "$writestr" > "$writefile"

if [ $? -ne 0 ]; then
	echo "Error: file could not be created"
	exit 1
fi





