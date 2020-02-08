/*
 *	Copyright 1994, 1995, 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000, 2001 DENX Software Engineering, Wolfgang Denk, wd@denx.de
 */

#include <common.h>
#include <command.h>
#include <net.h>
#include "tftpyaffs.h"
#include "bootp.h"

/* add by richard */
#include <asm/errno.h>
#include <linux/mtd/mtd.h>
#include <nand.h>
/* add end by richard */

#undef	ET_DEBUG

#if defined(CONFIG_CMD_NET)

#define WELL_KNOWN_PORT	69		/* Well known TFTP port #		*/
#define TIMEOUT		2000000UL		/* Seconds to timeout for a lost pkt	*/
#ifndef	CONFIG_NET_RETRY_COUNT
# define TIMEOUT_COUNT	1000		/* # of timeouts before giving up  */
#else
# define TIMEOUT_COUNT  (CONFIG_NET_RETRY_COUNT * 2)
#endif
					/* (for checking the image size)	*/
#define HASHES_PER_LINE	65		/* Number of "loading" hashes per line	*/

/*
 *	TFTP operations.
 */
#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5
#define TFTP_OACK	6

static IPaddr_t TftpServerIP;
static int	TftpServerPort;		/* The UDP port at their end		*/
static int	TftpOurPort;		/* The UDP port at our end		*/
static int	TftpTimeoutCount;
static ulong	TftpBlock;		/* packet sequence number		*/
static ulong	TftpLastBlock;		/* last packet sequence number received */
static ulong	TftpBlockWrap;		/* count of sequence number wraparounds */
static ulong	TftpBlockWrapOffset;	/* memory offset due to wrapping	*/
static int	TftpState;

#define STATE_RRQ	1
#define STATE_DATA	2
#define STATE_TOO_LARGE	3
#define STATE_BAD_MAGIC	4
#define STATE_OACK	5

#define TFTP_BLOCK_SIZE		512		    /* default TFTP block size	*/
#define TFTP_SEQUENCE_SIZE	((ulong)(1<<16))    /* sequence number is 16 bit */

#define DEFAULT_NAME_LEN	(8 + 4 + 1)
static char default_filename[DEFAULT_NAME_LEN];

#ifndef CONFIG_TFTP_FILE_NAME_MAX_LEN
#define MAX_LEN 128
#else
#define MAX_LEN CONFIG_TFTP_FILE_NAME_MAX_LEN
#endif

static char tftp_filename[MAX_LEN];

#ifdef CFG_DIRECT_FLASH_TFTP
extern flash_info_t flash_info[];
#endif

/* 512 is poor choice for ethernet, MTU is typically 1500.
 * Minus eth.hdrs thats 1468.  Can get 2x better throughput with
 * almost-MTU block sizes.  At least try... fall back to 512 if need be.
 */
#define TFTP_MTU_BLOCKSIZE 1024
static unsigned short TftpBlkSize=TFTP_BLOCK_SIZE;
static unsigned short TftpBlkSizeOption=TFTP_MTU_BLOCKSIZE;

#ifdef CONFIG_MCAST_TFTP
#include <malloc.h>
#define MTFTP_BITMAPSIZE	0x1000
static unsigned *Bitmap;
static int PrevBitmapHole,Mapsize=MTFTP_BITMAPSIZE;
static uchar ProhibitMcast=0, MasterClient=0;
static uchar Multicast=0;
extern IPaddr_t Mcast_addr;
static int Mcast_port;
static ulong TftpEndingBlock; /* can get 'last' block before done..*/

static void parse_multicast_oack(char *pkt,int len);

static void
mcast_cleanup(void)
{
	if (Mcast_addr) eth_mcast_join(Mcast_addr, 0);
	if (Bitmap) free(Bitmap);
	Bitmap=NULL;
	Mcast_addr = Multicast = Mcast_port = 0;
	TftpEndingBlock = -1;
}

#endif	/* CONFIG_MCAST_TFTP */

