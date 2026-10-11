//
// lockup.c - Source for the lockup debugging stuff
//
// $Revision: 30 $
//
// $Author: Danielt $
//
#include	<stdio.h>
#include	<ctype.h>
#include	<goose/goose.h>

char	goose_lockup_c_version[] = {"$Revision: 16 $"};
void dump_all_textures(void);
void show_texture_list1(void);



#if defined(SEATTLE)
unsigned int get_tmu_fragmentation(void);
void show_tmu_map( void );
#endif
unsigned int get_mm_frag(int);
void dump_used_handle( int );


static int	_process_called = 0;

extern struct object_node	*olist;
extern process_node_t		*pfree;
extern process_node_t		*plist;
extern texture_node_t		*tlist;

void install_vector(int, int (*)(int, int *));


#ifdef PORT_HOST
/* BIOS syscall 0x409 installs an exception-vector handler; the port has no exception vectors. */
void install_vector(int vector, int (*handler)(int, int *)) { (void)vector; (void)handler; }
#else
#ifndef CYGNUS

__asm__("\n"
"	.globl	install_vector\n"
"	.set		noreorder\n"
"install_vector:\n"
"	break		0x409\n"
"	nop\n"
"	jr			$31\n"
"	nop\n"
"	.set		reorder\n"
"");

#else

__asm__("\n"
"	.globl	install_vector\n"
"	.set		noreorder\n"
"install_vector:\n"
"	.word		0xD | (0x409 << 6)\n"
"	nop\n"
"	jr			$31\n"
"	nop\n"
"	.set		reorder\n"
"");

#endif
#endif /* PORT_HOST */


static char	*ld_commands[] = {
"A - Show Alpha Sprites List",
"B - Show Background List",
"C - Continue",
"D - Debugger Service Mode",
"F - Show Free Process List",
"M - Show TMU Memory Map",
"N - Show NON-Alpha Sprites List",
"O - Show Object List",
"P - Show Process List",
"S - Show Strings List",
"T - Show Texture List",

"V - Show Loaded Sound Banks",
"Anything else - Show This Help"
};

static void show_commands(void)
{
	unsigned	i;

	for(i = 0; i < sizeof(ld_commands)/sizeof(void *); i++)
	{
		fprintf(stderr, "%s\r\n", ld_commands[i]);
	}
	fprintf(stderr, "Command:  ");
}

void show_object_list(void)
{
	struct object_node	*p;

	p = olist;
	if(p)
	{
		fprintf(stderr, "\r\nCurrently running objects\r\n");
		fprintf(stderr, "%-12s", "NAME");
		fprintf(stderr, "%-7s",  "OID");
		fprintf(stderr, "%-7s",  "FLAGS");
		fprintf(stderr, "%-7s",  "ORDER");
		fprintf(stderr, "%-10s", "FUNC PTR");
		fprintf(stderr, "%-10s", "DATA PTR");
		fprintf(stderr, "\r\n");
		while(p)
		{
			fprintf(stderr, "%-12.8s", p->object_name);
			fprintf(stderr, "%-7hx",   p->object_id);
			fprintf(stderr, "%-7hx",   p->object_flags);
			fprintf(stderr, "%-7hd",   p->draw_order);
			fprintf(stderr, "%-10x",   (int)(p->object_draw_func));
			fprintf(stderr, "%-10x",   (int)(p->object_data));
			fprintf(stderr, "\r\n");
			p = p->next;
		}
	}
	else
	{
		fprintf(stderr, "\r\nThere were NO objects found in the list\r\n\r\n");
	}
}

void show_process_list(void)
{
	process_node_t	*p;

	p = plist;
	if(p)
	{
		fprintf(stderr, "\r\nCurrently running processes\r\n");
		fprintf(stderr, "%-12s", "NAME");
		fprintf(stderr, "%-7s",  "PID");
		fprintf(stderr, "%-7s",  "LEVEL");
		fprintf(stderr, "%-9s",  "SLEEP");
		fprintf(stderr, "%-10s", "ENTRY");
		fprintf(stderr, "%-10s", "WAKEUP");
		fprintf(stderr, "\r\n");
		while(p)
		{
			fprintf(stderr, "%-12.8s", p->process_name);
			fprintf(stderr, "%-7hx",   p->process_id);
			fprintf(stderr, "%-7hd",   p->run_level);

			if (p->process_sleep_time < 0)
				fprintf(stderr, "%-9s","Suspend");
			else
				fprintf(stderr, "%-9u",p->process_sleep_time);
			fprintf(stderr, "%-10x",   (int)(p->entry_func));
			fprintf(stderr, "%-10x",   (((int *)(p->process_stack_ptr))-10)[0]);
			fprintf(stderr, "\r\n");
			p = p->next;
		}
	}
	else
	{
		fprintf(stderr, "\r\nThere were NO processes found currently running\r\n\r\n");
	}
}

