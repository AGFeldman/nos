# This should be executed from inside of nos/
rsync -zarvp --exclude="*.pdf" --exclude=".git*" --exclude="src/doc/*" --exclude="spec/" . "freedom1:~/freedom1_shared_folder"
ssh freedom1 "chmod -R 777 /home/freedom1/freedom1_shared_folder"
