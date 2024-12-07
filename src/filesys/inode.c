#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCK_ENTRIES 122

#define INDIRECT_BLOCK_ENTRIES (BLOCK_SECTOR_SIZE / sizeof(block_sector_t))

#define DOUBLE_INDIRECT_BLOCK_ENTRIES (INDIRECT_BLOCK_ENTRIES * INDIRECT_BLOCK_ENTRIES)

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t direct_map_table[DIRECT_BLOCK_ENTRIES];
    block_sector_t indirect_block_sec;
    block_sector_t double_indirect_block_sec;
    bool is_dir;
    block_sector_t parent;
  };

struct buffer_head {
  uint8_t* data; /* Pointer to virtual memory buffer */
  bool dirty; /* Whether this buffer has been modified since read */
  bool used; /* Whether this buffer is in use*/
  bool access; /* Whether this buffer has been accessed recently */
  block_sector_t sector_idx; /* On disk block sector associated with this buffer */
};

# define BUFFER_SIZE 64
/* Array of 64 buffer_heads used for the filesys buffer cache */
struct buffer_head* buffer_heads;

/* Clock hand for clock eviction strategy */
static int clock_hand = 0;

void write_buffer_to_file(struct buffer_head* buffer_head) {
  block_write (fs_device, buffer_head->sector_idx, buffer_head->data);
}

void read_buffer_from_file(struct buffer_head* buffer_head) {
  block_read (fs_device, buffer_head->sector_idx, buffer_head->data);
}

/* Initialize the array of buffer_heads and allocate space for buffers */
void buffer_init() {
  buffer_heads = malloc(sizeof(struct buffer_head) * BUFFER_SIZE);
  uint8_t* buffer = malloc(BLOCK_SECTOR_SIZE * BUFFER_SIZE);
  for (int i = 0; i < BUFFER_SIZE; ++i) {
    struct buffer_head* buffer_head = buffer_heads + i;
    buffer_head->access = false;
    buffer_head->dirty = false;
    buffer_head->used = false;
    buffer_head->sector_idx = -1;
    buffer_head->data = buffer + (i * BLOCK_SECTOR_SIZE);
  }
}

/* Deallocate buffer_heads and write anything dirty to file */
void buffer_done() {
  for (int i = 0; i < BUFFER_SIZE; ++i) {
    struct buffer_head* buffer_head = buffer_heads + i;
    if (buffer_head->dirty) {
      write_buffer_to_file(buffer_head);
    }
  }
  free(buffer_heads->data);
  free(buffer_heads);
}

/* Returns the buffer_head with a matching sector_id. If one does not exist,
   creates one, evicting an existing buffer_head if they are in use. */
struct buffer_head* find_buffer_head(block_sector_t sector_idx) {
  struct buffer_head* victim = NULL;
  for (int i = 0; i < BUFFER_SIZE; ++i) {
    struct buffer_head* buffer_head = buffer_heads + i;

    // If we find a match, return it right away
    if (buffer_head->sector_idx == sector_idx) return buffer_head;

    // If we find an unused, use it as the victim
    if (!buffer_head->used && victim == NULL) victim = buffer_head;
  }

  // If necessary, evict an existing entry to use as the victim
  if (victim == NULL) victim = evict_buffer();
  victim->used = true;
  victim->sector_idx = sector_idx;
  read_buffer_from_file(victim);
  
  return victim;
}

/* Evicts a buffer entry and returns the victim. Before evicton, writes to file if dirty. */
struct buffer_head* evict_buffer() {
  struct buffer_head* victim = NULL;

  // TODO maybe add some synchronization logic around all the buffer_head stuff

  while (victim == NULL) {
    struct buffer_head* buffer_head = buffer_heads + clock_hand;
    if (buffer_head->access == true) {
      victim = buffer_head;
    } else {
      buffer_head->access = false;
    }
    clock_hand = (clock_hand + 1) % BUFFER_SIZE;
  }

  if (victim->dirty) {
    write_buffer_to_file(victim);
  }
  memset(victim->data, 0, BLOCK_SECTOR_SIZE);
  victim->dirty = false;
  victim->access = false;