ulong root_offset, root_start;
size_t root_size, root_maxsize;
static nand_info_t *nandflash;
size_t rwsize;
int skipfirstblk = 1;							

typedef struct erase_info	erase_info_t;

int percent_complete = -1;

static int tftp_write_yaffs_skip_bad(nand_info_t *nand, 
				ulong offset, size_t *length, u_char *buffer)
{
	int rval = 0, blocksize;
	size_t left_to_write = *length;
	u_char *p_buffer = buffer;
	
//meminfo just is nand, it is used to erase
	nand_info_t *meminfo = nand;
	erase_info_t erase;//must
	int result;
	int page, pages;
	ulong off;
	off = offset;
	if ((off & (meminfo->erasesize - 1)) != 0) {
		printf("Attempt to erase non block-aligned data\n");
		return -1;
	}
	if ((off & (meminfo->erasesize - 1)) > 
		(root_start + root_size - meminfo->erasesize)) {
		printf("Size exceeds partition or device limit\n");
		return -1;
	}
//	return 0;
	
	memset(&erase, 0, sizeof(erase));

	erase.mtd = meminfo;
	erase.len  = meminfo->erasesize;
	erase.addr = off;//modify

eraseblock:
	result = meminfo->erase(meminfo, &erase);
	if (result != 0) {
		printf("\rSkipping bad block at  "
			   "0x%08x				 "
			   "						 \n",
			   erase.addr);

		erase.addr += meminfo->erasesize;
		off += meminfo->erasesize;
		goto eraseblock;
	}
	if (skipfirstblk)
	{
		printf("Skipping first block at 0x%08x \n",
			   erase.addr);
		erase.addr += meminfo->erasesize;
		skipfirstblk = 0;
		goto eraseblock;
	}
	off = erase.addr;// offset had been changed
//	printf("\r erase.addr 0x%llx\n", off);
	pages = nand->erasesize / nand->writesize;// pages in ablock
//		for yaffs, let blocksize = oob + erasesize
	blocksize = (pages * nand->oobsize) + nand->erasesize;
	if (*length % (nand->writesize + nand->oobsize)) {
		printf("Attempt to write incomplete page"
			" in yaffs mode\n");
		//error: rwsize is not a multiple of pagesize_oob
		return -EINVAL;
	}

	if ((off & (nand->writesize - 1)) != 0) {
		printf("Attempt to write non page-aligned data\n");
		*length = 0;
		return -EINVAL;
	}

	while (left_to_write > 0) //Write one page at a time
	{
		size_t block_offset = off & (nand->erasesize - 1);
		size_t write_size;

		if (left_to_write < (blocksize - block_offset))
			write_size = left_to_write;//
		else
			write_size = blocksize - block_offset;//length is more than a block

//start write block and oob:
		size_t pagesize = nand->writesize;//page length
		size_t pagesize_oob = pagesize + nand->oobsize;
		struct mtd_oob_ops ops;

		ops.len = pagesize;
		ops.ooblen = nand->oobsize;
		ops.mode = MTD_OOB_AUTO;//for am335x, oob = yaffs tag +  1bit ecc
		ops.ooboffs = 0;

		pages = write_size / pagesize_oob;
		for (page = 0; page < pages; page++) {

			ops.datbuf = p_buffer;
			ops.oobbuf = ops.datbuf + pagesize;
			rval = nand->write_oob(nand, off, &ops);
			if (rval != 0)
			{
				printf ("NAND write to offset 0x%08lx failed %d\n", off, rval);
				*length -= left_to_write;
				return rval;
			}
			off += pagesize;
			p_buffer += pagesize_oob;
		}
		left_to_write -= write_size;
	}
	int percent = (int)lldiv((unsigned long long)(off - root_start) * 100 ,root_size);
//	if (percent != percent_complete || (*offset % 0x100000) == 0)//print prompt at 1MB
	{
		printf("\rWriting data at 0x%08lx -- %3d%% complete.",   off, percent);
		percent_complete = percent;
	}
	root_offset = off;
	return 0;
}

