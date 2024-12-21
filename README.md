# CSc 360: Operating Systems (Fall 2024)

# Programming Assignment 3 (P3)
# A Simple File System (SFS)

> **Spec Out**: Nov 1, 2024 <br>
> **Code Due**: Nov 29, 2024, 23:59

## Table of Contents

1. [Introduction](#1-introduction)
2. [Tutorial Schedule](#2-tutorial-schedule)
3. [Requirements](#3-requirements)
    1. [Part I](#31-part-i)
    2. [Part II](#32-part-ii)
    3. [Part III](#33-part-iii)
    4. [Part IV](#34-part-iv)
4. [File System Specification](#4-file-system-specification)
    1. [File System Superblock](#41-file-system-superblock)
    2. [Directory Entries](#42-directory-entries)
    3. [File Allocation Table (FAT)](#43-file-allocation-table-fat)
    4. [Test File System Image](#44-test-file-system-image)
5. [Byte Ordering](#5-byte-ordering)
6. [Submission Requirements](#6-submission-requirements)
    1. [Version Control and Submission](#61-version-control-and-submission)
    2. [Deliverables](#62-deliverables)
    3. [Rubric](#63-rubric)
7. [Plagiarism](#7-plagiarism)

## 1. Introduction

In `p1` and `p2`, you have built a shell environment and a multi-thread scheduler with process synchronization. Excellent job! What is still missing for a "real" operating system? A **file system**! In this assignment, you will implement utilities that perform operations on a file system similar to [Microsoft's FAT file system](https://en.wikipedia.org/wiki/File_Allocation_Table) with some improvement.

An example test file system image and the corresponding code is provided for your self-testing, available at [test.img](./createTestImage/test.img), but you can also create your own image following the specification, and your submission may be tested against other disk images following the same specification.

You should get comfortable examining the raw, binary data in the file system images using the program [`xxd`](https://linux.die.net/man/1/xxd).

**IMPORTANT**: Since you are dealing with binary data, functions intended for string manipulation such as `strcpy()` do **NOT** work (since binary data may contain binary `0` anywhere), and you should use functions intended for binary data such as `memcpy()`.

You shall implement your solution in `C` programming language only. Your work will be tested on `linux.csc.uvic.ca`, which you can remotely log in by ssh. You can also access Linux computers in ECS labs in person or remotely by following the guide at https://itsupport.cs.uvic.ca/services/e-learning/login_servers/.

**Be sure to test your code on `linux.csc.uvic.ca` before submission.**

## 2. Tutorial Schedule

In order to help you finish this programming assignment on time successfully, the schedule of the lectures has been synchronized with the tutorials and the assignment. There are two tutorials arranged during the course of this assignment.

| Date          | Tutorial                                                | Milestones by the Friday of that week                        |
|---------------|---------------------------------------------------------|--------------------------------------------------------------|
| Nov 4/5/6     | T9: P3 spec go-thru and practice questions              | Design done                                                  |
| Nov 11/12/13  | Reading break, **no tutorials this week**               | `diskinfo`/`disklist` done                                   |
| Nov 18/19/20  | T10: testing and submission instructions                | `diskget`/`diskput` done                                     |
| Nov 25/26/27  | **No tutorial but office hours offered**                | Final deliverable                                            |

**NOTE**: Please do attend the tutorials and follow the tutorial schedule closely.

## 3. Requirements

### 3.1. Part I

In Part I, you will write a program `diskinfo` that displays information about the file system. In order to complete Part I, you will need to read the file system super block and use the information in the super block to read the FAT.

Your program for Part I will be invoked as follows (output values here are just for illustration purposes):

```
./diskinfo test.img
```

Sample output:

```
Super block information:
Block size: 512
Block count: 6400
FAT starts: 1
FAT blocks: 50
Root directory start: 51
Root directory blocks: 8

FAT information:
Free Blocks: 6341
Reserved Blocks: 49
Allocated Blocks: 10
```

Please be sure to use the exact same output format as shown above.

### 3.2. Part II

In Part II, you will write a program `disklist`, with the routines already implemented for Part I, that displays the contents of the root directory or a given sub-directory in the file system.

Your program for Part II will be invoked as follows:

```
./disklist test.img /sub_dir
```

The directory listing should be formatted as follows:

1. The first column will contain:

    (a) F for regular files, or

    (b) D for directories;

    followed by a single space

2. then **10 characters** to show the file size, followed by a single space. Note that if the entry is a directory, there is no need to calculate the size of all the files or subdirectory inside this directory.
3. then **30 characters** for the file name, followed by a single space.
4. then the file creation date and time.

For example:

```
F       2560                        foo.txt 2015/11/15 12:00:00
F       5120                       foo2.txt 2015/11/15 12:00:00
F      48127                         makefs 2015/11/15 12:00:00
F          8                       foo3.txt 2015/11/15 12:00:00
```

### 3.3. Part III

In Part III, you will write a program `diskget` that copies a file from the file system image to the current directory in the host operating system. If the specified file is not found in the root directory **or** a given sub-directory of the file system image **or** the sub-directory does not exist in the file system, you should output the message

```
File not found.
```

and exit.

Your program for Part III will be invoked as follows:

```
./diskget test.img /sub_dir/foo2.txt foo.txt
```

### 3.4. Part IV

In Part IV, you will write a program `diskput` that copies a file from the current directory in the host operating system into the file system image, at the root directory or a given sub-directory. If the specified file is not found in the host operating system, you should output the message

```
File not found.
```

and exit.

Your program for Part IV will be invoked as follows:

```
./diskput test.img foo.txt /sub_dir/foo3.txt
```

If the specified `sub_dir` does not exist in the file system image yet, you should create the directory and then copy the file into it.

## 4. File System Specification

The FAT file system has three major components:
1. the super block,
2. the File Allocation Table (informally referred to as the FAT),
3. the directory structure.

Each of these three components is described in the subsections below.

### 4.1. File System Superblock

The first block (512 bytes) is reserved to contain information about the file system. The layout of the superblock is as follows:

```
Description                         Size            Default Value
File system identifier              8 bytes         CSC360FS
Block Size                          2 bytes         0x200
File system size (in blocks)        4 bytes         0x00001400
Block where FAT starts              4 bytes         0x00000001
Number of blocks in FAT             4 bytes         0x00000028
Block where root directory starts   4 bytes         0x00000029
Number of blocks in root dir        4 bytes         0x00000008
```

Note: Block number starts from 0 in the file system.

### 4.2. Directory Entries

Each directory entry takes 64 bytes, which implies there are 8 directory entries per 512 byte block.

Each directory entry has the following structure:

```
Description                 Size
Status                      1 byte
Starting Block              4 bytes
Number of Blocks            4 bytes
File Size (in bytes)        4 bytes
Creation Time               7 bytes
Modification Time           7 bytes
File Name                   31 bytes
unused (set to 0xFF)        6 bytes
```

The description of each field is as follows:

**Status**

This is a bit mask that is used to describe the status of the file. Currently only 3 of the bits are used. It is implied that only one of bit 2 or bit 1 can be set to 1. That is, an entry is either a normal file or it is a directory, not both.

```
Bit 0       set to 0 if this directory entry is available,
            set to 1 if it is in use
Bit 1       set to 1 if this entry is a normal file
Bit 2       set to 1 if this entry is a directory
```

**Starting Block**

This is the location on disk of the first block in the file

**Number of Blocks**

The total number of blocks in this file

**File Size**

The size of the file, in bytes. The size of this field implies that the largest file we can support is `2^32` bytes long.

**Creation Time**

The date and time when this file was created. The file system stores the system times as integer values in the format: `YYYYMMDDHHMMSS`

```
Field       Size
YYYY        2 bytes
MM          1 byte
DD          1 byte
HH          1 byte
MM          1 byte
SS          1 byte
```

**Modification Time**

The last time this file was modified. Stored in the same format as the **Creation Time** shown above.

**File Name**

The file name, null terminated. Because of the null terminator, the maximum length of any filename is 30 bytes. Valid characters are upper and lower case letters (`a-z`, `A-Z`), digits (`0-9`) and the underscore character (`_`).

### 4.3. File Allocation Table (FAT)

Each directory entry contains the starting block number for a file, let's say it is block number `X`.

To find the next block in the file, you should look at entry `X` in the FAT. If the value you find there does not indicate `End-of-File` (i.e., the last block, see below) then that value, say `Y`, is the next block number in the file.

That is, the first block is at block number `X`, you look in the FAT table at entry `X` and find the value `Y`. The second data block is at block number `Y`. Then you look in the FAT at entry `Y` to find the next data block number...

Continue this until you find the special value in the FAT entry indicating that you are at the last FAT entry of the file.

The FAT is really just a linked list, with the head of the list being stored in the "Starting Block" field in the directory entry, and the "next pointers" being stored in the FAT entries.

FAT entries are 4 bytes long (32 bits), which implies there are 128 FAT entries per block.

Special values for FAT entries are described in the following.

```
Value                   Meaning
0x00000000              This block is available
0x00000001              This block is reserved
0x00000002–0xFFFFFF00   Allocated blocks as part of files
0xFFFFFFFF              This is the last block in a file, i.e., End-of-File
```

### 4.4 Test File System Image

A sample file system image ([test.img](./createTestImage/test.img)) of size `3276800` bytes (512 bytes x 6400 blocks) is provided for you to test your implementation.
You can use `xxd test.img | more` or other hex editor to inspect the raw data and structure of the file system image.

Example output on the test image:

```
./diskinfo test.img
Super block information:
Block size: 512
Block count: 6400
FAT starts: 1
FAT blocks: 50
Root directory start: 51
Root directory blocks: 8

FAT information:
Free Blocks: 6341
Reserved Blocks: 49
Allocated Blocks: 10
```

The `test.img` is an empty file system image with an placeholder `current directory` `.` entry in the root directory.
```
./disklist test.img /
D          0                              . 2024/11/01 11:44:13
```

Putting a file `readme.txt` into the root directory and verify the success of the operation by `disklist` again.
```
./diskput test.img readme.txt /readme.txt

./disklist test.img /
D          0                              . 2024/11/01 11:44:13
F       3345                     readme.txt 2024/11/01 11:45:02
```

Using `diskget` to retrieve the file `readme.txt` back to the host operating system, the copied file should be identical to the original file.

```
./diskget test.img readme.txt readme-copy.txt

sha1sum readme.txt readme-copy.txt
0336ea6196863959d4965bc5342e02a8f2cc6ad5  readme.txt
0336ea6196863959d4965bc5342e02a8f2cc6ad5  readme-copy.txt
```

Another sample test image [`non-empty.img`](./createTestImage/non-empty.img) with subdirectories and files is provided.

```
./disklist non-empty.img /
D          0                              . 2024/11/20 19:57:09
D          0                        sub_Dir 2024/11/20 19:57:20
F         16                       test.txt 2024/11/20 19:57:37
F      31128                        cat.jpg 2024/11/20 19:57:54
```

```
./disklist non-empty.img /sub_Dir
F      45535                        201.jpg 2024/11/20 19:55:11
F         16                       test.txt 2024/11/20 19:57:37
```

SHA1 checksum of the files in the `non-empty.img`:

```
3c2ac88534a837ab138b1d94d26b63c8bc96ed28  201.jpg
1ab45bddca25936cd5563b65e9276bee27f5a48f  cat.jpg
dd9d29a308dc940a9c75f26532debb5e4bf6d6e9  test.txt
```

You can use `sha1sum <filename>` to check and compare the checksum of the files when copying them back with `diskget` to the host operating system.

## 5. Byte Ordering

Different hardware architectures store multi-byte data (like integers) in different orders. Consider the large integer: `0xDEADBEEF`

On the Intel architecture (Little Endian), it would be stored in memory as: `EF BE AD DE`

On the PowerPC (Big Endian), it would be stored in memory as: `DE AD BE EF`

Our file system will use Big Endian for storage. This will make debugging the file system by examining the raw data much easier.

This will mean that you have to convert all your integer values to Big Endian before writing them to disk. There are utility functions in `netinit/in.h` that do exactly that. (When sending data over the network, it is expected the data is in Big Endian format too.)

See the functions `htons()`, `htonl()`, `ntohs()` and `ntohl()` in the `man` manual.

The side effect of using these functions will be that your code will work on multiple platforms. (On machines that natively store integers in Big Endian format, like the original `PowerPC` Mac (not the Intel-based or the newer ARM ones), the above functions don't actually do anything but you should still use them!)

## 6. Submission Requirements

### 6.1. Version Control and Submission

Same as previous programming assignments, you shall use the department GitLab server `https://gitlab.csc.uvic.ca/` for version control during this assignment.

You shall create a new repository named `p3` under your personal assignment group, e.g., `https://gitlab.csc.uvic.ca/courses/2024091/CSC360_COSI/assignments/<netlink_id>/p3`, and commit your code to this repository.

You are required to commit and push your work-in-progress code at least weekly to your `p3` GitLab repository.

The submission of your `p3` assignment is the last commit before the due date. If you commit and push your code after the due date, the last commit before the due date will be considered as your final submission.

The TA will clone your `p3` repository, test and grade your code on `linux.csc.uvic.ca`.

**Note**: Use [`.gitignore`](https://git-scm.com/docs/gitignore/en) to ignore the files that should not be committed to the repository, e.g., the executable file, `.o` object files, etc.

### 6.2. Deliverables

Your submission will be marked by an automated script.

+ Your submission **must** include a `README.md`/`README.txt` file that describes whether you have implemented the individual features correctly. Including a README file is mandatory, and you will lose marks if you do not submit it in your GitLab repository.

+ Your submission **must** compile successfully with a single `make` command. The `Makefile` should be in the root directory of your `p3` repository. Your `Makefile` should produce the executables for Parts I–IV, including `diskinfo`, `disklist`, `diskget`, and `diskput` in the root directory of your repository after executing `make`.

+ Your program should return `0` on successful operations and non-zero values on failure. In bash, you can check the return value of the last command by using `echo $?`.

### 6.3. Rubric

#### Basic Functionality (Total: 1 Mark, 0.5 Mark each)

+ Your submission contains a `README.txt`/`README.md` that clearly describes which features you have implemented and whether they work correctly or not.
+ Your submission can be successfully compiled by `make` without errors. Four executables `diskinfo`, `disklist`, `diskget`, and `diskput` are produced in the root directory of your repository after `make`.

#### `diskinfo` (Total: 2 Mark)

+ The output of `./diskinfo test.img` follows the exact expected format as specified in Section 3.1. **(1 Mark)**
+ The output of `./diskinfo test.img` is correct, which will be tested against the provided test image, and one additional test image. **(Total: 1 Mark, 2 test cases, 0.5 Mark each)**

#### `disklist` (Total: 4 Mark)

+ The output of `./disklist test.img /sub_dir` follows the exact expected format as specified in Section 3.2. **(1 Mark)**
+ The output of `./disklist` is correct, which will be tested against several test cases, such as `./disklist test.img /`, `./disklist test.img /sub_dirA`, `./disklist test.img /sub_dirA/sub_dirB`, etc. **(Total: 3 Mark, 3 test cases, 1 Mark each)**

#### `diskget` (Total: 4 Mark)

+ `./diskget` outputs `File not found.` if the specified file is not found in the specified directory in the test image. **(0.5 Mark)**
+ `./diskget` copies the specified file from the specified directory in the test image to the current directory in the host operating system. **(Total: 2 Mark, 2 test cases, 1 Mark each)**
+ The copied file is identical to the original file in the test image. **(1.5 Mark)**

#### `diskput` (Total: 4 Mark)

+ `./diskput` outputs `File not found.` if the specified file is not found in the host operating system. **(0.5 Mark)**
+ `./diskput` copies the specified file from the host operating system to the specified directory in the test image, which can be verified by `./disklist` on the specified directory. **(Total: 2 Mark)**
+ When using `./diskget` to copy the file back to the host operating system, the copied file is identical to the original file in the host operating system. **(1 Mark)**
+ If the specified target subdirectory does not exist in the file system image when executing `./diskput test.img foo.txt /sub_dir/bar.txt`, a new directory `/sub_dir` should be created in the file system image, and the file `foo.txt` should be copied into the new directory. **(0.5 Mark)**

Note that the files for copying in `diskget` and `diskput` are not limited to text files. You should test with binary files as well, and verify they are identical with `sha1sum`.

## 7. Plagiarism

This assignment is to be done individually. You are encouraged to discuss the design of your solution with your classmates on Teams, but each person must implement their own assignment.

The markers will submit the code to an automated plagiarism detection program. Do not request/give source code from/to others; Do not put/use code at/from public repositories such as GitHub or so.

## A. An Exercise

### Q1. Consider the superblock shown below:

```
0000000: 4353 4333 3630 4653 0200 0000 1400 0000  CSC360FS........
0000010: 0001 0000 0028 0000 0029 0000 0008 0000  .....(...)......
0000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
```

+ (a) Which block does the FAT start from? How many blocks are used for the FAT?

+ (b) Which block does the root directory start from? How many blocks are used for the root directory?


### Q2. Consider the following block from the root directory:

```
0005200: 0300 0000 3100 0000 0500 000a 0007 d50b ....1...........
0005210: 0f0c 0000 07d5 0b0f 0c00 0066 6f6f 2e74 ...........foo.t
0005220: 7874 0000 0000 0000 0000 0000 0000 0000 xt..............
0005230: 0000 0000 0000 0000 0000 00ff ffff ffff ................
0005240: 0300 0000 3600 0000 0a00 0014 0007 d50b ....6...........
0005250: 0f0c 0000 07d5 0b0f 0c00 0066 6f6f 322e ...........foo2.
0005260: 7478 7400 0000 0000 0000 0000 0000 0000 txt.............
0005270: 0000 0000 0000 0000 0000 00ff ffff ffff ................
0005280: 0300 0000 4000 0000 5e00 00bb ff07 d50b ....@...^.......
0005290: 0f0c 0000 07d5 0b0f 0c00 006d 616b 6566 ...........makef
00052a0: 7300 0000 0000 0000 0000 0000 0000 0000 s...............
00052b0: 0000 0000 0000 0000 0000 00ff ffff ffff ................
00052c0: 0300 0000 9e00 0000 0100 0000 0807 d50b ................
00052d0: 0f0c 0000 07d5 0b0f 0c00 0066 6f6f 332e ...........foo3.
00052e0: 7478 7400 0000 0000 0000 0000 0000 0000 txt.............
00052f0: 0000 0000 0000 0000 0000 00ff ffff ffff ................
```

+ (a) How many files are listed in this directory? What are their names?

+ (b) How many blocks does the file makefs occupy on the disk?


### Q3. Given the root directory information from the previous question and the FAT table shown below:

```
0000200: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000210: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000220: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000230: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000240: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000250: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000260: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000270: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000280: 0000 0001 0000 0001 0000 0001 0000 0001 ................
0000290: 0000 0001 0000 0001 0000 0001 0000 0001 ................
00002a0: 0000 0001 0000 002a 0000 002b 0000 002c .......*...+...,
00002b0: 0000 002d 0000 002e 0000 002f 0000 0030 ...-......./...0
00002c0: ffff ffff 0000 0032 0000 0033 0000 0034 .......2...3...4
00002d0: 0000 0035 ffff ffff 0000 0037 0000 0038 ...5.......7...8
00002e0: 0000 0039 0000 003a 0000 003b 0000 003c ...9...:...;...<
00002f0: 0000 003d 0000 003e 0000 003f ffff ffff ...=...>...?....
```

+ (a) Which blocks does the file `foo.txt` occupy on the disk?

+ (b) Which blocks does the file `foo2.txt` occupy on the disk?

The End