  return victim;
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

static void get_disk_inode(struct inode* inode) {
  block_read(fs_device, inode->sector, &inode->data);
}

static void write_disk_inode(struct inode* inode) {
  block_write(fs_device, inode->sector, &inode->data);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  off_t sector_idx = pos / BLOCK_SECTOR_SIZE;

  static char zeros[BLOCK_SECTOR_SIZE];

  // Out of bounds
  if (sector_idx > DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES + DOUBLE_INDIRECT_BLOCK_ENTRIES - 1) {
    return -1;
  }
  
  // TODO maybe explore warnings in here?
  if (sector_idx < DIRECT_BLOCK_ENTRIES) {
    if (inode->data.direct_map_table[sector_idx] == 0) {
      free_map_allocate (1, inode->data.direct_map_table + sector_idx);
      block_write(fs_device, inode->data.direct_map_table[sector_idx], zeros);
    }
    return inode->data.direct_map_table[sector_idx];
  } else if (sector_idx < DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES) {
    // Allocate indirect table if not already allocated
    if (inode->data.indirect_block_sec == 0) {
      free_map_allocate (1, &inode->data.indirect_block_sec);
      block_write(fs_device, inode->data.indirect_block_sec, zeros);
    }
    block_sector_t* indirect_table = malloc(BLOCK_SECTOR_SIZE);
    block_read(fs_device, inode->data.indirect_block_sec, indirect_table);

    off_t indirect_index = sector_idx - DIRECT_BLOCK_ENTRIES;

    // Allocate sector if not already done so, and update indirect table
    if (indirect_table[indirect_index] == 0) {
      free_map_allocate (1, indirect_table + indirect_index);
      block_write(fs_device, indirect_table[indirect_index], zeros);
      block_write(fs_device, inode->data.indirect_block_sec, indirect_table);
    }
    block_sector_t sector = indirect_table[indirect_index];
    free(indirect_table);
    return sector;
  } else {
    // Allocate indirect table if not already allocated
    if (inode->data.double_indirect_block_sec == 0) {
      free_map_allocate (1, &inode->data.double_indirect_block_sec);
      block_write(fs_device, inode->data.double_indirect_block_sec, zeros);
    }

    block_sector_t* double_indirect_table = malloc(BLOCK_SECTOR_SIZE);
    block_read(fs_device, inode->data.double_indirect_block_sec, double_indirect_table);

    sector_idx -= (DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES);

    off_t double_indirect_index = sector_idx / INDIRECT_BLOCK_ENTRIES;
    off_t indirect_index = sector_idx % INDIRECT_BLOCK_ENTRIES;

    // Allocate indirect table if not already done so, and update double indirect table
    if (double_indirect_table[double_indirect_index] == 0) {
      free_map_allocate (1, double_indirect_table + double_indirect_index);
      block_write(fs_device, double_indirect_table[double_indirect_index], zeros);
      block_write(fs_device, inode->data.double_indirect_block_sec, double_indirect_table);
    }

    block_sector_t* indirect_table = malloc(BLOCK_SECTOR_SIZE);
    block_read(fs_device, double_indirect_table[double_indirect_index], indirect_table);

    // Allocate sector if not already done so, and update indirect table
    if (indirect_table[indirect_index] == 0) {
      free_map_allocate (1, indirect_table + indirect_index);
      block_write(fs_device, indirect_table[indirect_index], zeros);
      block_write(fs_device, double_indirect_table[double_indirect_index], indirect_table);
    }

    block_sector_t sector = indirect_table[indirect_index];
    free(indirect_table);
    free(double_indirect_table);
    return sector;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;
      disk_inode->parent = ROOT_DIR_SECTOR;
      // if (free_map_allocate (sectors, &disk_inode->start)) 
      //   {
     //   {
      //     block_write (fs_device, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;

      //         for (i = 0; i < sectors; i++) 
      //           block_write (fs_device, disk_inode->start + i, zeros);
      //       }
      //     success = true; 
      //   } 
      block_write (fs_device, sector, disk_inode);
      success = true;
      free (disk_inode);
    }
  return success;
}
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

void free_indirect_table(block_sector_t indirect_table_sector) {
  block_sector_t* indirect_table = malloc(BLOCK_SECTOR_SIZE);
  block_read(fs_device, indirect_table_sector, indirect_table);

  for (int i = 0; i < INDIRECT_BLOCK_ENTRIES; ++i) {
    if (indirect_table[i] != 0) {
      free_map_release(indirect_table[i], 1);
    }
  }

  free_map_release(indirect_table_sector, 1);
  free(indirect_table);
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          // Free direct table
          struct inode_disk disk_inode = inode->data; 
          for (int i = 0; i < DIRECT_BLOCK_ENTRIES; ++i) {
            if (disk_inode.direct_map_table[i] != 0) {
              free_map_release (disk_inode.direct_map_table[i], 1);
            }
          }

          if (disk_inode.indirect_block_sec != 0) {
            free_indirect_table(disk_inode.indirect_block_sec);
          }

          if (disk_inode.double_indirect_block_sec != 0) {
            block_sector_t* double_indirect_table = malloc(BLOCK_SECTOR_SIZE);
            block_read(fs_device, disk_inode.double_indirect_block_sec, double_indirect_table);

            for (int i = 0; i < INDIRECT_BLOCK_ENTRIES; ++i) {
              if (double_indirect_table[i] != 0) {
                free_indirect_table(double_indirect_table[i]);
              }
            }

            free_map_release(disk_inode.double_indirect_block_sec, 1);
            free(double_indirect_table);
          }

          // Free disk inode
          // TODO maybe some concurrency issues here, not sure where to put this
          write_disk_inode(inode);
          free_map_release (inode->sector, 1);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0 && offset < inode->data.length) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      struct buffer_head* buffer_head = find_buffer_head(sector_idx);
      memcpy(buffer + bytes_read, buffer_head->data + sector_ofs, chunk_size);
      buffer_head->access = true;

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  write_disk_inode(inode);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  if (offset + size > inode->data.length)
    inode->data.length = offset + size;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      struct buffer_head* buffer_head = find_buffer_head(sector_idx);
      memcpy(buffer_head->data + sector_ofs, buffer + bytes_written, chunk_size);
      buffer_head->access = true;
      buffer_head->dirty = true;

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  write_disk_inode(inode);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}


/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_is_dir(struct inode *inode_)
{
        return inode_->data.is_dir;
}

bool inode_set_parent(block_sector_t sector, block_sector_t parent)
{
	struct inode* inode_sec = inode_open(sector);
	if(inode_sec == NULL)
		return false;
	inode_sec->data.parent = parent;
	inode_close(inode_sec);
	return true;
}
struct inode* inode_get_parent(struct inode* inode)
{
	if(inode == NULL || &(inode->data) == NULL)
		return NULL;
	struct inode* parent = inode_open(inode->data.parent);
	return parent;
}