static int
erase_rootfs_rest_block(nand_info_t *meminfo, u32 offset)
{
	erase_info_t erase;
	int result;
	size_t block_offset;
	memset(&erase, 0, sizeof(erase));
	erase.mtd = meminfo;
	erase.len  = meminfo->erasesize;
	erase.addr = offset & ~(loff_t)(meminfo->erasesize - 1);//get align block addr
	block_offset = offset & (meminfo->erasesize - 1);//get offset in blocks
	if (block_offset != 0){
		erase.addr += meminfo->erasesize;
	}
	if ((erase.addr & (meminfo->erasesize - 1)) != 0) {
		printf("Attempt to erase non block-aligned data "
			   " at 0x%08x\n", erase.addr);	
		return -1;
	}
	printf("\n");

	for (;erase.addr < root_start + root_size;
		erase.addr += meminfo->erasesize) {

		result = meminfo->erase(meminfo, &erase);		
		if (result != 0) {
			printf("\nSkipping bad block at  "
				   "0x%08x				 "
				   "						 \n",
				   erase.addr);
		}
		int percent = (int)lldiv((unsigned long long)(erase.addr - root_start) * 100 , root_size);
		if (percent != percent_complete)
		{
			printf("\rErasing at 0x%08x -- %3d%% complete.",	erase.addr, percent);
			percent_complete = percent;
		}
	}
	printf("\rErasing at 0x%08x -- 100%% complete.\n", erase.addr);//new pageaddr
	percent_complete = -1;
	return 0;
}


static __inline__ void
store_block (unsigned block, uchar * src, unsigned len)
{
	ulong offset = block * TftpBlkSize + TftpBlockWrapOffset;
	ulong newsize = offset + len;
#ifdef CFG_DIRECT_FLASH_TFTP
	int i, rc = 0;

	for (i=0; i<CFG_MAX_FLASH_BANKS; i++) {
		/* start address in flash? */
		if (load_addr + offset >= flash_info[i].start[0]) {
			rc = 1;
			break;
		}
	}

	if (rc) { /* Flash is destination for this packet */
		rc = flash_write ((char *)src, (ulong)(load_addr+offset), len);
		if (rc) {
			flash_perror (rc);
			NetState = NETLOOP_FAIL;
			return;
		}
	}
	else
#endif /* CFG_DIRECT_FLASH_TFTP */
	{
		(void)memcpy((void *)(load_addr + offset%CONFIG_RAM_BUFFER_SIZE), src, len);
	}
#ifdef CONFIG_MCAST_TFTP
	if (Multicast)
		ext2_set_bit(block, Bitmap);
#endif

	/*add by richard*/
	int ret;
	if((newsize != 0 && (newsize % CONFIG_RAM_BUFFER_SIZE ==0)) 
		|| (len < TftpBlkSize))
	//Receive a frame,	   write a frame to nand
	{
		root_maxsize = CONFIG_YAFFS_NAND_BLOCK_SIZE; 
		if(len < TftpBlkSize)
			rwsize = newsize % CONFIG_YAFFS_NAND_BLOCK_SIZE;
		else
			rwsize = CONFIG_YAFFS_NAND_BLOCK_SIZE;

		ret = tftp_write_yaffs_skip_bad(nandflash, root_offset, 
										&rwsize,(u_char *)load_addr);
		if(ret)
		{
			printf("tftp_write_skip_bad failed\n");
		}

		/* last block ,now should erase rest block */
		if(len < TftpBlkSize && newsize < root_size){
			percent_complete = -1;
			erase_rootfs_rest_block(nandflash, root_offset);
		}
	}
	/*add by richard*/


	if (NetBootFileXferSize < newsize)
		NetBootFileXferSize = newsize;
}

static void TftpSend (void);
static void TftpTimeout (void);

/**********************************************************************/

