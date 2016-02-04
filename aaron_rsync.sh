# This should be executed from inside of nos/
rsync -zarvp --exclude="*.pdf" --exclude=".git*" --exclude="src/doc/*" --exclude="spec/" . "fire:~/Desktop/aaron_shared_folder"
ssh fire "chmod -R 777 /home/aaron/Desktop/aaron_shared_folder"
