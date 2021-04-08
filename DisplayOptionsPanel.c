/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DisplayOptionsPanel.h"

#include <stdbool.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Header.h"
#include "Object.h"
#include "OptionItem.h"
#include "ProvideCurses.h"


static const char* const DisplayOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Tamam  ", NULL};

static void DisplayOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult DisplayOptionsPanel_eventHandler(Panel* super, int ch) {
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) super;

   HandlerResult result = IGNORED;
   OptionItem* selected = (OptionItem*) Panel_getSelected(super);

   switch (ch) {
   case '\n':
   case '\r':
   case KEY_ENTER:
   case KEY_MOUSE:
   case KEY_RECLICK:
   case ' ':
      switch (OptionItem_kind(selected)) {
      case OPTION_ITEM_CHECK:
         CheckItem_toggle((CheckItem*)selected);
         result = HANDLED;
         break;
      case OPTION_ITEM_NUMBER:
         NumberItem_toggle((NumberItem*)selected);
         result = HANDLED;
         break;
      }
      break;
   case '-':
      if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
         NumberItem_decrease((NumberItem*)selected);
         result = HANDLED;
      }
      break;
   case '+':
      if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
         NumberItem_increase((NumberItem*)selected);
         result = HANDLED;
      }
      break;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      Header* header = this->scr->header;
      Header_calculateHeight(header);
      Header_reinit(header);
      Header_updateData(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

const PanelClass DisplayOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = DisplayOptionsPanel_delete
   },
   .eventHandler = DisplayOptionsPanel_eventHandler
};

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   DisplayOptionsPanel* this = AllocThis(DisplayOptionsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(DisplayOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(OptionItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Görünüm Ayarları");
   Panel_add(super, (Object*) CheckItem_newByRef("Ağaç Görünümü", &(settings->treeView)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Ağaç görünümü her zaman PID'ye göre sıralanır (htop 2 davranışı)", &(settings->treeViewAlwaysByPID)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Ağaç görünümü varsayılan olarak daraltılmıştır", &(settings->allBranchesCollapsed)));
   Panel_add(super, (Object*) CheckItem_newByRef("Diğer kullanıcıların işlemlerini gölgeleyin", &(settings->shadowOtherUsers)));
   Panel_add(super, (Object*) CheckItem_newByRef("Çekirdek dizilerini gizle", &(settings->hideKernelThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Kullanıcı alanı işlem konularını gizle", &(settings->hideUserlandThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("İşlemleri farklı bir renkte görüntüle", &(settings->highlightThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Özel iş parçacığı adlarını göster", &(settings->showThreadNames)));
   Panel_add(super, (Object*) CheckItem_newByRef("Program yolunu göster", &(settings->showProgramPath)));
   Panel_add(super, (Object*) CheckItem_newByRef("\"basename\" programını vurgulayın", &(settings->highlightBaseName)));
   Panel_add(super, (Object*) CheckItem_newByRef("Komutta exe, comm ve cmdline'ı birleştirin", &(settings->showMergedCommand)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Cmdline'da comm bulmaya çalışın (Komut birleştirildiğinde)", &(settings->findCommInCmdline)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Cmdline'dan exe'yi çıkarmaya çalışın (Komut birleştirildiğinde)", &(settings->stripExeFromCmdline)));
   Panel_add(super, (Object*) CheckItem_newByRef("Bellek sayacındaki büyük sayıları vurgulayıns", &(settings->highlightMegabytes)));
   Panel_add(super, (Object*) CheckItem_newByRef("Başlığın etrafında bir kenar boşluğu bırakın", &(settings->headerMargin)));
   Panel_add(super, (Object*) CheckItem_newByRef("Ayrıntılı CPU süresi (Sistem / IO-Wait / Hard-IRQ / Soft-IRQ / Steal / Guest", &(settings->detailedCPUTime)));
   Panel_add(super, (Object*) CheckItem_newByRef("CPU'ları 0 yerine 1'den say", &(settings->countCPUsFromOne)));
   Panel_add(super, (Object*) CheckItem_newByRef("Her yenilemede işlem adlarını güncelleyin", &(settings->updateProcessNames)));
   Panel_add(super, (Object*) CheckItem_newByRef("CPU ölçer yüzdesinde misafir süresi ekleyin", &(settings->accountGuestInCPUMeter)));
   Panel_add(super, (Object*) CheckItem_newByRef("Ayrıca CPU yüzdesini sayısal olarak göster", &(settings->showCPUUsage)));
   Panel_add(super, (Object*) CheckItem_newByRef("Ayrıca CPU frekansını göster", &(settings->showCPUFrequency)));
   #ifdef BUILD_WITH_CPU_TEMP
   Panel_add(super, (Object*) CheckItem_newByRef(
   #if defined(HTOP_LINUX)
                                                 "Ayrıca CPU sıcaklığını göster (libsensor gerektirir)",
   #elif defined(HTOP_FREEBSD)
                                                 "Ayrıca CPU sıcaklığını göster",
   #else
   #error Unknown temperature implementation!
   #endif
                                                 &(settings->showCPUTemperature)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Sıcaklığı Santigrat yerine Fahrenheit cinsinden göster", &(settings->degreeFahrenheit)));
   #endif
   Panel_add(super, (Object*) CheckItem_newByRef("Fare aktif", &(settings->enableMouse)));
   Panel_add(super, (Object*) NumberItem_newByRef("Güncelleme aralığı (saniye cinsinden)", &(settings->delay), -1, 1, 255));
   Panel_add(super, (Object*) CheckItem_newByRef("Yeni ve eski süreçleri vurgulayın", &(settings->highlightChanges)));
   Panel_add(super, (Object*) NumberItem_newByRef("- Vurgu süresi (saniye cinsinden)", &(settings->highlightDelaySecs), 0, 1, 24*60*60));
   Panel_add(super, (Object*) NumberItem_newByRef("Ana işlev çubuğunu gizle (0 - kapalı, 1 - sonraki girişe kadar ESC'de, 2 - kalıcı olarak)", &(settings->hideFunctionBar), 0, 0, 2));
   #ifdef HAVE_LIBHWLOC
   Panel_add(super, (Object*) CheckItem_newByRef("Varsayılan olarak yakınlığı seçerken topolojiyi göster", &(settings->topologyAffinity)));
   #endif
   return this;
}
