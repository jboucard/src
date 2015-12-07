/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
dwdwwdwda
dwadd
*/

#include "params.h"
#include "block.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"
#include "fs.h"
///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */

typedef struct fs_super_block super_block;
typedef struct fs_group_desc group_desc;
typedef struct dir_entry dir_entry;
typedef struct fs_inode inode;

super_block *super;
group_desc * groupdesc;
dir_entry *root;
inode * rootInode;

int DataBlockBitMap[DATABLOCKS/32+1]={ 0 };
int iNodeBitMap[INODES/32+1]={ 0 };

void *sfs_init(struct fuse_conn_info *conn)
{	
    super= (super_block* ) malloc(sizeof(super_block));
    groupdesc= (group_desc * ) malloc(sizeof(group_desc));  
	root= (dir_entry *) malloc (sizeof(dir_entry));
    char buf[512];
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");
	
    disk_open(diskFilePath);
     //write struct to the diskfile
    //read contents of block into a buffer and copy them into super block
    block_read(1, (void *) &buf); 
    memcpy((void *) super, &buf, sizeof(super_block)); 
    
    if (strcmp( super->mounted, "mount")!=0)
    {
	log_msg("\nIN IF STATEMENT\n");
	super->s_mnt_count=0;
	memmove(super->mounted, (void *) "mount", 5);
	//first time mounting to this system
	//initialize data structures accordingly
	super->s_inodes_count= INODES;
	super->s_blocks_count=DATABLOCKS; //blocks count
	super->first_d_block=108;
	super->s_free_blocks_count=DATABLOCKS; //free blocks count
	super->s_free_inodes_count=INODES; //free inodes count
	//super->s_mtime=
	groupdesc->block_bitmap=3;
	groupdesc->inode_bitmap=4;
	groupdesc->used_dirs_count=0;
/*	USE IF YOU HAVE MORE THAN ONE BLOCK GROUP
	s_blocks_per_group; //# Blocks per group
	 s_frags_per_group; //# fragments per group
	 s_inodes_per_group; //# inodes per grp */

	//initialize bitmaps to 0

	block_write(2, (void *) groupdesc);
	block_write(groupdesc->block_bitmap,  (void *)DataBlockBitMap);
	block_write(groupdesc->inode_bitmap,  (void *)iNodeBitMap);

    }
    super->s_mnt_count++;//add one to the mount count

    block_write(1, (void *) super);//write an updated initialized superblock to disk
    log_msg("MOUNT COUNT: %d\n",super->s_mnt_count);
	
    root->inode=0;//give root the first INODE
    root->file_type=DIR; //set root type to a directory
    memmove(root->name, (void *) "/", 1); //set name to "/"
    root->parent=0;//initialize pointer fields to 0
    root->child=0;
    root->subdirs=0;
    //fill out inode for root
	SetBit(iNodeBitMap,0);
	//initialize inode for root dir
	rootInode= (inode *) malloc(sizeof(inode));
	//rootInode->mode =
	rootInode->uid=geteuid();
	rootInode->size=BLOCK_SIZE;
	time_t t;
    time(&t);
	rootInode->ctime= t;
	rootInode->atime= t;
	rootInode->mtime= t;
	//rootInode->mode=USER_R_W;
	/*
struct fs_inode {
	uint16_t  mode; //File Mode
	uint32_t  size; // size in bytes
	uint32_t  atime; //last access time
	uint32_t  ctime; //creation time
	uint32_t  mtime; //modification time
	uint32_t  dtime; //deletion time
	uint16_t  gid;  //low 16 bits of group id (*)
	uint16_t  links_count; //links count
	uint32_t  blocks; //blocks count
	uint32_t  flags; //file flags
	
	uint32_t  i_block[N_BLOCKS]; //pointers to blocks 
	uint32_t generation; //file version
	uint32_t file_acl; //File Access Control List
	uint32_t dir_acl; //Directory Access Control List
	uint32_t faddr;  //fragment address

	uint8_t frag; //fragment number
	uint8_t fsize; //Fragment Size
	
	// The number of the block group which contains this file's inode
	uint32_t block_group; 
	
	pthread_mutex_t lock;
	
	};
	*/
    log_conn(conn);
    log_fuse_context(fuse_get_context());
	
	disk_close();
    return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
	dir_entry * de=(dir_entry*) malloc(sizeof(dir_entry));
    if (path_to_dir_entry(path, de)==-1){
		log_msg("\nPath to dir entry failed in getattr\n");
		return -1;	
	}
	log_msg("\nsize of dir_entry%d\n",sizeof(dir_entry));
	lstat(path, statbuf);
	

	log_msg("\nst_ino %d st_mode %d st_nlink %d st_atime %d st_ctime %d\n", statbuf->st_ino, statbuf->st_mode, statbuf->st_nlink=1, statbuf->st_atime,statbuf->st_ctime);
    statbuf->st_ino =  de->inode;
	statbuf->st_dev=0;
	statbuf->st_mode=(mode_t) (USER_X | USER_W |USER_R |DIR);
	statbuf->st_nlink=1;
	statbuf->st_uid=rootInode->uid;
	statbuf->st_gid=0;
	statbuf->st_rdev=0;
	statbuf->st_size=sizeof(dir_entry);//what do i return for the size if de is a folder?    
	statbuf->st_blksize=BLOCK_SIZE;
	
	statbuf->st_blocks=DATABLOCKS - super->s_free_blocks_count; 
	statbuf->st_atime=rootInode->atime;
	statbuf->st_mtime=rootInode->mtime;
	statbuf->st_ctime=rootInode->ctime;
	log_msg("\n creation time is : %d",ctime((time_t *)&rootInode->ctime));
	//free(de);
	return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    
    
    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);

    
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

   
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    
    
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
   
    
    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int path_to_dir_entry(const char * path, dir_entry * dirent)
{
	dir_entry *de;
	dir_entry *copy;
	
    //example /a/b/c/
    //delete first slash(root directory) 
    //get up to & including next slash (a/)
    //find d_entry for  (a/)
    //repeat until at end of path
    int i;
    char name[NAME_LEN];
    int namelen=0;
    for (i=0; i<strlen(path); i++)
    {
		name[namelen]= path[i];
		namelen++;
		
		if (path[i]=='/')
		{	//if root dir
			if (name[0]=='/'){
	    		de=root;
				log_msg("\nroot->inode %d de->inode %d\n",root->inode, de->inode);
			}
			else
			{
				//if not root then
 
				dirNumToEntry (de->subdirs, copy);//fill copy with first child of de
				while (!strncmp(name, copy->name, namelen-1))
				{	
					dirNumToEntry(copy->child, copy);

					if (strcmp(copy->name, de->name))
					{
						return -1;//looped through d_entries. can't find match 
					}
				}
				de=copy;
			}
			
			namelen=0;
		}
		else if (i==strlen(path)-1)
		{
			log_msg("path[i]: %c\n",path[i]);
			return -1;
		}

    } //turn path into d_entry
	dirent=de;
	log_msg("dirent->name %s and dirent->inode:%d de->inode%d\n", dirent->name,dirent->inode, de->inode);
	return 0;
} 


