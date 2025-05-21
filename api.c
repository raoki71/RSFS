/*
    implementation of API
*/

#include "def.h"

pthread_mutex_t mutex_for_fs_stat;//mutex used by RSFS_stat()

//initialize file system - should be called as the first thing before accessing this file system 
int RSFS_init(){
    char *debugTitle = "RSFS_init";

    //initialize data blocks
    for(int i=0; i<NUM_DBLOCKS; i++){
      void *block = malloc(BLOCK_SIZE); //a data block is allocated from memory
      if(block==NULL){
        printf("[%s] fails to init data_blocks\n", debugTitle);
        return -1;
      }
      data_blocks[i] = block;  
    } 

    //initialize bitmaps
    for(int i=0; i<NUM_DBLOCKS; i++) data_bitmap[i]=0;
    pthread_mutex_init(&data_bitmap_mutex,NULL);
    for(int i=0; i<NUM_INODES; i++) inode_bitmap[i]=0;
    pthread_mutex_init(&inode_bitmap_mutex,NULL);    

    //initialize inodes
    for(int i=0; i<NUM_INODES; i++){
        inodes[i].length=0;
    }
    pthread_mutex_init(&inodes_mutex,NULL); 

    //initialize open file table
    for(int i=0; i<NUM_OPEN_FILE; i++){
        struct open_file_entry entry=open_file_table[i];
        entry.used=0; //each entry is not used initially
        pthread_mutex_init(&entry.entry_mutex,NULL);
        entry.position=0;
        entry.access_flag=-1;
        // entry.ref=0;
        entry.inode_number=-1;
    }
    pthread_mutex_init(&open_file_table_mutex,NULL); 

    //initialize root inode
    root_inode_number = allocate_inode();
    if(root_inode_number<0){
        printf("[%s] fails to allocate root inode\n", debugTitle);
        return -1;
    }
    pthread_mutex_init(&root_dir_mutex,NULL); 
    
    
    //initialize mutex_for_fs_stat
    pthread_mutex_init(&mutex_for_fs_stat,NULL);

    //return 0 means success
    return 0;
}


//create file
//if file does not exist, create the file and return 0;
//if file_name already exists, return -1; 
//otherwise (other errors), return -2.
int RSFS_create(char file_name){

    //search root_dir for dir_entry matching provided file_name
    struct dir_entry *dir_entry = search_dir(file_name);

    if(dir_entry){//already exists
        printf("[create] file (%c) already exists.\n", file_name);
        return -1;
    }else{

        if(DEBUG) printf("[create] file (%c) does not exist.\n", file_name);

        //get a free inode 
        char inode_number = allocate_inode();
        if(inode_number<0){
            printf("[create] fail to allocate an inode.\n");
            return -2;
        } 
        if(DEBUG) printf("[create] allocate inode with number:%d.\n", inode_number);

        //insert (file_name, inode_number) to root directory entry
        dir_entry = insert_dir(file_name, inode_number);
        if(DEBUG) printf("[create] insert a dir_entry with file_name:%c.\n", dir_entry->name);
        
        return 0;
    }
}



//delete file
int RSFS_delete(char file_name){

    char debug_title[32] = "[RSFS_delete]";

    //to do: find the corresponding dir_entry
    struct dir_entry *dir_entry = search_dir(file_name);
    if(dir_entry==NULL){
        printf("%s director entry does not exist for file (%c)\n", 
            debug_title, file_name);
        return -1;
    }

    //to do: find the corresponding inode
    int inode_number = dir_entry->inode_number;
    if(inode_number<0 || inode_number>=NUM_INODES){
        printf("%s inode number (%d) is invalid.\n", 
            debug_title, inode_number);
        return -2;
    }
    pthread_mutex_lock(&inodes_mutex);
    struct inode *inode = &inodes[inode_number];

    //to do: find the data blocks, free them in data-bitmap
    pthread_mutex_lock(&data_bitmap_mutex);
    for(int i = 0; i <= inode->length/BLOCK_SIZE; i++){
        int block_number = inode->block[i];
        if(block_number>=0) {
            data_bitmap[block_number]=0; 
            //ADDITION TO ORIGINAL CODE: delete the corresponding data block data  
            memset(data_blocks[block_number], 0, BLOCK_SIZE);
            inode->block[i] = -1;
        }
    }
    pthread_mutex_unlock(&data_bitmap_mutex);
    pthread_mutex_unlock(&inodes_mutex);

    //to do: free the inode in inode-bitmap
    pthread_mutex_lock(&inode_bitmap_mutex);
    inode_bitmap[inode_number]=0;
    pthread_mutex_unlock(&inode_bitmap_mutex);

    //to do: free the dir_entry
    int ret = delete_dir(file_name);
    
    return 0;
}


