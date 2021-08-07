/* This file, and all previous versions of it, is hereby licensed
   under the GNU General Public License. See COPYING for details. */

/* skylark@mips.for.ever */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

#include <libc/malloc.h>
#include <libc/string.h>

#include "dregman3.h"

/* Define the module info section */
PSP_MODULE_INFO("fonthack", 0, 1, 1);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define MODDIR "ms0:/fontmod"
#define ORGDIR "flash0:/font"

int execfonthack(void)
{
	int res, flg=0;
	char *ptr;

	pspDebugScreenPrintf("[*] Reading registry...\n");
	res=parse_registry();
	if(res) {
		pspDebugScreenPrintf("[%d] The registry is inconsistent, it will be fixed.\n",0);
		flg=1;
	}
	pspDebugScreenPrintf("[*] Changing path_name...\n");
	ptr=get_offset("DATA/FONT/path_name");
	if(!ptr) {
		pspDebugScreenPrintf("[%d] Your registry is weird. Send it to skylark@ircwire.net.\n",1);
		return 1;
	}
	if(strlen(ptr)!=strlen(MODDIR)) {
		pspDebugScreenPrintf("[%d] Your registry is weird. Send it to skylark@ircwire.net.\n",2);
		return 1;
	}
	if(!strcmp(ptr,MODDIR))
		strcpy(ptr,ORGDIR);
	else
		strcpy(ptr,MODDIR); /* pwned! */
	pspDebugScreenPrintf("[*] Writing registry...\n");
	res=fixup_registry();
	if(res!=1) {
		if(!flg)
			pspDebugScreenPrintf("[%d] Oops, bluescreen! I am really sorry :(.\n",2);
		else
			pspDebugScreenPrintf("[%d] Registry inconsistencies have been overwritten.\n",2);
		return 1;
	}
	pspDebugScreenPrintf("[*] Hehe, I knew it'd work!\n");
}

int main(int argc, char *argv[])
{
	int done=0;
	SceCtrlData pad;

	pspDebugScreenInit();
	pspDebugScreenPrintf("[*] FontHack 0.92 - (c) 2006 Skylark\n\n");
	execfonthack();
	pspDebugScreenPrintf("\n[*] Hold the power button for hard reboot.\n");
	while(!done){
    		sceCtrlReadBufferPositive(&pad, 1); 
		if (pad.Buttons != 0)
			if (pad.Buttons & PSP_CTRL_CIRCLE)
				done=1;
	}

	return 0;
}
