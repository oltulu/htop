/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CRT.h"

#include <errno.h>
#include <langinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ProvideCurses.h"
#include "XUtils.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#if !defined(NDEBUG) && defined(HAVE_MEMFD_CREATE)
#include <sys/mman.h>
#endif


#define ColorIndex(i,j) ((7-(i))*8+(j))

#define ColorPair(i,j) COLOR_PAIR(ColorIndex(i,j))

#define Black   COLOR_BLACK
#define Red     COLOR_RED
#define Green   COLOR_GREEN
#define Yellow  COLOR_YELLOW
#define Blue    COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan    COLOR_CYAN
#define White   COLOR_WHITE

#define ColorPairGrayBlack  ColorPair(Magenta,Magenta)
#define ColorIndexGrayBlack ColorIndex(Magenta,Magenta)

#define ColorPairWhiteDefault  ColorPair(Red, Red)
#define ColorIndexWhiteDefault ColorIndex(Red, Red)

static const char* const CRT_treeStrAscii[LAST_TREE_STR] = {
   [TREE_STR_VERT] = "|",
   [TREE_STR_RTEE] = "`",
   [TREE_STR_BEND] = "`",
   [TREE_STR_TEND] = ",",
   [TREE_STR_OPEN] = "+",
   [TREE_STR_SHUT] = "-",
   [TREE_STR_ASC]  = "+",
   [TREE_STR_DESC] = "-",
};

#ifdef HAVE_LIBNCURSESW

static const char* const CRT_treeStrUtf8[LAST_TREE_STR] = {
   [TREE_STR_VERT] = "\xe2\x94\x82", // │
   [TREE_STR_RTEE] = "\xe2\x94\x9c", // ├
   [TREE_STR_BEND] = "\xe2\x94\x94", // └
   [TREE_STR_TEND] = "\xe2\x94\x8c", // ┌
   [TREE_STR_OPEN] = "+",            // +, TODO use 🮯 'BOX DRAWINGS LIGHT HORIZONTAL
                                     // WITH VERTICAL STROKE' (U+1FBAF, "\xf0\x9f\xae\xaf") when
                                     // Unicode 13 is common
   [TREE_STR_SHUT] = "\xe2\x94\x80", // ─
   [TREE_STR_ASC]  = "\xe2\x96\xb3", // △
   [TREE_STR_DESC] = "\xe2\x96\xbd", // ▽
};

bool CRT_utf8 = false;

#endif

const char* const* CRT_treeStr = CRT_treeStrAscii;

static const int* CRT_delay;

const char* CRT_degreeSign;

static const char* initDegreeSign(void) {
#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      return "\xc2\xb0";

   static char buffer[4];
   // this might fail if the current locale does not support wide characters
   int r = snprintf(buffer, sizeof(buffer), "%lc", 176);
   if (r > 0)
      return buffer;
#endif

   return "";
}

const int* CRT_colors;