//print status of the file system
void RSFS_stat(){

    pthread_mutex_lock(&mutex_for_fs_stat);


    printf("\nCurrent status of the file system:\n\n %16s%10s%10s\n", "File Name", "Length", "iNode #");

    //list files
    for(int i=0; i<BLOCK_SIZE/sizeof(struct dir_entry); i++){
        struct dir_entry *dir_entry = (struct dir_entry *)root_data_block + i;
        if(dir_entry->name==0) continue;
        
        int inode_number = dir_entry->inode_number;
        struct inode *inode = &inodes[inode_number];
        
        printf("%16c%10d%10d\n", dir_entry->name, inode->length, inode_number);
    }
    
    
    //data blocks
    int db_used=0;
    for(int i=0; i<NUM_DBLOCKS; i++) db_used+=data_bitmap[i];
    printf("\nTotal Data Blocks: %4d,  Used: %d,  Unused: %d\n", NUM_DBLOCKS, db_used, NUM_DBLOCKS-db_used);

    //inodes
    int inodes_used=0;
    for(int i=0; i<NUM_INODES; i++) inodes_used+=inode_bitmap[i];
    printf("Total iNode Blocks: %3d,  Used: %d,  Unused: %d\n", NUM_INODES, inodes_used, NUM_INODES-inodes_used);

    //open files
    int of_num=0;
    for(int i=0; i<NUM_OPEN_FILE; i++) of_num+=open_file_table[i].used;
    printf("Total Opened Files: %3d\n\n", of_num);

    pthread_mutex_unlock(&mutex_for_fs_stat);
}





//------ implementation of the following functions is incomplete --------------------------------------------------------- 



//open a file with RSFS_RDONLY or RSFS_RDWR flags
//return a file descriptor if succeed; 
//otherwise return a negative integer value
int RSFS_open(char file_name, int access_flag){
    //to do: check to make sure access_flag is either RSFS_RDONLY or RSFS_RDWR
    if(access_flag!=RSFS_RDONLY && access_flag!=RSFS_RDWR){
        printf("[open] access_flag is invalid.\n");
        return -1;
    }
    
    int fd = -1;
    //to do: find dir_entry matching file_name
    struct dir_entry *dir_entry = search_dir(file_name);
    if(dir_entry==NULL)
        return fd; //there is no such file, so return invalid fd=-1

    //to do: find the corresponding inode
    int inode_number = dir_entry->inode_number;
    struct inode *inode = &inodes[inode_number];

    //concurrent handler: used semaphore to implement reader-writer problem
    if(access_flag==RSFS_RDWR) {
        //decrement the semaphore value i.e. RDWR is exclusive access
        sem_wait(&inode->rw_mutex);
    }
    if(access_flag==RSFS_RDONLY) {
        //lock the race condtion read_count
        sem_wait(&inode->mutex);
        inode->read_count++;
        //condition to check the calling thread needs to wait ifthe read_count is 1
        if(inode->read_count==1) {
            sem_wait(&inode->rw_mutex);
        }
        //unlock the race condition
        sem_post(&inode->mutex);
    }
    
    //to do: find an unused open-file-entry in open-file-table and fill the fields of the entry properly
    fd = allocate_open_file_entry(access_flag, inode_number);
    
    //to do: return the index of the open-file-entry in open-file-table as file descriptor
    return fd;
}



