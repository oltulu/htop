/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CPUMeter.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


static const int CPUMeter_attributes[] = {
   CPU_NICE,
   CPU_NORMAL,
   CPU_SYSTEM,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   CPU_IOWAIT
};

typedef struct CPUMeterData_ {
   unsigned int cpus;
   Meter** meters;
} CPUMeterData;

static void CPUMeter_init(Meter* this) {
   unsigned int cpu = this->param;
   if (cpu == 0) {
      Meter_setCaption(this, "Avg");
   } else if (this->pl->cpuCount > 1) {
      char caption[10];
      xSnprintf(caption, sizeof(caption), "%3u", Settings_cpuId(this->pl->settings, cpu - 1));
      Meter_setCaption(this, caption);
   }
}

static void CPUMeter_updateValues(Meter* this) {
   unsigned int cpu = this->param;
   if (cpu > this->pl->cpuCount) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "yok");
      for (uint8_t i = 0; i < this->curItems; i++)
         this->values[i] = 0;
      return;
   }
   memset(this->values, 0, sizeof(double) * CPU_METER_ITEMCOUNT);

   char cpuUsageBuffer[8] = { 0 };
   char cpuFrequencyBuffer[16] = { 0 };
   char cpuTemperatureBuffer[16] = { 0 };

   double percent = Platform_setCPUValues(this, cpu);

   if (this->pl->settings->showCPUUsage) {
      xSnprintf(cpuUsageBuffer, sizeof(cpuUsageBuffer), "%.1f%%", percent);
   }

   if (this->pl->settings->showCPUFrequency) {
      double cpuFrequency = this->values[CPU_METER_FREQUENCY];
      if (isnan(cpuFrequency)) {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "N/A");
      } else {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "%4uMHz", (unsigned)cpuFrequency);
      }
   }

   #ifdef BUILD_WITH_CPU_TEMP
   if (this->pl->settings->showCPUTemperature) {
      double cpuTemperature = this->values[CPU_METER_TEMPERATURE];
      if (isnan(cpuTemperature))
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "N/A");
      else if (this->pl->settings->degreeFahrenheit)
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%3d%sF", (int)(cpuTemperature * 9 / 5 + 32), CRT_degreeSign);
      else
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%d%sC", (int)cpuTemperature, CRT_degreeSign);
   }
   #endif

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s%s%s%s",
             cpuUsageBuffer,
             (cpuUsageBuffer[0] && (cpuFrequencyBuffer[0] || cpuTemperatureBuffer[0])) ? " " : "",
             cpuFrequencyBuffer,
             (cpuFrequencyBuffer[0] && cpuTemperatureBuffer[0]) ? " " : "",
             cpuTemperatureBuffer);
}

static void CPUMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;
   if (this->param > this->pl->cpuCount) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "yok");
      return;
   }
   xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NORMAL]);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], ":");
   RichString_appendAscii(out, CRT_colors[CPU_NORMAL], buffer);
   if (this->pl->settings->detailedCPUTime) {
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_KERNEL]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "sy:");
      RichString_appendAscii(out, CRT_colors[CPU_SYSTEM], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NICE]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "ni:");
      RichString_appendAscii(out, CRT_colors[CPU_NICE_TEXT], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "hi:");
      RichString_appendAscii(out, CRT_colors[CPU_IRQ], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_SOFTIRQ]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "si:");
      RichString_appendAscii(out, CRT_colors[CPU_SOFTIRQ], buffer);
      if (!isnan(this->values[CPU_METER_STEAL])) {
         xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_STEAL]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "st:");
         RichString_appendAscii(out, CRT_colors[CPU_STEAL], buffer);
      }
      if (!isnan(this->values[CPU_METER_GUEST])) {
         xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_GUEST]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "gu:");
         RichString_appendAscii(out, CRT_colors[CPU_GUEST], buffer);
      }
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IOWAIT]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "wa:");
      RichString_appendAscii(out, CRT_colors[CPU_IOWAIT], buffer);
   } else {
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_KERNEL]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "sys:");
      RichString_appendAscii(out, CRT_colors[CPU_SYSTEM], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NICE]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "low:");
      RichString_appendAscii(out, CRT_colors[CPU_NICE_TEXT], buffer);
      if (!isnan(this->values[CPU_METER_IRQ])) {
         xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "vir:");
         RichString_appendAscii(out, CRT_colors[CPU_GUEST], buffer);
      }
   }

   #ifdef BUILD_WITH_CPU_TEMP
   if (this->pl->settings->showCPUTemperature) {
      char cpuTemperatureBuffer[10];
      double cpuTemperature = this->values[CPU_METER_TEMPERATURE];
      if (isnan(cpuTemperature)) {
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "N/A");
      } else if (this->pl->settings->degreeFahrenheit) {
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%5.1f%sF", cpuTemperature * 9 / 5 + 32, CRT_degreeSign);
      } else {
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%5.1f%sC", cpuTemperature, CRT_degreeSign);
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "çöp:");
      RichString_appendWide(out, CRT_colors[METER_VALUE], cpuTemperatureBuffer);
   }
   #endif
}

