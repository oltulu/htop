/*
htop - Action.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Action.h"

#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>

#include "CategoriesPanel.h"
#include "CommandScreen.h"
#include "CRT.h"
#include "EnvScreen.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "IncSet.h"
#include "InfoScreen.h"
#include "ListItem.h"
#include "Macros.h"
#include "MainPanel.h"
#include "OpenFilesScreen.h"
#include "Process.h"
#include "ProcessLocksScreen.h"
#include "ProvideCurses.h"
#include "ScreenManager.h"
#include "SignalsPanel.h"
#include "TraceScreen.h"
#include "Vector.h"
#include "XUtils.h"

#if (defined(HAVE_LIBHWLOC) || defined(HAVE_LINUX_AFFINITY))
#include "Affinity.h"
#include "AffinityPanel.h"
#endif


Object* Action_pickFromVector(State* st, Panel* list, int x, bool followProcess) {
   MainPanel* mainPanel = st->mainPanel;
   Header* header = st->header;

   int y = ((Panel*)mainPanel)->y;
   ScreenManager* scr = ScreenManager_new(header, st->settings, st, false);
   scr->allowFocusChange = false;
   ScreenManager_add(scr, list, x - 1);
   ScreenManager_add(scr, (Panel*)mainPanel, -1);
   Panel* panelFocus;
   int ch;
   bool unfollow = false;
   int pid = followProcess ? MainPanel_selectedPid(mainPanel) : -1;
   if (followProcess && header->pl->following == -1) {
      header->pl->following = pid;
      unfollow = true;
   }
   ScreenManager_run(scr, &panelFocus, &ch);
   if (unfollow) {
      header->pl->following = -1;
   }
   ScreenManager_delete(scr);
   Panel_move((Panel*)mainPanel, 0, y);
   Panel_resize((Panel*)mainPanel, COLS, LINES - y - 1);
   if (panelFocus == list && ch == 13) {
      if (followProcess) {
         const Process* selected = (const Process*)Panel_getSelected((Panel*)mainPanel);
         if (selected && selected->pid == pid)
            return Panel_getSelected(list);

         beep();
      } else {
         return Panel_getSelected(list);
      }
   }

   return NULL;
}

// ----------------------------------------

static void Action_runSetup(State* st) {
   ScreenManager* scr = ScreenManager_new(st->header, st->settings, st, true);
   CategoriesPanel* panelCategories = CategoriesPanel_new(scr, st->settings, st->header, st->pl);
   ScreenManager_add(scr, (Panel*) panelCategories, 16);
   CategoriesPanel_makeMetersPage(panelCategories);
   Panel* panelFocus;
   int ch;
   ScreenManager_run(scr, &panelFocus, &ch);
   ScreenManager_delete(scr);
   if (st->settings->changed) {
      Header_writeBackToSettings(st->header);
   }
}

static bool changePriority(MainPanel* panel, int delta) {
   bool anyTagged;
   bool ok = MainPanel_foreachProcess(panel, Process_changePriorityBy, (Arg) { .i = delta }, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static void addUserToVector(ht_key_t key, void* userCast, void* panelCast) {
   const char* user = userCast;
   Panel* panel = panelCast;
   Panel_add(panel, (Object*) ListItem_new(user, key));
}

bool Action_setUserOnly(const char* userName, uid_t* userId) {
   const struct passwd* user = getpwnam(userName);
   if (user) {
      *userId = user->pw_uid;
      return true;
   }
   *userId = (uid_t)-1;
   return false;
}

static void tagAllChildren(Panel* panel, Process* parent) {
   parent->tag = true;
   pid_t ppid = parent->pid;
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* p = (Process*) Panel_get(panel, i);
      if (!p->tag && Process_isChildOf(p, ppid)) {
         tagAllChildren(panel, p);
      }
   }
}

static bool expandCollapse(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p)
      return false;

   p->showChildren = !p->showChildren;
   return true;
}

static bool collapseIntoParent(Panel* panel) {
   const Process* p = (Process*) Panel_getSelected(panel);
   if (!p)
      return false;

   pid_t ppid = Process_getParentPid(p);
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* q = (Process*) Panel_get(panel, i);
      if (q->pid == ppid) {
         q->showChildren = false;
         Panel_setSelected(panel, i);
         return true;
      }
   }
   return false;
}

Htop_Reaction Action_setSortKey(Settings* settings, ProcessField sortKey) {
   Settings_setSortKey(settings, sortKey);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_UPDATE_PANELHDR | HTOP_KEEP_FOLLOWING;
}

// ----------------------------------------

static Htop_Reaction actionSetSortColumn(State* st) {
   Htop_Reaction reaction = HTOP_OK;
   Panel* sortPanel = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Sırala   ", "İptal "));
   Panel_setHeader(sortPanel, "Sırala");
   const ProcessField* fields = st->settings->fields;
   for (int i = 0; fields[i]; i++) {
      char* name = String_trim(Process_fields[fields[i]].name);
      Panel_add(sortPanel, (Object*) ListItem_new(name, fields[i]));
      if (fields[i] == Settings_getActiveSortKey(st->settings))
         Panel_setSelected(sortPanel, i);

      free(name);
   }
   const ListItem* field = (const ListItem*) Action_pickFromVector(st, sortPanel, 15, false);
   if (field) {
      reaction |= Action_setSortKey(st->settings, field->key);
   }
   Object_delete(sortPanel);

   if (st->pauseProcessUpdate)
      ProcessList_sort(st->pl);

   return reaction | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionSortByPID(State* st) {
   return Action_setSortKey(st->settings, PID);
}

static Htop_Reaction actionSortByMemory(State* st) {
   return Action_setSortKey(st->settings, PERCENT_MEM);
}

static Htop_Reaction actionSortByCPU(State* st) {
   return Action_setSortKey(st->settings, PERCENT_CPU);
}

static Htop_Reaction actionSortByTime(State* st) {
   return Action_setSortKey(st->settings, TIME);
}

static Htop_Reaction actionToggleKernelThreads(State* st) {
   st->settings->hideKernelThreads = !st->settings->hideKernelThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleUserlandThreads(State* st) {
   st->settings->hideUserlandThreads = !st->settings->hideUserlandThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleProgramPath(State* st) {
   st->settings->showProgramPath = !st->settings->showProgramPath;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleMergedCommand(State* st) {
   st->settings->showMergedCommand = !st->settings->showMergedCommand;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleTreeView(State* st) {
   st->settings->treeView = !st->settings->treeView;

   if (!st->settings->allBranchesCollapsed) ProcessList_expandTree(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionExpandOrCollapseAllBranches(State* st) {
   st->settings->allBranchesCollapsed = !st->settings->allBranchesCollapsed;
   if (st->settings->allBranchesCollapsed)
      ProcessList_collapseAllBranches(st->pl);
   else
      ProcessList_expandTree(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionIncFilter(State* st) {
   IncSet* inc = (st->mainPanel)->inc;
   IncSet_activate(inc, INC_FILTER, (Panel*)st->mainPanel);
   st->pl->incFilter = IncSet_filter(inc);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncSearch(State* st) {
   IncSet_reset(st->mainPanel->inc, INC_SEARCH);
   IncSet_activate(st->mainPanel->inc, INC_SEARCH, (Panel*)st->mainPanel);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionHigherPriority(State* st) {
   bool changed = changePriority(st->mainPanel, -1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionLowerPriority(State* st) {
   bool changed = changePriority(st->mainPanel, 1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionInvertSortOrder(State* st) {
   Settings_invertSortOrder(st->settings);
   if (st->pauseProcessUpdate)
      ProcessList_sort(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionExpandOrCollapse(State* st) {
   bool changed = expandCollapse((Panel*)st->mainPanel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionCollapseIntoParent(State* st) {
   if (!st->settings->treeView) {
      return HTOP_OK;
   }
   bool changed = collapseIntoParent((Panel*)st->mainPanel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionExpandCollapseOrSortColumn(State* st) {
   return st->settings->treeView ? actionExpandOrCollapse(st) : actionSetSortColumn(st);
}

static Htop_Reaction actionQuit(ATTR_UNUSED State* st) {
   return HTOP_QUIT;
}

static Htop_Reaction actionSetAffinity(State* st) {
   if (st->pl->cpuCount == 1)
      return HTOP_OK;

#if (defined(HAVE_LIBHWLOC) || defined(HAVE_LINUX_AFFINITY))
   const Process* p = (const Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   Affinity* affinity1 = Affinity_get(p, st->pl);
   if (!affinity1)
      return HTOP_OK;

   int width;
   Panel* affinityPanel = AffinityPanel_new(st->pl, affinity1, &width);
   width += 1; /* paneller arasına boşluk ekliyoruz */
   Affinity_delete(affinity1);

   const void* set = Action_pickFromVector(st, affinityPanel, width, true);
   if (set) {
      Affinity* affinity2 = AffinityPanel_getAffinity(affinityPanel, st->pl);
      bool ok = MainPanel_foreachProcess(st->mainPanel, Affinity_set, (Arg) { .v = affinity2 }, NULL);
      if (!ok)
         beep();
      Affinity_delete(affinity2);
   }
   Object_delete(affinityPanel);