static void
TftpSend (void)
{
	volatile uchar *	pkt;
	volatile uchar *	xp;
	int			len = 0;
	volatile ushort *s;

#ifdef CONFIG_MCAST_TFTP
	/* Multicast TFTP.. non-MasterClients do not ACK data. */
	if (Multicast
	 && (TftpState == STATE_DATA)
	 && (MasterClient == 0))
		return;
#endif
	/*
	 *	We will always be sending some sort of packet, so
	 *	cobble together the packet headers now.
	 */
	pkt = NetTxPacket + NetEthHdrSize() + IP_HDR_SIZE;

	switch (TftpState) {

	case STATE_RRQ:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TFTP_RRQ);
		pkt = (uchar *)s;
		strcpy ((char *)pkt, tftp_filename);
		pkt += strlen(tftp_filename) + 1;
		strcpy ((char *)pkt, "octet");
		pkt += 5 /*strlen("octet")*/ + 1;
		strcpy ((char *)pkt, "timeout");
		pkt += 7 /*strlen("timeout")*/ + 1;
		sprintf((char *)pkt, "%lu", TIMEOUT);
#ifdef ET_DEBUG
		printf("send option \"timeout %s\"\n", (char *)pkt);
#endif
		pkt += strlen((char *)pkt) + 1;
		/* try for more effic. blk size */
		pkt += sprintf((char *)pkt,"blksize%c%d%c",
				0,TftpBlkSizeOption,0);
#ifdef CONFIG_MCAST_TFTP
		/* Check all preconditions before even trying the option */
		if (!ProhibitMcast
		 && (Bitmap=malloc(Mapsize))
		 && eth_get_dev()->mcast) {
			free(Bitmap);
			Bitmap=NULL;
			pkt += sprintf((char *)pkt,"multicast%c%c",0,0);
		}
#endif /* CONFIG_MCAST_TFTP */
		len = pkt - xp;
		break;

	case STATE_OACK:
#ifdef CONFIG_MCAST_TFTP
		/* My turn!  Start at where I need blocks I missed.*/
		if (Multicast)
			TftpBlock=ext2_find_next_zero_bit(Bitmap,(Mapsize*8),0);
		/*..falling..*/
#endif
	case STATE_DATA:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TFTP_ACK);
		*s++ = htons(TftpBlock);
		pkt = (uchar *)s;
		len = pkt - xp;
		break;

	case STATE_TOO_LARGE:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TFTP_ERROR);
		*s++ = htons(3);
		pkt = (uchar *)s;
		strcpy ((char *)pkt, "File too large");
		pkt += 14 /*strlen("File too large")*/ + 1;
		len = pkt - xp;
		break;

	case STATE_BAD_MAGIC:
		xp = pkt;
		s = (ushort *)pkt;
		*s++ = htons(TFTP_ERROR);
		*s++ = htons(2);
		pkt = (uchar *)s;
		strcpy ((char *)pkt, "File has bad magic");
		pkt += 18 /*strlen("File has bad magic")*/ + 1;
		len = pkt - xp;
		break;
	}

	NetSendUDPPacket(NetServerEther, TftpServerIP, TftpServerPort, TftpOurPort, len);
}