void show_free_process_list(void)
{
	process_node_t	*p;

	p = pfree;
	if(p)
	{
		fprintf(stderr, "\r\nCurrently running processes\r\n");
		fprintf(stderr, "%-12s", "NAME");
		fprintf(stderr, "%-7s",  "PID");
		fprintf(stderr, "%-7s",  "LEVEL");
		fprintf(stderr, "\r\n");
		while(p)
		{
			fprintf(stderr, "%-12.8s", "FREE");
			fprintf(stderr, "%-7hx",   p->process_id);
			fprintf(stderr, "%-7hd",   p->run_level);
			fprintf(stderr, "\r\n");
			p = p->next;
		}
	}
	else
	{
		fprintf(stderr, "\r\nThere were NO processes found on the free list\r\n\r\n");
	}
}

#if defined(BANSHEE)
#define GRX		"BANSHEE"
#define TRAM	(1<<24)

#elif defined(VOODOO2)
#define GRX		"VOODOO2"
#define TRAM	(1<<23)

#else
#define GRX		"VOODOO1"
#define TRAM	(1<<22)
#endif

void show_texture_list(void)
{
	texture_node_t	*tn;
	unsigned int	total_tmu_used = 0;
	unsigned int	tmu_mem;
	unsigned int	i;
	unsigned int	width;
	unsigned int	height;
	unsigned int	mult;

	tn = tlist;
	if(tn)
	{
		fprintf(stderr, "\r\nCurrently loaded textures\r\n");
		fprintf(stderr, "%-15s", "NAME");
		fprintf(stderr, "%-7s",  "TID");
		fprintf(stderr, "%-7s",  "COUNT");
		fprintf(stderr, "%-8s",  "SIZE");
		fprintf(stderr, "%-8s",  "LOCKED");
#ifndef VEGAS
		fprintf(stderr, "%-6s",  "MMID");
#else
		fprintf(stderr, "%-10s", "TCTL PTR");
#endif
		fprintf(stderr, "%-8s",  "FORMAT");
		fprintf(stderr, "%-9s",  "MIPMAPS");
		fprintf(stderr, "%-6s",  "FRAG");
		fprintf(stderr, "\r\n");
		while(tn)
		{
			width = tn->texture_info.header.width;
			height = tn->texture_info.header.height;
			mult = 1;
			if(tn->texture_info.header.format >= GR_TEXFMT_16BIT)
			{
				mult = 2;
			}
			tmu_mem = 0;
			for(i = tn->texture_info.header.large_lod; i <= tn->texture_info.header.small_lod; i++)
			{
				tmu_mem += (width * height * mult);
				width >>= 1;
				height >>= 1;
			}
			fprintf(stderr, "%-15.12s", tn->texture_name);
			fprintf(stderr, "%-7hx", tn->texture_id);
			fprintf(stderr, "%-7hd", tn->texture_count);
			fprintf(stderr, "%-8u", tmu_mem);
			fprintf(stderr, "%-8s", (tn->texture_flags & TEXTURE_LOCKED?"YES":"NO"));
#ifndef VEGAS
			fprintf(stderr, "%-6d", (int)tn->texture_handle);
#else
			fprintf(stderr, "%-10.8X", (int)tn->texture_handle);
#endif
			fprintf(stderr, "%-8s", (tn->texture_info.header.format >= GR_TEXFMT_16BIT ? "16 BIT" : "8 BIT"));
			fprintf(stderr, "%-9d", tn->texture_info.header.small_lod - tn->texture_info.header.large_lod + 1);
#ifndef VEGAS
			fprintf(stderr, "%-6u", get_mm_frag(tn->texture_handle));
#else
			fprintf(stderr, "%-6s", "n/a");
#endif
			fprintf(stderr, "\r\n");
			total_tmu_used += tmu_mem;
			tn = tn->next;
		}
		fprintf(stderr, "\r\nTotal TMU Memory used:                       %u\r\n", total_tmu_used);
		fprintf(stderr, "TMU Space Available:                         %u\r\n", TRAM - total_tmu_used);
		fprintf(stderr, "Percentage Available:                        %2.0f%%\r\n", ((float)(TRAM - total_tmu_used) / (float)TRAM) * 100.0f);
#if defined(SEATTLE)
		fprintf(stderr, "Total Lost due to TMU memory fragmentation:  %u\r\n", get_tmu_fragmentation());
#endif
		fprintf(stderr, "\r\n");
	}
	else
	{
		fprintf(stderr, "\r\nThere are NO Textures Currently loaded\r\n\r\n");
	}
}