static int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT] = {
   [COLORSCHEME_DEFAULT] = {
      [RESET_COLOR] = ColorPair(White, Black),
      [DEFAULT_COLOR] = ColorPair(White, Black),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = ColorPair(White, Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Black, White),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = A_BOLD | ColorPair(Red, Black),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = A_BOLD | ColorPair(Cyan, Black),
      [BATTERY] = A_BOLD | ColorPair(Cyan, Black),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red, Black),
      [METER_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [METER_TEXT] = ColorPair(Cyan, Black),
      [METER_VALUE] = A_BOLD | ColorPair(Cyan, Black),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Black),
      [METER_VALUE_IOREAD] = ColorPair(Green, Black),
      [METER_VALUE_IOWRITE] = ColorPair(Blue, Black),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(White, Black),
      [METER_VALUE_OK] = ColorPair(Green, Black),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Black),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green, Black),
      [PROCESS] = A_NORMAL,
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow, Black),
      [PROCESS_MEGABYTES] = ColorPair(Cyan, Black),
      [PROCESS_GIGABYTES] = ColorPair(Green, Black),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan, Black),
      [PROCESS_TREE] = ColorPair(Cyan, Black),
      [PROCESS_R_STATE] = ColorPair(Green, Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Black),
      [PROCESS_NEW] = ColorPair(Black, Green),
      [PROCESS_TOMB] = ColorPair(Black, Red),
      [PROCESS_THREAD] = ColorPair(Green, Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green, Black),
      [PROCESS_COMM] = ColorPair(Magenta, Black),
      [PROCESS_THREAD_COMM] = ColorPair(Blue, Black),
      [BAR_BORDER] = A_BOLD,
      [BAR_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [SWAP] = ColorPair(Red, Black),
      [SWAP_CACHE] = ColorPair(Yellow, Black),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan, Black),
      [GRAPH_2] = ColorPair(Cyan, Black),
      [MEMORY_USED] = ColorPair(Green, Black),
      [MEMORY_BUFFERS] = ColorPair(Blue, Black),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [MEMORY_CACHE] = ColorPair(Yellow, Black),
      [MEMORY_SHARED] = ColorPair(Magenta, Black),
      [HUGEPAGE_1] = ColorPair(Green, Black),
      [HUGEPAGE_2] = ColorPair(Yellow, Black),
      [HUGEPAGE_3] = ColorPair(Red, Black),
      [HUGEPAGE_4] = ColorPair(Blue, Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Cyan, Black),
      [LOAD_AVERAGE_FIVE] = A_BOLD | ColorPair(Cyan, Black),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White, Black),
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan, Black),
      [CLOCK] = A_BOLD,
      [DATE] = A_BOLD,
      [DATETIME] = A_BOLD,
      [CHECK_BOX] = ColorPair(Cyan, Black),
      [CHECK_MARK] = A_BOLD,
      [CHECK_TEXT] = A_NORMAL,
      [HOSTNAME] = A_BOLD,
      [CPU_NICE] = ColorPair(Blue, Black),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [CPU_NORMAL] = ColorPair(Green, Black),
      [CPU_SYSTEM] = ColorPair(Red, Black),
      [CPU_IOWAIT] = A_BOLD | ColorPairGrayBlack,
      [CPU_IRQ] = ColorPair(Yellow, Black),
      [CPU_SOFTIRQ] = ColorPair(Magenta, Black),
      [CPU_STEAL] = ColorPair(Cyan, Black),
      [CPU_GUEST] = ColorPair(Cyan, Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Cyan, Black),
      [PRESSURE_STALL_SIXTY] = A_BOLD | ColorPair(Cyan, Black),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(White, Black),
      [ZFS_MFU] = ColorPair(Blue, Black),
      [ZFS_MRU] = ColorPair(Yellow, Black),
      [ZFS_ANON] = ColorPair(Magenta, Black),
      [ZFS_HEADER] = ColorPair(Cyan, Black),
      [ZFS_OTHER] = ColorPair(Magenta, Black),
      [ZFS_COMPRESSED] = ColorPair(Blue, Black),
      [ZFS_RATIO] = ColorPair(Magenta, Black),
      [ZRAM] = ColorPair(Yellow, Black),
   },
   [COLORSCHEME_MONOCHROME] = {
      [RESET_COLOR] = A_NORMAL,
      [DEFAULT_COLOR] = A_NORMAL,
      [FUNCTION_BAR] = A_REVERSE,
      [FUNCTION_KEY] = A_NORMAL,
      [PANEL_HEADER_FOCUS] = A_REVERSE,
      [PANEL_HEADER_UNFOCUS] = A_REVERSE,
      [PANEL_SELECTION_FOCUS] = A_REVERSE,
      [PANEL_SELECTION_FOLLOW] = A_REVERSE,
      [PANEL_SELECTION_UNFOCUS] = A_BOLD,
      [FAILED_SEARCH] = A_REVERSE | A_BOLD,
      [FAILED_READ] = A_BOLD,
      [PAUSED] = A_BOLD | A_REVERSE,
      [UPTIME] = A_BOLD,
      [BATTERY] = A_BOLD,
      [LARGE_NUMBER] = A_BOLD,
      [METER_SHADOW] = A_DIM,
      [METER_TEXT] = A_NORMAL,
      [METER_VALUE] = A_BOLD,
      [METER_VALUE_ERROR] = A_BOLD,
      [METER_VALUE_IOREAD] = A_NORMAL,
      [METER_VALUE_IOWRITE] = A_NORMAL,
      [METER_VALUE_NOTICE] = A_BOLD,
      [METER_VALUE_OK] = A_NORMAL,
      [METER_VALUE_WARN] = A_BOLD,
      [LED_COLOR] = A_NORMAL,
      [TASKS_RUNNING] = A_BOLD,
      [PROCESS] = A_NORMAL,
      [PROCESS_SHADOW] = A_DIM,
      [PROCESS_TAG] = A_BOLD,
      [PROCESS_MEGABYTES] = A_BOLD,
      [PROCESS_GIGABYTES] = A_BOLD,
      [PROCESS_BASENAME] = A_BOLD,
      [PROCESS_TREE] = A_BOLD,
      [PROCESS_R_STATE] = A_BOLD,
      [PROCESS_D_STATE] = A_BOLD,
      [PROCESS_HIGH_PRIORITY] = A_BOLD,
      [PROCESS_LOW_PRIORITY] = A_DIM,
      [PROCESS_NEW] = A_BOLD,
      [PROCESS_TOMB] = A_DIM,
      [PROCESS_THREAD] = A_BOLD,
      [PROCESS_THREAD_BASENAME] = A_REVERSE,
      [PROCESS_COMM] = A_BOLD,
      [PROCESS_THREAD_COMM] = A_REVERSE,
      [BAR_BORDER] = A_BOLD,
      [BAR_SHADOW] = A_DIM,
      [SWAP] = A_BOLD,
      [SWAP_CACHE] = A_NORMAL,
      [GRAPH_1] = A_BOLD,
      [GRAPH_2] = A_NORMAL,
      [MEMORY_USED] = A_BOLD,
      [MEMORY_BUFFERS] = A_NORMAL,
      [MEMORY_BUFFERS_TEXT] = A_NORMAL,
      [MEMORY_CACHE] = A_NORMAL,
      [MEMORY_SHARED] = A_NORMAL,
      [HUGEPAGE_1] = A_BOLD,
      [HUGEPAGE_2] = A_NORMAL,
      [HUGEPAGE_3] = A_REVERSE | A_BOLD,
      [HUGEPAGE_4] = A_REVERSE,
      [LOAD_AVERAGE_FIFTEEN] = A_DIM,
      [LOAD_AVERAGE_FIVE] = A_NORMAL,
      [LOAD_AVERAGE_ONE] = A_BOLD,
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD,
      [CLOCK] = A_BOLD,
      [DATE] = A_BOLD,
      [DATETIME] = A_BOLD,
      [CHECK_BOX] = A_BOLD,
      [CHECK_MARK] = A_NORMAL,
      [CHECK_TEXT] = A_NORMAL,
      [HOSTNAME] = A_BOLD,
      [CPU_NICE] = A_NORMAL,
      [CPU_NICE_TEXT] = A_NORMAL,
      [CPU_NORMAL] = A_BOLD,
      [CPU_SYSTEM] = A_BOLD,
      [CPU_IOWAIT] = A_NORMAL,
      [CPU_IRQ] = A_BOLD,
      [CPU_SOFTIRQ] = A_BOLD,
      [CPU_STEAL] = A_DIM,
      [CPU_GUEST] = A_DIM,
      [PRESSURE_STALL_THREEHUNDRED] = A_DIM,
      [PRESSURE_STALL_SIXTY] = A_NORMAL,
      [PRESSURE_STALL_TEN] = A_BOLD,
      [ZFS_MFU] = A_NORMAL,
      [ZFS_MRU] = A_NORMAL,
      [ZFS_ANON] = A_DIM,
      [ZFS_HEADER] = A_BOLD,
      [ZFS_OTHER] = A_DIM,
      [ZFS_COMPRESSED] = A_BOLD,
      [ZFS_RATIO] = A_BOLD,
      [ZRAM] = A_NORMAL,
   },
   [COLORSCHEME_BLACKONWHITE] = {
      [RESET_COLOR] = ColorPair(Black, White),
      [DEFAULT_COLOR] = ColorPair(Black, White),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = ColorPair(Black, White),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Blue, White),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = ColorPair(Red, White),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = ColorPair(Yellow, White),
      [BATTERY] = ColorPair(Yellow, White),
      [LARGE_NUMBER] = ColorPair(Red, White),
      [METER_SHADOW] = ColorPair(Blue, White),
      [METER_TEXT] = ColorPair(Blue, White),
      [METER_VALUE] = ColorPair(Black, White),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, White),
      [METER_VALUE_IOREAD] = ColorPair(Green, White),
      [METER_VALUE_IOWRITE] = ColorPair(Yellow, White),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(Yellow, White),
      [METER_VALUE_OK] = ColorPair(Green, White),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, White),
      [LED_COLOR] = ColorPair(Green, White),
      [TASKS_RUNNING] = ColorPair(Green, White),
      [PROCESS] = ColorPair(Black, White),
      [PROCESS_SHADOW] = A_BOLD | ColorPair(Black, White),
      [PROCESS_TAG] = ColorPair(White, Blue),
      [PROCESS_MEGABYTES] = ColorPair(Blue, White),
      [PROCESS_GIGABYTES] = ColorPair(Green, White),
      [PROCESS_BASENAME] = ColorPair(Blue, White),
      [PROCESS_TREE] = ColorPair(Green, White),
      [PROCESS_R_STATE] = ColorPair(Green, White),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, White),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, White),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, White),
      [PROCESS_NEW] = ColorPair(White, Green),
      [PROCESS_TOMB] = ColorPair(White, Red),
      [PROCESS_THREAD] = ColorPair(Blue, White),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue, White),
      [PROCESS_COMM] = ColorPair(Magenta, White),
      [PROCESS_THREAD_COMM] = ColorPair(Green, White),
      [BAR_BORDER] = ColorPair(Blue, White),
      [BAR_SHADOW] = ColorPair(Black, White),
      [SWAP] = ColorPair(Red, White),
      [SWAP_CACHE] = ColorPair(Yellow, White),
      [GRAPH_1] = A_BOLD | ColorPair(Blue, White),
      [GRAPH_2] = ColorPair(Blue, White),
      [MEMORY_USED] = ColorPair(Green, White),
      [MEMORY_BUFFERS] = ColorPair(Cyan, White),
      [MEMORY_BUFFERS_TEXT] = ColorPair(Cyan, White),
      [MEMORY_CACHE] = ColorPair(Yellow, White),
      [MEMORY_SHARED] = ColorPair(Magenta, White),
      [HUGEPAGE_1] = ColorPair(Green, White),
      [HUGEPAGE_2] = ColorPair(Yellow, White),
      [HUGEPAGE_3] = ColorPair(Red, White),
      [HUGEPAGE_4] = ColorPair(Blue, White),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Black, White),
      [LOAD_AVERAGE_FIVE] = ColorPair(Black, White),
      [LOAD_AVERAGE_ONE] = ColorPair(Black, White),
      [LOAD] = ColorPair(Black, White),
      [HELP_BOLD] = ColorPair(Blue, White),
      [CLOCK] = ColorPair(Black, White),
      [DATE] = ColorPair(Black, White),
      [DATETIME] = ColorPair(Black, White),
      [CHECK_BOX] = ColorPair(Blue, White),
      [CHECK_MARK] = ColorPair(Black, White),
      [CHECK_TEXT] = ColorPair(Black, White),
      [HOSTNAME] = ColorPair(Black, White),
      [CPU_NICE] = ColorPair(Cyan, White),
      [CPU_NICE_TEXT] = ColorPair(Cyan, White),
      [CPU_NORMAL] = ColorPair(Green, White),
      [CPU_SYSTEM] = ColorPair(Red, White),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black, White),
      [CPU_IRQ] = ColorPair(Blue, White),
      [CPU_SOFTIRQ] = ColorPair(Blue, White),
      [CPU_STEAL] = ColorPair(Cyan, White),
      [CPU_GUEST] = ColorPair(Cyan, White),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Black, White),
      [PRESSURE_STALL_SIXTY] = ColorPair(Black, White),
      [PRESSURE_STALL_TEN] = ColorPair(Black, White),
      [ZFS_MFU] = ColorPair(Cyan, White),
      [ZFS_MRU] = ColorPair(Yellow, White),
      [ZFS_ANON] = ColorPair(Magenta, White),
      [ZFS_HEADER] = ColorPair(Yellow, White),
      [ZFS_OTHER] = ColorPair(Magenta, White),
      [ZFS_COMPRESSED] = ColorPair(Cyan, White),
      [ZFS_RATIO] = ColorPair(Magenta, White),
      [ZRAM] = ColorPair(Yellow, White)
   },
   [COLORSCHEME_LIGHTTERMINAL] = {
      [RESET_COLOR] = ColorPair(Black, Black),
      [DEFAULT_COLOR] = ColorPair(Black, Black),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = ColorPair(Black, Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Blue, Black),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = ColorPair(Red, Black),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = ColorPair(Yellow, Black),
      [BATTERY] = ColorPair(Yellow, Black),
      [LARGE_NUMBER] = ColorPair(Red, Black),
      [METER_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [METER_TEXT] = ColorPair(Blue, Black),
      [METER_VALUE] = ColorPair(Black, Black),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Black),
      [METER_VALUE_IOREAD] = ColorPair(Green, Black),
      [METER_VALUE_IOWRITE] = ColorPair(Yellow, Black),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPairWhiteDefault,
      [METER_VALUE_OK] = ColorPair(Green, Black),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Black),
      [TASKS_RUNNING] = ColorPair(Green, Black),
      [PROCESS] = ColorPair(Black, Black),
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = ColorPair(White, Blue),
      [PROCESS_MEGABYTES] = ColorPair(Blue, Black),
      [PROCESS_GIGABYTES] = ColorPair(Green, Black),
      [PROCESS_BASENAME] = ColorPair(Green, Black),
      [PROCESS_TREE] = ColorPair(Blue, Black),
      [PROCESS_R_STATE] = ColorPair(Green, Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Black),
      [PROCESS_NEW] = ColorPair(Black, Green),
      [PROCESS_TOMB] = ColorPair(Black, Red),
      [PROCESS_THREAD] = ColorPair(Blue, Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue, Black),
      [PROCESS_COMM] = ColorPair(Magenta, Black),
      [PROCESS_THREAD_COMM] = ColorPair(Yellow, Black),
      [BAR_BORDER] = ColorPair(Blue, Black),
      [BAR_SHADOW] = ColorPairGrayBlack,
      [SWAP] = ColorPair(Red, Black),
      [SWAP_CACHE] = ColorPair(Yellow, Black),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan, Black),
      [GRAPH_2] = ColorPair(Cyan, Black),
      [MEMORY_USED] = ColorPair(Green, Black),
      [MEMORY_BUFFERS] = ColorPair(Cyan, Black),
      [MEMORY_BUFFERS_TEXT] = ColorPair(Cyan, Black),
      [MEMORY_CACHE] = ColorPair(Yellow, Black),
      [MEMORY_SHARED] = ColorPair(Magenta, Black),
      [HUGEPAGE_1] = ColorPair(Green, Black),
      [HUGEPAGE_2] = ColorPair(Yellow, Black),
      [HUGEPAGE_3] = ColorPair(Red, Black),
      [HUGEPAGE_4] = ColorPair(Blue, Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Black, Black),
      [LOAD_AVERAGE_FIVE] = ColorPair(Black, Black),
      [LOAD_AVERAGE_ONE] = ColorPair(Black, Black),
      [LOAD] = ColorPairWhiteDefault,
      [HELP_BOLD] = ColorPair(Blue, Black),
      [CLOCK] = ColorPairWhiteDefault,
      [DATE] = ColorPairWhiteDefault,
      [DATETIME] = ColorPairWhiteDefault,
      [CHECK_BOX] = ColorPair(Blue, Black),
      [CHECK_MARK] = ColorPair(Black, Black),
      [CHECK_TEXT] = ColorPair(Black, Black),
      [HOSTNAME] = ColorPairWhiteDefault,
      [CPU_NICE] = ColorPair(Cyan, Black),
      [CPU_NICE_TEXT] = ColorPair(Cyan, Black),
      [CPU_NORMAL] = ColorPair(Green, Black),
      [CPU_SYSTEM] = ColorPair(Red, Black),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black, Black),
      [CPU_IRQ] = A_BOLD | ColorPair(Blue, Black),
      [CPU_SOFTIRQ] = ColorPair(Blue, Black),
      [CPU_STEAL] = ColorPair(Black, Black),
      [CPU_GUEST] = ColorPair(Black, Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Black, Black),
      [PRESSURE_STALL_SIXTY] = ColorPair(Black, Black),
      [PRESSURE_STALL_TEN] = ColorPair(Black, Black),
      [ZFS_MFU] = ColorPair(Cyan, Black),
      [ZFS_MRU] = ColorPair(Yellow, Black),
      [ZFS_ANON] = A_BOLD | ColorPair(Magenta, Black),
      [ZFS_HEADER] = ColorPair(Black, Black),
      [ZFS_OTHER] = A_BOLD | ColorPair(Magenta, Black),
      [ZFS_COMPRESSED] = ColorPair(Cyan, Black),
      [ZFS_RATIO] = A_BOLD | ColorPair(Magenta, Black),
      [ZRAM] = ColorPair(Yellow, Black),
   },
   [COLORSCHEME_MIDNIGHT] = {
      [RESET_COLOR] = ColorPair(White, Blue),
      [DEFAULT_COLOR] = ColorPair(White, Blue),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = A_NORMAL,
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, White),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = A_BOLD | ColorPair(Yellow, Blue),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = A_BOLD | ColorPair(Red, Blue),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = A_BOLD | ColorPair(Yellow, Blue),
      [BATTERY] = A_BOLD | ColorPair(Yellow, Blue),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red, Blue),
      [METER_SHADOW] = ColorPair(Cyan, Blue),
      [METER_TEXT] = ColorPair(Cyan, Blue),
      [METER_VALUE] = A_BOLD | ColorPair(Cyan, Blue),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Blue),
      [METER_VALUE_IOREAD] = ColorPair(Green, Blue),
      [METER_VALUE_IOWRITE] = ColorPair(Black, Blue),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(White, Blue),
      [METER_VALUE_OK] = ColorPair(Green, Blue),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Blue),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green, Blue),
      [PROCESS] = ColorPair(White, Blue),
      [PROCESS_SHADOW] = A_BOLD | ColorPair(Black, Blue),
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow, Blue),
      [PROCESS_MEGABYTES] = ColorPair(Cyan, Blue),
      [PROCESS_GIGABYTES] = ColorPair(Green, Blue),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan, Blue),
      [PROCESS_TREE] = ColorPair(Cyan, Blue),
      [PROCESS_R_STATE] = ColorPair(Green, Blue),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Blue),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Blue),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Blue),
      [PROCESS_NEW] = ColorPair(Blue, Green),
      [PROCESS_TOMB] = ColorPair(Blue, Red),
      [PROCESS_THREAD] = ColorPair(Green, Blue),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green, Blue),
      [PROCESS_COMM] = ColorPair(Magenta, Blue),
      [PROCESS_THREAD_COMM] = ColorPair(Black, Blue),
      [BAR_BORDER] = A_BOLD | ColorPair(Yellow, Blue),
      [BAR_SHADOW] = ColorPair(Cyan, Blue),
      [SWAP] = ColorPair(Red, Blue),
      [SWAP_CACHE] = A_BOLD | ColorPair(Yellow, Blue),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan, Blue),
      [GRAPH_2] = ColorPair(Cyan, Blue),
      [MEMORY_USED] = A_BOLD | ColorPair(Green, Blue),
      [MEMORY_BUFFERS] = A_BOLD | ColorPair(Cyan, Blue),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Cyan, Blue),
      [MEMORY_CACHE] = A_BOLD | ColorPair(Yellow, Blue),
      [MEMORY_SHARED] = A_BOLD | ColorPair(Magenta, Blue),
      [HUGEPAGE_1] = A_BOLD | ColorPair(Green, Blue),
      [HUGEPAGE_2] = A_BOLD | ColorPair(Yellow, Blue),
      [HUGEPAGE_3] = A_BOLD | ColorPair(Red, Blue),
      [HUGEPAGE_4] = A_BOLD | ColorPair(White, Blue),
      [LOAD_AVERAGE_FIFTEEN] = A_BOLD | ColorPair(Black, Blue),
      [LOAD_AVERAGE_FIVE] = A_NORMAL | ColorPair(White, Blue),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White, Blue),
      [LOAD] = A_BOLD | ColorPair(White, Blue),
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan, Blue),
      [CLOCK] = ColorPair(White, Blue),
      [DATE] = ColorPair(White, Blue),
      [DATETIME] = ColorPair(White, Blue),
      [CHECK_BOX] = ColorPair(Cyan, Blue),
      [CHECK_MARK] = A_BOLD | ColorPair(White, Blue),
      [CHECK_TEXT] = A_NORMAL | ColorPair(White, Blue),
      [HOSTNAME] = ColorPair(White, Blue),
      [CPU_NICE] = A_BOLD | ColorPair(Cyan, Blue),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Cyan, Blue),
      [CPU_NORMAL] = A_BOLD | ColorPair(Green, Blue),
      [CPU_SYSTEM] = A_BOLD | ColorPair(Red, Blue),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black, Blue),
      [CPU_IRQ] = A_BOLD | ColorPair(Black, Blue),
      [CPU_SOFTIRQ] = ColorPair(Black, Blue),
      [CPU_STEAL] = ColorPair(White, Blue),
      [CPU_GUEST] = ColorPair(White, Blue),
      [PRESSURE_STALL_THREEHUNDRED] = A_BOLD | ColorPair(Black, Blue),
      [PRESSURE_STALL_SIXTY] = A_NORMAL | ColorPair(White, Blue),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(White, Blue),
      [ZFS_MFU] = A_BOLD | ColorPair(White, Blue),
      [ZFS_MRU] = A_BOLD | ColorPair(Yellow, Blue),
      [ZFS_ANON] = A_BOLD | ColorPair(Magenta, Blue),
      [ZFS_HEADER] = A_BOLD | ColorPair(Yellow, Blue),
      [ZFS_OTHER] = A_BOLD | ColorPair(Magenta, Blue),
      [ZFS_COMPRESSED] = A_BOLD | ColorPair(White, Blue),
      [ZFS_RATIO] = A_BOLD | ColorPair(Magenta, Blue),
      [ZRAM] = A_BOLD | ColorPair(Yellow, Blue),
   },
   [COLORSCHEME_BLACKNIGHT] = {
      [RESET_COLOR] = ColorPair(Cyan, Black),
      [DEFAULT_COLOR] = ColorPair(Cyan, Black),
      [FUNCTION_BAR] = ColorPair(Black, Green),
      [FUNCTION_KEY] = ColorPair(Cyan, Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Black, White),
      [FAILED_SEARCH] = ColorPair(Red, Green),
      [FAILED_READ] = A_BOLD | ColorPair(Red, Black),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Green),
      [UPTIME] = ColorPair(Green, Black),
      [BATTERY] = ColorPair(Green, Black),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red, Black),
      [METER_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [METER_TEXT] = ColorPair(Cyan, Black),
      [METER_VALUE] = ColorPair(Green, Black),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Black),
      [METER_VALUE_IOREAD] = ColorPair(Green, Black),
      [METER_VALUE_IOWRITE] = ColorPair(Blue, Black),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(White, Black),
      [METER_VALUE_OK] = ColorPair(Green, Black),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Black),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green, Black),
      [PROCESS] = ColorPair(Cyan, Black),
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow, Black),
      [PROCESS_MEGABYTES] = A_BOLD | ColorPair(Green, Black),
      [PROCESS_GIGABYTES] = A_BOLD | ColorPair(Yellow, Black),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Green, Black),
      [PROCESS_TREE] = ColorPair(Cyan, Black),
      [PROCESS_THREAD] = ColorPair(Green, Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue, Black),
      [PROCESS_COMM] = ColorPair(Magenta, Black),
      [PROCESS_THREAD_COMM] = ColorPair(Yellow, Black),
      [PROCESS_R_STATE] = ColorPair(Green, Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Black),
      [PROCESS_NEW] = ColorPair(Black, Green),
      [PROCESS_TOMB] = ColorPair(Black, Red),
      [BAR_BORDER] = A_BOLD | ColorPair(Green, Black),
      [BAR_SHADOW] = ColorPair(Cyan, Black),
      [SWAP] = ColorPair(Red, Black),
      [SWAP_CACHE] = ColorPair(Yellow, Black),
      [GRAPH_1] = A_BOLD | ColorPair(Green, Black),
      [GRAPH_2] = ColorPair(Green, Black),
      [MEMORY_USED] = ColorPair(Green, Black),
      [MEMORY_BUFFERS] = ColorPair(Blue, Black),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [MEMORY_CACHE] = ColorPair(Yellow, Black),
      [MEMORY_SHARED] = ColorPair(Magenta, Black),
      [HUGEPAGE_1] = ColorPair(Green, Black),
      [HUGEPAGE_2] = ColorPair(Yellow, Black),
      [HUGEPAGE_3] = ColorPair(Red, Black),
      [HUGEPAGE_4] = ColorPair(Blue, Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Green, Black),
      [LOAD_AVERAGE_FIVE] = ColorPair(Green, Black),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(Green, Black),
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan, Black),
      [CLOCK] = ColorPair(Green, Black),
      [CHECK_BOX] = ColorPair(Green, Black),
      [CHECK_MARK] = A_BOLD | ColorPair(Green, Black),
      [CHECK_TEXT] = ColorPair(Cyan, Black),
      [HOSTNAME] = ColorPair(Green, Black),
      [CPU_NICE] = ColorPair(Blue, Black),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [CPU_NORMAL] = ColorPair(Green, Black),
      [CPU_SYSTEM] = ColorPair(Red, Black),
      [CPU_IOWAIT] = ColorPair(Yellow, Black),
      [CPU_IRQ] = A_BOLD | ColorPair(Blue, Black),
      [CPU_SOFTIRQ] = ColorPair(Blue, Black),
      [CPU_STEAL] = ColorPair(Cyan, Black),
      [CPU_GUEST] = ColorPair(Cyan, Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Green, Black),
      [PRESSURE_STALL_SIXTY] = ColorPair(Green, Black),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(Green, Black),
      [ZFS_MFU] = ColorPair(Blue, Black),
      [ZFS_MRU] = ColorPair(Yellow, Black),
      [ZFS_ANON] = ColorPair(Magenta, Black),
      [ZFS_HEADER] = ColorPair(Yellow, Black),
      [ZFS_OTHER] = ColorPair(Magenta, Black),
      [ZFS_COMPRESSED] = ColorPair(Blue, Black),
      [ZFS_RATIO] = ColorPair(Magenta, Black),
      [ZRAM] = ColorPair(Yellow, Black),
   },
   [COLORSCHEME_BROKENGRAY] = { 0 } // dynamically generated.
};

