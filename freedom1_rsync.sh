# This should be executed from inside of nos/
rsync -zarv --chmod=777 --exclude="*.pdf" --exclude=".git*" --exclude="src/doc/*" --exclude="spec/" . "freedom1:~/freedom1_shared_folder"
