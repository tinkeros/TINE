#define COMPONENT_COMPILER 1
#define COMPONENT_GRAPHICS 1
#define IMPORT_BUILTINS 1
#define INIT_EVERYTHING 1
#define FPTRS 1
//#define GEN_HEADERS 1
#define USE_DOLDOC 1
#include "HCRT_TOS.HC"
#ifdef GEN_HEADERS
#include"GenHeaders.HC";
CDoc *unfound=DocNew("unfound.DD");
#endif
static U0 BootstrapImportSymbol(U8 *name,U8 *ptr,I64 type) {
    CHash *find=HashFind(name,Fs->hash_table,-1);
    if(find&&find->type&HTT_GLBL_VAR) {
        find(CHashGlblVar*)->data_addr=ptr;
    } else if(find&&find->type&HTT_FUN&&!(find(CHashFun*)->flags&1<<Ff_INTERNAL)) {
        find(CHashFun*)->exe_addr=ptr;
        Btr(&find(CHashFun*)->flags,Cf_EXTERN);
    } else if(name[0]=='_'||!StrNCmp(name,"SYS",3)) {
	find=CAlloc(sizeof(CHashExport));
	find->str=StrNew(name);
	find->type=HTT_EXPORT_SYS_SYM;
	find(CHashExport*)->val=ptr;
	HashAdd(find,Fs->hash_table);
#ifdef GEN_HEADERS
       DocPrint(unfound,"%s\n",name);
#endif
    } else {
#ifdef GEN_HEADERS
       DocPrint(unfound,"%s\n",name);
#endif
    }
}
asm {
//I am embeding these into the binary so end-users don't mess up thier KERNEL*.HH files
  KERNELA_BIN::
  BINFILE "KERNELA.HH";
  DU8 0;
  KERNELB_BIN::
  BINFILE "KERNELB.HH";
  DU8 0;

};
U0 LoadImps() {
  try {
   ExePutS("#define FPTRS\n;;");
    ExePutS(KERNELA_BIN,"KERNELA.HH");
    __BootstrapForeachSymbol(&BootstrapImportSymbol);
#ifdef GEN_HEADERS
    DocWrite(unfound);
    DocDel(unfound);
#endif
//Load asm externs second
    ExePrint("#define BOOT_TOS\n;;");
    ExePutS(KERNELB_BIN,"KERNELB.HH");
  } catch {
    TOSPrint("FDSF:%s\n",&(Fs->except_ch));
    Fs->catch_except=TRUE;
  };
}

U0 StartUpTasks()
{
  User;
  WinTileVert;
  try {
    ExeFile("WallPaper.HC");
    ExePrint("WallPaperInit;;");
    ExePrint("#include \"AMOUSE.HC\";;");
  } catch PutExcept;
  ACInit("KERNEL*.HH*");
  AutoComplete(ON);
}
U0 __FreeCPUs() {
  Free(cpu_structs),
  Free(mp_cores);
  Free(_boot_gs);
}

cpu_structs[Gs->num=__CoreNum]=Gs;//It should be 0
SetFs(adam_task);
//DEBUGGING INFORMATION WILL BE APPENDED TO THESE SYMBOLS
LoadImps;
//Load symbols into adam_task
if(FileFind("HCRT.DBG.Z"))
  DbgInfoRead("HCRT.DBG.Z");
//Our entering thread doesnt have a cur_thrd,so spawn one
Gs->seth_task=Spawn(&CoreAPSethTask,NULL,"Seth");

U0 __InitCPUs() {
  I64 idx;
  for (idx=Gs->num+1;idx<mp_cnt;++idx) {
    __SpawnCore(idx,&CoreAPSethTask);
    while (!Bt(&seth_tasks_ready,idx))
      PAUSE;
  }
}
__InitCPUs;
try {
  ExePrint("%s;\n",__CmdLineBootText);
} catch PutExcept;
if (__IsCmdLine) {
  CTask *u=User;
  DeathWait(&u);
  ExitTINE;
} else {
  sys_winmgr_task=Spawn(&WinMgrTask);
  Refresh; //Wait for window manager.
  StartUpTasks;
  SrvTaskCont;
}
#include "PUBLIC.HC";;