int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

ColorScheme CRT_colorScheme = COLORSCHEME_DEFAULT;

ATTR_NORETURN
static void CRT_handleSIGTERM(ATTR_UNUSED int sgn) {
   CRT_done();
   _exit(0);
}

#ifndef NDEBUG

static int stderrRedirectNewFd = -1;
static int stderrRedirectBackupFd = -1;

static int createStderrCacheFile(void) {
#if defined(HAVE_MEMFD_CREATE)
   return memfd_create("htop.stderr-redirect", 0);
#elif defined(O_TMPFILE)
   return open("/tmp", O_TMPFILE | O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
#else
   char tmpName[] = "htop.stderr-redirectXXXXXX";
   mode_t curUmask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
   int r = mkstemp(tmpName);
   umask(curUmask);
   if (r < 0)
      return r;

   (void) unlink(tmpName);

   return r;
#endif /* HAVE_MEMFD_CREATE */
}

static void redirectStderr(void) {
   stderrRedirectNewFd = createStderrCacheFile();
   if (stderrRedirectNewFd < 0) {
      /* ignore failure */
      return;
   }

   stderrRedirectBackupFd = dup(STDERR_FILENO);
   dup2(stderrRedirectNewFd, STDERR_FILENO);
}

static void dumpStderr(void) {
   if (stderrRedirectNewFd < 0)
      return;

   fsync(STDERR_FILENO);
   dup2(stderrRedirectBackupFd, STDERR_FILENO);
   lseek(stderrRedirectNewFd, 0, SEEK_SET);

   bool header = false;
   char buffer[8192];
   for (;;) {
      errno = 0;
      ssize_t res = read(stderrRedirectNewFd, buffer, sizeof(buffer));
      if (res < 0) {
         if (errno == EINTR)
            continue;

         break;
      }

      if (res == 0) {
         break;
      }

      if (res > 0) {
         if (!header) {
            fprintf(stderr, ">>>>>>>>>> stderr çıkışı >>>>>>>>>>\n\n");
            header = true;
         }
         (void)! write(STDERR_FILENO, buffer, res);
      }
   }

   if (header)
      fprintf(stderr, "\n<<<<<<<<<< stderr çıkışı <<<<<<<<<<\n");

   close(stderrRedirectNewFd);
   stderrRedirectNewFd = -1;
}

#else /* !NDEBUG */

static void redirectStderr(void) {
}

static void dumpStderr(void) {
}

#endif /* !NDEBUG */

static struct sigaction old_sig_handler[32];

void CRT_init(const Settings* settings, bool allowUnicode) {
   redirectStderr();

   initscr();
   noecho();
   CRT_delay = &(settings->delay);
   CRT_colors = CRT_colorSchemes[settings->colorScheme];
   CRT_colorScheme = settings->colorScheme;

   for (int i = 0; i < LAST_COLORELEMENT; i++) {
      unsigned int color = CRT_colorSchemes[COLORSCHEME_DEFAULT][i];
      CRT_colorSchemes[COLORSCHEME_BROKENGRAY][i] = color == (A_BOLD | ColorPairGrayBlack) ? ColorPair(White, Black) : color;
   }

   halfdelay(*CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
   curs_set(0);

   if (has_colors()) {
      start_color();
   }

   const char* termType = getenv("TERM");
   if (termType && String_eq(termType, "linux")) {
      CRT_scrollHAmount = 20;
   } else {
      CRT_scrollHAmount = 5;
   }

   if (termType && (String_startsWith(termType, "xterm") || String_eq(termType, "vt220"))) {
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033[7~", KEY_HOME);
      define_key("\033[8~", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033O2R", KEY_F(15));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[14;2~", KEY_F(15));
      define_key("\033[17;2~", KEY_F(18));
      char sequence[3] = "\033a";
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
   }

   struct sigaction act;
   sigemptyset (&act.sa_mask);
   act.sa_flags = (int)SA_RESETHAND | SA_NODEFER;
   act.sa_handler = CRT_handleSIGSEGV;
   sigaction (SIGSEGV, &act, &old_sig_handler[SIGSEGV]);
   sigaction (SIGFPE, &act, &old_sig_handler[SIGFPE]);
   sigaction (SIGILL, &act, &old_sig_handler[SIGILL]);
   sigaction (SIGBUS, &act, &old_sig_handler[SIGBUS]);
   sigaction (SIGPIPE, &act, &old_sig_handler[SIGPIPE]);
   sigaction (SIGSYS, &act, &old_sig_handler[SIGSYS]);
   sigaction (SIGABRT, &act, &old_sig_handler[SIGABRT]);

   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);

   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = COLORSCHEME_MONOCHROME;
   CRT_setColors(CRT_colorScheme);

#ifdef HAVE_LIBNCURSESW
   if (allowUnicode && String_eq(nl_langinfo(CODESET), "UTF-8")) {
      CRT_utf8 = true;
   } else {
      CRT_utf8 = false;
   }
#else
   (void) allowUnicode;
#endif

   CRT_treeStr =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CRT_treeStrUtf8 :
#endif
      CRT_treeStrAscii;

#if NCURSES_MOUSE_VERSION > 1
   mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
   mousemask(BUTTON1_RELEASED, NULL);
#endif

   CRT_degreeSign = initDegreeSign();
}

void CRT_done() {
   curs_set(1);
   endwin();

   dumpStderr();
}

void CRT_fatalError(const char* note) {
   const char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(*CRT_delay);
   return ret;
}

void CRT_disableDelay() {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   halfdelay(*CRT_delay);
}

void CRT_setColors(int colorScheme) {
   CRT_colorScheme = colorScheme;

   for (short int i = 0; i < 8; i++) {
      for (short int j = 0; j < 8; j++) {
         if (ColorIndex(i, j) != ColorIndexGrayBlack && ColorIndex(i, j) != ColorIndexWhiteDefault) {
            short int bg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? (j == 0 ? -1 : j)
                     : j;
            init_pair(ColorIndex(i, j), i, bg);
         }
      }
   }

   short int grayBlackFg = COLORS > 8 ? 8 : 0;
   short int grayBlackBg = (colorScheme != COLORSCHEME_BLACKNIGHT) ? -1 : 0;
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   init_pair(ColorIndexWhiteDefault, White, -1);

   CRT_colors = CRT_colorSchemes[colorScheme];
}

void CRT_handleSIGSEGV(int signal) {
   CRT_done();

   fprintf(stderr, "\n\n"
      "ÖNEMLİ PROGRAM HATASI TESPİT EDİLDİ\n"
      "===================================\n"
      "Lütfen https://htop.dev/issues adresinden bu sorunun önceden bildirilip bildirilmediğini kontrol edin.\n"
      "Daha önce benzer bir sorun bildirilmemişse, lütfen aşağıdaki bilgilerle yeni bir sorun oluşturun:\n"
      "\n"
      "- Htop sürümünüz (htop - sürüm)\n"
      "- İşletim sisteminiz ve çekirdek sürümünüz (uname -a)\n"
      "- Dağıtımınız ve sürümünüz(lsb_release -a)\n"
      "- Yeniden üretmek için olası adımlar (Nasıl oldu?)\n"
   );

#ifdef HAVE_EXECINFO_H
   fprintf(stderr, "- Sorunun geri takibi (aşağıya bakın)\n");
#endif

   fprintf(stderr,
      "\n"
   );

   const char* signal_str = strsignal(signal);
   if (!signal_str) {
      signal_str = "bilinmeyen sebep";
   }
   fprintf(stderr,
      "hata bilgisi:\n"
      "------------------\n"
      "%d (%s) sinyali alındı.\n"
      "\n",
      signal, signal_str
   );

#ifdef HAVE_EXECINFO_H
   fprintf(stderr,
      "Geri izleme bilgileri:\n"
      "----------------------\n"
      "Sorun tespit edildiğinde aşağıdaki işlev çağrıları etkindi:\n"
      "---\n"
   );

   void *backtraceArray[256];

   size_t size = backtrace(backtraceArray, ARRAYSIZE(backtraceArray));
   backtrace_symbols_fd(backtraceArray, size, 2);
   fprintf(stderr,
      "---\n"
      "\n"
      "Yukarıdaki bilgileri çalışmayı daha pratik hale getirmek için,\n"
      "ikili programınızın bir demontajını sağlamalısınız.\n"
      "Bu genellikle aşağıdaki komutu çalıştırarak yapılabilir.:\n"
      "\n"
   );

#ifdef HTOP_DARWIN
   fprintf(stderr, "   otool -tvV `which htop` > ~/htop.otool\n");
#else
   fprintf(stderr, "   objdump -d -S -w `which htop` > ~/htop.objdump\n");
#endif

   fprintf(stderr,
      "\n"
      "Lütfen oluşturulan dosyayı raporunuza ekleyin.\n"
      "\n"
   );
#endif

   fprintf(stderr,
      "Bu programı hata ayıklama sembolleriyle veya bir hata ayıklayıcı içinde çalıştırmak daha fazla bilgi sağlayabilir.\n"
      "\n"
      "Htop'u geliştirmeye yardımcı olduğunuz için teşekkür ederiz!\n"
      "\n"
      "htop " VERSION " iptal ediliyor.\n"
      "\n"
   );

   /* Call old sigsegv handler; may be default exit or third party one (e.g. ASAN) */
   if (sigaction (signal, &old_sig_handler[signal], NULL) < 0) {
      /* This avoids an infinite loop in case the handler could not be reset. */
      fprintf(stderr,
         "!!! Zincirli işleyici geri yüklenemedi. Çıkışı zorlamak.\n"
      );
      _exit(1);
   }

   /* Trigger the previous signal handler. */
   raise(signal);

   // Always terminate, even if installed handler returns
   fprintf(stderr,
      "!!! Zincirli işleyici çıkmadı. Çıkışı zorlamak.\n"
   );
   _exit(1);
}