//append the content in buf to the end of the file of descriptor fd
//return the number of bytes actually appended to the file
int RSFS_append(int fd, void *buf, int size){
    //to do: check the sanity of the arguments: 
    // fd should be in [0,NUM_OPEN_FILE] and size>0.
    if(fd<0 || fd>=NUM_OPEN_FILE) {
        printf("[RSFS_append] fd is invalid.\n");
        return 0;
    }
    if(size <= 0) {
        printf("[RSFS_append] size is invalid\n");
        return 0;
    }
    //to do: get the open file entry corresponding to fd
    struct open_file_entry *of_entry = &open_file_table[fd];
    //to do: check if the file is opened with RSFS_RDWR mode; 
    // otherwise return zero
    if(of_entry->access_flag!=RSFS_RDWR) return 0;
    //to do: get the current position
    int current_position = of_entry->position;
    //to do: get the inode 
    int inode_number = of_entry->inode_number;


    //to do: append the content in buf to the data blocks of the file 
    // from the end of the file; allocate new block(s) when needed 
    // - (refer to lecture L22 on how)

    //allocate an empty block to this inode associated with fd
    if(inode_number<0){
        printf("[RSFS_append] fails to allocate root inode\n");
        return -1;
    }
    //direct pointers up to 8
    struct inode *inode = &inodes[inode_number];
    char direct_pointer; //direct pointer
    int num_bytes_appended = 0;  //the number of bytes appened


    //iterates from the last entry of inode data block so it achieves the append funcionality
    //the quotient of inode->length/BLOCK_SIZE is the last index of the current inode block
    for(int i=inode->length/BLOCK_SIZE; i<NUM_POINTERS; i++) {
        direct_pointer = inode->block[i];
        //if the given direct pointer points nothing
        //allocate a new data block to a direct pointer of this inode
        if(direct_pointer==-1) {
            if((direct_pointer = allocate_data_block())==-1) return 0;
            inode->block[i] = direct_pointer; //update the direct pointer number
        }

        //this is an important variable which tells the remaining number of bytes to fill in for the buffer
        //if the buffer exceeds its size, the loop allocates a new block for it
        int BLOCK_SIZE_LEFT = BLOCK_SIZE - strlen(data_blocks[direct_pointer]);
        if(BLOCK_SIZE_LEFT>0) {
            if(BLOCK_SIZE_LEFT>=size) {
                //CASE: a buffer can fit in the remaing block space
                if(i==NUM_POINTERS-1 && BLOCK_SIZE==size) {
                    void *buf1 = (void*) malloc(size-1);
                    memcpy(buf1, buf, size-1);
                    strcat(data_blocks[direct_pointer], buf1);
                    char lastchar = ((char*)buf)[size-1];
                    ((char*)data_blocks[direct_pointer])[BLOCK_SIZE-1] = lastchar;

                    // inode->length += size;   //HERE IS THE CODE CAUSING TROUBLE when fseek(fd, 3) and write(fd, buf, 253)

                    num_bytes_appended += size-1;
                    break;
                }
                inode->length += size;
                num_bytes_appended += size;
                strcat(data_blocks[direct_pointer], buf);
                break;
            } else {
                //CASE: a buffer is bigger than the size of remaing block size
                inode->length += BLOCK_SIZE_LEFT;
                num_bytes_appended += BLOCK_SIZE_LEFT;
                size -= BLOCK_SIZE_LEFT;

                //when the buffer size is bigger than a block size, it will fill in the remaining portion of the block
                //pointed to by the direct pointer, and then the split buffer will be carried over to a next block
                void *buf1 = (void*) malloc(sizeof(char) * BLOCK_SIZE_LEFT);
                void *buf2 = (void*) malloc(sizeof(char) * size);
                memmove(buf1, buf, BLOCK_SIZE_LEFT);
                memmove(buf2, (char*) buf+BLOCK_SIZE_LEFT, size);
                buf = buf2;

                strcat(data_blocks[direct_pointer], buf1);
                free(buf1);
            }
        }
    }
    //to do: update the current position in open file entry
    of_entry->position = inode->length - 1;

    //to do: return the number of bytes appended to the file
    return num_bytes_appended;
}