static void
TftpYaffsHandler (uchar * pkt, unsigned dest, unsigned src, unsigned len)
{
	ushort proto;
	ushort *s;
	int i;

	if (dest != TftpOurPort) {
#ifdef CONFIG_MCAST_TFTP
		if (Multicast
		 && (!Mcast_port || (dest != Mcast_port)))
#endif
		return;
	}
	if (TftpState != STATE_RRQ && src != TftpServerPort) {
		return;
	}

	if (len < 2) {
		return;
	}
	len -= 2;
	/* warning: don't use increment (++) in ntohs() macros!! */
	s = (ushort *)pkt;
	proto = *s++;
	pkt = (uchar *)s;
	switch (ntohs(proto)) {

	case TFTP_RRQ:
	case TFTP_WRQ:
	case TFTP_ACK:
		break;
	default:
		break;

	case TFTP_OACK:
#ifdef ET_DEBUG
		printf("Got OACK: %s %s\n", pkt, pkt+strlen(pkt)+1);
#endif
		TftpState = STATE_OACK;
		TftpServerPort = src;
		/*
		 * Check for 'blksize' option.
		 * Careful: "i" is signed, "len" is unsigned, thus
		 * something like "len-8" may give a *huge* number
		 */
		for (i=0; i+8<len; i++) {
			if (strcmp ((char*)pkt+i,"blksize") == 0) {
				TftpBlkSize = (unsigned short)
					simple_strtoul((char*)pkt+i+8,NULL,10);
#ifdef ET_DEBUG
				printf ("Blocksize ack: %s, %d\n",
					(char*)pkt+i+8,TftpBlkSize);
#endif
				break;
			}
		}
#ifdef CONFIG_MCAST_TFTP
		parse_multicast_oack((char *)pkt,len-1);
		if ((Multicast) && (!MasterClient))
			TftpState = STATE_DATA;	/* passive.. */
		else
#endif
		TftpSend (); /* Send ACK */
		break;
	case TFTP_DATA:
		if (len < 2)
			return;
		len -= 2;
		TftpBlock = ntohs(*(ushort *)pkt);

		/*
		 * RFC1350 specifies that the first data packet will
		 * have sequence number 1. If we receive a sequence
		 * number of 0 this means that there was a wrap
		 * around of the (16 bit) counter.
		 */
		if (TftpBlock == 0) {
			TftpBlockWrap++;
			TftpBlockWrapOffset += TftpBlkSize * TFTP_SEQUENCE_SIZE;
			printf ("\n\t %lu MB received\n\t ", TftpBlockWrapOffset>>20);
		} else {
			if (((TftpBlock - 1) % 10) == 0) {
//				putc ('#');  del by richard
			} else if ((TftpBlock % (10 * HASHES_PER_LINE)) == 0) {
//				puts ("\n\t ");   del by richard
			}
		}

#ifdef ET_DEBUG
		if (TftpState == STATE_RRQ) {
			puts ("Server did not acknowledge timeout option!\n");
		}
#endif

		if (TftpState == STATE_RRQ || TftpState == STATE_OACK) {
			/* first block received */
			TftpState = STATE_DATA;
			TftpServerPort = src;
			TftpLastBlock = 0;
			TftpBlockWrap = 0;
			TftpBlockWrapOffset = 0;

#ifdef CONFIG_MCAST_TFTP
			if (Multicast) { /* start!=1 common if mcast */
				TftpLastBlock = TftpBlock - 1;
			} else
#endif
			if (TftpBlock != 1) {	/* Assertion */
				printf ("\nTFTP error: "
					"First block is not block 1 (%ld)\n"
					"Starting again\n\n",
					TftpBlock);
				NetStartAgain ();
				break;
			}
		}

		if (TftpBlock == TftpLastBlock) {
			/*
			 *	Same block again; ignore it.
			 */
			break;
		}

		TftpLastBlock = TftpBlock;
		NetSetTimeout (TIMEOUT * CFG_HZ, TftpTimeout);

		store_block (TftpBlock - 1, pkt + 2, len);

		/*
		 *	Acknoledge the block just received, which will prompt
		 *	the server for the next one.
		 */
#ifdef CONFIG_MCAST_TFTP
		/* if I am the MasterClient, actively calculate what my next
		 * needed block is; else I'm passive; not ACKING
		 */
		if (Multicast) {
			if (len < TftpBlkSize)  {
				TftpEndingBlock = TftpBlock;
			} else if (MasterClient) {
				TftpBlock = PrevBitmapHole =
					ext2_find_next_zero_bit(
						Bitmap,
						(Mapsize*8),
						PrevBitmapHole);
				if (TftpBlock > ((Mapsize*8) - 1)) {
					printf ("tftpfile too big\n");
					/* try to double it and retry */
					Mapsize<<=1;
					mcast_cleanup();
					NetStartAgain ();
					return;
				}
				TftpLastBlock = TftpBlock;
			}
		}
#endif
		TftpSend ();

#ifdef CONFIG_MCAST_TFTP
		if (Multicast) {
			if (MasterClient && (TftpBlock >= TftpEndingBlock)) {
				puts ("\nMulticast tftp done\n");
				mcast_cleanup();
				NetState = NETLOOP_SUCCESS;
			}
		}
		else
#endif
		if (len < TftpBlkSize) {
			/*
			 *	We received the whole thing.  Try to
			 *	run it.
			 */
			puts ("\ndone\n");
			NetState = NETLOOP_SUCCESS;
		}
		break;

	case TFTP_ERROR:
		printf ("\nTFTP error: '%s' (%d)\n",
					pkt + 2, ntohs(*(ushort *)pkt));
		puts ("Starting again\n\n");
#ifdef CONFIG_MCAST_TFTP
		mcast_cleanup();
#endif
		NetStartAgain ();
		break;
	}
}


