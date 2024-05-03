Main Function (main()):
1)Checks if the correct number of command-line arguments is provided.
2)Opens the specified file (argv[1]) in read-write mode using open().
3)Obtains information about the file using fstat().
4)Maps the content of the file to memory using mmap() to get a pointer (address) to the mapped memory.
5)Superblock Information Extraction:
Extracts superblock information (block size, block count, FAT starts, FAT blocks, root directory starts, root directory blocks) from the mapped memory.
6)Conditional Compilation (#if defined):
Chooses the appropriate function (diskinfo(), disklist(), diskget(), diskput()) based on the preprocessor directives (PART1, PART2, PART3, PART4).
7)Calls the selected function with the necessary arguments.

diskinfo() Function:
1)Checks if the correct number of command-line arguments is provided.
2)FAT Block Information:
Iterates through FAT blocks and counts free, reserved, and allocated blocks.
3)Prints superblock and FAT information.

disklist() Function:
1)Checks if the correct number of command-line arguments is provided.
2)Splits the directory path into tokens.
3)Traverses through directory entries to find the specified directory.
4)Prints the contents of the directory.

diskget() Function:
1)Checks if the correct number of command-line arguments is provided.
2)Splits the directory path into tokens.
3)Traverses through directory entries to find the specified file.
4)Opens a destination file in write-binary mode (wb).
5)Copies the file content block by block from the file system to the local file system.

diskput() Function:
1)Checks if the correct number of command-line arguments is provided.
2)Opens the source file in read-binary mode (rb).
3)Determines Destination Path.
4)Tokenizes the destination path.
5)Traverses through directory entries to find the specified destination directory.
6)Checks if the file already exists in the destination directory.
7)Updates FAT and File Entry: Finds a free entry in the directory and updates the FAT.
8)Sets Attributes for directory entry.
9)Copies the content of the source file to the file system block by block.


Error: When a file from local system is copied to the file system img (using diskput()), although the content is copied correctly, it is followed by random symbols until end of file. I tried to remove this using memset to set the files remaining entries to 0, but was unable to make it work.