void dump_used_handle( int mmid )
{
	texture_node_t	*tn;
	unsigned int	tmu_mem;
	unsigned int	i;
	unsigned int	width;
	unsigned int	height;
	unsigned int	mult;
	
	tn = tlist;
	while( tn )
	{
		if( (int)(tn->texture_handle) == mmid )
		{
			width = tn->texture_info.header.width;
			height = tn->texture_info.header.height;
			mult = 1;
			if(tn->texture_info.header.format >= GR_TEXFMT_16BIT)
			{
				mult = 2;
			}
			tmu_mem = 0;
			for(i = tn->texture_info.header.large_lod; i <= tn->texture_info.header.small_lod; i++)
			{
				tmu_mem += (width * height * mult);
				width >>= 1;
				height >>= 1;
			}
			fprintf(stderr, "%-14.12s", tn->texture_name);
			fprintf(stderr, "%-6hx", tn->texture_id);
			fprintf(stderr, "%-5hd", tn->texture_count);
			fprintf(stderr, "%-8u", tmu_mem);
			fprintf(stderr, "%-8s", (tn->texture_flags & TEXTURE_LOCKED?"YES":"NO"));
			fprintf(stderr, "%-6d", (int)tn->texture_handle);
			fprintf(stderr, "%-8s", (tn->texture_info.header.format >= GR_TEXFMT_16BIT ? "16 BIT" : "8 BIT"));
			fprintf(stderr, "%-9d", tn->texture_info.header.small_lod - tn->texture_info.header.large_lod + 1);
#ifndef VEGAS
			fprintf(stderr, "%-6u", get_mm_frag(tn->texture_handle));
#endif
			return;
		}
		
		tn = tn->next;
	}
}

/* CarnEvil's revision 16: no 'Z' user menu, the key is echoed before the switch */
static int list_dumper(int cause, int *regs)
{
	int	done = 0;
	int	cont = 1;
	int	c;

	while(!done)
	{
		show_commands();
		c = getchar();
		fprintf(stderr, "%c\r\n", c);
		switch(toupper(c))
		{
			case 'A':				// Show alpha sprites list
			case 'B':				// Show background sprites list
			case 'N':				// Show non-alpha sprites list
			case 'S':				// Show Strings list
			{
				fprintf(stderr, "Not yet implemented\r\n");
				break;
			}
			case 'C':				// Continue execution
			{
				cont = 1;
				done = 1;
				break;
			}
			case 'D':				// Goto debugger service mode
			{
				cont = 0;
				done = 1;
				break;
			}
			case 'F':				// Show the free process list
			{
				show_free_process_list();
				break;
			}
			case 'M':				// Show TMU Memory Map
			{
				show_tmu_map();
				break;
			}
			case 'O':				// Show Object list
			{
				show_object_list();
				break;
			}
			case 'P':				// Show the process list
			{
				show_process_list();
				break;
			}
			case 'T':				// Show the texture list
			{
				show_texture_list();
				break;
			}
			case 'V':				// Sound banks
			{
				snd_report_banks();
				break;
			}
		}
	}
	if(!_process_called)
	{
		if(cont)
		{
			regs[PC] += 4;
			return(0);
		}
		else
		{
			return(1);
		}
	}
	return(0);
}


static int null_lock(int cause, int *regs);   /* defined last in this revision */


void enable_lockup(void)
{
	install_vector(13, list_dumper);
}

void disable_lockup(void)
{
	install_vector(13, null_lock);
}

void enable_tlb_dump(void)
{
	install_vector(2, list_dumper);
	install_vector(3, list_dumper);
}


void do_list_dump(void)
{
	_process_called = 1;
	list_dumper(0, NULL);
	_process_called = 0;
}

static int null_lock(int cause, int *regs)
{
#ifdef DEBUG
	fprintf(stderr, "LOCKUP with lockups NOT enabled\r\n");
#endif
	regs[PC] += 4;
	return(0);
}