static void AllCPUsMeter_getRange(const Meter* this, int* start, int* count) {
   const CPUMeterData* data = this->meterData;
   unsigned int cpus = data->cpus;
   switch(Meter_name(this)[0]) {
      default:
      case 'A': // All
         *start = 0;
         *count = cpus;
         break;
      case 'L': // First Half
         *start = 0;
         *count = (cpus+1) / 2;
         break;
      case 'R': // Second Half
         *start = (cpus+1) / 2;
         *count = cpus / 2;
         break;
   }
}

static void AllCPUsMeter_updateValues(Meter* this) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_updateValues(meters[i]);
}

static void CPUMeterCommonInit(Meter* this, int ncol) {
   unsigned int cpus = this->pl->cpuCount;
   CPUMeterData* data = this->meterData;
   if (!data) {
      data = this->meterData = xMalloc(sizeof(CPUMeterData));
      data->cpus = cpus;
      data->meters = xCalloc(cpus, sizeof(Meter*));
   }
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      if (!meters[i])
         meters[i] = Meter_new(this->pl, start + i + 1, (const MeterClass*) Class(CPUMeter));

      Meter_init(meters[i]);
   }

   if (this->mode == 0)
      this->mode = BAR_METERMODE;

   int h = Meter_modes[this->mode]->h;
   this->h = h * ((count + ncol - 1) / ncol);
}

static void CPUMeterCommonUpdateMode(Meter* this, int mode, int ncol) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   this->mode = mode;
   int h = Meter_modes[mode]->h;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   this->h = h * ((count + ncol - 1) / ncol);
}

static void AllCPUsMeter_done(Meter* this) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_delete((Object*)meters[i]);
   free(data->meters);
   free(data);
}

static void SingleColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 1);
}

static void SingleColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 1);
}

static void DualColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 2);
}

static void DualColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 2);
}

static void QuadColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 4);
}

static void QuadColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 4);
}

static void OctoColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 8);
}

static void OctoColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 8);
}

static void CPUMeterCommonDraw(Meter* this, int x, int y, int w, int ncol) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   int colwidth = (w - ncol) / ncol + 1;
   int diff = (w - (colwidth * ncol));
   int nrows = (count + ncol - 1) / ncol;
   for (int i = 0; i < count; i++) {
      int d = (i / nrows) > diff ? diff : (i / nrows); // dynamic spacer
      int xpos = x + ((i / nrows) * colwidth) + d;
      int ypos = y + ((i % nrows) * meters[0]->h);
      meters[i]->draw(meters[i], xpos, ypos, colwidth);
   }
}

static void DualColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterCommonDraw(this, x, y, w, 2);
}

static void QuadColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterCommonDraw(this, x, y, w, 4);
}

static void OctoColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterCommonDraw(this, x, y, w, 8);
}


static void SingleColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}


const MeterClass CPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = CPUMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = CPU_METER_ITEMCOUNT,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "CPU",
   .uiName = "CPU",
   .caption = "CPU",
   .init = CPUMeter_init
};

const MeterClass AllCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "Tüm CPU",
   .uiName = "CPUlar (1/1)",
   .description = "CPUlar  (1/1): Tüm CPUlar",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = SingleColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .description = "CPUs (1&2/2): 2 kısa sütunda tüm CPU'lar",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = DualColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .description = "CPUs (1/2): listenin ilk yarısı",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = SingleColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .description = "CPUs (2/2): listenin ikinci yarısı",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = SingleColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .description = "CPUs (1&2/4): 2 kısa sütunun ilk yarısı",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = DualColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .description = "CPUs (3&4/4): 2 kısa sütunda ikinci yarı",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = DualColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs4",
   .uiName = "CPUs (1&2&3&4/4)",
   .description = "CPUs (1&2&3&4/4): 4 kısa sütunda tüm CPU'lar",
   .caption = "CPU",
   .draw = QuadColCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = QuadColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs4",
   .uiName = "CPUs (1-4/8)",
   .description = "CPUs (1-4/8): 4 kısa sütunun ilk yarısı",
   .caption = "CPU",
   .draw = QuadColCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = QuadColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs4",
   .uiName = "CPUs (5-8/8)",
   .description = "CPUs (5-8/8): 4 kısa sütunda ikinci yarı",
   .caption = "CPU",
   .draw = QuadColCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = QuadColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs8",
   .uiName = "CPUs (1-8/8)",
   .description = "CPUs (1-8/8): 8 daha kısa sütunda tüm CPU'lar",
   .caption = "CPU",
   .draw = OctoColCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = OctoColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs8",
   .uiName = "CPUs (1-8/16)",
   .description = "CPUs (1-8/16): 8 kısa sütunun ilk yarısı",
   .caption = "CPU",
   .draw = OctoColCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = OctoColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs8",
   .uiName = "CPUs (9-16/16)",
   .description = "CPUs (9-16/16): 8 kısa sütunda ikinci yarı",
   .caption = "CPU",
   .draw = OctoColCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = OctoColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};