int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    dir_entry *de;
	dir_entry *copy;
	//turn path into d_entry
	if (path_to_dir_entry(path,de)==-1)
		return -1;

    /*de now equals the d_entry that corresponds to the passed path*/

    //loop through d_entries in path and call filler funciton on their names
	if(de->subdirs==0)
		return retstat;
	dirNumToEntry(de->subdirs,copy);		
	while(copy->name != de->name)
	{
		if( filler(buf, copy->name, NULL, 0)==0)
			return 0;
		dirNumToEntry(copy->child, copy);
	}   	    
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    
    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}
//pass a directory number, fill passed dir_entry with corresponding struct
void dirNumToEntry ( int dirNum, dir_entry * d_entry)
{
	disk_open(diskFilePath);
	char * buf[BLOCK_SIZE];
	//if(filetype==DIR)
	if (dirNum%2==0)
	{
		block_read(47+(dirNum/2), buf);
		memcpy(d_entry, buf, sizeof(dir_entry));
	}
	else
	{
		block_read(47+(dirNum/2), buf);
		memcpy(d_entry, buf+sizeof(dir_entry), sizeof(dir_entry)); //double check if you get errors
	}
	disk_close();
	
}


  int GetBit( int A[ ],  int k )
   {
      return ( (A[k/32] & (1 << (k%32) )) != 0 ) ;     
   }
  void  SetBit( int A[ ],  int k )
   {
      A[k/32] |= 1 << (k%32);  // Set the bit at the k-th position in A[i]
   }
   void  ClearBit( int A[ ],  int k )                
   {
      A[k/32] &= ~(1 << (k%32));
   }



int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