//update current position of the file (which is in the open_file_entry) to offset
//return -1 if fd is invalid; otherwise return the current position after the update
int RSFS_fseek(int fd, int offset){
    //to do: sanity test of fd; if fd is not valid, return -1    
    if(fd<0 || fd>=NUM_OPEN_FILE) {
        printf("[RSFS_fseek] fd is invalid.\n");
        return -1;
    }

    //to do: get the correspondng open file entry
    struct open_file_entry *of_entry = &open_file_table[fd];

    //to do: get the current position
    int current_position = of_entry->position;

    //to do: get the inode and file length
    int inode_number = of_entry->inode_number;
    struct inode *inode = &inodes[inode_number];
    int length = inode->length;

    //to do: check if argument offset is not within 0...length, 
    // do not proceed and return current position
    if(offset<0 || offset>length)
       return current_position;

    //to do: update the current position to offset, and 
    // return the new current position
    current_position = offset;
    of_entry->position = current_position;

    return current_position;
}

//read up to size bytes to buf from file's current position towards the end
//return -1 if fd is invalid; otherwise return the number of bytes actually read
int RSFS_read(int fd, void *buf, int size){
    //to do: sanity test of fd and size (the size should not be negative)    
    if(fd<0 || fd>=NUM_OPEN_FILE) {
        printf("[RSFS_append] fd is invalid.\n");
        return 0;
    }
    if(size <= 0) {
        printf("[RSFS_append] size is invalid\n");
    }

    //to do: get the corresponding open file entry
    struct open_file_entry *of_entry = &open_file_table[fd];    

    //to do: get the current position
    int current_position = of_entry->position;
    
    //to do: get the corresponding inode 
    int inode_number = of_entry->inode_number;
    struct inode *inode = &inodes[inode_number];
    int length = inode->length;


    //to do: read from the file

    //this is going to be the actual read count
    //the # bytes read = size                   if the buf size is <= inode length - current_position
    //      or         = length - start_offset  if the buf size is >  inode length - current_position
    int num_bytes_read;
    //this is the index where the read starts in a data block
    int start_offset = current_position % BLOCK_SIZE;
    //this is the index where the read ends in a data block
    int final_offset;
    //the final-read-pointer that points to nonempty data block needed to be read in the end
    int final_read_pointer;
    //compute the number of bytes that will be read, final offset and final read pointer (which direct pointer)
    if(size+current_position <= length) {
        //case: the given buffer size is smaller than the inode length
        num_bytes_read = size;
        final_offset = (size+current_position) % BLOCK_SIZE - 1;
        final_read_pointer = (size + current_position) / BLOCK_SIZE;
    } else {
        //case: the given buffer size is larger than the inode lenght
        num_bytes_read = length - current_position;
        final_offset = (length - 1) % BLOCK_SIZE;
        final_read_pointer = (length - current_position - 1) / BLOCK_SIZE;
    }

    //direct pointer loop starts from current position divided by BLOCK_SIZE, which determines the correct direct pointer index
    for(int i=current_position/BLOCK_SIZE; i<NUM_POINTERS; i++) {
        char direct_pointer = inode->block[i];
        if(i==0 && i==final_read_pointer) {
            //CASE: read from start_offset to final_offset in the very first block pointed to by the first direct pointer
            void *buf1 = (void*) malloc(sizeof(char) * (final_offset-start_offset+1));
            memcpy(buf1, (char*) data_blocks[direct_pointer]+start_offset,sizeof(char)*(final_offset+1));
            strcat(buf, buf1);
            break;
        } else if(i==0 && i!=final_read_pointer) {
            //CASE: read from start_offset to BLOCK_SIZE
            void *buf1 = (void*) malloc(sizeof(char) * (BLOCK_SIZE - start_offset));
            memmove(buf1, (char*) data_blocks[direct_pointer]+start_offset, BLOCK_SIZE - start_offset);
            strcat(buf, buf1);
            free(buf1);
        } else if(i!=0 && i!=final_read_pointer) {
            //CASE: read entire BLOCK_SIZE
            strcat(buf, data_blocks[direct_pointer]);
        } else {
            //CASE: read from start position of block (i.e 0) to final_offset
            data_blocks[direct_pointer];
            strcat(buf, data_blocks[direct_pointer]);
            break;
        }
    }

    //to do: update the current position in open file entry
    of_entry->position += num_bytes_read - 1;

    //to do: return the actual number of bytes read
    return num_bytes_read;
}


