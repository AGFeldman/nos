# This should be executed from inside of nos/
rsync -zarv --chmod=777 --exclude="*.pdf" --exclude=".git*" --exclude="src/doc/*" --exclude="spec/" . "fire:~/Desktop/aaron_shared_folder"
