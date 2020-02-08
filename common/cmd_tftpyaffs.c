/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <net.h>
#include <nand.h>			//add by richard
#include "../net/tftpyaffs.h"//add by richard


extern int do_bootm (cmd_tbl_t *, int, int, char *[]);

static int netboot_common (proto_t, cmd_tbl_t *, int , char *[]);

int do_tftpyaffs (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return netboot_common (TFTPYAFFS, cmdtp, argc, argv);
}

U_BOOT_CMD(
	tfyaffs,	5,	1,	do_tftpyaffs,
	"boot image via network using TFTP protocol and write to nand",
	"[loadAddress] [[hostIPaddr:]bootfilename] offset|partition [size]"
);



static void netboot_update_env (void)
{
	char tmp[22];

	if (NetOurGatewayIP) {
		ip_to_string (NetOurGatewayIP, tmp);
		setenv ("gatewayip", tmp);
	}

	if (NetOurSubnetMask) {
		ip_to_string (NetOurSubnetMask, tmp);
		setenv ("netmask", tmp);
	}

	if (NetOurHostName[0])
		setenv ("hostname", NetOurHostName);

	if (NetOurRootPath[0])
		setenv ("rootpath", NetOurRootPath);

	if (NetOurIP) {
		ip_to_string (NetOurIP, tmp);
		setenv ("ipaddr", tmp);
	}

	if (NetServerIP) {
		ip_to_string (NetServerIP, tmp);
		setenv ("serverip", tmp);
	}

	if (NetOurDNSIP) {
		ip_to_string (NetOurDNSIP, tmp);
		setenv ("dnsip", tmp);
	}
#if defined(CONFIG_BOOTP_DNS2)
	if (NetOurDNS2IP) {
		ip_to_string (NetOurDNS2IP, tmp);
		setenv ("dnsip2", tmp);
	}
#endif
	if (NetOurNISDomain[0])
		setenv ("domain", NetOurNISDomain);

#if defined(CONFIG_CMD_SNTP) \
    && defined(CONFIG_BOOTP_TIMEOFFSET)
	if (NetTimeOffset) {
		sprintf (tmp, "%d", NetTimeOffset);
		setenv ("timeoffset", tmp);
	}
#endif
#if defined(CONFIG_CMD_SNTP) \
    && defined(CONFIG_BOOTP_NTPSERVER)
	if (NetNtpServerIP) {
		ip_to_string (NetNtpServerIP, tmp);
		setenv ("ntpserverip", tmp);
	}
#endif
}

extern int
arg_off_size(int argc, char *argv[], nand_info_t *nand, ulong *off, size_t *size);

static int
netboot_common (proto_t proto, cmd_tbl_t *cmdtp, int argc, char *argv[])
{
	char *s;
	char *end;
	int   rcode = 0;
	int   size;
	ulong addr;
	extern ulong root_offset, root_start;
	extern size_t root_size, root_maxsize;


	/* pre-set load_addr */
	if ((s = getenv("loadaddr")) != NULL) {
		load_addr = simple_strtoul(s, NULL, 16);
	}

	switch (argc) {
	case 1:
		break;

	case 2: /*
		 * Only one arg - accept two forms:
		 * Just load address, or just boot file name. The latter
		 * form must be written in a format which can not be
		 * mis-interpreted as a valid number.
		 */
		 //tow arg:   tftp filename
		 addr = simple_strtoul(argv[1], &end, 16);
		 if (end == (argv[1] + strlen(argv[1])))//sure argv[1] is a  number
			 load_addr = addr;
		 else
		 //if argv[1] is not a number, it must be a bootfilename
			 copy_filename(BootFile, argv[1], sizeof(BootFile));
		 root_offset = CONFIG_YAFFS2_OFFSET_DEFAULT;
		 root_size = CONFIG_SYS_NAND_BLOCK_SIZE;//default write_size is a block
		 root_maxsize = 0x500000;//default write_maxsize is 5MB
		 break;
		 
	case 3:
		load_addr = simple_strtoul(argv[1], NULL, 16);
		copy_filename(BootFile, argv[2], sizeof(BootFile));
		root_offset = CONFIG_YAFFS2_OFFSET_DEFAULT;
		root_size = CONFIG_SYS_NAND_BLOCK_SIZE;//default write_size is a block
		root_maxsize = 0x500000;//default write_maxsize is 5MB
		break;
	case 4://tftp addr filename off
	case 5://tftp addr filename off size
		{
			int dev; //add by richard
			dev = nand_curr_device;
			load_addr = simple_strtoul(argv[1], NULL, 16);
			copy_filename(BootFile, argv[2], sizeof(BootFile));
			
			/* get nand offset and size (maybe a partition offset)*/
			if (arg_off_size(argc - 3, argv + 3, &nand_info[nand_curr_device],
						&root_offset, &root_size) != 0){
				/* tand , addr, 	filename, offset/partition size */
				/* argv , arv + 1, argv + 2, argv +3 */
				printf("error : arg_off_size failed \n");
				return 1;
			}
			root_start = root_offset;
			printf("nand root_start 0x%lx\n",root_start);
			/* size is unspecified */
	/*
			if (argc < 5)
				adjust_size_for_badblocks(&root_size, root_offset, dev);*/
//			root_offset += CONFIG_SYS_NAND_BLOCK_SIZE;//For yaffs, skip first block
			break;
		}
		
	default: printf ("Usage:\n%s\n", cmdtp->usage);
		show_boot_progress (-80);
		return 1;
	}


	show_boot_progress (80);
	if ((size = NetLoop(proto)) < 0) {
		show_boot_progress (-81);
		return 1;
	}

	show_boot_progress (81);
	/* NetLoop ok, update environment */
	netboot_update_env();

	/* done if no file was loaded (no errors though) */
	if (size == 0) {
		show_boot_progress (-82);
		return 0;
	}

	/* flush cache */
	flush_cache(load_addr, size);

	/* Loading ok, check if we should attempt an auto-start */
	if (((s = getenv("autostart")) != NULL) && (strcmp(s,"yes") == 0)) {
		char *local_args[2];
		local_args[0] = argv[0];
		local_args[1] = NULL;

		printf ("Automatic boot of image at addr 0x%08lX ...\n",
			load_addr);
		show_boot_progress (82);
		rcode = do_bootm (cmdtp, 0, 1, local_args);
	}

#ifdef CONFIG_AUTOSCRIPT
	if (((s = getenv("autoscript")) != NULL) && (strcmp(s,"yes") == 0)) {
		printf ("Running autoscript at addr 0x%08lX", load_addr);

		s = getenv ("autoscript_uname");
		if (s)
			printf (":%s ...\n", s);
		else
			puts (" ...\n");

		show_boot_progress (83);
		rcode = autoscript (load_addr, s);
	}
#endif
	if (rcode < 0)
		show_boot_progress (-83);
	else
		show_boot_progress (84);
	return rcode;
}

