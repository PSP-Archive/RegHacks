/* This file, and all previous versions of it, is hereby licensed
   under the GNU General Public License. See COPYING for details. */

/* skylark@mips.for.ever */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

#include <libc/malloc.h>

#include "dregman3.h"

/* Define the module info section */
PSP_MODULE_INFO("buttonhack", 0, 1, 1);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int execbtnhack(void)
{
	int res, flg=0;
	char *ptr;

	pspDebugScreenPrintf("[*] Reading registry...\n");
	res=parse_registry();
	if(res) {
		pspDebugScreenPrintf("[%d] The registry is inconsistent, it will be fixed.\n",0);
		flg=1;
	}
	pspDebugScreenPrintf("[*] Changing button_assign...\n");
	ptr=get_offset("CONFIG/SYSTEM/XMB/button_assign");
	if(!ptr) {
		pspDebugScreenPrintf("[%d] Your registry is weird. Send it to skylark@ircwire.net.\n",1);
		return 1;
	}
	if(*ptr) {
		pspDebugScreenPrintf("  [*] Changing from [1] to [0]...\n");
		*ptr=0;
	} else {
		pspDebugScreenPrintf("  [*] Changing from [0] to [1]...\n");
		*ptr=1;
	}
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
	return 0;
}

int main(int argc, char *argv[])
{
	int done;
	SceCtrlData pad;

	pspDebugScreenInit();
	pspDebugScreenPrintf("[*] ButtonHack 0.92 - (c) 2006 Skylark\n\n");
	execbtnhack();
	pspDebugScreenPrintf("\n[*] Hold the power button for hard reboot.\n");
	while(!done){
    		sceCtrlReadBufferPositive(&pad, 1); 
		if (pad.Buttons != 0)
			if (pad.Buttons & PSP_CTRL_CIRCLE)
				done=1;
	}

	return 0;
}
