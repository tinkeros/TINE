#exe {Cd(__DIR__);;};;
#define FPTRS 1
#include "KERNELA.HH"
Bool dol_doc_inited=FALSE;
Bool IsRaw() {
  return __IsCmdLine;
}
asm {
  ALIGN 16,0; //MUST ALIGN SYS_FIXED_AREA TO 16 TO AVOID SEGUALTS WITH FXSAVE
};
CSysFixedArea sys_fixed_area,*SYS_FIXED_AREA;
SYS_FIXED_AREA=&sys_fixed_area;
U0 InitFX() {
  I64 noreg ptr;
  ptr=SYS_FIXED_AREA;
  MOV RAX,U64 &ptr[RBP];
  FXSAVE U64[RAX]
}
InitFX;
CHeapCtrl ahc,ahd;
CTask _boot_task;
SetFs(&_boot_task);
HeapCtrlInit(&ahc,Fs,0);
HeapCtrlInit(&ahd,Fs,1);
Fs->data_heap=&ahd;
Fs->code_heap=&ahc;
U8 *__bootdir="T:/";
__bootdir[0]=VFsGetDrv;
Fs->cur_dir=StrNew(__bootdir,&ahd);
U32 SYS_CTRL_ALT_FLAGS=0;
CCPU **mp_cores=CAlloc(mp_cnt*sizeof CCPU*,&ahd);
CCPU *_boot_gs;
CCPU *Gs() {
  if (!GetGs) // this part only will be executed on boot
    SetGs(_boot_gs=CAlloc(sizeof CCPU,&ahd));
  return GetGs;
}
mp_cores[0]=Gs;
QueInit(&(Gs->next_dying));
CTask *Fs() {
  return GetFs;
}
TaskInit(Fs,0);
#include "Kernel/KGLBLS.HC"
#include "DOLDOC/DocExt.HC"
#include "Kernel/KUTILS.HC"
#include "Kernel/KMATH.HC"
#include "Kernel/HASH.HC"
#include "Kernel/STR.HC"
#include "KERNELB.HH"
#include "GR/Gr.HH"
public extern I64 ExeCmdLine(CCmpCtrl *cc);
CTask *adam_task=Fs;
#include "Kernel/KDate.HC"
#include "Compiler/CHASH.HC"
#include "Kernel/STRB.HC"
#include "Kernel/FUNSEG.HC"
#include "Kernel/STRPRINT.HC"
#include "Kernel/KDATATYPES.HC"
#include "Kernel/COMPRESS.HC"
#include "Kernel/QSORT.HC"
#include "Kernel/KDBG.HC"
#include "Kernel/KEXCEPT.HC"
#include "Kernel/DISPLAY.HC"
#include "Mem/MakeMem.HC"
#include "BlkDev2/MakeBlkDev.HC"
#include "Kernel/Sched.HC"
#ifdef COMPONENT_COMPILER
#include "Compiler/CMP_PRJ.HC"
CInit;
#endif
#include "Adam/AMATH.HC"
#include "Kernel/FONT.HC"
#include "Kernel/FONT2.HC"
#include "Kernel/KMISC.HC"
#include "Kernel/KBMESSAGE.HC"
#include "Kernel/MOUSE.HC"
#include "Kernel/JOB.HC"
#include "Kernel/KTASK.HC"
#include "Kernel/KDefine.HC"
#include "Kernel/KLOAD.HC"
#include "Kernel/KEND.HC"
#include "Kernel/EDLITE.HC"
#ifdef COMPONENT_GRAPHICS
#include "Adam/TASK_SETTINGS.HC"
#include "Adam/ASND.HC"
#include "Adam/AMathODE.HC.Z";
#include "GR/MakeGr.HC"
#include "Adam/MENU.HC"
#include "Adam/WIN.HC"
#include "DOLDOC/MakeDoc.HC"
//See KeyDev.HC in TempleOS
U0 PutS(U8 *st)
{//Use $LK,"Print",A="MN:Print"$(). See $LK,"Keyboard Devices",A="HI:Keyboard Devices/System"$.
//Don't use this.  $LK,"See Print() shortcut.",A="FF:::/Doc/HolyC.DD,DemoHolyC"$
  if( __IsCmdLine) {
    TOSPrint("%s",st);
  }
  I64 ch;
  U8 *ptr;
  Bool cont=TRUE;
  if (!st) return;
  CKeyDevEntry *tmpk=keydev.put_key_head.next;
  if (!Bt(&(Fs->display_flags),DISPLAYf_SILENT)) {
    if (kbd.scan_code & SCF_SCROLL && sys_focus_task==Fs)
      while (kbd.scan_code & SCF_SCROLL)
        Yield;
    while (cont && tmpk!=&keydev.put_key_head) {
      if (tmpk->put_s) {
        if ((*tmpk->put_s)(st))
	  break;
      } else {
        ptr=st;
        while (ch=*ptr++)
	  if ((*tmpk->put_key)(ch,0))
	    cont=FALSE;
      }
      tmpk=tmpk->next;
    }
  }
}
fp_puts2=&PutS;
U0 PutKey(I64 ch=0,I64 sc=0)
{//See $LK,"Keyboard Devices",A="HI:Keyboard Devices/System"$.
  CKeyDevEntry *tmpk;
  if (ch||sc) {
    tmpk=keydev.put_key_head.next;
    if (!Bt(&(Fs->display_flags),DISPLAYf_SILENT)) {
      if (kbd.scan_code & SCF_SCROLL && sys_focus_task==Fs)
	while (kbd.scan_code & SCF_SCROLL)
	  Yield; //Wait on SCROLL LOCK Key
      while (tmpk!=&keydev.put_key_head) {
	if ((!(sc&SCF_KEY_DESC) || tmpk->flags & KDF_HAS_DESCS) &&
	      (*tmpk->put_key)(ch,sc))
	  break;
	tmpk=tmpk->next;
      }
    }
  }
}
#help_index "Ctrls"
#include "Ctrls/CTRLSA.HC"
#include "Ctrls/CTRLSBTTN.HC"
#include "Ctrls/CTRLSSLIDER.HC"
#include "Adam/WINMGR.HC"
#include "AutoComplete/MakeAC.HC"
#include "God/MakeGod.HC"
#include "Adam/Opt/Find.HC"
#include "Adam/ARegistry.HC"
#include "Adam/AHash.HC"
#include "Adam/ADskA.HC"
#include "Adam/ADskB.HC"
#include "Adam/ADBG.HC"
#include "Diff.HC"
#include "CPURep.HC"
#include "Kernel/KeyDev.HC"
#include "Adam/FILEMGR.HC"
#include "Adam/Training.HC"
#include "Adam/InFile.HC"
#include "Adam/Opt/Mount.HC"
#include "Adam/TaskRep.HC"
#include "Adam/Opt/DocUtils.HC"
#include "Adam/Opt/StrUtils.HC"
#include "Adam/Opt/Merge.HC"
#include "HomeKeyPlugIns.HC"

#endif
