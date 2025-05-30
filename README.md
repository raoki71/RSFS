
@author -   Rei Aoki

# Spring 2025 ComS 352 Project 2

## RSFS (Rediculously Simple File System)


Before reading ahead, please note that there is _one minor issue_ worth mentioning.

When RSFS_write() first tries to write upto ENTIRE inode data blocks (i.e. NUM_POINTERS*BLOCK_SIZE), it writes to the 
    data_blocks[...] correctly. However, when it tries to update inode->length (api.c file), the behavior changes abruptly. I commented out the
    code in line 302 (api.c file). Also, please read the RSFS_write() documentation.

    Debug Guide - [test_advanced_write] test to write 253 character: ...
    
    Uncomment   
            File apic.c - 
                    line:302 inode->length += size      //HERE IS THE CODE ...
            File application.c -
                    line:234 char newText[300] = "000...999" 
                    line:242 RSFS_write(fd[i], newText, 253)
                    line:250 RSFS_print_inode_data_blocks()
    
To see the behavior of data blocks, please use the debug function, RSFS_print_inode_data_blocks() which is also commented out in application.c after RSFS_stat().
    
    RSFS_fseek(fd, 3)
    RSFS_write(fd, newText, 252) <---  works fine 
                                        since 3 + 252 < NUM_POINTERS*BLOCK_SIZE
    However, after  

    RSFS_fseek(fd, 3)
    write(fd, newText, 253) <--- RSFS_print_inode_data_blocks() outputs 
                                    some blank datalocks from Bob to George
    
    //this case should also work since 3 + 253 <= NUM_POINTERS*BLOCK_SIZE

Interestingly, second writing with RSFS_write() works perfectly fine upto the size of $256$. As you can see in the concurrent test cases.



### File def.h

Library addition for concurrent test cases

    line:10 #include <semaphore.h>

    line:39-46 struct inode{...}
        int read_count - count the number of readers reading in critical section
        sem_t rw_mutex - binary semaphore (lock) for both readers and writers
        sem_t mutex - lock used by readers to protect the race condition, reaad_count

    line:115-118 Added the declarationgs of helper functions
        int RSFS_print_data_blocks();
        int RSFS_print_inode_data_blocks();
        int RSFS_print_OFT();




### File: api.c 

Deletion of data contents in data_blocks. 

    line: 128-129   RSFS_delete(char file_name)
    Also the inode->block[i] is set to -1.
    

#### (1) Open file function:

    line:196-235 RSFS_open(char file_name, int access_flag)
    
Checks the validity of access_flag: The flag should be either RD or RDWR. Otherwise return $-1$ to indicate the flag is invalid.
    Surround with mutexes to ensure the safety for concurrency.
    First, search the root directory with the given name (int filename) to get the desired directory entry. This is achievd by calling search_dir(filename).
    If the file name is present, get the associated inode number.
    Note that each entry of the Open File Table corresponds to a file descriptor, i.e. fd. 
    Next, given the obtained inode number, set the necessary fields (fd, used, access_flag, inode_number, position) with given arguments.
    
Return the fd $\geq$ 0.

#### <ins> Concurrent Case for open() </ins>
I used semaphore library to impelement read-write lock, read count, and mutex.
If the access flag is RDWR, the open is exclusive to a single RDWR thread, so sem_wait() to decreament semaphore value. If the access flag is RDONLY, while incrementing the read count with mutex lock, it checks the read count so that the calling thread needs to wait util RDWR is done.

#### (2) Append function:

    line:241-335 RSFS_append(int fd, void *buf, int size)

Appends the given size of a buffer to the end of the file content.
Given the file descriptor fd, it obtains an open file entry from the open file table. It will acquire an inode associated with the fd.
Given the file length of the current inode, it iterates from the last position upto the maximum number of direct pointers.

The start index is given by the following computation:
    
    int i = inode->length/BLOCK_SIZE

The quotient obtained from above will be the index last of the inode block which contains a pseudo direct pointer number. Depending on the last position of the existent data blocks, buffer will be split into two portions. The first portion will be appended to the remaining space on the data block (after the last position). The second buffer portion will be carried over to a next new data block. Repeat until all the buffer is appended or no data space is available. Finally, set the correct position according to the end of the file, which is the length of the inode.
    
Return the number of bytes appeneded to the file. 

#### (3) Fseek function:
    line: 342-371 RSFS_fseek(int fd, int offset)
    
Given a fd and offset value, update the current position of the file in the open file table. The range of offset is from $0$ to inode length. 
If the given offset is out of range, simply return current position without updating anything.

#### (4) Read function:
    line: 375-453 RSFS_read(int fd, void *buf, int size) (line:341-421)
    
Given a file descriptor, inode, the read will start from the current_position on the open file. 
The start offset and end offset determines where it wants to start reading and where it wants to stop reading.

start_offset is modulo of block size with respect to current position: 
    
    current_position % BLOCK_SIZE

end_offset is modulo of block size with respect to buffer size: 
    
    size % BLOCK_SIZE - 1
    
Then, it iterates through the data block, reading each data block, based on the given parameter, size.
The final-read-pointer determines the last data block that needs to be read.

It is noted that there are four cases to consider while reading:
    
    Case 1: i == 0 && i == fina_read_pointer
            If the first direct pointer is also the final-read-pointer, it will read with 
            the single block.
    Case 2: i == 0 && i != final_poointer
            If the first pointer is not the final-read-pointer, it will read from start 
            offset to BLOCK SIZE.
    Case 3: i != 0 && i != fina_read_pointer
            If the direct pointer is somewhere in the middle between the first and final-
            read-pointer, it will read an entire single BLOCK SIZE.
    Case 4: i != 0 && i == fina_read_pointer
            If the final-read-pointer is different from the first direct pointer, it will 
            read from start position of a block (i.e. 0) to end offset.

Finally, update the current position in the open file entry according to the number of bytes read.

Return the number of bytes read.

#### (5) Close function:
    
    line: 457-488 RSFS_close(int fd)

First, checks the validity of file descriptor: $0$ $\leq$ fd $<$ NUM_OPEN_FILE

Given the valid fd, call the system call free_open_file_entry to free an open file entry. It simply set the field, used = $0$. Returns $0$ if it succeeds, otherwise $-1$.

#### <ins> Concurrent Case for close() </ins>
If the access flag is RDWR, the close is exclusive to a single RDWR thread, so sem_post() to signal a thread to wake up if there is any. If the access flag is RDONLY, while decrementing the read count with mutex lock, it wakes up a thread if the count is $0$. Then finally close the file.

#### (6) Write function:

    line: 493-596 RSFS_write(int fd, void *buf, int size)
    
Write function utilizes the similar operation of RSFS_append(). It first writes to the buffer from the current position through the end of the same block. It then clears the remaing garbage data blocks (BLOCK_SIZE_GARBAGE). After the above operations are done, simply pass the split buffer to RSFS_append() to do the rest of the job.

Return the number of bytes written.

#### (*) Debug functions:
- Print out all data blocks (0 ... NUM_DBLOCKS)

        line: 599-604 RSFS_print_data_blocks()

- Print out each inode data blocks for each inode[i]: 

    0 <= i < NUM_INODES all direct pointers[j]: 0 <= j < NUM_POINTERS

        line: 607-617 RSFS_print_inode_data_blocks()

- Print out all the open file table entries. Associated inode data blocks are printed out.
       
        line: 620-632 RSFS_print_OFT()
    