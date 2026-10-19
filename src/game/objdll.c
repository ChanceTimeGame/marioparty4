#include "game/object.h"
#include "game/dvd.h"
#include "game/memory.h"
#ifdef TARGET_PC
#include "port/host.h"
#endif

#ifndef __MWERKS__
#include <string.h>
#endif

#if defined(__linux__)
#include <dlfcn.h>
#endif

#ifdef TARGET_PC
#include <stdio.h>
#endif

typedef s32 (*DLLProlog)(void);
typedef void (*DLLEpilog)(void);

#ifdef TARGET_PC
typedef void (*DLLObjectSetup)(void);
#endif

omDllData *omDLLinfoTbl[OM_DLL_MAX];

static FileListEntry *omDLLFileList;

void omDLLDBGOut(void)
{
	OSReport("DLL DBG OUT\n");
}

void omDLLInit(FileListEntry *ovl_list)
{
	s32 i;
	OSReport("DLL DBG OUT\n");
	for(i=0; i<OM_DLL_MAX; i++) {
		omDLLinfoTbl[i] = NULL;
	}
	omDLLFileList = ovl_list;
}

s32 omDLLStart(s16 overlay, s16 flag)
{
	s32 dllno;
	OSReport("DLLStart %d %d\n", overlay, flag);
#ifdef TARGET_PC
	/* Flushed state beacon for scripted runs (mp4sweep.ps1): survives a crash, unlike OSReport. */
	fprintf(stderr, "PB_OVL %d %s\n", overlay, omDLLFileList && omDLLFileList[overlay].name ? omDLLFileList[overlay].name : "?");
	fflush(stderr);
#endif
	dllno = omDLLSearch(overlay);
	if(dllno >= 0 && !flag) {
		omDllData *dll = omDLLinfoTbl[dllno];
#ifdef TARGET_PC
		OSReport("objdll>Already Loaded %s\n", dll->name);
#else
		OSReport("objdll>Already Loaded %s(%08x %08x)\n", dll->name, dll->module, dll->bss);
		
		omDLLInfoDump(&dll->module->info);
		omDLLHeaderDump(dll->module);
		memset(dll->bss, 0, dll->module->bssSize);
		HuMemDCFlushAll();
		dll->ret = ((DLLProlog)dll->module->prolog)();
#endif
		OSReport("objdll> %s prolog end\n", dll->name);
		return dllno;
	} else {
		for(dllno=0; dllno<OM_DLL_MAX; dllno++) {
			if(omDLLinfoTbl[dllno] == NULL) {
				break;
			}
		}
		if(dllno == OM_DLL_MAX) {
			return -1;
		}
		omDLLLink(&omDLLinfoTbl[dllno], overlay, TRUE);
		return dllno;
	}
}

void omDLLNumEnd(s16 overlay, s16 flag)
{
	s16 dllno;
	if(overlay < 0) {
		OSReport("objdll>omDLLNumEnd Invalid dllno %d\n", overlay);
		return;
	}
	OSReport("objdll>omDLLNumEnd %d %d\n", overlay, flag);
	dllno = omDLLSearch(overlay);
	if(dllno < 0) {
		OSReport("objdll>omDLLNumEnd not found DLL No%d\n", overlay);
		return;
	}
	omDLLEnd(dllno, flag);
}

void omDLLEnd(s16 dllno, s16 flag)
{
	OSReport("objdll>omDLLEnd %d %d\n", dllno, flag);
	if(flag == 1) {
		OSReport("objdll>End DLL:%s\n", omDLLinfoTbl[dllno]->name);
		omDLLUnlink(omDLLinfoTbl[dllno], 1);
		omDLLinfoTbl[dllno] = NULL;
	} else {
		omDllData *dll;
		dll = omDLLinfoTbl[dllno];
#ifdef __MWERKS__
		OSReport("objdll>Call Epilog\n");
		((DLLEpilog)dll->module->epilog)();
#endif
		OSReport("objdll>End DLL stayed:%s\n", omDLLinfoTbl[dllno]->name);
	}
	OSReport("objdll>End DLL finish\n");
}

#ifdef TARGET_PC
#include <stdio.h>
#include <stdlib.h>

