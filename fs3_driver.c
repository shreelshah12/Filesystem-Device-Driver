////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the FS3 storage system.
//
//   Author        : Shreel Shah

// Includes
#include <string.h>
#include <stdlib.h>
#include <cmpsc311_log.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <math.h>
#include <fs3_cache.h>

//
// Defines
#define SECTOR_INDEX_NUMBER(x) ((int)(x/FS3_SECTOR_SIZE))
//
// Static Global Variables
int mountStatus = -1; //checker for mountstatus
FS3MetaTrack *tracks; //list of track metadata structures
int sizeOfTracks = 0; //sizetrack
int maxTracks = 1;
extern FS3_CACHE_SCORES scores;
//
// Implementation



////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_check_file_count
// Description  : This function dynamically increases the file size
//
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_check_file_count() {
	//Condition triggered if current tracks size is reached. Doubles the size of tracks
	if(maxTracks == sizeOfTracks){
		tracks = realloc(tracks, maxTracks*2*sizeof(FS3MetaTrack));
		if(tracks == NULL){
			free(tracks);
			logMessage(LOG_ERROR_LEVEL, "Could not reallocate brother");
			return(-1);
		}
		maxTracks*=2;
		return(0);
	}
	return(0);
}

//command block structure
FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret){
	FS3CmdBlk value = 0x0;
	value = (uint64_t)op << 60;
	value = value | ((uint64_t)sec << 44);
	value = value | ((uint64_t)trk << 12); 
	value = value | ((uint64_t)ret << 11);
	return value; //construct cmdblck according to bit pattern given: shift to allocate specific bits for each 
}
int deconstruct_fs3_cmdblock(FS3CmdBlk cmdblock, uint8_t *op, uint16_t *sec, uint32_t *trk, uint8_t *ret){
	*op = cmdblock >> 60;
	*sec = cmdblock >> 44;
	*trk = cmdblock >> 12;
	*ret = cmdblock >> 11;
	return 0; //deconstruct is essentially opposite of construct 
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_mount_disk
// Description  : FS3 interface, mount/initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_mount_disk(void) {
	uint8_t op;
	uint16_t sec;
	uint32_t trk;
	uint8_t ret;

	if(mountStatus != -1){
		return(-1); //check mounting status
	}
	//implement the mount opcodes and call construct/deconstruct for cmdblock
	tracks = malloc(sizeof(FS3MetaTrack)); 
	FS3CmdBlk mountCommand = construct_fs3_cmdblock(0, 0, 0, 0);
	FS3CmdBlk returnCommand = fs3_syscall(mountCommand, NULL);
	deconstruct_fs3_cmdblock(returnCommand, &op, &sec, &trk, &ret);
	mountStatus = ret; //update mountstatus
	return (ret);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_unmount_disk
// Description  : FS3 interface, unmount the disk, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_unmount_disk(void) {
	uint8_t op;
	uint16_t sec;
	uint32_t trk;
	uint8_t ret;

	if (mountStatus != 0)
	{
		return (-1); //check mounting status
	}
	//free every track
	for(int i = 0; i<sizeOfTracks; i++){
		free(tracks[i].Name);
		tracks[i].Name = NULL; 
	}
	free(tracks);
	tracks = NULL;
	FS3CmdBlk unmountCommand = construct_fs3_cmdblock(4, 0, 0, 0); //opcode for unmount 
	FS3CmdBlk returnCommand = fs3_syscall(unmountCommand, NULL); //syscall
	deconstruct_fs3_cmdblock(returnCommand, &op, &sec, &trk, &ret);

	if (ret == -1)
	{
		return (-1); 
	}
	mountStatus = -1; //update mountstatus
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t fs3_open(char *path) {
	//loops to find correct track
	for(int i = 0; i <sizeOfTracks; i++){
		if(strcmp(tracks[i].Name, path) == 0){
			if(tracks[i].Status == 0){
				return(-1);
			}
			tracks[i].Status = 0;
			return(0);
		} 
	}
	fs3_check_file_count();

	if (mountStatus == -1)
	{
		fs3_mount_disk(); //mount
	}
	//setting structure parameters
	FS3MetaTrack track = {0};
	track.Handle = sizeOfTracks; 
	track.Name = malloc(strlen(path)+1);
	strcpy(track.Name, path);	
	track.Status = 0;
	track.TrackNo = sizeOfTracks;
	memcpy(&tracks[sizeOfTracks], &track, sizeof(FS3MetaTrack));

	sizeOfTracks++;


	return (track.Handle);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_close(int16_t fd) {
	if (fd >= sizeOfTracks || tracks[fd].Status == -1)
	{
		return (-1);  //checks to ensure closing correct file and if close can be applied
	}
	tracks[fd].Pointer = 0; //reset pos
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t fs3_read(int16_t fd, void *buf, int32_t count) {
	uint8_t op;
	uint16_t sec;
	uint32_t trk;
	uint8_t ret;

	char *readBuff = (char *)buf;
	char *readFiller = (char *)malloc(FS3_SECTOR_SIZE); //local buffer

	if (fd >= sizeOfTracks) //check if valid fd
	{
		return (-1);
	}

	if (tracks[fd].Size - tracks[fd].Pointer < count) //check if read goes past size
	{
		return (-1);
	}
	//call seek for right file check
	FS3CmdBlk seekCommand = construct_fs3_cmdblock(1, 0, tracks[fd].Handle, 0);
	FS3CmdBlk returnCommand = fs3_syscall(seekCommand, NULL);
	deconstruct_fs3_cmdblock(returnCommand, &op, &sec, &trk, &ret);
	
	if (op == -1)
	{
		printf("Seek operation failed.");
		return(-1);
	}

	int bytesRead = 0;
	int initialBytes = tracks[fd].Pointer % 1024; //bytes in sector
	//loop as many times as sectors since we can only write to 1 sector at a time
	while (bytesRead < count)
	{
		if(fs3_get_cache(fd, floor(tracks[fd].Pointer/FS3_SECTOR_SIZE))){
			memcpy(readFiller, fs3_get_cache(fd, floor(tracks[fd].Pointer/FS3_SECTOR_SIZE)), FS3_SECTOR_SIZE);
			scores.FS3_HITS++;
		}
		else{
			scores.FS3_MISSES++;
					FS3CmdBlk readCommand = construct_fs3_cmdblock(2, floor(tracks[fd].Pointer/FS3_SECTOR_SIZE), 0, 0);
		FS3CmdBlk returnCommand = fs3_syscall(readCommand, readFiller);
		deconstruct_fs3_cmdblock(returnCommand, &op, &sec, &trk, &ret);

		if (op == -1) //check
		{
			printf("Read operation failed.");
			return(-1);
		}
		}


		int sectorBytesLeft = FS3_SECTOR_SIZE - initialBytes; // Between 0 and 1024
		int bytesToRead = sectorBytesLeft; 

		if (count - bytesRead < FS3_SECTOR_SIZE) // If there is less than sectorBytesLeft bytes to read A.K.A. Last while loop run
		{
			bytesToRead = fmin(count - bytesRead, sectorBytesLeft); //bytedtoread will be the min of the two - last while run
		}

		memcpy(&readBuff[bytesRead], &readFiller[initialBytes], bytesToRead); //memcopy local buffer to buffer passed

		tracks[fd].Pointer = tracks[fd].Pointer + bytesToRead;
		initialBytes = tracks[fd].Pointer % 1024; //update initialbytes based on new pointer (pos) 
		bytesRead += bytesToRead; //increment byteswritten
	}
	free(readFiller);
	readFiller = NULL;

	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t fs3_write(int16_t fd, void *buf, int32_t count) {
	
	uint8_t op;
	uint16_t sec;
	uint32_t trk;
	uint8_t ret;

	//check if file exists and is open (currently not working oops)
	if (fd >= sizeOfTracks || tracks[fd].Status == -1)
	{
		return (-1);
	}
	char * writeBuff = (char *)buf;
	char * writeFiller = (char *) calloc(FS3_SECTOR_SIZE, sizeof(char));

	//call seek to make sure we are on the right file.
	FS3CmdBlk seekCommand = construct_fs3_cmdblock(1, 0, tracks[fd].Handle, 0);
	FS3CmdBlk returnCommand = fs3_syscall(seekCommand, NULL);
	deconstruct_fs3_cmdblock(returnCommand, &op, &sec, &trk, &ret);
	
	if (op == -1)
	{
		printf("Seek operation failed.");
		return(-1);
	}

	int initialBytes = tracks[fd].Pointer % 1024; //how many bytes inside the current sector
	int bytesWritten = 0;

	//loop as many times as there are sectors since we can only write to 1 sector at a time (ie. if count >1024)
	while (bytesWritten < count)
	{
		int sectorBytesLeft = FS3_SECTOR_SIZE - initialBytes; // Between 0 and 1024, the rest of the sector that does not have data written in it
		int bytesToWrite = sectorBytesLeft;

		if (count - bytesWritten < FS3_SECTOR_SIZE) // If there is less than sectorBytesLeft bytes to write A.K.A. Last while loop run
		{
			bytesToWrite = fmin(count - bytesWritten, sectorBytesLeft);
		}

			char *initialSector = (char *)malloc(FS3_SECTOR_SIZE); // Buffer for previous information found in read
			
			FS3CmdBlk readCommand = construct_fs3_cmdblock(2, floor(tracks[fd].Pointer/FS3_SECTOR_SIZE), 0, 0);
			FS3CmdBlk returnCommand = fs3_syscall(readCommand, initialSector);
			deconstruct_fs3_cmdblock(returnCommand, &op, &sec, &trk, &ret);


			// Populate writeFiller with initialSector
			memcpy(writeFiller, initialSector, FS3_SECTOR_SIZE);

			// writeFiller is prepared for sys_call
			memcpy(&writeFiller[initialBytes], &writeBuff[bytesWritten], bytesToWrite);
			free(initialSector);
			initialSector = NULL;


		// Write sys_call
		FS3CmdBlk writeCommand = construct_fs3_cmdblock(3, floor(tracks[fd].Pointer/FS3_SECTOR_SIZE), 0, 0);
		FS3CmdBlk writeReturnCommand = fs3_syscall(writeCommand, writeFiller);
		deconstruct_fs3_cmdblock(writeReturnCommand, &op, &sec, &trk, &ret);

		if (op == -1)
		{
			printf("Write operation failed.");
			return(-1);
		}

		tracks[fd].Pointer = tracks[fd].Pointer + bytesToWrite; // Update Pointer
		initialBytes = tracks[fd].Pointer % 1024; // Update initialBytes based on new Pointer (should be 0)
		bytesWritten += bytesToWrite; // Increment bytesWritten
	}


	// Update Size if needed
	if(tracks[fd].Pointer > tracks[fd].Size){
		tracks[fd].Size = tracks[fd].Pointer;
	}

	if(fs3_put_cache(fd,floor(tracks[fd].Pointer/FS3_SECTOR_SIZE),writeFiller) == 0){
		scores.FS3_HITS++;
	}
	else{
		scores.FS3_MISSES++;
	}
	free(writeFiller); //recheck placement for freeing
	writeFiller = NULL;

	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - off	set of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_seek(int16_t fd, uint32_t loc) {
	if (fd >= sizeOfTracks || tracks[fd].Size < loc) //ensuring we are doing a valid seek with checks
	{
		return (-1);
	}
	tracks[fd].Pointer = loc; //seek to specific point with given loc
	return (0);
}
