/*
htop - CommandLine.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CommandLine.h"

#include <assert.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "Action.h"
#include "CRT.h"
#include "Hashtable.h"
#include "Header.h"
#include "IncSet.h"
#include "MainPanel.h"
#include "MetersPanel.h"
#include "Panel.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "ScreenManager.h"
#include "Settings.h"
#include "UsersTable.h"
#include "XUtils.h"


static void printVersionFlag(const char* name) {
   printf("%s " VERSION "\n", name);
}

static void printHelpFlag(const char* name) {
   printf("%s " VERSION "\n"
         COPYRIGHT "\n"
         "Released under the GNU GPLv2.\n\n"
         "-C --no-color                   Tek renkli bir renk düzeni kullanın\n"
         "-d --delay=DELAY                Güncellemeler arasındaki gecikmeyi saniyenin onda biri olarak ayarlayın\n"
         "-F --filter=FILTER              Yalnızca verilen filtreyle eşleşen komutları göster\n"
         "-h --help                       Bu yardım ekranını yazdırın\n"
         "-H --highlight-changes[=DELAY]  Yeni ve eski süreçleri vurgulayın\n"
         "-M --no-mouse                   Fareyi devre dışı bırakın\n"
         "-p --pid=PID[,PID,PID...]       Yalnızca verilen PID'yi göster\n"
         "-s --sort-key=COLUMN            Liste görünümünde SÜTUNA göre sırala (liste için --sort-key = yardım deneyin)\n"
         "-t --tree                       Ağaç görünümünü göster (-s ile birleştirilebilir)\n"
         "-u --user[=USERNAME]            Yalnızca belirli bir kullanıcı (veya $ USER) için işlemleri göster\n"
         "-U --no-unicode                 Unicode kullanmayın, düz ASCII\n"
         "-V --version                    Sürüm bilgilerini yazdır\n", name);
   Platform_longOptionsUsage(name);
   printf("\n"
         "Uzun seçenekler tek bir çizgi ile geçilebilir.\n\n"
         "Çevrimiçi yardım için %s içinde F1'e basın.\n"
         "Daha fazla bilgi için \"%s man\" bakın.\n", name, name);
}

// ----------------------------------------

typedef struct CommandLineSettings_ {
   Hashtable* pidMatchList;
   char* commFilter;
   uid_t userId;
   int sortKey;
   int delay;
   bool useColors;
   bool enableMouse;
   bool treeView;
   bool allowUnicode;
   bool highlightChanges;
   int highlightDelaySecs;
} CommandLineSettings;

static CommandLineSettings parseArguments(const char* program, int argc, char** argv) {

   CommandLineSettings flags = {
      .pidMatchList = NULL,
      .commFilter = NULL,
      .userId = (uid_t)-1, // -1 is guaranteed to be an invalid uid_t (see setreuid(2))
      .sortKey = 0,
      .delay = -1,
      .useColors = true,
      .enableMouse = true,
      .treeView = false,
      .allowUnicode = true,
      .highlightChanges = false,
      .highlightDelaySecs = -1,
   };

   const struct option long_opts[] =
   {
      {"help",       no_argument,         0, 'h'},
      {"version",    no_argument,         0, 'V'},
      {"delay",      required_argument,   0, 'd'},
      {"sort-key",   required_argument,   0, 's'},
      {"user",       optional_argument,   0, 'u'},
      {"no-color",   no_argument,         0, 'C'},
      {"no-colour",  no_argument,         0, 'C'},
      {"no-mouse",   no_argument,         0, 'M'},
      {"no-unicode", no_argument,         0, 'U'},
      {"tree",       no_argument,         0, 't'},
      {"pid",        required_argument,   0, 'p'},
      {"filter",     required_argument,   0, 'F'},
      {"highlight-changes", optional_argument, 0, 'H'},
      PLATFORM_LONG_OPTIONS
      {0,0,0,0}
   };

   int opt, opti=0;
   /* Parse arguments */
   while ((opt = getopt_long(argc, argv, "hVMCs:td:u::Up:F:H::", long_opts, &opti))) {
      if (opt == EOF) break;
      switch (opt) {
         case 'h':
            printHelpFlag(program);
            exit(0);
         case 'V':
            printVersionFlag(program);
            exit(0);
         case 's':
            assert(optarg); /* please clang analyzer, cause optarg can be NULL in the 'u' case */
            if (String_eq(optarg, "yardım")) {
               for (int j = 1; j < LAST_PROCESSFIELD; j++) {
                  const char* name = Process_fields[j].name;
                  const char* description = Process_fields[j].description;
                  if (name) printf("%19s %s\n", name, description);
               }
               exit(0);
            }
            flags.sortKey = 0;
            for (int j = 1; j < LAST_PROCESSFIELD; j++) {
               if (Process_fields[j].name == NULL)
                  continue;
               if (String_eq(optarg, Process_fields[j].name)) {
                  flags.sortKey = j;
                  break;
               }
            }
            if (flags.sortKey == 0) {
               fprintf(stderr, "Hata: geçersiz sütun \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'd':
            if (sscanf(optarg, "%16d", &(flags.delay)) == 1) {
               if (flags.delay < 1) flags.delay = 1;
               if (flags.delay > 100) flags.delay = 100;
            } else {
               fprintf(stderr, "Hata: geçersiz gecikme değeri \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'u':
         {
            const char *username = optarg;
            if (!username && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
               username = argv[optind++];
            }

            if (!username) {
               flags.userId = geteuid();
            } else if (!Action_setUserOnly(username, &(flags.userId))) {
               fprintf(stderr, "Hata: geçersiz kullanıcı\"%s\".\n", username);
               exit(1);
            }
            break;
         }
         case 'C':
            flags.useColors = false;
            break;
         case 'M':
            flags.enableMouse = false;
            break;
         case 'U':
            flags.allowUnicode = false;
            break;
         case 't':
            flags.treeView = true;
            break;
         case 'p': {
            assert(optarg); /* please clang analyzer, cause optarg can be NULL in the 'u' case */
            char* argCopy = xStrdup(optarg);
            char* saveptr;
            const char* pid = strtok_r(argCopy, ",", &saveptr);

            if (!flags.pidMatchList) {
               flags.pidMatchList = Hashtable_new(8, false);
            }

            while(pid) {
                unsigned int num_pid = atoi(pid);
                //  deepcode ignore CastIntegerToAddress: we just want a non-NUll pointer here
                Hashtable_put(flags.pidMatchList, num_pid, (void *) 1);
                pid = strtok_r(NULL, ",", &saveptr);
            }
            free(argCopy);

            break;
         }
         case 'F': {
            assert(optarg);
            free_and_xStrdup(&flags.commFilter, optarg);
            break;
         }
         case 'H': {
            const char *delay = optarg;
            if (!delay && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
                delay = argv[optind++];
            }
            if (delay) {
                if (sscanf(delay, "%16d", &(flags.highlightDelaySecs)) == 1) {
                   if (flags.highlightDelaySecs < 1)
                      flags.highlightDelaySecs = 1;
                } else {
                   fprintf(stderr, "Hata: geçersiz vurgulama gecikme değeri\"%s\".\n", delay);
                   exit(1);
                }
            }
            flags.highlightChanges = true;
            break;
         }

         default:
           if (Platform_getLongOption(opt, argc, argv) == false)
              exit(1);
           break;
      }
   }
   return flags;
}

static void millisleep(unsigned long millisec) {
   struct timespec req = {
      .tv_sec = 0,
      .tv_nsec = millisec * 1000000L
   };
   while(nanosleep(&req,&req)==-1) {
      continue;
   }
}

static void setCommFilter(State* state, char** commFilter) {
   ProcessList* pl = state->pl;
   IncSet* inc = state->mainPanel->inc;

   IncSet_setFilter(inc, *commFilter);
   pl->incFilter = IncSet_filter(inc);

   free(*commFilter);
   *commFilter = NULL;
}

int CommandLine_run(const char* name, int argc, char** argv) {

   /* initialize locale */
   const char* lc_ctype;
   if ((lc_ctype = getenv("LC_CTYPE")) || (lc_ctype = getenv("LC_ALL")))
      setlocale(LC_CTYPE, lc_ctype);
   else
      setlocale(LC_CTYPE, "");

   CommandLineSettings flags = parseArguments(name, argc, argv);

   Platform_init();

   Process_setupColumnWidths();

   UsersTable* ut = UsersTable_new();
   ProcessList* pl = ProcessList_new(ut, flags.pidMatchList, flags.userId);

   Settings* settings = Settings_new(pl->cpuCount);
   pl->settings = settings;

   Header* header = Header_new(pl, settings, 2);

   Header_populateFromSettings(header);

   if (flags.delay != -1)
      settings->delay = flags.delay;
   if (!flags.useColors)
      settings->colorScheme = COLORSCHEME_MONOCHROME;
   if (!flags.enableMouse)
      settings->enableMouse = false;
   if (flags.treeView)
      settings->treeView = true;
   if (flags.highlightChanges)
      settings->highlightChanges = true;
   if (flags.highlightDelaySecs != -1)
      settings->highlightDelaySecs = flags.highlightDelaySecs;
   if (flags.sortKey > 0) {
      // -t -s <key> means "tree sorted by key"
      // -s <key> means "list sorted by key" (previous existing behavior)
      if (!flags.treeView) {
         settings->treeView = false;
      }
      Settings_setSortKey(settings, flags.sortKey);
   }

   CRT_init(settings, flags.allowUnicode);

   MainPanel* panel = MainPanel_new();
   ProcessList_setPanel(pl, (Panel*) panel);

   MainPanel_updateTreeFunctions(panel, settings->treeView);

   State state = {
      .settings = settings,
      .ut = ut,
      .pl = pl,
      .mainPanel = panel,
      .header = header,
      .pauseProcessUpdate = false,
      .hideProcessSelection = false,
   };

   MainPanel_setState(panel, &state);
   if (flags.commFilter)
      setCommFilter(&state, &(flags.commFilter));

   ScreenManager* scr = ScreenManager_new(header, settings, &state, true);
   ScreenManager_add(scr, (Panel*) panel, -1);

   ProcessList_scan(pl, false);
   millisleep(75);
   ProcessList_scan(pl, false);

   if (settings->allBranchesCollapsed)
      ProcessList_collapseAllBranches(pl);

   ScreenManager_run(scr, NULL, NULL);

   attron(CRT_colors[RESET_COLOR]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[RESET_COLOR]);
   refresh();

   Platform_done();

   CRT_done();

   if (settings->changed) {
      int r = Settings_write(settings);
      if (r < 0)
         fprintf(stderr, "Yapılandırma değere kaydedilemez %s: %s\n", settings->filename, strerror(-r));
   }

   Header_delete(header);
   ProcessList_delete(pl);

   ScreenManager_delete(scr);
   MetersPanel_cleanup();

   UsersTable_delete(ut);
   Settings_delete(settings);

   if (flags.pidMatchList)
      Hashtable_delete(flags.pidMatchList);

   return 0;
}