// A missing overlay is not survivable, and must not be allowed to look like it
// is. Before this, a failed load logged one line and carried straight on into
// objectSetup() -- an unchecked call through a null pointer -- so the symptom
// was a bare access violation with nothing saying which overlay was wanted.
//
// Stop, and say enough to tell the two causes apart. A library that is simply
// absent means the overlay was never built and there is nothing to fix in the
// code; a library that exists but will not load is a genuine link problem --
// usually an unresolved import, which for this tree means a symbol that needs
// adding to the game's dol.def (see "Symbol export gotchas" in CLAUDE.md).
//
// Kept identical in both titles; see games/mp5/src/game/objdll.c.
static void omDLLLinkFailed(const char *name, s16 overlay, const char *osError)
{
	char path[1024];
	FILE *probe = fopen(port_module_file(name, path, sizeof(path)), "rb");
	const BOOL present = (probe != NULL);
	if(probe != NULL) {
		fclose(probe);
	}
	// stderr, not OSReport: OSReport goes through aurora::Module, which has
	// silently emitted nothing from some call sites, and this is the one
	// message that must never be the one that goes missing.
	fprintf(stderr,
		"\n"
		"================================================================\n"
		" CHANCE TIME: cannot load overlay %d (%s)\n"
		"\n",
		(int)overlay, name);
	if(!present) {
		fprintf(stderr,
			" There is no %s next to the executable.\n"
			" That overlay is not built -- check PB_REL_FILES in the game's\n"
			" game.cmake.\n",
			name);
	} else {
		fprintf(stderr,
			" %s exists but the OS refused to load it:\n"
			"   %s\n"
			" That is a real link failure rather than a missing overlay --\n"
			" most often an unresolved import, which here usually means a\n"
			" symbol that needs adding to the game's dol.def.\n",
			name, (osError != NULL) ? osError : "(no error text)");
	}
	fprintf(stderr,
		"\n"
		" Stopping here on purpose. Continuing would run the game with an\n"
		" overlay whose ObjectSetup never executed, and every later failure\n"
		" would point somewhere unrelated.\n"
		"================================================================\n");
	fflush(stderr);
	OSReport("objdll>FATAL: overlay %d (%s) could not be loaded\n", (int)overlay, name);
	fflush(stdout);
	// Inside a Chance Time host: the game stops and the players go back to the
	// picker, which shows this (port/host.h). The calling process is parked for
	// good; the game loop ends at its next frame and frees it with the rest.
	if(HuPrcCurrentGet() != NULL) {
		char text[256];
		snprintf(text, sizeof(text), "Part of this game is missing from the Chance Time install (%s).", name);
		if(port_report_error(text)) {
			for(;;) {
				HuPrcVSleep();
			}
		}
	}
	// _Exit rather than exit: this runs on a libco fiber, and atexit handlers
	// tearing down Dawn and SDL from a fiber stack would turn a clear stop into
	// a confusing crash after it. Both streams are flushed above.
	_Exit(1);
}
#endif

omDllData *omDLLLink(omDllData **dll_ptr, s16 overlay, s16 flag)
{
	omDllData *dll;
	FileListEntry *dllFile = &omDLLFileList[overlay];
	OSReport("objdll>Link DLL:%s\n", dllFile->name);
	dll = HuMemDirectMalloc(HEAP_SYSTEM, sizeof(omDllData));
	*dll_ptr = dll;
	dll->name = dllFile->name;
#ifdef _WIN32
    {
		// By full path from dol.dll's folder: a host process runs from elsewhere (SINGLE_PROCESS_PLAN).
		char path[1024];
		dll->hModule = LoadLibraryExA(port_module_file(dllFile->name, path, sizeof(path)), NULL,
			LOAD_WITH_ALTERED_SEARCH_PATH);
	}
	if (dll->hModule == NULL) {
		char winErr[64];
		snprintf(winErr, sizeof(winErr), "LoadLibrary failed, GetLastError() = %lu",
			(unsigned long)GetLastError());
		omDLLLinkFailed(dllFile->name, overlay, winErr);
	}
#elif defined(__linux__)
	{
		// RPATH has to be set properly in CMake
		char path[1024];
		dll->handle = dlopen(port_module_file(dllFile->name, path, sizeof(path)), RTLD_LAZY);
		if (dll->handle == NULL) {
			omDLLLinkFailed(dllFile->name, overlay, dlerror());
		}
	}
#elif defined(__MWERKS__)
	dll->module = HuDvdDataReadDirect(dllFile->name, HEAP_SYSTEM);
	dll->bss = HuMemDirectMalloc(HEAP_SYSTEM, dll->module->bssSize);
	if(OSLink(&dll->module->info, dll->bss) != TRUE) {
		OSReport("objdll>++++++++++++++++ DLL Link Failed\n");
	}
	omDLLInfoDump(&dll->module->info);
	omDLLHeaderDump(dll->module);
	OSReport("objdll>LinkOK %08x %08x\n", dll->module, dll->bss);
#else
	OSReport("DLL/so loading is not implemented for this platform");
#endif
	if(flag == 1) {
		OSReport("objdll> %s prolog start\n", dllFile->name);
#ifdef _WIN32
		{
		DLLObjectSetup objectSetup = (DLLObjectSetup)GetProcAddress(dll->hModule, "ObjectSetup");
		if(objectSetup == NULL) {
			omDLLLinkFailed(dllFile->name, overlay, "loaded, but exports no ObjectSetup symbol");
		}
		objectSetup();
		}
#elif defined(__linux__)
		{
		DLLObjectSetup objectSetup = (DLLObjectSetup)dlsym(dll->handle, "ObjectSetup");
		if(objectSetup == NULL) {
			omDLLLinkFailed(dllFile->name, overlay, "loaded, but exports no ObjectSetup symbol");
		}
		objectSetup();
		}
#else
		dll->ret = ((DLLProlog)dll->module->prolog)();
#endif
		OSReport("objdll> %s prolog end\n", dllFile->name);
	}
	return dll;
}