#endif
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionKill(State* st) {
   Panel* signalsPanel = SignalsPanel_new();
   const ListItem* sgn = (ListItem*) Action_pickFromVector(st, signalsPanel, 15, true);
   if (sgn && sgn->key != 0) {
      Panel_setHeader((Panel*)st->mainPanel, "Gönderiliyor...");
      Panel_draw((Panel*)st->mainPanel, false, true, true, State_hideFunctionBar(st));
      refresh();
      MainPanel_foreachProcess(st->mainPanel, Process_sendSignal, (Arg) { .i = sgn->key }, NULL);
      napms(500);
   }
   Panel_delete((Object*)signalsPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFilterByUser(State* st) {
   Panel* usersPanel = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Göster   ", "İptal "));
   Panel_setHeader(usersPanel, "İşlemlerini göster:");
   UsersTable_foreach(st->ut, addUserToVector, usersPanel);
   Vector_insertionSort(usersPanel->items);
   ListItem* allUsers = ListItem_new("Tüm Kullanıcılar", -1);
   Panel_insert(usersPanel, 0, (Object*) allUsers);
   const ListItem* picked = (ListItem*) Action_pickFromVector(st, usersPanel, 20, false);
   if (picked) {
      if (picked == allUsers) {
         st->pl->userId = (uid_t)-1;
      } else {
         Action_setUserOnly(ListItem_getRef(picked), &(st->pl->userId));
      }
   }
   Panel_delete((Object*)usersPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

Htop_Reaction Action_follow(State* st) {
   st->pl->following = MainPanel_selectedPid(st->mainPanel);
   Panel_setSelectionColor((Panel*)st->mainPanel, PANEL_SELECTION_FOLLOW);
   return HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionSetup(State* st) {
   Action_runSetup(st);
   int headerHeight = Header_calculateHeight(st->header);
   Panel_move((Panel*)st->mainPanel, 0, headerHeight);
   Panel_resize((Panel*)st->mainPanel, COLS, LINES-headerHeight-1);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionLsof(State* st) {
   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   OpenFilesScreen* ofs = OpenFilesScreen_new(p);
   InfoScreen_run((InfoScreen*)ofs);
   OpenFilesScreen_delete((Object*)ofs);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowLocks(State* st) {
   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p) return HTOP_OK;
   ProcessLocksScreen* pls = ProcessLocksScreen_new(p);
   InfoScreen_run((InfoScreen*)pls);
   ProcessLocksScreen_delete((Object*)pls);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionStrace(State* st) {
   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   TraceScreen* ts = TraceScreen_new(p);
   bool ok = TraceScreen_forkTracer(ts);
   if (ok) {
      InfoScreen_run((InfoScreen*)ts);
   }
   TraceScreen_delete((Object*)ts);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTag(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   Process_toggleTag(p);
   Panel_onKey((Panel*)st->mainPanel, KEY_DOWN);
   return HTOP_OK;
}

static Htop_Reaction actionRedraw(ATTR_UNUSED State* st) {
   clear();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTogglePauseProcessUpdate(State* st) {
   st->pauseProcessUpdate = !st->pauseProcessUpdate;
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static const struct {
   const char* key;
   const char* info;
} helpLeft[] = {
   { .key = " Oklar: ", .info = "kaydırma işlemi listesi" },
   { .key = " Sayılar: ", .info = "artımlı PID araması" },
   { .key = "   F3 /: ", .info = "artımlı ad araması" },
   { .key = "   F4 \\: ",.info = "artımlı ad filtreleme" },
   { .key = "   F5 t: ", .info = "ağaç görünümü" },
   { .key = "      p: ", .info = "program yolunu değiştir" },
   { .key = "      m: ", .info = "birleştirilmiş komutu aç / kapat" },
   { .key = "      Z: ", .info = "işlem güncellemelerini duraklat / devam ettir" },
   { .key = "      u: ", .info = "tek bir kullanıcının işlemlerini göster" },
   { .key = "      H: ", .info = "kullanıcı işlem dizilerini gizle/göster" },
   { .key = "      K: ", .info = "çekirdek dizilerini gizle/göster" },
   { .key = "      F: ", .info = "imleç süreci takip eders" },
   { .key = "  + - *: ", .info = "ağacı genişlet/daralt/tümünü değiştir" },
   { .key = "N P M T: ", .info = "PID, CPU%, MEM% veya TIME göre sırala" },
   { .key = "      I: ", .info = "sıralama düzenini ters çevir" },
   { .key = " F6 > .: ", .info = "sıralama sütununu seçin" },
   { .key = NULL, .info = NULL }
};

static const struct {
   const char* key;
   const char* info;
} helpRight[] = {
   { .key = "  Boşluk: ", .info = "etiket işlemi" },
   { .key = "      c: ", .info = "etiket süreci ve alt öğeleri" },
   { .key = "      U: ", .info = "tüm süreçlerin etiketini kaldır" },
   { .key = "   F9 k: ", .info = "işlemi/etiketli işlemleri sonlandır" },
   { .key = "   F7 ]: ", .info = "daha yüksek öncelik (yalnızca kök)" },
   { .key = "   F8 [: ", .info = "düşük öncelikli (+ güzel)" },
#if (defined(HAVE_LIBHWLOC) || defined(HAVE_LINUX_AFFINITY))
   { .key = "      a: ", .info = "CPU benzeşimini ayarla" },
#endif
   { .key = "      e: ", .info = "süreç ortamını göster" },
   { .key = "      i: ", .info = "IO önceliğini ayarla" },
   { .key = "      l: ", .info = "lsof ile açık dosyaları listeleme" },
   { .key = "      x: ", .info = "işlemin dosya kilitlerini listeleyin" },
   { .key = "      s: ", .info = "sistem çağrılarını strace ile izleme" },
   { .key = "      w: ", .info = "birden çok satıra sarma işlemi komutu" },
   { .key = " F2 C S: ", .info = "kur" },
   { .key = "   F1 h: ", .info = "bu yardım ekranını göster" },
   { .key = "  F10 q: ", .info = "çık" },
   { .key = NULL, .info = NULL }
};

static inline void addattrstr( int attr, const char* str) {
   attrset(attr);
   addstr(str);
}

static Htop_Reaction actionHelp(State* st) {
   clear();
   attrset(CRT_colors[HELP_BOLD]);

   for (int i = 0; i < LINES - 1; i++)
      mvhline(i, 0, ' ', COLS);

   int line = 0;

   mvaddstr(line++, 0, "htop " VERSION " - " COPYRIGHT);
   mvaddstr(line++, 0, "GNU GPLv2 altında yayınlandı. Daha fazla bilgi için 'man' sayfasına bakın.");

   attrset(CRT_colors[DEFAULT_COLOR]);
   line++;
   mvaddstr(line++, 0, "CPU Kullanı Barı: ");

   addattrstr(CRT_colors[BAR_BORDER], "[");
   if (st->settings->detailedCPUTime) {
      addattrstr(CRT_colors[CPU_NICE_TEXT], "düşük"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_SYSTEM], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_IRQ], "irq"); addstr("/");
      addattrstr(CRT_colors[CPU_SOFTIRQ], "soft-irq"); addstr("/");
      addattrstr(CRT_colors[CPU_STEAL], "steal"); addstr("/");
      addattrstr(CRT_colors[CPU_GUEST], "misafir"); addstr("/");
      addattrstr(CRT_colors[CPU_IOWAIT], "io-bekle");
      addattrstr(CRT_colors[BAR_SHADOW], " kullanılan%");
   } else {
      addattrstr(CRT_colors[CPU_NICE_TEXT], "low-priority"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_SYSTEM], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_GUEST], "virtualiz");
      addattrstr(CRT_colors[BAR_SHADOW], "               used%");
   }
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Hafıza Barı:    ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[MEMORY_USED], "kullanılan"); addstr("/");
   addattrstr(CRT_colors[MEMORY_BUFFERS_TEXT], "tamponlar"); addstr("/");
   addattrstr(CRT_colors[MEMORY_SHARED], "paylaşılan"); addstr("/");
   addattrstr(CRT_colors[MEMORY_CACHE], "önbellek");
   addattrstr(CRT_colors[BAR_SHADOW], "                     kullanılan/toplam");
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Takas Barı:      ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[SWAP], "used");
#ifdef HTOP_LINUX
   addattrstr(CRT_colors[BAR_SHADOW], "/");
   addattrstr(CRT_colors[SWAP_CACHE], "önbellek");
   addattrstr(CRT_colors[BAR_SHADOW], "                                    kullanılan/toplam");
#else
   addattrstr(CRT_colors[BAR_SHADOW], "                                          kullanılan/toplam");
#endif
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Başlık sayaçlarının tipi ve düzeni kurulum ekranında yapılandırılabilir.");
   if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
      mvaddstr(line, 0, "Monokromda, sayaçlar sırayla farklı karakterler olarak görüntülenir: |#*@$%&.");
   }
   line++;

   mvaddstr(line++, 0, "İşlem durumu: R: çalışıyor; S: uyku; T: izlendi / durduruldu; Z: zombi; D: disk uyku");

   line++;

   int item;
   for (item = 0; helpLeft[item].key; item++) {
      attrset(CRT_colors[DEFAULT_COLOR]);
      mvaddstr(line + item, 10, helpLeft[item].info);
      attrset(CRT_colors[HELP_BOLD]);
      mvaddstr(line + item, 1,  helpLeft[item].key);
      if (String_eq(helpLeft[item].key, "      H: ")) {
         attrset(CRT_colors[PROCESS_THREAD]);
         mvaddstr(line + item, 33, "İş Parçacığı");
      } else if (String_eq(helpLeft[item].key, "      K: ")) {
         attrset(CRT_colors[PROCESS_THREAD]);
         mvaddstr(line + item, 27, "İş Parçacığı");
      }
   }
   int leftHelpItems = item;

   for (item = 0; helpRight[item].key; item++) {
      attrset(CRT_colors[HELP_BOLD]);
      mvaddstr(line + item, 41, helpRight[item].key);
      attrset(CRT_colors[DEFAULT_COLOR]);
      mvaddstr(line + item, 50, helpRight[item].info);
   }
   line += MAXIMUM(leftHelpItems, item);
   line++;

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr(line++, 0, "Geri dönmek için herhangi bir tuşa basın.");
   attrset(CRT_colors[DEFAULT_COLOR]);
   refresh();
   CRT_readKey();
   clear();

   return HTOP_RECALCULATE | HTOP_REDRAW_BAR | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionUntagAll(State* st) {
   for (int i = 0; i < Panel_size((Panel*)st->mainPanel); i++) {
      Process* p = (Process*) Panel_get((Panel*)st->mainPanel, i);
      p->tag = false;
   }
   return HTOP_REFRESH;
}

static Htop_Reaction actionTagAllChildren(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   tagAllChildren((Panel*)st->mainPanel, p);
   return HTOP_OK;
}

static Htop_Reaction actionShowEnvScreen(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   EnvScreen* es = EnvScreen_new(p);
   InfoScreen_run((InfoScreen*)es);
   EnvScreen_delete((Object*)es);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowCommandScreen(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   CommandScreen* cmdScr = CommandScreen_new(p);
   InfoScreen_run((InfoScreen*)cmdScr);
   CommandScreen_delete((Object*)cmdScr);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

void Action_setBindings(Htop_Action* keys) {
   keys[' '] = actionTag;
   keys['*'] = actionExpandOrCollapseAllBranches;
   keys['+'] = actionExpandOrCollapse;
   keys[','] = actionSetSortColumn;
   keys['-'] = actionExpandOrCollapse;
   keys['.'] = actionSetSortColumn;
   keys['/'] = actionIncSearch;
   keys['<'] = actionSetSortColumn;
   keys['='] = actionExpandOrCollapse;
   keys['>'] = actionSetSortColumn;
   keys['?'] = actionHelp;
   keys['C'] = actionSetup;
   keys['F'] = Action_follow;
   keys['H'] = actionToggleUserlandThreads;
   keys['I'] = actionInvertSortOrder;
   keys['K'] = actionToggleKernelThreads;
   keys['M'] = actionSortByMemory;
   keys['N'] = actionSortByPID;
   keys['P'] = actionSortByCPU;
   keys['S'] = actionSetup;
   keys['T'] = actionSortByTime;
   keys['U'] = actionUntagAll;
   keys['Z'] = actionTogglePauseProcessUpdate;
   keys['['] = actionLowerPriority;
   keys['\014'] = actionRedraw; // Ctrl+L
   keys['\177'] = actionCollapseIntoParent;
   keys['\\'] = actionIncFilter;
   keys[']'] = actionHigherPriority;
   keys['a'] = actionSetAffinity;
   keys['c'] = actionTagAllChildren;
   keys['e'] = actionShowEnvScreen;
   keys['h'] = actionHelp;
   keys['k'] = actionKill;
   keys['l'] = actionLsof;
   keys['m'] = actionToggleMergedCommand;
   keys['p'] = actionToggleProgramPath;
   keys['q'] = actionQuit;
   keys['s'] = actionStrace;
   keys['t'] = actionToggleTreeView;
   keys['u'] = actionFilterByUser;
   keys['w'] = actionShowCommandScreen;
   keys['x'] = actionShowLocks;
   keys[KEY_F(1)] = actionHelp;
   keys[KEY_F(2)] = actionSetup;
   keys[KEY_F(3)] = actionIncSearch;
   keys[KEY_F(4)] = actionIncFilter;
   keys[KEY_F(5)] = actionToggleTreeView;
   keys[KEY_F(6)] = actionSetSortColumn;
   keys[KEY_F(7)] = actionHigherPriority;
   keys[KEY_F(8)] = actionLowerPriority;
   keys[KEY_F(9)] = actionKill;
   keys[KEY_F(10)] = actionQuit;
   keys[KEY_F(18)] = actionExpandCollapseOrSortColumn;
   keys[KEY_RECLICK] = actionExpandOrCollapse;
}
