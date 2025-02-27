/*
htop - CategoriesPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "CategoriesPanel.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "AvailableColumnsPanel.h"
#include "AvailableMetersPanel.h"
#include "ColorsPanel.h"
#include "ColumnsPanel.h"
#include "DisplayOptionsPanel.h"
#include "FunctionBar.h"
#include "ListItem.h"
#include "MetersPanel.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "Vector.h"


static const char* const CategoriesFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Bitti  ", NULL};

static void CategoriesPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   CategoriesPanel* this = (CategoriesPanel*) object;
   Panel_done(super);
   free(this);
}

void CategoriesPanel_makeMetersPage(CategoriesPanel* this) {
   MetersPanel* leftMeters = MetersPanel_new(this->settings, "Sol sütun", this->header->columns[0], this->scr);
   MetersPanel* rightMeters = MetersPanel_new(this->settings, "Sağ sütun", this->header->columns[1], this->scr);
   leftMeters->rightNeighbor = rightMeters;
   rightMeters->leftNeighbor = leftMeters;
   Panel* availableMeters = (Panel*) AvailableMetersPanel_new(this->settings, this->header, (Panel*) leftMeters, (Panel*) rightMeters, this->scr, this->pl);
   ScreenManager_add(this->scr, (Panel*) leftMeters, 20);
   ScreenManager_add(this->scr, (Panel*) rightMeters, 20);
   ScreenManager_add(this->scr, availableMeters, -1);
}

static void CategoriesPanel_makeDisplayOptionsPage(CategoriesPanel* this) {
   Panel* displayOptions = (Panel*) DisplayOptionsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, displayOptions, -1);
}

static void CategoriesPanel_makeColorsPage(CategoriesPanel* this) {
   Panel* colors = (Panel*) ColorsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, colors, -1);
}

static void CategoriesPanel_makeColumnsPage(CategoriesPanel* this) {
   Panel* columns = (Panel*) ColumnsPanel_new(this->settings);
   Panel* availableColumns = (Panel*) AvailableColumnsPanel_new(columns);
   ScreenManager_add(this->scr, columns, 20);
   ScreenManager_add(this->scr, availableColumns, -1);
}

static HandlerResult CategoriesPanel_eventHandler(Panel* super, int ch) {
   CategoriesPanel* this = (CategoriesPanel*) super;

   HandlerResult result = IGNORED;

   int selected = Panel_getSelectedIndex(super);
   switch (ch) {
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_UP:
      case KEY_CTRL('P'):
      case KEY_DOWN:
      case KEY_CTRL('N'):
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END: {
         int previous = selected;
         Panel_onKey(super, ch);
         selected = Panel_getSelectedIndex(super);
         if (previous != selected)
            result = HANDLED;
         break;
      }
      default:
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }
   if (result == HANDLED) {
      int size = ScreenManager_size(this->scr);
      for (int i = 1; i < size; i++)
         ScreenManager_remove(this->scr, 1);

      switch (selected) {
         case 0:
            CategoriesPanel_makeMetersPage(this);
            break;
         case 1:
            CategoriesPanel_makeDisplayOptionsPage(this);
            break;
         case 2:
            CategoriesPanel_makeColorsPage(this);
            break;
         case 3:
            CategoriesPanel_makeColumnsPage(this);
            break;
      }
   }
   return result;
}

const PanelClass CategoriesPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = CategoriesPanel_delete
   },
   .eventHandler = CategoriesPanel_eventHandler
};

CategoriesPanel* CategoriesPanel_new(ScreenManager* scr, Settings* settings, Header* header, ProcessList* pl) {
   CategoriesPanel* this = AllocThis(CategoriesPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(CategoriesFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->scr = scr;
   this->settings = settings;
   this->header = header;
   this->pl = pl;
   Panel_setHeader(super, "Kur");
   Panel_add(super, (Object*) ListItem_new("Metre", 0));
   Panel_add(super, (Object*) ListItem_new("Görüntüleme seçenekleri", 0));
   Panel_add(super, (Object*) ListItem_new("Renkler", 0));
   Panel_add(super, (Object*) ListItem_new("Sütunlar", 0));
   return this;
}
