/* This file, and all previous versions of it, is hereby licensed
   under the GNU General Public License. See COPYING for details. */

/* skylark@mips.for.ever */

#include <pspdebug.h>

#include <libc/stdio.h>
#include <libc/string.h>
#include <libc/stdlib.h>
#include <libc/malloc.h>

#include "sha1.h"

#define ET_HEADER 0
#define ET_STRING 1
#define ET_INTEGER 2
#define ET_SUBDIR 3

#define printk pspDebugScreenPrintf

typedef struct ent {
	int type;
	char *name;
	unsigned offset;
	union {
		struct {
			int idx;
			int paridx;
			int nkids;
			int fail;
			struct ent **kids;
			struct ent *nextheader;
			struct ent *parent;
		} header;
		struct {
			int secret;
			char *str;
		} string;
		struct {
			int val;
		} integer;
		struct {
			struct ent *header;
		} subdir;
	};
} ent;

static ent *hdrlist;
static void addheader(ent *p)
{
	p->header.nextheader=hdrlist;
	hdrlist=p;
}
static ent *findheader(char *n,int p)
{
	ent *e;
	for(e=hdrlist;e;e=e->header.nextheader)
		if(p==e->header.paridx && !strcmp(n,e->name))
			return e;
	return NULL;
}

typedef unsigned char uchar;

static void getcheck(uchar *block, int len, uchar *check)
{
	uchar save[4];
	uchar res[20];
	struct sha_ctx ctx;

	memcpy(save, block+14, 4);
	memset(block+14, 0, 4);

	sha_init(&ctx);
	sha_update(&ctx, block, len);
	sha_final(&ctx);
	sha_digest(&ctx, res);

	memcpy(block+14, save, 4);

	check[0] = res[4] ^ res[3] ^ res[2] ^ res[1] ^ res[0];
	check[1] = res[9] ^ res[8] ^ res[7] ^ res[6] ^ res[5];
	check[2] = res[14] ^ res[13] ^ res[12] ^ res[11] ^ res[10];
	check[3] = res[19] ^ res[18] ^ res[17] ^ res[16] ^ res[15];
}

static int checkcheck(uchar *block, int len)
{
	uchar res[4];
	getcheck(block, len, res);
	if(!memcmp(res, block+14, 4))
		return 1;
	return 0;
}

static void fixcheck(uchar *block, int len)
{
	uchar res[4];
	getcheck(block, len, res);
	memcpy(block+14, res, 4);
}

static unsigned char d[131072];
static struct {
	short _0;
	short _1;
	short parent;
	short _2;
	short _3;
	short nent;
	short nblk;
	char name[28];
	short _4;
	short fat[7];
} a[256];

static void parse_subdir(int i, short *f, ent *hdr)
{
	ent *e=calloc(sizeof(ent),1);
	e->type=ET_SUBDIR;
	e->name=strdup(d+i+1);
	e->offset=i+1;
	hdr->header.kids[hdr->header.nkids++]=e;
}

static void parse_int(int i, short *f, ent *hdr)
{
	ent *e=calloc(sizeof(ent),1);
	e->type=ET_INTEGER;
	e->name=strdup(d+i+1);
	e->integer.val=*(int *)(d+i+28);
	e->offset=i+28;
	hdr->header.kids[hdr->header.nkids++]=e;
}

static void parse_string(int i, int s, short *f, ent *hdr)
{
	int l=*(short *)(d+i+28);
	ent *e=calloc(sizeof(ent),1);
	char x[l];
	int j,k;
	for(j=0;j<l;j++) {
		k=d[i+31]+(j>>5);
		x[j]=d[(j&31)+32*(k&15)+512*f[k>>4]];
	}
	x[l]=0;
	e->type=ET_STRING;
	e->name=strdup(d+i+1);
	e->string.secret=s;
	e->string.str=strdup(x);
	e->offset=32*(d[i+31]&15)+512*f[d[i+31]>>4];
	hdr->header.kids[hdr->header.nkids++]=e;
}

static void parse_header(int i, short *f, ent *hdr)
{
	hdr->offset=i;
}

static int parse(int i, short *f, ent *hdr)
{
	switch(d[i]) {
	case 0x01:
		parse_subdir(i,f,hdr);
		break;
	case 0x02:
		parse_int(i,f,hdr);
		break;
	case 0x03:
		parse_string(i,0,f,hdr);
		break;
	case 0x04:
		parse_string(i,1,f,hdr);
		break;
	case 0x0f:
	case 0x10:
	case 0x1f:
	case 0x20:
	case 0x2f:
	case 0x30:
	case 0x3f:
	case 0x40:
		parse_header(i,f,hdr);
		break;
	case 0x00:
		/* WTF? */
		break;
	default:
		return 1;
	}
	return 0;
}