static void
TftpTimeout (void)
{
	if (++TftpTimeoutCount > TIMEOUT_COUNT) {
		puts ("\nRetry count exceeded; starting again\n");
#ifdef CONFIG_MCAST_TFTP
		mcast_cleanup();
#endif
		NetStartAgain ();
	} else {
//		puts ("T "); del by richard
		NetSetTimeout (TIMEOUT * CFG_HZ, TftpTimeout);
		TftpSend ();
	}
}


void
TftpYaffsStart (void)
{
#ifdef CONFIG_TFTP_PORT
	char *ep;             /* Environment pointer */
#endif

	TftpServerIP = NetServerIP;
	if (BootFile[0] == '\0') {
		sprintf(default_filename, "%02lX%02lX%02lX%02lX.img",
			NetOurIP & 0xFF,
			(NetOurIP >>  8) & 0xFF,
			(NetOurIP >> 16) & 0xFF,
			(NetOurIP >> 24) & 0xFF	);

		strncpy(tftp_filename, default_filename, MAX_LEN);
		tftp_filename[MAX_LEN-1] = 0;

		printf ("*** Warning: no boot file name; using '%s'\n",
			tftp_filename);
	} else {
		char *p = strchr (BootFile, ':');

		if (p == NULL) {
			strncpy(tftp_filename, BootFile, MAX_LEN);
			tftp_filename[MAX_LEN-1] = 0;
		} else {
			*p++ = '\0';
			TftpServerIP = string_to_ip (BootFile);
			strncpy(tftp_filename, p, MAX_LEN);
			tftp_filename[MAX_LEN-1] = 0;
		}
	}

#if defined(CONFIG_NET_MULTI)
	printf ("Using %s device\n", eth_get_name());
#endif
	puts ("TFTP from server ");	print_IPaddr (TftpServerIP);
	puts ("; our IP address is ");	print_IPaddr (NetOurIP);

	/* Check if we need to send across this subnet */
	if (NetOurGatewayIP && NetOurSubnetMask) {
	    IPaddr_t OurNet	= NetOurIP    & NetOurSubnetMask;
	    IPaddr_t ServerNet	= TftpServerIP & NetOurSubnetMask;

	    if (OurNet != ServerNet) {
		puts ("; sending through gateway ");
		print_IPaddr (NetOurGatewayIP) ;
	    }
	}
	putc ('\n');

	printf ("Filename '%s'.", tftp_filename);

	if (NetBootFileSize) {
		printf (" Size is 0x%x Bytes = ", NetBootFileSize<<9);
		print_size (NetBootFileSize<<9, "");
	}

	putc ('\n');

	printf ("Load address: 0x%lx\n", load_addr);

	puts ("Loading: *\b");

	NetSetTimeout (TIMEOUT * CFG_HZ, TftpTimeout);
	NetSetHandler (TftpYaffsHandler);

	TftpServerPort = WELL_KNOWN_PORT;
	TftpTimeoutCount = 0;
	TftpState = STATE_RRQ;
	/* Use a pseudo-random port unless a specific port is set */
	TftpOurPort = 1024 + (get_timer(0) % 3072);

#ifdef CONFIG_TFTP_PORT
	if ((ep = getenv("tftpdstp")) != NULL) {
		TftpServerPort = simple_strtol(ep, NULL, 10);
	}
	if ((ep = getenv("tftpsrcp")) != NULL) {
		TftpOurPort= simple_strtol(ep, NULL, 10);
	}
#endif
	TftpBlock = 0;

	/* zero out server ether in case the server ip has changed */
	memset(NetServerEther, 0, 6);
	/* Revert TftpBlkSize to dflt */
	TftpBlkSize = TFTP_BLOCK_SIZE;
#ifdef CONFIG_MCAST_TFTP
	mcast_cleanup();
#endif
/* yaffs variable initial */
	int dev = nand_curr_device; //add by richard
	nandflash = &nand_info[dev];
	skipfirstblk = 1;
	percent_complete = -1;
/* yaffs variable initial */

	TftpSend ();
}