//close file: return 0 if succeed; otherwise return -1
int RSFS_close(int fd){
    //to do: sanity test of fd    
    if(fd<0 || fd>=NUM_OPEN_FILE) {
        printf("[RSFS_close] fd is invalid.\n");
        return -1;
    }
    //to do: get the corresponding open file entry
    struct open_file_entry *of_entry = &open_file_table[fd];
    int inode_number = of_entry->inode_number;
    //to do: get the corresponding inode 
    struct inode *inode = &inodes[inode_number];

    //concurrent handler: use semaphore to post threads singaling them to wake
    if(of_entry->access_flag==RSFS_RDWR) {
        //RDWR is exclusive. Once it is done writing signal any waiting threads.
        sem_post(&inode->rw_mutex);
    }
    if(of_entry->access_flag==RSFS_RDONLY) {
        //mutex for the race condition read count
        sem_wait(&inode->mutex);
        //decrement the read count to close the file
        inode->read_count--;
        //wakes up a thread if any
        if(inode->read_count==0)
            sem_post(&inode->rw_mutex);
        sem_post(&inode->mutex);
    }

    //to do: release this open file entry in the open file table
    free_open_file_entry(fd);
    return 0;
}



//write the content of size (bytes) in buf to the file (of descripter fd) 
int RSFS_write(int fd, void *buf, int size){
    //to do: sanity test of fd and size (the size should not be negative)    
    if(fd<0 || fd>=NUM_OPEN_FILE) {
        printf("[RSFS_write] fd is invalid.\n");
        return 0;
    }
    if(size <= 0) {
        printf("[RSFS_write] size is invalid\n");
        return 0;
    }

    //to do: get the corresponding open file entry
    struct open_file_entry *of_entry = &open_file_table[fd];    

    //to do: get the current position
    int current_position = of_entry->position;
    
    //to do: get the corresponding inode 
    int inode_number = of_entry->inode_number;
    struct inode *inode = &inodes[inode_number];
    int length = inode->length;

    //return value: the number of bytes written
    int num_bytes_written = 0;
    //start and final offset within a certain block determined by the current position and the inode length, respectively.
    int start_offset = current_position % BLOCK_SIZE;
    int final_offset = length % BLOCK_SIZE;

    //the final-read-pointer that points to nonempty data block needed to be read in the end
    int start_write_pointer = current_position / BLOCK_SIZE;
    int final_write_pointer = (size + current_position) / BLOCK_SIZE;

    //the nonempty data block pointed to by the final direct pointer in the inode
    int final_direct_pointer = (length - 1) / BLOCK_SIZE;

    //write operation
    for(int i=start_write_pointer; i<NUM_POINTERS; i++) {
        int direct_pointer = inode->block[i];
        if(i==start_write_pointer) {
            //if the given direct pointer points nothing
            if(direct_pointer==-1)  break;

            int data_length = strlen(data_blocks[direct_pointer]);
            
            //cut the data block content into separate parts based on the start offset
            void *buf1 = (void*) malloc(sizeof(char) * BLOCK_SIZE);
            memset(buf1, 0, BLOCK_SIZE);
            memmove(buf1, data_blocks[direct_pointer], start_offset);

            //clear the second parts which will be garbage
            memset(data_blocks[direct_pointer], 0, BLOCK_SIZE);
            data_blocks[direct_pointer] = buf1;

            int BLOCK_SIZE_GARBAGE = BLOCK_SIZE - start_offset;
            if(size >= BLOCK_SIZE_GARBAGE) {
                //split the buffer into parts so that the write operation can be done iteratively per each block
                void *buf2 = (void*) malloc(sizeof(char) * BLOCK_SIZE_GARBAGE);
                void *buf3 = (void*) malloc(sizeof(char) * (size-BLOCK_SIZE_GARBAGE));
                memcpy(buf2, buf, BLOCK_SIZE-start_offset);
                memmove(buf3, buf+BLOCK_SIZE_GARBAGE, size-BLOCK_SIZE_GARBAGE);

                //write into the garbage portion from buffer of the same size
                strcat(data_blocks[direct_pointer], buf2);
                buf = buf3;
                free(buf2);

                //update the field values accordingly
                inode->length -= data_length - start_offset;
                inode->length += BLOCK_SIZE_GARBAGE;
                num_bytes_written += BLOCK_SIZE_GARBAGE;
                //decrement the size which will be passed to next for loop iteration
                size -= BLOCK_SIZE_GARBAGE;
            } else {
                //case: if the buffer can be fit in wihtin the remaining data block, write all of them
                strcat(data_blocks[direct_pointer], buf);
                inode->length -= data_length - start_offset;
                inode->length += size;
                num_bytes_written += size;
                if(final_write_pointer == final_direct_pointer) {
                    return num_bytes_written;
                }
                //size of buffer is now zero
                size = 0;
            }
        } else {
            //CASE: if the write needs to write beyond the first block where the current position resides, clear all contents in the tail

            //if the given direct pointer points nothing
            if(direct_pointer==-1) break;
            //clear all characters in the data block
            int data_length = strlen(data_blocks[direct_pointer]);
            memset(data_blocks[direct_pointer], 0, data_length * (sizeof(char)));
            inode->length -= data_length;
            
            if(final_write_pointer < final_direct_pointer) {
                free_data_block(direct_pointer);
            }
        }
    }
    //if all the above operation is done, pass the same fd, but the split buf and size into append() function to do the rest.
    if(size > 0) num_bytes_written += RSFS_append(fd, buf, size);

    return num_bytes_written;
}