static int walk_fatents(void)
{
	int i,j,k=0;
	uchar *buf;
	for(i=0;i<256;i++)
		if(a[i].nblk) {
			ent *e=calloc(sizeof(ent),1);
			e->type=ET_HEADER;
			e->name=strdup(a[i].name);
			e->header.idx=i;
			e->header.paridx=a[i].parent;
			e->header.kids=calloc(sizeof(ent *),a[i].nent);

			/* reassemble segments
			   it can be done better in-place
			   bleh i don't care foo */
			buf=malloc(a[i].nblk*512);
			for(j=0;j<a[i].nblk;j++)
				memcpy(buf+512*j,d+512*a[i].fat[j],512);
			if(!checkcheck(buf,a[i].nblk*512)) {
				k=1;
				e->header.fail=1;
			}
			free(buf);

			/* walk the walk */
			for(j=0;j<=a[i].nent;j++)
				k|=parse(32*(j&15)+512*a[i].fat[j>>4],a[i].fat,e);
			addheader(e);
		}
	return k;
}

static int resolve_refs(void)
{
	ent *i,*f;
	int j,k=0;
	for(i=hdrlist;i;i=i->header.nextheader)
		for(j=0;j<i->header.nkids;j++)
			if(i->header.kids[j]->type==ET_SUBDIR) {
				f=findheader(i->header.kids[j]->name,i->header.idx);
				if(!f) {
					i->header.kids[j]->subdir.header=NULL;
					k=1;
				} else {
					i->header.kids[j]->subdir.header=f;
					f->header.parent=i;
				}
			}
	return k;
}

unsigned char *get_offset(char *str)
{
	char *nt,sep;
	ent *cur=NULL,*i,*f;
	int j;
	unsigned addr=0xFFFFFFFF;
	if(*str=='/')
		str++;

	while(*str) {
		nt=strchr(str,'/');
		if(!nt)
			nt=str+strlen(str);
		sep=*nt;
		*nt=0;
		nt++;

		if(!cur) {
			f=NULL;
			for(i=hdrlist;i;i=i->header.nextheader)
				if(!i->header.parent)
					if(!strcmp(i->name,str))
						f=i;
			if(!sep || !f) {
				addr=0xFFFFFFFF;
				break;
			}
		} else {
			f=NULL;
			for(j=0;j<cur->header.nkids;j++)
				if(!strcmp(cur->header.kids[j]->name,str))
					f=cur->header.kids[j];
			if(!f) {
				addr=0xFFFFFFFF;
				break;
			}
		}

		cur=f;
		addr=cur->offset;
		if(sep) {
			if(cur->type==ET_SUBDIR) {
				cur=cur->subdir.header;
				if(!cur) {
					addr=0xFFFFFFFF;
					break;
				}
				addr=cur->offset;
			} else if(cur->type!=ET_HEADER) {
				addr=0xFFFFFFFF;
				break;
			}
			str=nt;
		} else
			str=nt-1;
	}
	if(addr==0xFFFFFFFF)
		return NULL;
	return d+addr;
}

int parse_registry(void)
{
	FILE *f=fopen("flash1:/registry/system.dreg","r");
	int k=0,l;
	if(!f) {
		printk("  [A] Failed opening registry files.\n");
		return 1;
	}
	fread(d,131072,1,f);
	fclose(f);
	f=fopen("flash1:/registry/system.ireg","r");
	if(!f) {
		printk("  [B] Failed opening registry files.\n");
		return 1;
	}
	fseek(f,0x5C,SEEK_SET);
	fread(a,256*58,1,f);
	fclose(f);
	k|=walk_fatents();
	if(k) {
		printk("  [C] Checksum errors encountered.\n");
		return 1;
	}
	k|=l=resolve_refs();
	if(l) {
		printk("  [D] Dangling pointers encountered.\n");
		return 1;
	}
	return k;
}

int fixup_registry(void)
{
	int i,j,k=0;
	uchar *buf;
	FILE *f;
	for(i=0;i<256;i++)
		if(a[i].nblk) {
			buf=malloc(a[i].nblk*512);
			for(j=0;j<a[i].nblk;j++)
				memcpy(buf+512*j,d+512*a[i].fat[j],512);
			if(!checkcheck(buf,a[i].nblk*512)) {
				k++;
				fixcheck(buf,a[i].nblk*512);
				memcpy(d+512*a[i].fat[0],buf,512);
			}
			free(buf);
		}
	printk("  [*] Fixed %d registry blocks.\n", k);
	f=fopen("flash1:/registry/system.dreg","w");
	fwrite(d,131072,1,f);
	fclose(f);
	return k;
}