#ifdef CONFIG_MCAST_TFTP
/* Credits: atftp project.
 */

/* pick up BcastAddr, Port, and whether I am [now] the master-client. *
 * Frame:
 *    +-------+-----------+---+-------~~-------+---+
 *    |  opc  | multicast | 0 | addr, port, mc | 0 |
 *    +-------+-----------+---+-------~~-------+---+
 * The multicast addr/port becomes what I listen to, and if 'mc' is '1' then
 * I am the new master-client so must send ACKs to DataBlocks.  If I am not
 * master-client, I'm a passive client, gathering what DataBlocks I may and
 * making note of which ones I got in my bitmask.
 * In theory, I never go from master->passive..
 * .. this comes in with pkt already pointing just past opc
 */
static void parse_multicast_oack(char *pkt, int len)
{
 int i;
 IPaddr_t addr;
 char *mc_adr, *port,  *mc;

	mc_adr=port=mc=NULL;
	/* march along looking for 'multicast\0', which has to start at least
	 * 14 bytes back from the end.
	 */
	for (i=0;i<len-14;i++)
		if (strcmp (pkt+i,"multicast") == 0)
			break;
	if (i >= (len-14)) /* non-Multicast OACK, ign. */
		return;

	i+=10; /* strlen multicast */
	mc_adr = pkt+i;
	for (;i<len;i++) {
		if (*(pkt+i) == ',') {
			*(pkt+i) = '\0';
			if (port) {
				mc = pkt+i+1;
				break;
			} else {
				port = pkt+i+1;
			}
		}
	}
	if (!port || !mc_adr || !mc ) return;
	if (Multicast && MasterClient) {
		printf ("I got a OACK as master Client, WRONG!\n");
		return;
	}
	/* ..I now accept packets destined for this MCAST addr, port */
	if (!Multicast) {
		if (Bitmap) {
			printf ("Internal failure! no mcast.\n");
			free(Bitmap);
			Bitmap=NULL;
			ProhibitMcast=1;
			return ;
		}
		/* I malloc instead of pre-declare; so that if the file ends
		 * up being too big for this bitmap I can retry
		 */
		if (!(Bitmap = malloc (Mapsize))) {
			printf ("No Bitmap, no multicast. Sorry.\n");
			ProhibitMcast=1;
			return;
		}
		memset (Bitmap,0,Mapsize);
		PrevBitmapHole = 0;
		Multicast = 1;
	}
	addr = string_to_ip(mc_adr);
	if (Mcast_addr != addr) {
		if (Mcast_addr)
			eth_mcast_join(Mcast_addr, 0);
		if (eth_mcast_join(Mcast_addr=addr, 1)) {
			printf ("Fail to set mcast, revert to TFTP\n");
			ProhibitMcast=1;
			mcast_cleanup();
			NetStartAgain();
		}
	}
	MasterClient = (unsigned char)simple_strtoul((char *)mc,NULL,10);
	Mcast_port = (unsigned short)simple_strtoul(port,NULL,10);
	printf ("Multicast: %s:%d [%d]\n", mc_adr, Mcast_port, MasterClient);
	return;
}

#endif /* Multicast TFTP */

#endif