//DEBUG function: printing out direct pointer blocks for each inode
int RSFS_print_data_blocks() {
    for(int i = 0; i < NUM_DBLOCKS; i++) {
        printf("db[%d]: %s\n", i , data_blocks[i]);
    }
    return 0;
}

//DEBUG function: printing out inodes blocks pointed to by direct pointers
int RSFS_print_inode_data_blocks() {
    for(int i=0; i<NUM_INODES; i++) {
        printf("inode[%d]\n", i);
        struct inode *inode = &inodes[i];
        for(int j=0; j<NUM_POINTERS; j++) {
            int dir_ptr = inode->block[j];
            printf("[%d]->data block[%d]: %s\n", j, dir_ptr, data_blocks[dir_ptr]);
        }
    }
    return 0;
}

//DEBUG function: printing out OFT
int RSFS_print_OFT() {
    for(int i=0; i<NUM_INODES; i++) {
        printf("Entry[%d]: \n", i);
        struct open_file_entry *entry = &open_file_table[i];
        int inode_number = entry->inode_number;
        struct inode *inode = &inodes[inode_number];
        for(int j=0; j<NUM_POINTERS; j++) {
            int dir_ptr = inode->block[j];
            printf("inode %d: direct[%d]-> %s\n", inode_number, j, data_blocks[dir_ptr]);
        }
    }
    return 0;
}