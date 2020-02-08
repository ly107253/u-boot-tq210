/*
 *	LiMon - BOOTP/TFTP.
 *
 *	Copyright 1994, 1995, 2000 Neil Russell.
 *	(See License)
 */

#ifndef __TFTP_YAFFS_H__
#define __TFTP_YAFFS_H__

/**********************************************************************/
/*
 *	Global functions and variables.
 */

/* tftpyaffs.c */

#define CONFIG_YAFFS_NAND_BLOCK_SIZE 0x21000 //BLOCK+OOB=132K
#define CONFIG_RAM_BUFFER_SIZE CONFIG_YAFFS_NAND_BLOCK_SIZE //
#ifndef CONFIG_SYS_NAND_BLOCK_SIZE
#define CONFIG_SYS_NAND_BLOCK_SIZE 0x20000
#endif
#define CONFIG_YAFFS2_OFFSET_DEFAULT 0x700000

extern void	TftpYaffsStart (void);	/* Begin TFTP get */
extern ulong root_offset, root_start;
extern size_t root_size, root_maxsize;

/**********************************************************************/

#endif /* __TFTP_YAFFS_H__ */
