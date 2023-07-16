U0 TempWallPaper(CTask *_task)
{
  I64 i,l=TEXT_ROWS-1;
  CHashTable *old_hash=Fs->hash_table;
  CCPU *c;
  U8 *st;
  _task->text_attr=BLUE<<4+WHITE;
  TextPrint(Fs,0,l--,BLUE<<4+WHITE,"ProgressBars:%016X %016X %016X %016X",
        progress1,progress2,progress3,progress4);
  if (scrncast.record)
    TextPrint(Fs,44-4,0,BLUE<<4+RED,"*");
  for (i=0;i<mp_cnt;i++) {
    c=cpu_structs[i];
    if (i&1)
      TextPrint(Fs,44+i*2,0,BLUE<<4+WHITE,"%2tf",100.0*(1.-c->idle_factor));
    else
      TextPrint(Fs,44+i*2,0,BLUE<<4+YELLOW,"%2tf",100.0*(1.-c->idle_factor));
  }
  st=ScanCode2KeyName(kbd.last_down_scan_code);
  TextPrint(Fs,80-18,0,BLUE<<4+YELLOW,"%18ts",st);
  Free(st);

  Fs->hash_table=old_hash;
  wall->last_calc_idle_cnt=winmgr.t->calc_idle_cnt;
}