void omDLLUnlink(omDllData *dll_ptr, s16 flag)
{
	OSReport("odjdll>Unlink DLL:%s\n", dll_ptr->name);
#ifdef _WIN32
    FreeLibrary(dll_ptr->hModule);
#elif defined(__linux__)
	dlclose(dll_ptr->handle);
#else
	if(flag == 1) {
		OSReport("objdll>Unlink DLL epilog\n");
		((DLLEpilog)dll_ptr->module->epilog)();
		OSReport("objdll>Unlink DLL epilog finish\n");
	}
	if(OSUnlink(&dll_ptr->module->info) != TRUE) {
		OSReport("objdll>+++++++++++++++++ DLL Unlink Failed\n");
	}
	HuMemDirectFree(dll_ptr->bss);
	HuMemDirectFree(dll_ptr->module);
#endif
	HuMemDirectFree(dll_ptr);
}

s32 omDLLSearch(s16 overlay)
{
	s32 i;
	FileListEntry *dllFile = &omDLLFileList[overlay];
	OSReport("Search:%s\n", dllFile->name);
	for(i=0; i<OM_DLL_MAX; i++) {
		omDllData *dll = omDLLinfoTbl[i];
		if(dll != NULL && strcmp(dll->name, dllFile->name) == 0) {
			OSReport("+++++++++++ Find%d: %s\n", i, dll->name);
			return i;
		}
	}
	return -1;
}

void omDLLInfoDump(OSModuleInfo *module)
{
	OSReport("===== DLL Module Info dump ====\n");
	OSReport("                   ID:0x%08x\n", module->id);
	OSReport("             LinkPrev:0x%08x\n", module->link.prev);
	OSReport("             LinkNext:0x%08x\n", module->link.next);
	OSReport("          Section num:%d\n", module->numSections);
	OSReport("Section info tbl ofst:0x%08x\n", module->sectionInfoOffset);
	OSReport("           nameOffset:0x%08x\n", module->nameOffset);
	OSReport("             nameSize:%d\n", module->nameSize);
	OSReport("              version:0x%08x\n", module->version);
	OSReport("===============================\n");
}

void omDLLHeaderDump(OSModuleHeader *module)
{
	OSReport("==== DLL Module Header dump ====\n");
	OSReport("          bss Size:0x%08x\n", module->bssSize);
	OSReport("        rel Offset:0x%08x\n", module->relOffset);
	OSReport("        imp Offset:0x%08x\n", module->impOffset);
	OSReport("    prolog Section:%d\n", module->prologSection);
	OSReport("    epilog Section:%d\n", module->epilogSection);
	OSReport("unresolved Section:%d\n", module->unresolvedSection);
	OSReport("       prolog func:0x%08x\n", module->prolog);
	OSReport("       epilog func:0x%08x\n", module->epilog);
	OSReport("   unresolved func:0x%08x\n", module->unresolved);
	OSReport("================================\n");
}

#ifdef TARGET_PC
// Unloads every overlay still loaded, stayed ones included, when the game loop has returned: each
// holds a reference on dol.dll, which the host unloads next (docs/SINGLE_PROCESS_PLAN.md). No
// epilogs: like a console reset, the whole image goes. The slots are cleared; their HuMem
// blocks go with the heaps.
void omDLLFreeAll(void)
{
	s32 i;
	for(i = 0; i < OM_DLL_MAX; i++) {
		if(omDLLinfoTbl[i] != NULL) {
#ifdef _WIN32
			FreeLibrary(omDLLinfoTbl[i]->hModule);
#elif defined(__linux__)
			dlclose(omDLLinfoTbl[i]->handle);
#endif
			omDLLinfoTbl[i] = NULL;
		}
	}
}
#endif
