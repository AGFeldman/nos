#!/bin/bash
# cd to the directory of this script
cd "$(dirname "$0")"
rsync -zarv --chmod=777 --exclude="*.pdf" --exclude=".git*" --exclude="src/doc/*" --exclude="spec/" . "freedom1:~/freedom1_shared_folder"
