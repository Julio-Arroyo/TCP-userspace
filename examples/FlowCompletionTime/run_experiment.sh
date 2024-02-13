#!/usr/bin/env bash

if [ -z "$1" ]; then
  echo "Usage: $0 [FILE_SIZE]"
  exit 1
fi

mkdir files
mkdir output

FILESIZE="$1"
TRANSMITTED_FILE="files/file_${FILESIZE}.txt"

echo "Creating file..."
python3 ../examples/FlowCompletionTime/create_file.py "$FILESIZE"

echo "Launching client..."
./fct_client "$FILESIZE"
sleep 2

echo "Is there anything between here..."
diff ./output/received_file.txt "$TRANSMITTED_FILE"
echo "... and here?"

rm -r files
# rm -r output

