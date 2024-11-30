I have completely implemented p3 

diskinfo.c
- the output of diskinfo follows the exact expected format and is correct 

disklist.c
- the output of disklist follows tthe exact expected format and is correct listing the files and directories contained in the inputted path with their names, sizes, types and create times
- non-existent locations will be empty  

diskget.c 
- outputs file not found when the requested file does not exist in the specified location
- properly copies the specified file from the directory in the image to the current directory of the host OS and is identical to the original file in the image 

diskput.c 
- outputs file not found when the file to be put in the image is not found within the host OS current directory 
- properly copies the specified file from the host OS to the specified directory under the specified file name in the image and can be verified using disklist
- when using diskget to copy the file back to the host OS, the copied file is identical to the original file in the host OS 
- if the specified target subdirectory doesnt exist in the image then a new directory is created in the image and the file to put in is copied into this new directory 