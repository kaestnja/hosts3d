/* hosts3d.cpp - 10 May 11
   Hosts3D - 3D Real-Time Network Monitor
   Copyright (c) 2006-2011  Del Castle
   Continuation note: This repository is an independent community-maintained
   continuation of the original Hosts3D upstream.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details. */

#include <ctype.h>  //tolower()
#include <algorithm>
#include <map>
#include <math.h>  //cos(), sin(), sqrt()
#include <signal.h>  //signal()
#include <stdio.h>
#include <stdlib.h>  //abs(), atoi(), system()
#include <string>
#include <string.h>
#include <sys/stat.h>  //mkdir()
#include <time.h>
#include <vector>
#ifdef __MINGW32__
#include <getopt.h>  //getopt()
#include <winsock2.h>
#else
#include <arpa/inet.h>  //inet_addr(), inet_ntoa()
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>  //fcntl()
#include <limits.h>
#include <netdb.h>
#include <syslog.h>  //syslog()
#include <sys/wait.h>
#include <unistd.h>  //close(), usleep()
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "version.h"
#include "glwin.h"
#include "objects.h"
#ifdef __MINGW32__
#include <windows.h>
#endif

FILE *pfile;  //packet traffic record file
bool goRun = true, goSize = true, goAnim = true, mMove = false, mView = false, dAnom = false, refresh = false, animate = false, fullscn = false;
static const int DEFAULT_WIN_W = 1024;
static const int DEFAULT_WIN_H = 600;
static const unsigned int DEFAULT_DYNAMIC_HOST_TTL_SECONDS = 300;
static const unsigned int DEFAULT_DYNAMIC_HOST_CLEANUP_INTERVAL_SECONDS = 30;
static const unsigned char LOCAL_HSEN_MAX_INTERFACES = 16;
static const unsigned int HELP_OVERLAY_MAX_LINES = 128;
static const int HELP_OVERLAY_W = 480;
static const int HELP_OVERLAY_MARGIN = 10;
static const int HELP_OVERLAY_HEADER_H = 28;
static const int HELP_OVERLAY_FOOTER_H = 18;
static const int HELP_OVERLAY_KEY_COL_W = 118;
static const int OSD_MIN_W = 420;
static const int OSD_MIN_H = 380;
static const int OSD_LINE_H = 13;
static const int OSD_PKT_GAP_H = 18;
static const int OSD_PKT_ICON_SCALE = 6;
static const int OSD_PKT_ICON_COL_W = 34;
static const int OSD_PKT_ROW_H = 28;
static const int OSD_PKT_SECTION_GAP_H = 16;
static const int OSD_PKT_TREE_INDENT_W = 8;
static const int OSD_PKT_ROW_ICON_Y = 16;
static const int OSD_PKT_ROW_TEXT_Y = 20;
static const int OSD_PKT_ROW_HILITE_TOP_Y = 2;
static const int OSD_PKT_ROW_HILITE_BOTTOM_Y = 26;
static const unsigned int OSD_PKT_HIT_MAX = 24;
static const int PACKET_STATUS_W = 260;
static const int PACKET_STATUS_LINE_H = 13;
static const int PACKET_STATUS_PAD_X = 8;
static const int PACKET_STATUS_PAD_Y = 8;
static const int OSD_CHAR_W = 6;
static const int OSD_MARGIN_X = 10;
static const int OSD_MARGIN_Y = 16;
static const int OSD_BOX_PAD_X = 8;
static const int OSD_BOX_PAD_Y = 8;
static const int OSD_LABEL_COL_CHARS = 29;
static const int OSD_VALUE_COL_CHARS = 16;
static const int OSD_BOX_W = (OSD_BOX_PAD_X * 2) + ((OSD_LABEL_COL_CHARS + OSD_VALUE_COL_CHARS) * OSD_CHAR_W);
static const int OSD_VALUE_X = OSD_BOX_PAD_X + (OSD_LABEL_COL_CHARS * OSD_CHAR_W);
static const unsigned int OSD_MAX_ROWS = 24;
static const unsigned int PACKET_TRIPLE_SIZE_THRESHOLD = 512;
static const double PACKET_CENTER_Y = 5.0;
static const double PACKET_LABEL_Y = 7.0;
static const double RAD2DEG = 57.29577951308232;
static const unsigned char PACKET_COLOR_COUNT = 6;
static const unsigned char PACKET_SHAPE_COUNT = 5;
static const int PACKET_LIST_BASE = 15;
static const int ALERT_LIST_BASE = PACKET_LIST_BASE + (PACKET_COLOR_COUNT * PACKET_SHAPE_COUNT);
static const int OBJ_LIST_COUNT = ALERT_LIST_BASE + 2;
int wWin = DEFAULT_WIN_W, hWin = DEFAULT_WIN_H, mBxx, mBxy, mPsx = 0, mPsy = 0, mWhl = 0, GLResult[6] = {0, 0, 0, 0, 0, 0};  //2D GUI result identification
time_t atime = 0, distm;  //packet traffic display time offset
time_t packetTrafficStarted = 0;
ptrc_type ptrc = stp;  //packet traffic state
pkrc_type pkrp;  //packet traffic replay packet
bool pkrLegacy = false;
unsigned short frame = 0;
unsigned long long fps = 0, rpoff;  //packet traffic replay time offset
char goHosts = 2, htdtls[960];
unsigned int osdTextLineCount = 0;
view_type vwdef[2] = {{0.0, 0.0, {0.0, 75.0, -360.0}, {0.0, 75.0, MOV - 360.0}}, {90.0, 0.0, {-360.0, 75.0, 0.0}, {MOV - 360.0, 75.0, 0.0}}};  //default views
sett_type setts = {false, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 5000, "", "<sudo command> hsen", "1", "eth0", "", "<sudo command> killall hsen"
  , {"", "", "", ""}, ins, off, alt, {vwdef[0], vwdef[0], vwdef[0], vwdef[0], vwdef[0]}};  //settings
host_type *seltd = 0, *lnkht = 0;  //selected host, link line host
MyLL lnksLL, altsLL, pktsLL;  //dynamic data struct for links, alerts, packets
MyHT hstsByIp; // data struct that arranges hosts by ip address
MyHT hstsByPos; // data struct that arranges hosts by (x, y, z) co-ordinates
std::map<unsigned long, bool> hostDynamicStateByIp;  //true = dynamic, false = static
GLuint objsDraw;  //GL compiled objects
MyGLWin GLWin;  //2D GUI
bool useIpAddrForGlName; // if true, use the host's IP address as the 'name' for host GL objects. Otherwise use the host_type pointer.
bool dynamicHostsEnabled = true;
unsigned int dynamicHostTtlSeconds = DEFAULT_DYNAMIC_HOST_TTL_SECONDS;
unsigned int dynamicHostCleanupIntervalSeconds = DEFAULT_DYNAMIC_HOST_CLEANUP_INTERVAL_SECONDS;
time_t dynamicHostLastCleanup = 0;
bool helpOverlayVisible = false;
unsigned int helpOverlayLineCount = 0, helpOverlayScroll = 0;

struct help_overlay_line_type
{
  char key[48];
  char desc[160];
};

help_overlay_line_type helpOverlayLines[HELP_OVERLAY_MAX_LINES];

enum osd_color_type { osdNormal, osdAccent, osdAlert };

enum osd_row_action_type
{
  osaNone = 0,
  osaSensorFilter,
  osaPacketFilter,
  osaPortFilter,
  osaDisplayMode,
  osaDisplayScope,
  osaOnActiveAction,
  osaAnomalyDetection,
  osaFastPackets,
  osaAddDestinationHosts,
  osaShowBroadcastHosts,
  osaNewHostPackets,
  osaNewHostLinks,
  osaShowPacketDestPort,
  osaPacketLimit,
  osaPacketAnimation,
  osaAcknowledgeAnomalies
};

struct osd_row_type
{
  bool section;
  osd_color_type valueColor;
  unsigned char action;
  char label[40];
  char value[24];
};

osd_row_type osdRows[OSD_MAX_ROWS];

struct osd_row_hit_type
{
  int left, top, right, bottom;
  unsigned char action;
};

osd_row_hit_type osdRowHits[OSD_MAX_ROWS];
unsigned int osdRowHitCount = 0;

enum pflt_type
{
  pfAll = 0,
  pfTcpAll,
  pfTcpControl,
  pfTcpSetupFinish,
  pfTcpReset,
  pfTcpPayload,
  pfTcpLargePayload,
  pfTcpDiscovery,
  pfUdpAll,
  pfUdpPayload,
  pfUdpLargePayload,
  pfUdpDiscovery,
  pfIcmpAll,
  pfArpAll,
  pfArpRequest,
  pfArpReply,
  pfArpGratuitous,
  pfOtherAll,
  pfOtherFragmented
};

struct packet_tree_row_type
{
  unsigned char filter, indent, colorIndex, shape;
  const char *label;
};

struct host_runtime_meta_type
{
  unsigned char pr, shp, clr;
  unsigned short port;
  char dnm[256];
  bool knownNetpos;
};

struct osd_pkt_hit_type
{
  int left, top, right, bottom;
  unsigned char filter;
  bool active;
};

host_runtime_meta_type hostRuntimeMetaDefault = {0, 0, 0, 0, "", false};
std::map<unsigned long, host_runtime_meta_type> hostRuntimeMetaByIp;
osd_pkt_hit_type osdPacketHits[OSD_PKT_HIT_MAX];
unsigned int osdPacketHitCount = 0;
unsigned char packetTreeFilter = pfAll;
char packetTrafficLabel[128] = "traffic.hpt";
char packetTrafficPath[512] = "";
unsigned int knownNetposExactHostCount = 0;

struct localhsen_if_type
{
  char id[256];
  char name[256];
  char klass[16];
  bool selected;
  unsigned char sensorId;
};

struct localhsen_pid_type
{
  unsigned long pid;
  unsigned long long created;
};

localhsen_if_type localHsenIfs[LOCAL_HSEN_MAX_INTERFACES];
unsigned char localHsenIfCount = 0;
bool localHsenWindowsAutostart = false;
bool localHsenWindowsPromisc = false;
int localHsenUiAutostart = -1, localHsenUiPromisc = -1;
int localHsenUiIfaceChecks[LOCAL_HSEN_MAX_INTERFACES];
int localHsenUiIfaceSensors[LOCAL_HSEN_MAX_INTERFACES];

#ifdef __MINGW32__
#define usleep(usec) (Sleep((usec) / 1000))
#endif

#define CTRLKEY(key) ((key) + 512)
#define SHIFTKEY(key) ((key) + 1024)
#define CTRLSHIFTKEY(key) ((key) + 1536)

enum keyact_type
{
  kaMoveForward, kaMoveBack, kaMoveLeft, kaMoveRight,
  kaViewHome, kaViewHomeAlt, kaViewPos1, kaViewPos2, kaViewPos3, kaViewPos4,
  kaViewSave1, kaViewSave2, kaViewSave3, kaViewSave4,
  kaLayoutRestore1, kaLayoutRestore2, kaLayoutRestore3, kaLayoutRestore4,
  kaLayoutSave1, kaLayoutSave2, kaLayoutSave3, kaLayoutSave4,
  kaSelectAll, kaInvertSelection, kaSelectNamed,
  kaSelMoveUp, kaSelMoveDown, kaSelMoveForward, kaSelMoveBack, kaSelMoveLeft, kaSelMoveRight,
  kaFindHosts, kaNextSelectedHost, kaPrevSelectedHost, kaToggleSelectionPersistant, kaCycleIpNameDisplay,
  kaToggleAddDestinationHosts, kaMakeHost, kaEditHostname, kaEditRemarks,
  kaCreateLinkLine, kaDeleteLinkLine, kaAutoLinksAll, kaToggleNewHostLinks, kaAutoLinksSelection, kaStopAutoLinksSelection, kaDeleteAllLinks,
  kaShowSelectionPackets, kaHideSelectionPackets, kaShowAllPackets, kaToggleNewHostPackets,
  kaShowSensor1Packets, kaShowSensor2Packets, kaShowSensor3Packets, kaShowSensor4Packets, kaShowSensor5Packets, kaShowSensor6Packets, kaShowSensor7Packets, kaShowSensor8Packets, kaShowSensor9Packets, kaShowAllSensorPackets, kaPrevSensorPackets, kaNextSensorPackets,
  kaToggleBroadcasts, kaDecreasePacketLimit, kaIncreasePacketLimit, kaTogglePacketDestPort, kaTogglePacketSpeed, kaPacketsOff,
  kaRecordPacketTraffic, kaReplayPacketTraffic, kaSkipReplayPacket, kaStopPacketTraffic, kaOpenPacketTrafficFile, kaSavePacketTrafficFile,
  kaToggleAnimation, kaAcknowledgeAllAnomalies, kaToggleOsd, kaExportSelectionCsv, kaShowHostInformation, kaShowSelectionInformation, kaShowHelp,
  kaCount
};

struct keybind_type
{
  const char *name;
  int key;
};

static keybind_type keybinds[kaCount] =
{
  {"move_forward", GLFW_KEY_UP},
  {"move_back", GLFW_KEY_DOWN},
  {"move_left", GLFW_KEY_LEFT},
  {"move_right", GLFW_KEY_RIGHT},
  {"view_home", GLFW_KEY_HOME},
  {"view_home_alt", CTRLKEY(GLFW_KEY_HOME)},
  {"view_position_1", GLFW_KEY_F1},
  {"view_position_2", GLFW_KEY_F2},
  {"view_position_3", GLFW_KEY_F3},
  {"view_position_4", GLFW_KEY_F4},
  {"save_view_position_1", CTRLKEY(GLFW_KEY_F1)},
  {"save_view_position_2", CTRLKEY(GLFW_KEY_F2)},
  {"save_view_position_3", CTRLKEY(GLFW_KEY_F3)},
  {"save_view_position_4", CTRLKEY(GLFW_KEY_F4)},
  {"restore_layout_1", SHIFTKEY(GLFW_KEY_F1)},
  {"restore_layout_2", SHIFTKEY(GLFW_KEY_F2)},
  {"restore_layout_3", SHIFTKEY(GLFW_KEY_F3)},
  {"restore_layout_4", SHIFTKEY(GLFW_KEY_F4)},
  {"save_layout_1", CTRLSHIFTKEY(GLFW_KEY_F1)},
  {"save_layout_2", CTRLSHIFTKEY(GLFW_KEY_F2)},
  {"save_layout_3", CTRLSHIFTKEY(GLFW_KEY_F3)},
  {"save_layout_4", CTRLSHIFTKEY(GLFW_KEY_F4)},
  {"select_all_hosts", CTRLKEY('A')},
  {"invert_selection", CTRLKEY('S')},
  {"select_all_named_hosts", CTRLKEY('N')},
  {"move_selection_up", 'Q'},
  {"move_selection_down", 'E'},
  {"move_selection_forward", 'W'},
  {"move_selection_back", 'S'},
  {"move_selection_left", 'A'},
  {"move_selection_right", 'D'},
  {"find_hosts", 'F'},
  {"select_next_host_in_selection", GLFW_KEY_TAB},
  {"select_previous_host_in_selection", CTRLKEY(GLFW_KEY_TAB)},
  {"toggle_selection_persistant_ip_name", 'T'},
  {"cycle_ip_name_display", 'C'},
  {"toggle_add_destination_hosts", CTRLKEY('D')},
  {"make_host", 'M'},
  {"edit_selected_hostname", 'N'},
  {"edit_selected_remarks", 'R'},
  {"create_link_line", 'L'},
  {"delete_link_line", CTRLKEY('L')},
  {"auto_link_all_hosts", 'Y'},
  {"toggle_auto_link_new_hosts", CTRLKEY('Y')},
  {"auto_link_selection", 'J'},
  {"stop_auto_link_selection", CTRLKEY('J')},
  {"delete_all_link_lines", CTRLKEY('R')},
  {"show_selection_packets", 'P'},
  {"hide_selection_packets", CTRLKEY('P')},
  {"show_all_packets", 'U'},
  {"toggle_show_packets_new_hosts", CTRLKEY('U')},
  {"show_sensor_1_packets", '1'},
  {"show_sensor_2_packets", '2'},
  {"show_sensor_3_packets", '3'},
  {"show_sensor_4_packets", '4'},
  {"show_sensor_5_packets", '5'},
  {"show_sensor_6_packets", '6'},
  {"show_sensor_7_packets", '7'},
  {"show_sensor_8_packets", '8'},
  {"show_sensor_9_packets", '9'},
  {"show_all_sensor_packets", '0'},
  {"show_previous_sensor_packets", ','},
  {"show_next_sensor_packets", '.'},
  {"toggle_broadcasts", 'B'},
  {"decrease_packet_limit", '-'},
  {"increase_packet_limit", '='},
  {"toggle_show_packet_destination_port", CTRLKEY('T')},
  {"toggle_packet_speed", 'Z'},
  {"packets_off", 'K'},
  {"record_packet_traffic", GLFW_KEY_F12},
  {"replay_packet_traffic", GLFW_KEY_F10},
  {"skip_replay_packet", GLFW_KEY_PAGEUP},
  {"stop_packet_traffic", GLFW_KEY_F11},
  {"open_packet_traffic_file", GLFW_KEY_F7},
  {"save_packet_traffic_file", GLFW_KEY_F8},
  {"toggle_animation", GLFW_KEY_SPACE},
  {"acknowledge_all_anomalies", CTRLKEY('K')},
  {"toggle_osd", 'O'},
  {"export_selection_csv", 'X'},
  {"show_selected_host_information", 'I'},
  {"show_selection_information", 'G'},
  {"show_help", 'H'}
};

enum menustate_type { msNone, msToggle, msChoice };

struct menu_selection_state_type
{
  bool any, mixedLock, mixedColour, mixedZone;
  bool lockOn;
  unsigned char colour;
  unsigned char zone;
};

static void ensureHsdDataDir();
static void helpOverlayLoad();
static void helpOverlayToggle();
static void helpOverlayBounds(int *left, int *top, int *right, int *bottom);
static bool helpOverlayMouseOver();
static void helpOverlayScrollDelta(int delta);
static void drawHelpOverlay();
static bool parseBindingValue(const char *value, int *out);
static void bindingLabel(int encoded, char *buf, size_t bufsz);
static const char *menuLabelWithHint(const char *title, int hotkey, menustate_type state = msNone, bool active = false);
static int menuDepthForId(int menuid);
static int menuItemHotkey(int value, keyact_type action);
static int menuItemWidth(const char *label, bool submenu);
static void addMenuItem(const char *title, int items, int sub, int value, int mnemonic = 0, keyact_type action = kaCount, menustate_type state = msNone, bool active = false);
static keyact_type keyActionFromInput(int encoded);
static void triggerKeyAction(keyact_type action);
static keyact_type menuActionForValue(int value);
static unsigned char hostZone(host_type *ht);
static void menuSelectionStateCb(void **data, long arg1, long arg2, long arg3, long arg4);
static menu_selection_state_type menuSelectionState();
static bool hostIsDynamic(host_type *ht);
static void hostSetDynamic(host_type *ht, bool dynamic);
static void hostPromoteStatic(host_type *ht);
static bool hostShouldPersist(host_type *ht);
static host_runtime_meta_type *hostRuntimeMeta(host_type *ht, bool create = true);
static bool hostIsKnownNetpos(host_type *ht);
static bool hostShouldDrawInShowHostMode(host_type *ht);
static bool hostShouldDrawLabel(host_type *ht);
static const char *hostLabelText(host_type *ht);
static void hostRuntimeNotePacket(host_type *ht, const pkex_type *pkex, unsigned short port);
static void hostRuntimeNoteDiscoveryName(host_type *ht, const char *name);
static bool parseNetposExactIpToken(const char *token, in_addr *ip);
static bool hostApplyNetposLayout(host_type *ht);
static void netposExactHostsSync();
static const char *packetShapeFilterLabel(unsigned char psf);
static bool parsePacketShapeFilterValue(const char *value, unsigned char *out);
static bool parsePacketTcpFilterValue(const char *value, unsigned char *out);
static unsigned char packetArpSubtype(const pkex_type *pkex);
static unsigned short packetImportantPort(const pkif_type *pkt);
static bool packetIsNameDiscovery(const pkif_type *pkt);
static unsigned char packetColorIndex(const pkex_type *pkex);
static unsigned char packetShape(const pkex_type *pkex);
static const packet_tree_row_type *packetTreeRows(unsigned int *count = 0);
static const char *packetTreeFilterLabel(unsigned char filter);
static const char *packetTreeFilterString(unsigned char filter);
static bool parsePacketTreeFilterValue(const char *value, unsigned char *out);
static bool packetFilterAllActive();
static bool packetTreeFilterActive(unsigned char filter);
static bool packetTreeMatches(const pkex_type *pkex, unsigned char filter);
static void packetTreeFilterMigrateLegacy();
static void packetFilterResetAll();
static void packetFilterSetSensor(unsigned char sen);
static void packetFilterSetTree(unsigned char filter);
static void packetFilterSetPort(unsigned short port);
static void osdRowHitsClear();
static void osdRowHitAdd(int left, int top, int right, int bottom, unsigned char action);
static bool osdRowHitProcess(int x, int y);
static void osdPacketHitsClear();
static void osdPacketHitAdd(int left, int top, int right, int bottom, unsigned char filter);
static bool osdPacketHitProcess(int x, int y);
static void setStringValue(char *dst, size_t dstsz, const char *src);
static void packetTrafficLabelSet(const char *path);
static const char *packetTrafficPathGet();
static void packetTrafficPathSet(const char *path);
static bool packetTrafficRecordPathNext(char *path, size_t pathsz, char *label, size_t labelsz);
static void packetTrafficResolvePath(const char *name, char *path, size_t pathsz);
static unsigned int packetTrafficReplayListCreate(const char *fl, char *firstName, size_t firstNameSz);
static bool packetTrafficReplayStart(const char *path);
static void packetTrafficDurationLabel(time_t started, char *buf, size_t bufsz);
static void drawPacketTrafficStatus();
void alrtsDestroy();
void hostsSet(unsigned char mbr, unsigned char val);
void osdUpdate();
host_type *hostIP(in_addr ip, bool crt);
host_type *hostCreate(in_addr ip, bool dynamic);
void hostDetails();
void mnuProcess(int m);
static void hostDeleteManaged(host_type *ht);
void pcktsDestroy(bool gh = false);
void offsetReset();
static void dynamicHostsCleanupMaybe();
static void settsSave();
static void localHsenDialogOpen();
static bool localHsenStopManaged(bool showmsg = false);
static bool localHsenSaveUiState();
static bool localHsenStartSelected(bool showmsg = false);
static bool localHsenDiscoverInterfaces(bool keepSelections = true);
static void localHsenAutostartMaybe();

static const int MENU_LEVEL_INDENT = 18;
static int menuBuildDepth = 0;
static int menuBaseX = 0;

void hsdStop(int sig)
{
  goRun = false;
}

HtKeyType keyFromCoOrdinates (int px, int py, int pz) {
  // todo: measure/fix collisions
  return (HtKeyType)(0xffff & ((px * 31) + (py * 37) + (pz * 41)));
}

HtKeyType keyFromIpAddress(in_addr ip) {
  // Thomas Wang hash
  // http://burtleburtle.net/bob/hash/integer.html

  unsigned int addr = ip.s_addr;

  addr += ~(addr << 15);
  addr ^= (addr >> 10);
  addr += (addr << 3);
  addr ^= (addr >> 6);
  addr += ~(addr << 11);
  addr ^= (addr >>16 );

  return (HtKeyType) addr;
}

// HtFirstThatCallback callback for hostIp()
bool isHostIpAddress(void *data, long arg1, long arg2, long arg3, long arg4) {
  host_type *ht = (host_type *) data;
  unsigned long saddress = (unsigned long) arg1;
  if (ht->hip.s_addr == saddress) {
    return true;
  }
  return false;
}

// HtFirstThatCallback callback for hostAtPos()
bool isHostHere(void * data, long pht_, long px_, long py_, long pz_) {
  host_type *ht = (host_type *) data;
  host_type *pht = (host_type *) pht_;
  int px = (int) px_;
  int py = (int) py_;
  int pz = (int) pz_;

  if ((ht != pht) && (ht->px == px) && (ht->py == py) && (ht->pz == pz)) {
    return true;
  }
  return false;
}

//check if a host is at a position
host_type *hostAtpos(int px, int py, int pz, host_type *pht)
{
  HtKeyType key = keyFromCoOrdinates(px, py, pz);
  return (host_type *) hstsByPos.firstThat(key, 2, &isHostHere,
					   (long) (void *)pht,
					   (long) px, (long) py, (long) pz);
}

//add an entry to the hstsByIp hashtable
void newHostByIp(host_type *ht) {
  HtKeyType key = keyFromIpAddress(ht->hip);
  (void)hstsByIp.newEntryAtBucket(key, (void *) ht);
}

// remove an entry from the hstsByPos hashtable
void hostByPositionPreMove(host_type *ht) {
  HtKeyType key = keyFromCoOrdinates(ht->px, ht->py, ht->pz);
  (void)hstsByPos.deleteEntryAtBucket(0, key, (void *) ht);
}

// add an entry to the hstsByPos hashtable
void hostByPositionPostMove(host_type *ht) {
  HtKeyType key = keyFromCoOrdinates(ht->px, ht->py, ht->pz);
  (void)hstsByPos.newEntryAtBucket(key, (void *) ht);
}

//find blank host position
void hostPos(host_type *ht, unsigned char hr, unsigned char sp)
{
  unsigned char hsp = sp * SPC;
  unsigned short hdm = hr * hsp;  //dimensions
  int tpx = ht->px, tpy = ht->py, tpz = ht->pz, spx = tpx, spz = tpz
    , addx = (tpx >= 0 ? hsp : -hsp), addy = (tpy >= 0 ? hsp : -hsp), addz = (tpz >= 0 ? hsp : -hsp);
  while (hostAtpos(tpx, tpy, tpz, ht))
  {
    tpx += addx;
    if (abs(tpx - spx) == hdm)
    {
      tpx = spx;
      tpz += addz;
      if (abs(tpz - spz) == hdm)
      {
        tpz = spz;
        tpy += addy;
      }
    }
  }
  ht->px = tpx;
  ht->py = tpy;
  ht->pz = tpz;
}

//detect host collision on move
void moveCollision(host_type *cht)
{
  host_type *ht;
  if ((ht = cht->col))
  {
    if (ht->col == cht) ht->col = 0;
    else
    {
      while (ht->col != cht) ht = ht->col;
      ht->col = cht->col;
    }
    cht->col = 0;
  }
  if ((ht = hostAtpos(cht->px, cht->py, cht->pz, cht)))
  {
    cht->col = (ht->col ? ht->col : ht);
    ht->col = cht;
  }
}

static unsigned long hostDynamicStateKey(in_addr ip)
{
  return (unsigned long) ip.s_addr;
}

static bool hostIsDynamic(host_type *ht)
{
  std::map<unsigned long, bool>::const_iterator it;
  if (!ht) return false;
  it = hostDynamicStateByIp.find(hostDynamicStateKey(ht->hip));
  if (it == hostDynamicStateByIp.end()) return false;
  return it->second;
}

static void hostSetDynamic(host_type *ht, bool dynamic)
{
  if (!ht) return;
  hostDynamicStateByIp[hostDynamicStateKey(ht->hip)] = dynamic;
}

static host_runtime_meta_type *hostRuntimeMeta(host_type *ht, bool create)
{
  std::map<unsigned long, host_runtime_meta_type>::iterator it;
  if (!ht) return 0;
  it = hostRuntimeMetaByIp.find(hostDynamicStateKey(ht->hip));
  if (it != hostRuntimeMetaByIp.end()) return &it->second;
  if (!create) return 0;
  hostRuntimeMetaByIp[hostDynamicStateKey(ht->hip)] = hostRuntimeMetaDefault;
  return &hostRuntimeMetaByIp[hostDynamicStateKey(ht->hip)];
}

static bool hostIsKnownNetpos(host_type *ht)
{
  host_runtime_meta_type *meta = hostRuntimeMeta(ht, false);
  return (meta && meta->knownNetpos);
}

static bool hostShouldDrawInShowHostMode(host_type *ht)
{
  return (ht && (hostIsKnownNetpos(ht) || ht->vis || ht->sld || ht->anm));
}

static bool hostShouldDrawLabel(host_type *ht)
{
  if (!ht) return false;
  if (!(hostIsKnownNetpos(ht) || ht->pip || (setts.sips == all) || ((setts.sips == sel) && ht->sld))) return false;
  if ((setts.sipn == nms) && !hostIsKnownNetpos(ht) && !*ht->htnm) return false;
  return true;
}

static const char *hostLabelText(host_type *ht)
{
  if (!ht) return "";
  if (*ht->htnm && (setts.sipn != ips)) return ht->htnm;
  return ht->htip;
}

static unsigned short packetImportantPort(const pkif_type *pkt)
{
  if (!pkt || ((pkt->pr != IPPROTO_TCP) && (pkt->pr != IPPROTO_UDP))) return 0;
  if (!pkt->srcpt) return pkt->dstpt;
  if (!pkt->dstpt) return pkt->srcpt;
  if ((pkt->srcpt >= 49152) && (pkt->dstpt < 49152)) return pkt->dstpt;
  if ((pkt->dstpt >= 49152) && (pkt->srcpt < 49152)) return pkt->srcpt;
  return (pkt->srcpt < pkt->dstpt ? pkt->srcpt : pkt->dstpt);
}

static void hostRuntimeNotePacket(host_type *ht, const pkex_type *pkex, unsigned short port)
{
  host_runtime_meta_type *meta = hostRuntimeMeta(ht);
  if (!meta || !pkex) return;
  meta->pr = pkex->pk.pr;
  meta->shp = packetShape(pkex) + 1;
  meta->clr = packetColorIndex(pkex) + 1;
  meta->port = port;
}

static void hostRuntimeNoteDiscoveryName(host_type *ht, const char *name)
{
  host_runtime_meta_type *meta = hostRuntimeMeta(ht);
  if (!meta || !name || !*name) return;
  strncpy(meta->dnm, name, sizeof(meta->dnm) - 1);
  meta->dnm[sizeof(meta->dnm) - 1] = '\0';
}

static bool parseNetposExactIpToken(const char *token, in_addr *ip)
{
  char buf[32], *slash;
  unsigned long addr;
  if (!token || !*token || !ip) return false;
  strncpy(buf, token, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  slash = strchr(buf, '/');
  if (!slash || strcmp(slash, "/32")) return false;
  *slash = '\0';
  if (!*buf) return false;
  addr = inet_addr(buf);
  if ((addr == INADDR_NONE) && strcmp(buf, "255.255.255.255")) return false;
  ip->s_addr = addr;
  return true;
}

static bool hostApplyNetposLayout(host_type *ht)
{
  int oldx, oldy, oldz, hnet;
  unsigned char oldclr;
  if (!ht) return false;
  oldx = ht->px;
  oldy = ht->py;
  oldz = ht->pz;
  oldclr = ht->clr;
  hostByPositionPreMove(ht);
  hnet = hostNet(ht);
  if (!hnet)
  {
    hostByPositionPostMove(ht);
    return false;
  }
  if (hnet == 1) hostPos(ht, HPR, 1);
  hostByPositionPostMove(ht);
  moveCollision(ht);
  return ((oldx != ht->px) || (oldy != ht->py) || (oldz != ht->pz) || (oldclr != ht->clr));
}

static void netposExactHostsSync()
{
  std::map<unsigned long, bool>::iterator mit;
  std::map<unsigned long, bool> exactHosts;
  FILE *npos;
  char line[512], net[19];
  unsigned int exactCount = 0;
  bool changed = false;
  bool pausedHosts = false;

  if (!goHosts)
  {
    goHosts = 1;
    while (goHosts == 1) usleep(1000);
    pausedHosts = true;
  }

  for (std::map<unsigned long, host_runtime_meta_type>::iterator it = hostRuntimeMetaByIp.begin(); it != hostRuntimeMetaByIp.end(); ++it)
    it->second.knownNetpos = false;

  if ((npos = fopen(hsddata("netpos.txt"), "r")))
  {
    while (fgets(line, sizeof(line), npos))
    {
      in_addr ip;
      if ((sscanf(line, "pos %18s", net) == 1) && parseNetposExactIpToken(net, &ip))
        exactHosts[(unsigned long)ip.s_addr] = true;
    }
    fclose(npos);
  }

  for (mit = exactHosts.begin(); mit != exactHosts.end(); ++mit)
  {
    host_type *ht;
    in_addr ip;
    ip.s_addr = (in_addr_t)mit->first;
    ht = hostIP(ip, false);
    if (!ht)
    {
      ht = hostCreate(ip, false);
      changed = true;
    }
    else
    {
      if (hostIsDynamic(ht))
      {
        hostPromoteStatic(ht);
        changed = true;
      }
      if (hostApplyNetposLayout(ht)) changed = true;
    }
    hostRuntimeMeta(ht)->knownNetpos = true;
    if (seltd == ht) hostDetails();
    exactCount++;
  }

  if (knownNetposExactHostCount != exactCount)
  {
    knownNetposExactHostCount = exactCount;
    changed = true;
  }

  if (changed)
  {
    refresh = true;
    osdUpdate();
  }

  if (pausedHosts) goHosts = 0;
}

static void hostPromoteStatic(host_type *ht)
{
  hostSetDynamic(ht, false);
}

static bool hostShouldPersist(host_type *ht)
{
  return (ht && (!hostIsDynamic(ht) || ht->lck));
}

static bool hostResolveName(host_type *ht)
{
  hostent *resolved;
  if (!ht) return false;
  resolved = gethostbyaddr((const char *)&ht->hip, sizeof(ht->hip), AF_INET);
  if (!resolved || !resolved->h_name || !*resolved->h_name) return false;
  strncpy(ht->htnm, resolved->h_name, sizeof(ht->htnm) - 1);
  ht->htnm[sizeof(ht->htnm) - 1] = '\0';
  hostPromoteStatic(ht);
  if (seltd == ht) hostDetails();
  refresh = true;
  return true;
}

static void hostCollisionDetach(host_type *cht)
{
  host_type *ht;
  if (!cht || !(ht = cht->col)) return;
  if (ht->col == cht) ht->col = cht->col;
  else
  {
    while (ht->col != cht) ht = ht->col;
    ht->col = cht->col;
  }
  cht->col = 0;
}

//create host
host_type *hostCreate(in_addr ip, bool dynamic = true)
{
  host_type host = {0, 0, 0, 0, 0, setts.anm, setts.nhp, 0, setts.nhl, 0, SPC, 0, SPC, 0, 0, 0, 0, ip, "", "", "", ""}, *ht;
  strcpy(host.htip, inet_ntoa(ip));
  for (unsigned char cnt = 0; cnt < SVCS; cnt++) host.svc[cnt] = -1;

  ht = new host_type(host);

  int hnet = hostNet(ht);
  if (hnet != 2) hostPos(ht, HPR, 1);

  newHostByIp(ht);
  hostSetDynamic(ht, dynamic);

  if (hnet == 2) moveCollision(ht);

  hostByPositionPostMove(ht);

  refresh = true;
  return ht;
}

//create/delete link
void linkCreDel(host_type *sht, host_type *dht, unsigned char sp, bool dl = false)
{
  link_type *lk;
  lnksLL.Start(2);
  while ((lk = (link_type *)lnksLL.Read(2)))
  {
    if (((lk->sht == sht) && (lk->dht == dht)) || ((lk->sht == dht) && (lk->dht == sht)) || (!dht && ((lk->sht == sht) || (lk->dht == sht))))
    {
      if (dl)
      {
        delete lk;
        lk = 0;
        lnksLL.Delete(2);
      }
      if (dht) return;
    }
    if (lk) lnksLL.Next(2);
  }
  if (!dht) return;
  link_type link = {sht, dht, sp};
  lnksLL.Write(new link_type(link));
}

//create alert
void alrtCreate(aobj_type ao, unsigned char pr, host_type *ht)
{
  if (ao)
  {
    alrt_type *al;
    altsLL.End(1);
    while ((al = (alrt_type *)altsLL.Read(1)))
    {
      if (al->ao == actv)
      {
        if (al->frm != frame) break;
        if (al->ht == ht) return;
      }
      else if ((ao == text) && (al->ht == ht))
      {
        al->pr = pr;
        al->sz = 7;
        return;
      }
      altsLL.Prev(1);
    }
  }
  alrt_type alrt = {ao, pr, 7, frame, ht};
  altsLL.Write(new alrt_type(alrt));
}

static unsigned char packetArpSubtype(const pkex_type *pkex)
{
  if (!pkex || (pkex->pk.pr != IPPROTO_ARP)) return 0;
  return pkex->spr;
}

static bool packetFilterAllActive()
{
  return (!setts.sen && !setts.prt && (packetTreeFilter == pfAll));
}

static bool packetTreeFilterActive(unsigned char filter)
{
  return !setts.sen && !setts.prt && (packetTreeFilter == filter);
}

static void packetFilterResetAll()
{
  setts.sen = 0;
  setts.pr = 0;
  setts.psf = 0;
  setts.ptf = 0;
  setts.prt = 0;
  packetTreeFilter = pfAll;
}

static void packetFilterSetSensor(unsigned char sen)
{
  if ((setts.sen == sen) && (packetTreeFilter == pfAll) && !setts.prt) packetFilterResetAll();
  else
  {
    packetFilterResetAll();
    setts.sen = sen;
  }
}

static void packetFilterSetTree(unsigned char filter)
{
  if ((packetTreeFilter == filter) && !setts.sen && !setts.prt) packetFilterResetAll();
  else
  {
    packetFilterResetAll();
    packetTreeFilter = filter;
  }
}

static void packetFilterSetPort(unsigned short port)
{
  if ((setts.prt == port) && (packetTreeFilter == pfAll) && !setts.sen) packetFilterResetAll();
  else
  {
    packetFilterResetAll();
    setts.prt = port;
  }
}

static void osdRowHitsClear()
{
  osdRowHitCount = 0;
}

static void osdRowHitAdd(int left, int top, int right, int bottom, unsigned char action)
{
  if (!action || (osdRowHitCount >= OSD_MAX_ROWS)) return;
  osdRowHits[osdRowHitCount].left = left;
  osdRowHits[osdRowHitCount].top = top;
  osdRowHits[osdRowHitCount].right = right;
  osdRowHits[osdRowHitCount].bottom = bottom;
  osdRowHits[osdRowHitCount].action = action;
  osdRowHitCount++;
}

static void osdCycleSensorFilter()
{
  if (setts.sen && (setts.sen < 9)) packetFilterSetSensor(setts.sen + 1);
  else if (setts.sen == 9) packetFilterResetAll();
  else packetFilterSetSensor(1);
  pcktsDestroy(true);
}

static void osdCyclePacketFilter()
{
  unsigned int count = 0;
  const packet_tree_row_type *rows = packetTreeRows(&count);
  unsigned int idx = 0;

  if (!rows || !count) return;
  if (!setts.sen && !setts.prt)
  {
    for (idx = 0; idx < count; idx++)
      if (rows[idx].filter == packetTreeFilter) break;
    if (idx >= count) idx = 0;
    idx = (idx + 1) % count;
  }
  else idx = 1;  // leave "all" and move directly to the first real tree filter

  if (!idx) packetFilterResetAll();
  else packetFilterSetTree(rows[idx].filter);
  pcktsDestroy(true);
}

static void osdCyclePortFilter()
{
  static const unsigned short ports[] = {0, 53, 80, 137, 443, 5353, 5355};
  unsigned int idx = 0;
  for (; idx < (sizeof(ports) / sizeof(ports[0])); idx++)
    if (setts.prt == ports[idx]) break;
  if (idx >= (sizeof(ports) / sizeof(ports[0]))) idx = 0;
  idx = (idx + 1) % (sizeof(ports) / sizeof(ports[0]));
  if (!ports[idx]) packetFilterResetAll();
  else packetFilterSetPort(ports[idx]);
  pcktsDestroy(true);
}

static void osdCycleDisplayMode()
{
  if (setts.sipn == ips) setts.sipn = ins;
  else if (setts.sipn == ins) setts.sipn = nms;
  else setts.sipn = ips;
}

static unsigned char osdDisplayScopeIndex()
{
  if (setts.sona == ipn) return 3;
  if (setts.sips == sel) return 1;
  if (setts.sips == all) return 2;
  return 0;
}

static void osdCycleDisplayScope()
{
  unsigned char next = (osdDisplayScopeIndex() + 1) % 4;
  if (next == 0)
  {
    setts.sips = off;
    if (setts.sona == ipn) setts.sona = don;
  }
  else if (next == 1)
  {
    setts.sips = sel;
    if (setts.sona == ipn) setts.sona = don;
  }
  else if (next == 2)
  {
    setts.sips = all;
    if (setts.sona == ipn) setts.sona = don;
  }
  else setts.sona = ipn;
}

static void osdCycleOnActiveAction()
{
  if (setts.sona == don) setts.sona = alt;
  else if (setts.sona == alt) setts.sona = ipn;
  else if (setts.sona == ipn) setts.sona = hst;
  else if (setts.sona == hst) setts.sona = slt;
  else setts.sona = don;
}

static void osdCyclePacketLimit()
{
  static const unsigned int limits[] = {1000, 5000, 10000, 25000, 50000, 100000, 250000, 500000, 1000000};
  unsigned int idx = 0;
  for (; idx < (sizeof(limits) / sizeof(limits[0])); idx++)
    if (setts.pks == limits[idx]) break;
  if (idx >= (sizeof(limits) / sizeof(limits[0]))) idx = 0;
  else idx = (idx + 1) % (sizeof(limits) / sizeof(limits[0]));
  setts.pks = limits[idx];
  pcktsDestroy(true);
}

static bool osdRowHitProcess(int x, int y)
{
  for (unsigned int cnt = 0; cnt < osdRowHitCount; cnt++)
  {
    osd_row_hit_type *hit = &osdRowHits[cnt];
    if ((x < hit->left) || (x > hit->right) || (y > hit->top) || (y < hit->bottom)) continue;
    switch (hit->action)
    {
      case osaSensorFilter: osdCycleSensorFilter(); break;
      case osaPacketFilter: osdCyclePacketFilter(); break;
      case osaPortFilter: osdCyclePortFilter(); break;
      case osaDisplayMode: osdCycleDisplayMode(); break;
      case osaDisplayScope: osdCycleDisplayScope(); break;
      case osaOnActiveAction: osdCycleOnActiveAction(); break;
      case osaAnomalyDetection:
        setts.anm = !setts.anm;
        dAnom = setts.anm;
        break;
      case osaFastPackets: setts.fsp = !setts.fsp; break;
      case osaAddDestinationHosts:
        setts.adh = !setts.adh;
        if (setts.adh && setts.bct) setts.bct = 0;
        break;
      case osaShowBroadcastHosts:
        setts.bct = !setts.bct;
        if (setts.bct && setts.adh) setts.adh = 0;
        break;
      case osaNewHostPackets: setts.nhp = !setts.nhp; break;
      case osaNewHostLinks: setts.nhl = !setts.nhl; break;
      case osaShowPacketDestPort: setts.pdp = !setts.pdp; break;
      case osaPacketLimit: osdCyclePacketLimit(); break;
      case osaPacketAnimation: goAnim = !goAnim; break;
      case osaAcknowledgeAnomalies:
        hostsSet(2, 0);
        dAnom = false;
        alrtsDestroy();
        break;
      default:
        return false;
    }
    refresh = true;
    osdUpdate();
    return true;
  }
  return false;
}

static const char *packetShapeFilterLabel(unsigned char psf)
{
  switch (psf)
  {
    case pCube + 1: return "Cube";
    case pDouble + 1: return "Double";
    case pTriple + 1: return "Triple";
    case pSphere + 1: return "Sphere";
    case pPyramid + 1: return "Pyramid";
    default: return "All";
  }
}

static bool parsePacketShapeFilterValue(const char *value, unsigned char *out)
{
  if (!value || !out) return false;
  if (!strcmp(value, "all")) *out = 0;
  else if (!strcmp(value, "cube")) *out = pCube + 1;
  else if (!strcmp(value, "double")) *out = pDouble + 1;
  else if (!strcmp(value, "triple")) *out = pTriple + 1;
  else if (!strcmp(value, "sphere")) *out = pSphere + 1;
  else if (!strcmp(value, "pyramid")) *out = pPyramid + 1;
  else return false;
  return true;
}

static bool parsePacketTcpFilterValue(const char *value, unsigned char *out)
{
  if (!value || !out) return false;
  if (!strcmp(value, "all")) *out = 0;
  else if (!strcmp(value, "setup_finish")) *out = 1;
  else if (!strcmp(value, "reset")) *out = 2;
  else return false;
  return true;
}

static const packet_tree_row_type *packetTreeRows(unsigned int *count)
{
  static const packet_tree_row_type rows[] =
  {
    {pfAll,               0, 0, pDouble,  "All Traffic"},
    {pfTcpAll,            0, 2, pDouble,  "TCP"},
    {pfTcpControl,        1, 2, pCube,    "control / no payload"},
    {pfTcpSetupFinish,    1, 5, pCube,    "setup / finish"},
    {pfTcpReset,          1, 1, pCube,    "reset"},
    {pfTcpPayload,        1, 2, pDouble,  "payload"},
    {pfTcpLargePayload,   1, 2, pTriple,  "larger payload"},
    {pfTcpDiscovery,      1, 2, pSphere,  "discovery"},
    {pfUdpAll,            0, 3, pDouble,  "UDP"},
    {pfUdpPayload,        1, 3, pDouble,  "payload"},
    {pfUdpLargePayload,   1, 3, pTriple,  "larger payload"},
    {pfUdpDiscovery,      1, 3, pSphere,  "discovery"},
    {pfIcmpAll,           0, 1, pPyramid, "ICMP"},
    {pfArpAll,            0, 4, pCube,    "ARP"},
    {pfArpRequest,        1, 4, pCube,    "request"},
    {pfArpReply,          1, 4, pDouble,  "reply"},
    {pfArpGratuitous,     1, 4, pSphere,  "gratuitous"},
    {pfOtherAll,          0, 0, pCube,    "OTHER"},
    {pfOtherFragmented,   1, 0, pPyramid, "fragmented"}
  };
  if (count) *count = sizeof(rows) / sizeof(rows[0]);
  return rows;
}

static const char *packetTreeFilterLabel(unsigned char filter)
{
  switch (filter)
  {
    case pfTcpAll: return "TCP";
    case pfTcpControl: return "TCP control / no payload";
    case pfTcpSetupFinish: return "TCP setup / finish";
    case pfTcpReset: return "TCP reset";
    case pfTcpPayload: return "TCP payload";
    case pfTcpLargePayload: return "TCP larger payload";
    case pfTcpDiscovery: return "TCP discovery";
    case pfUdpAll: return "UDP";
    case pfUdpPayload: return "UDP payload";
    case pfUdpLargePayload: return "UDP larger payload";
    case pfUdpDiscovery: return "UDP discovery";
    case pfIcmpAll: return "ICMP";
    case pfArpAll: return "ARP";
    case pfArpRequest: return "ARP request";
    case pfArpReply: return "ARP reply";
    case pfArpGratuitous: return "ARP gratuitous";
    case pfOtherAll: return "OTHER";
    case pfOtherFragmented: return "OTHER fragmented";
    default: return "All Traffic";
  }
}

static const char *packetTreeFilterString(unsigned char filter)
{
  switch (filter)
  {
    case pfTcpAll: return "tcp";
    case pfTcpControl: return "tcp_control";
    case pfTcpSetupFinish: return "tcp_setup_finish";
    case pfTcpReset: return "tcp_reset";
    case pfTcpPayload: return "tcp_payload";
    case pfTcpLargePayload: return "tcp_large_payload";
    case pfTcpDiscovery: return "tcp_discovery";
    case pfUdpAll: return "udp";
    case pfUdpPayload: return "udp_payload";
    case pfUdpLargePayload: return "udp_large_payload";
    case pfUdpDiscovery: return "udp_discovery";
    case pfIcmpAll: return "icmp";
    case pfArpAll: return "arp";
    case pfArpRequest: return "arp_request";
    case pfArpReply: return "arp_reply";
    case pfArpGratuitous: return "arp_gratuitous";
    case pfOtherAll: return "other";
    case pfOtherFragmented: return "other_fragmented";
    default: return "all";
  }
}

static bool parsePacketTreeFilterValue(const char *value, unsigned char *out)
{
  if (!value || !out) return false;
  if (!strcmp(value, "all")) *out = pfAll;
  else if (!strcmp(value, "tcp")) *out = pfTcpAll;
  else if (!strcmp(value, "tcp_control")) *out = pfTcpControl;
  else if (!strcmp(value, "tcp_setup_finish")) *out = pfTcpSetupFinish;
  else if (!strcmp(value, "tcp_reset")) *out = pfTcpReset;
  else if (!strcmp(value, "tcp_payload")) *out = pfTcpPayload;
  else if (!strcmp(value, "tcp_large_payload")) *out = pfTcpLargePayload;
  else if (!strcmp(value, "tcp_discovery")) *out = pfTcpDiscovery;
  else if (!strcmp(value, "udp")) *out = pfUdpAll;
  else if (!strcmp(value, "udp_payload")) *out = pfUdpPayload;
  else if (!strcmp(value, "udp_large_payload")) *out = pfUdpLargePayload;
  else if (!strcmp(value, "udp_discovery")) *out = pfUdpDiscovery;
  else if (!strcmp(value, "icmp")) *out = pfIcmpAll;
  else if (!strcmp(value, "arp")) *out = pfArpAll;
  else if (!strcmp(value, "arp_request")) *out = pfArpRequest;
  else if (!strcmp(value, "arp_reply")) *out = pfArpReply;
  else if (!strcmp(value, "arp_gratuitous")) *out = pfArpGratuitous;
  else if (!strcmp(value, "other")) *out = pfOtherAll;
  else if (!strcmp(value, "other_fragmented")) *out = pfOtherFragmented;
  else return false;
  return true;
}

static bool packetTreeMatches(const pkex_type *pkex, unsigned char filter)
{
  const pkif_type *pkt;
  bool discovery;
  if (!pkex || (filter == pfAll)) return true;
  pkt = &pkex->pk;
  discovery = packetIsNameDiscovery(pkt);
  switch (filter)
  {
    case pfTcpAll: return (pkt->pr == IPPROTO_TCP);
    case pfTcpControl: return (pkt->pr == IPPROTO_TCP) && !discovery && !pkex->pld;
    case pfTcpSetupFinish: return (pkt->pr == IPPROTO_TCP) && !discovery && !pkex->pld && (pkex->syn || pkex->fin) && !pkex->rst;
    case pfTcpReset: return (pkt->pr == IPPROTO_TCP) && !discovery && !pkex->pld && (pkex->rst != 0);
    case pfTcpPayload: return (pkt->pr == IPPROTO_TCP) && !discovery && pkex->pld && (pkex->sz < PACKET_TRIPLE_SIZE_THRESHOLD);
    case pfTcpLargePayload: return (pkt->pr == IPPROTO_TCP) && !discovery && pkex->pld && (pkex->sz >= PACKET_TRIPLE_SIZE_THRESHOLD);
    case pfTcpDiscovery: return (pkt->pr == IPPROTO_TCP) && discovery;
    case pfUdpAll: return (pkt->pr == IPPROTO_UDP);
    case pfUdpPayload: return (pkt->pr == IPPROTO_UDP) && !discovery && pkex->pld && (pkex->sz < PACKET_TRIPLE_SIZE_THRESHOLD);
    case pfUdpLargePayload: return (pkt->pr == IPPROTO_UDP) && !discovery && pkex->pld && (pkex->sz >= PACKET_TRIPLE_SIZE_THRESHOLD);
    case pfUdpDiscovery: return (pkt->pr == IPPROTO_UDP) && discovery;
    case pfIcmpAll: return (pkt->pr == IPPROTO_ICMP);
    case pfArpAll: return (pkt->pr == IPPROTO_ARP);
    case pfArpRequest: return (pkt->pr == IPPROTO_ARP) && (packetArpSubtype(pkex) == 1);
    case pfArpReply: return (pkt->pr == IPPROTO_ARP) && (packetArpSubtype(pkex) == 2);
    case pfArpGratuitous: return (pkt->pr == IPPROTO_ARP) && (packetArpSubtype(pkex) == 3);
    case pfOtherAll: return (pkt->pr != IPPROTO_ICMP) && (pkt->pr != IPPROTO_TCP) && (pkt->pr != IPPROTO_UDP) && (pkt->pr != IPPROTO_ARP);
    case pfOtherFragmented: return (pkt->pr == IPPROTO_FRAG);
    default: return true;
  }
}

static void packetTreeFilterMigrateLegacy()
{
  if (packetTreeFilter != pfAll) return;
  if (setts.ptf == 1) packetTreeFilter = pfTcpSetupFinish;
  else if (setts.ptf == 2) packetTreeFilter = pfTcpReset;
  else if (setts.pr == IPPROTO_TCP)
  {
    if (setts.psf == (pCube + 1)) packetTreeFilter = pfTcpControl;
    else if (setts.psf == (pTriple + 1)) packetTreeFilter = pfTcpLargePayload;
    else if (setts.psf == (pSphere + 1)) packetTreeFilter = pfTcpDiscovery;
    else if (setts.psf == (pDouble + 1)) packetTreeFilter = pfTcpPayload;
    else packetTreeFilter = pfTcpAll;
  }
  else if (setts.pr == IPPROTO_UDP)
  {
    if (setts.psf == (pTriple + 1)) packetTreeFilter = pfUdpLargePayload;
    else if (setts.psf == (pSphere + 1)) packetTreeFilter = pfUdpDiscovery;
    else if (setts.psf == (pDouble + 1)) packetTreeFilter = pfUdpPayload;
    else packetTreeFilter = pfUdpAll;
  }
  else if (setts.pr == IPPROTO_ICMP) packetTreeFilter = pfIcmpAll;
  else if (setts.pr == IPPROTO_ARP) packetTreeFilter = pfArpAll;
  else if (setts.pr == IPPROTO_OTHER)
  {
    if (setts.psf == (pPyramid + 1)) packetTreeFilter = pfOtherFragmented;
    else packetTreeFilter = pfOtherAll;
  }
  setts.pr = 0;
  setts.psf = 0;
  setts.ptf = 0;
}

static unsigned char packetColorIndex(const pkex_type *pkex)
{
  unsigned char pr;
  if (!pkex) return 0;
  pr = pkex->pk.pr;
  if ((pr == IPPROTO_TCP) && !pkex->pld)
  {
    if (pkex->rst) return 1;
    if (pkex->syn || pkex->fin) return 5;
    return 2;
  }
  switch (pr)
  {
    case IPPROTO_ICMP: return 1;
    case IPPROTO_TCP: return 2;
    case IPPROTO_UDP: return 3;
    case IPPROTO_ARP: return 4;
    default: return 0;
  }
}

static bool packetIsNameDiscovery(const pkif_type *pkt)
{
  if ((pkt->pr != IPPROTO_TCP) && (pkt->pr != IPPROTO_UDP)) return false;
  return (pkt->srcpt == 53) || (pkt->dstpt == 53) || (pkt->srcpt == 137) || (pkt->dstpt == 137)
    || (pkt->srcpt == 5353) || (pkt->dstpt == 5353) || (pkt->srcpt == 5355) || (pkt->dstpt == 5355);
}

static unsigned char packetShape(const pkex_type *pkex)
{
  const pkif_type *pkt = &pkex->pk;
  if (packetIsNameDiscovery(pkt)) return pSphere;
  if ((pkt->pr == IPPROTO_ICMP) || (pkt->pr == IPPROTO_FRAG)) return pPyramid;
  if (pkt->pr == IPPROTO_ARP)
  {
    if (packetArpSubtype(pkex) == 2) return pDouble;
    if (packetArpSubtype(pkex) == 3) return pSphere;
    return pCube;
  }
  if ((pkt->pr == IPPROTO_TCP) || (pkt->pr == IPPROTO_UDP))
  {
    if (!pkex->pld) return pCube;
    return (pkex->sz >= PACKET_TRIPLE_SIZE_THRESHOLD ? pTriple : pDouble);
  }
  return pCube;
}

static int packetListIndex(unsigned char colorIndex, unsigned char shape)
{
  return PACKET_LIST_BASE + (colorIndex * PACKET_SHAPE_COUNT) + shape;
}

//create packet
void pcktCreate(pkex_type *pkex, host_type *sht, host_type *dht, bool lk)
{
  pkif_type *pkt = &pkex->pk;
  unsigned char shape = packetShape(pkex), color = packetColorIndex(pkex);
  if ((pktsLL.Num() < setts.pks) && (sht->shp || dht->shp) && (!setts.sen || (pkt->sen == setts.sen))
    && (!setts.prt || (pkt->srcpt == setts.prt) || (pkt->dstpt == setts.prt))
    && packetTreeMatches(pkex, packetTreeFilter) && goAnim)
  {
    pckt_type pckt = {pkt->pr, shape, color, pkt->dstpt, frame, {(double)sht->px, (double)sht->py, (double)sht->pz}, dht}, *pk;
    pktsLL.End(2);
    while ((pk = (pckt_type *)pktsLL.Read(2)))
    {
      if (pk->frm != frame) break;
      if ((pk->cu.x == pckt.cu.x) && (pk->cu.y == pckt.cu.y) && (pk->cu.z == pckt.cu.z)
        && (pk->ht == pckt.ht) && (pk->pr == pckt.pr) && (pk->shp == pckt.shp) && (pk->clr == pckt.clr) && (pk->dstpt == pckt.dstpt)) return;
      pktsLL.Prev(2);
    }
    pktsLL.Write(new pckt_type(pckt));
    if (lk && (sht->alk || dht->alk)) linkCreDel(sht, dht, 0);
    if (setts.sona == hst)
    {
      sht->vis = 254;
      setts.mvd = 255;
    }
    else if (setts.sona == slt) sht->sld = 1;
  }
}

void hostDestroyCb(void **data, long arg1, long arg2, long arg3, long arg4) {
  host_type *ht = *((host_type **) data);
  hostDynamicStateByIp.erase(hostDynamicStateKey(ht->hip));
  hostRuntimeMetaByIp.erase(hostDynamicStateKey(ht->hip));
  delete ht;
  *data = 0;
}

//destroy hosts LL
void hostsDestroy()
{
  hstsByIp.forEach(1, hostDestroyCb, 0, 0, 0, 0);
  hstsByIp.destroy();
  hstsByPos.destroy();
  hostDynamicStateByIp.clear();
}

//destroy links LL
void linksDestroy()
{
  lnkht = 0;
  link_type *lk;
  lnksLL.Start(1);
  while ((lk = (link_type *)lnksLL.Read(1)))
  {
    delete lk;
    lnksLL.Next(1);
  }
  lnksLL.Destroy();
}

//destroy alerts LL
void alrtsDestroy()
{
  alrt_type *al;
  altsLL.Start(1);
  while ((al = (alrt_type *)altsLL.Read(1)))
  {
    delete al;
    altsLL.Next(1);
  }
  altsLL.Destroy();
}

static const char *osdOnOff(bool enabled)
{
  return (enabled ? "On" : "Off");
}

static const char *sipnToLabel(sipn_type mode)
{
  switch (mode)
  {
  case ips: return "IP";
  case ins: return "IP + Name";
  default: return "Name Only";
  }
}

static const char *sipsToLabel(sips_type mode)
{
  switch (mode)
  {
  case sel: return "Selection";
  case all: return "All";
  default: return "Off";
  }
}

static const char *sonaToLabel(sona_type mode)
{
  switch (mode)
  {
  case alt: return "Alert";
  case ipn: return "Show IP + Name";
  case hst: return "Show Host";
  case slt: return "Select Host";
  default: return "Do Nothing";
  }
}

static const char *osdDisplayScopeLabel()
{
  return ((setts.sona == ipn) ? "On-Active" : sipsToLabel(setts.sips));
}

static void packetTrafficLabelSet(const char *path)
{
  const char *base = "traffic.hpt";
  if (path && *path)
  {
    base = path;
    for (const char *ptr = path; *ptr; ptr++)
    {
      if ((*ptr == '\\') || (*ptr == '/')) base = ptr + 1;
    }
    if (!*base) base = "traffic.hpt";
  }
  strncpy(packetTrafficLabel, base, sizeof(packetTrafficLabel) - 1);
  packetTrafficLabel[sizeof(packetTrafficLabel) - 1] = '\0';
}

static const char *packetTrafficPathGet()
{
  if (!*packetTrafficPath) packetTrafficPathSet(hsddata("traffic.hpt"));
  return packetTrafficPath;
}

static void packetTrafficPathSet(const char *path)
{
  if (path && *path) setStringValue(packetTrafficPath, sizeof(packetTrafficPath), path);
  else setStringValue(packetTrafficPath, sizeof(packetTrafficPath), hsddata("traffic.hpt"));
  packetTrafficLabelSet(packetTrafficPath);
}

static bool packetTrafficRecordPathNext(char *path, size_t pathsz, char *label, size_t labelsz)
{
  char name[64];
  setStringValue(path, pathsz, hsddata("traffic.hpt"));
  if (!fileExists(path))
  {
    setStringValue(label, labelsz, "traffic.hpt");
    return true;
  }
  for (unsigned int cnt = 1; cnt < 10000; cnt++)
  {
    snprintf(name, sizeof(name), "traffic-%03u.hpt", cnt);
    setStringValue(path, pathsz, hsddata(name));
    if (!fileExists(path))
    {
      setStringValue(label, labelsz, name);
      return true;
    }
  }
  path[0] = '\0';
  label[0] = '\0';
  return false;
}

static void packetTrafficResolvePath(const char *name, char *path, size_t pathsz)
{
  if (name && *name && (strchr(name, '\\') || strchr(name, '/') || strchr(name, ':'))) setStringValue(path, pathsz, name);
  else if (name && *name) setStringValue(path, pathsz, hsddata(name));
  else setStringValue(path, pathsz, hsddata("traffic.hpt"));
}

static unsigned int packetTrafficReplayListCreate(const char *fl, char *firstName, size_t firstNameSz)
{
  std::vector<std::string> files;
  FILE *flist = fopen(fl, "w");
  if (firstNameSz) firstName[0] = '\0';
  if (!flist) return 0;
#ifdef __MINGW32__
  char mask[512];
  WIN32_FIND_DATA nlist;
  setStringValue(mask, sizeof(mask), hsddata("*.hpt"));
  HANDLE nl = FindFirstFile(mask, &nlist);
  if (nl != INVALID_HANDLE_VALUE)
  {
    do
    {
      if ((nlist.cFileName[0] != '.') && !(nlist.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(nlist.cFileName);
    } while (FindNextFile(nl, &nlist));
    FindClose(nl);
  }
#else
  DIR *dir = opendir(HSDDATA);
  if (dir)
  {
    dirent *ent;
    while ((ent = readdir(dir)))
    {
      std::string name = ent->d_name;
      if ((name.size() > 4) && (name[0] != '.') && (name.rfind(".hpt") == (name.size() - 4))) files.push_back(name);
    }
    closedir(dir);
  }
#endif
  std::sort(files.begin(), files.end());
  for (size_t cnt = 0; cnt < files.size(); cnt++)
  {
    fprintf(flist, "%s\n", files[cnt].c_str());
    if (!cnt && firstNameSz) setStringValue(firstName, firstNameSz, files[cnt].c_str());
  }
  fclose(flist);
  return (unsigned int)files.size();
}

static bool packetTrafficReplayStart(const char *path)
{
  if ((pfile = fopen(path, "rb")))
  {
    char hpt[4] = {0, 0, 0, 0};
    pkrLegacy = false;
    if (fread(hpt, 1, 3, pfile) == 3)
    {
      if (!strcmp(hpt, "HP2"))
      {
        if (fread(&pkrp, sizeof(pkrp), 1, pfile) == 1)
        {
          pcktsDestroy();
          packetTrafficPathSet(path);
          packetTrafficStarted = time(0);
          ptrc = rpy;
          offsetReset();
          goHosts = 0;
          return true;
        }
      }
      else if (!strcmp(hpt, "HPT"))
      {
        pkrc1_type pkr1;
        if (fread(&pkr1, sizeof(pkr1), 1, pfile) == 1)
        {
          memset(&pkrp, 0, sizeof(pkrp));
          pkrLegacy = true;
          pkrp.ptime = pkr1.ptime;
          pkrp.pk.id = 85;
          pkrp.pk.pk = pkr1.pk;
          pcktsDestroy();
          packetTrafficPathSet(path);
          packetTrafficStarted = time(0);
          ptrc = rpy;
          offsetReset();
          goHosts = 0;
          return true;
        }
      }
    }
    fclose(pfile);
  }
  return false;
}

static void packetTrafficDurationLabel(time_t started, char *buf, size_t bufsz)
{
  unsigned int hours, mins, secs;
  time_t elapsed = (started ? (time(0) - started) : 0);
  if (elapsed < 0) elapsed = 0;
  hours = (unsigned int)(elapsed / 3600);
  mins = (unsigned int)((elapsed % 3600) / 60);
  secs = (unsigned int)(elapsed % 60);
  snprintf(buf, bufsz, "%02u:%02u:%02u", hours, mins, secs);
}

static void osdAddSection(const char *label)
{
  osd_row_type *row;
  if (osdTextLineCount >= OSD_MAX_ROWS) return;
  row = &osdRows[osdTextLineCount++];
  memset(row, 0, sizeof(osd_row_type));
  row->section = true;
  strncpy(row->label, label, sizeof(row->label) - 1);
}

static void osdAddRow(const char *label, const char *value, osd_color_type valueColor = osdNormal, unsigned char action = osaNone)
{
  osd_row_type *row;
  if (osdTextLineCount >= OSD_MAX_ROWS) return;
  row = &osdRows[osdTextLineCount++];
  memset(row, 0, sizeof(osd_row_type));
  row->valueColor = valueColor;
  row->action = action;
  strncpy(row->label, label, sizeof(row->label) - 1);
  strncpy(row->value, value, sizeof(row->value) - 1);
}

static void osdValueColor(osd_color_type color)
{
  switch (color)
  {
  case osdAccent:
    glColor3ub(yellow[0], yellow[1], yellow[2]);
    break;
  case osdAlert:
    glColor3ub(brred[0], brred[1], brred[2]);
    break;
  default:
    glColor3ub(white[0], white[1], white[2]);
    break;
  }
}

static void osdBounds(int *left, int *top, int *right, int *bottom)
{
  *right = wWin - OSD_MARGIN_X;
  *left = *right - OSD_BOX_W;
  if (*left < 6)
  {
    *left = 6;
    *right = *left + OSD_BOX_W;
  }
  *top = hWin - OSD_MARGIN_Y + 5;
  *bottom = *top - ((int)osdTextLineCount * OSD_LINE_H) - (OSD_BOX_PAD_Y * 2) + 5;
}

static void drawOsdPanel()
{
  int left, top, right, bottom, labelX, valueX, y;
  osdBounds(&left, &top, &right, &bottom);
  osdRowHitsClear();
  labelX = left + OSD_BOX_PAD_X;
  valueX = left + OSD_VALUE_X;
  y = top - OSD_BOX_PAD_Y - 10;

  glColor3ub(15, 15, 20);
  glRecti(left, top, right, bottom);
  glColor3ub(dlgrey[0], dlgrey[1], dlgrey[2]);
  glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
  glEnd();
  glBegin(GL_LINES);
    glVertex2i(valueX - 8, top - 5);
    glVertex2i(valueX - 8, bottom + 5);
  glEnd();

  for (unsigned int cnt = 0; cnt < osdTextLineCount; cnt++)
  {
    osd_row_type *row = &osdRows[cnt];
    if (row->section)
    {
      int lineStart = labelX + ((int)strlen(row->label) * OSD_CHAR_W) + 8;
      glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
      glRasterPos2i(labelX, y);
      GLWin.DrawString((const unsigned char *)row->label);
      glColor3ub(dlgrey[0], dlgrey[1], dlgrey[2]);
      if (lineStart < (right - OSD_BOX_PAD_X))
      {
        glBegin(GL_LINES);
          glVertex2i(lineStart, y + 3);
          glVertex2i(right - OSD_BOX_PAD_X, y + 3);
        glEnd();
      }
    }
    else
    {
      if (row->action)
        osdRowHitAdd(left + 2, y + 5, right - 2, y - OSD_LINE_H + 4, row->action);
      glColor3ub(grey[0], grey[1], grey[2]);
      glRasterPos2i(labelX, y);
      GLWin.DrawString((const unsigned char *)row->label);
      osdValueColor(row->valueColor);
      glRasterPos2i(valueX, y);
      GLWin.DrawString((const unsigned char *)row->value);
    }
    y -= OSD_LINE_H;
  }
}

//update osd text
void osdUpdate()
{
  char sensorLabel[16], packetFilterLabel[32], portLabel[16], knownLabel[24];
  char packetLimitLabel[16], visiblePacketsLabel[16];
  if (setts.sen) snprintf(sensorLabel, sizeof(sensorLabel), "%u", setts.sen);
  else strcpy(sensorLabel, "All");
  strcpy(packetFilterLabel, packetTreeFilterLabel(packetTreeFilter));
  if (setts.prt) snprintf(portLabel, sizeof(portLabel), "%u", setts.prt);
  else strcpy(portLabel, "All");
  if (knownNetposExactHostCount) snprintf(knownLabel, sizeof(knownLabel), "Always (%u)", knownNetposExactHostCount);
  else strcpy(knownLabel, "None");
  snprintf(packetLimitLabel, sizeof(packetLimitLabel), "%7u", setts.pks);
  snprintf(visiblePacketsLabel, sizeof(visiblePacketsLabel), "%7u", (unsigned int)pktsLL.Num());
  osdTextLineCount = 0;
  osdAddSection("FILTERS");
  osdAddRow("Sensor Filter", sensorLabel, (setts.sen ? osdAccent : osdNormal), osaSensorFilter);
  osdAddRow("Packet Filter", packetFilterLabel, ((packetTreeFilter != pfAll) ? osdAccent : osdNormal), osaPacketFilter);
  osdAddRow("Port Filter", portLabel, (setts.prt ? osdAccent : osdNormal), osaPortFilter);
  osdAddSection("LABELS");
  osdAddRow("Display Mode", sipnToLabel(setts.sipn), osdNormal, osaDisplayMode);
  osdAddRow("Display Scope", osdDisplayScopeLabel(), ((setts.sips != off) || (setts.sona == ipn) ? osdAccent : osdNormal), osaDisplayScope);
  osdAddRow("Known NetPos /32", knownLabel, (knownNetposExactHostCount ? osdAccent : osdNormal));
  osdAddRow("On-Active Action", sonaToLabel(setts.sona), osdNormal, osaOnActiveAction);
  osdAddSection("PACKETS");
  osdAddRow("Anomaly Detection", osdOnOff(setts.anm), (setts.anm ? osdNormal : osdAccent), osaAnomalyDetection);
  osdAddRow("Fast Packets", osdOnOff(setts.fsp), (setts.fsp ? osdAccent : osdNormal), osaFastPackets);
  osdAddRow("Add Destination Hosts", osdOnOff(setts.adh), (setts.adh ? osdAccent : osdNormal), osaAddDestinationHosts);
  osdAddRow("Show Broadcast Hosts", osdOnOff(setts.bct), (setts.bct ? osdAccent : osdNormal), osaShowBroadcastHosts);
  osdAddRow("New Host Packets", osdOnOff(setts.nhp), (setts.nhp ? osdAccent : osdNormal), osaNewHostPackets);
  osdAddRow("New Host Links", osdOnOff(setts.nhl), (setts.nhl ? osdAccent : osdNormal), osaNewHostLinks);
  osdAddRow("Show Packet Dest Port", osdOnOff(setts.pdp), (setts.pdp ? osdAccent : osdNormal), osaShowPacketDestPort);
  osdAddRow("Packet Limit", packetLimitLabel, osdNormal, osaPacketLimit);
  osdAddSection("RUNTIME");
  osdAddRow("Packet Animation", (goAnim ? "Running" : "Paused"), (goAnim ? osdNormal : osdAccent), osaPacketAnimation);
  osdAddRow("Visible Packets", visiblePacketsLabel);
  osdAddRow("Unacked Anomalies", (dAnom ? "Y" : ""), (dAnom ? osdAlert : osdNormal), osaAcknowledgeAnomalies);
  if (lnkht) osdAddRow("Link Line", "Pending", osdAccent);
}

static void osdPacketHitsClear()
{
  osdPacketHitCount = 0;
}

static void osdPacketHitAdd(int left, int top, int right, int bottom, unsigned char filter)
{
  if (osdPacketHitCount >= OSD_PKT_HIT_MAX) return;
  osdPacketHits[osdPacketHitCount].left = left;
  osdPacketHits[osdPacketHitCount].top = top;
  osdPacketHits[osdPacketHitCount].right = right;
  osdPacketHits[osdPacketHitCount].bottom = bottom;
  osdPacketHits[osdPacketHitCount].filter = filter;
  osdPacketHits[osdPacketHitCount].active = true;
  osdPacketHitCount++;
}

static bool osdPacketHitProcess(int x, int y)
{
  for (unsigned int cnt = 0; cnt < osdPacketHitCount; cnt++)
  {
    osd_pkt_hit_type *hit = &osdPacketHits[cnt];
    if (!hit->active) continue;
    if ((x >= hit->left) && (x <= hit->right) && (y >= hit->bottom) && (y <= hit->top))
    {
      if (hit->filter == pfAll) packetFilterResetAll();
      else packetFilterSetTree(hit->filter);
      pcktsDestroy(true);
      osdUpdate();
      refresh = true;
      return true;
    }
  }
  return false;
}

static void osdDrawPacketMini(int cx, int cy, unsigned char colorIndex, unsigned char shape, double scale)
{
  glPushMatrix();
  glTranslated((double)cx, (double)cy, 0.0);
  glScaled(scale, scale, scale);
  glRotated(25.0, 1.0, 0.0, 0.0);
  glRotated(45.0, 0.0, 1.0, 0.0);
  glCallList(objsDraw + packetListIndex(colorIndex, shape));
  glPopMatrix();
}

static void osdDrawPktLegend(int px, int py)
{
  unsigned int rowsCount = 0;
  const packet_tree_row_type *rows = packetTreeRows(&rowsCount);
  int left = px, right = px + OSD_BOX_W, top, bottom;
  int listTop, iconX, rowTop, rowBottom, rowIconY, rowTextY;
  py -= (osdTextLineCount * OSD_LINE_H) + OSD_PKT_GAP_H;
  osdPacketHitsClear();
  top = py + 12;
  listTop = py - 12;
  iconX = left + OSD_BOX_PAD_X + 13;
  bottom = listTop - ((int)rowsCount * OSD_PKT_ROW_H) - 24;
  glColor3ub(15, 15, 20);
  glRecti(left, top, right, bottom);
  glColor3ub(dlgrey[0], dlgrey[1], dlgrey[2]);
  glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
  glEnd();
  glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
  glRasterPos2i(left + OSD_BOX_PAD_X, py + 2);
  GLWin.DrawString((const unsigned char *)"PACKET FILTERS");
  glBegin(GL_LINES);
    glVertex2i(left + 96, py + 5);
    glVertex2i(right - OSD_BOX_PAD_X, py + 5);
  glEnd();
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, (GLdouble)wWin, 0.0, (GLdouble)hWin, -128.0, 128.0);
  glMatrixMode(GL_MODELVIEW);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  for (unsigned int cnt = 0; cnt < rowsCount; cnt++)
  {
    int indent = rows[cnt].indent * OSD_PKT_TREE_INDENT_W;
    bool active = ((rows[cnt].filter == pfAll) ? packetFilterAllActive() : packetTreeFilterActive(rows[cnt].filter));
    rowTop = listTop - OSD_PKT_ROW_HILITE_TOP_Y - ((int)cnt * OSD_PKT_ROW_H);
    rowBottom = listTop - OSD_PKT_ROW_HILITE_BOTTOM_Y - ((int)cnt * OSD_PKT_ROW_H);
    rowIconY = listTop - OSD_PKT_ROW_ICON_Y - ((int)cnt * OSD_PKT_ROW_H);
    rowTextY = listTop - OSD_PKT_ROW_TEXT_Y - ((int)cnt * OSD_PKT_ROW_H);
    if (active)
    {
      glColor3ub(dlblue[0], dlblue[1], dlblue[2]);
      glRecti(left + OSD_BOX_PAD_X, rowTop, right - OSD_BOX_PAD_X, rowBottom);
      glColor3ub(yellow[0], yellow[1], yellow[2]);
      glBegin(GL_LINE_LOOP);
        glVertex2i(left + OSD_BOX_PAD_X, rowTop);
        glVertex2i(right - OSD_BOX_PAD_X, rowTop);
        glVertex2i(right - OSD_BOX_PAD_X, rowBottom);
        glVertex2i(left + OSD_BOX_PAD_X, rowBottom);
      glEnd();
    }
    if (rows[cnt].indent)
    {
      glColor3ub(dlgrey[0], dlgrey[1], dlgrey[2]);
      glBegin(GL_LINES);
        glVertex2i(left + OSD_BOX_PAD_X + 8, rowTextY - 3);
        glVertex2i(left + OSD_BOX_PAD_X + 8 + indent - 2, rowTextY - 3);
      glEnd();
    }
    osdPacketHitAdd(left + OSD_BOX_PAD_X, rowTop, right - OSD_BOX_PAD_X, rowBottom, rows[cnt].filter);
    osdDrawPacketMini(iconX + indent, rowIconY, rows[cnt].colorIndex, rows[cnt].shape, OSD_PKT_ICON_SCALE);
  }
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  for (unsigned int cnt = 0; cnt < rowsCount; cnt++)
  {
    int indent = rows[cnt].indent * OSD_PKT_TREE_INDENT_W;
    rowTextY = listTop - OSD_PKT_ROW_TEXT_Y - ((int)cnt * OSD_PKT_ROW_H);
    if (rows[cnt].indent) glColor3ub(white[0], white[1], white[2]);
    else glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
    glRasterPos2i(left + OSD_BOX_PAD_X + OSD_PKT_ICON_COL_W + indent, rowTextY);
    GLWin.DrawString((const unsigned char *)rows[cnt].label);
  }
}

static void drawPacketTrafficStatus()
{
  int left, bottom, top, right, y;
  char elapsed[16], replayTime[32];
  if (ptrc <= hlt) return;
  left = 6;
  bottom = 6;
  top = bottom + 72;
  if (ptrc == rpy) top += PACKET_STATUS_LINE_H;
  right = left + PACKET_STATUS_W;
  packetTrafficDurationLabel(packetTrafficStarted, elapsed, sizeof(elapsed));

  glColor3ub(15, 15, 20);
  glRecti(left, top, right, bottom);
  if (ptrc == rec) glColor3ub(red[0], red[1], red[2]);
  else glColor3ub(green[0], green[1], green[2]);
  glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
  glEnd();

  y = top - PACKET_STATUS_PAD_Y - 10;
  glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
  glRasterPos2i(left + PACKET_STATUS_PAD_X, y);
  GLWin.DrawString((const unsigned char *)"PACKET TRAFFIC");
  y -= PACKET_STATUS_LINE_H + 2;

  glColor3ub(white[0], white[1], white[2]);
  glRasterPos2i(left + PACKET_STATUS_PAD_X, y);
  GLWin.DrawString((const unsigned char *)(ptrc == rec ? "Mode: Recording" : "Mode: Replay"));
  y -= PACKET_STATUS_LINE_H;

  glRasterPos2i(left + PACKET_STATUS_PAD_X, y);
  snprintf(replayTime, sizeof(replayTime), "Elapsed: %s", elapsed);
  GLWin.DrawString((const unsigned char *)replayTime);
  y -= PACKET_STATUS_LINE_H;

  if (ptrc == rpy)
  {
    strftime(replayTime, sizeof(replayTime), "Packet Time: %d-%m-%y %H:%M:%S", localtime(&pkrp.ptime.tv_sec));
    glRasterPos2i(left + PACKET_STATUS_PAD_X, y);
    GLWin.DrawString((const unsigned char *)replayTime);
    y -= PACKET_STATUS_LINE_H;
  }

  snprintf(replayTime, sizeof(replayTime), "File: %.36s", packetTrafficLabel);
  glRasterPos2i(left + PACKET_STATUS_PAD_X, y);
  GLWin.DrawString((const unsigned char *)replayTime);
}

static void helpOverlayBounds(int *left, int *top, int *right, int *bottom)
{
  int width = HELP_OVERLAY_W;
  if (width > (wWin - (HELP_OVERLAY_MARGIN * 2))) width = wWin - (HELP_OVERLAY_MARGIN * 2);
  if (width < 220) width = 220;
  *left = HELP_OVERLAY_MARGIN;
  *right = *left + width;
  if (*right > (wWin - HELP_OVERLAY_MARGIN)) *right = wWin - HELP_OVERLAY_MARGIN;
  *top = hWin - HELP_OVERLAY_MARGIN;
  *bottom = HELP_OVERLAY_MARGIN;
}

static void helpOverlayClampScroll()
{
  int left, top, right, bottom, visible;
  helpOverlayBounds(&left, &top, &right, &bottom);
  visible = (top - bottom - HELP_OVERLAY_HEADER_H - HELP_OVERLAY_FOOTER_H) / OSD_LINE_H;
  if (visible < 1) visible = 1;
  if (helpOverlayLineCount <= (unsigned int) visible) helpOverlayScroll = 0;
  else if (helpOverlayScroll > (helpOverlayLineCount - (unsigned int) visible)) helpOverlayScroll = helpOverlayLineCount - (unsigned int) visible;
}

static void helpOverlayLoad()
{
  FILE *text;
  char line[256];
  helpOverlayLineCount = 0;
  helpOverlayScroll = 0;
  if (!(text = fopen(hsddata("controls.txt"), "r"))) return;
  while ((helpOverlayLineCount < HELP_OVERLAY_MAX_LINES) && fgets(line, sizeof(line), text))
  {
    char *key = line, *desc = 0, *end;
    help_overlay_line_type *row = &helpOverlayLines[helpOverlayLineCount];
    memset(row, 0, sizeof(help_overlay_line_type));
    end = line + strlen(line);
    while ((end > line) && ((end[-1] == '\n') || (end[-1] == '\r'))) *(--end) = '\0';
    desc = strchr(line, '\t');
    if (desc)
    {
      *desc = '\0';
      desc++;
    }
    else
    {
      key = (char *)"";
      desc = line;
    }
    snprintf(row->key, sizeof(row->key), "%s", key);
    snprintf(row->desc, sizeof(row->desc), "%s", desc);
    helpOverlayLineCount++;
  }
  fclose(text);
  helpOverlayClampScroll();
}

static void helpOverlayToggle()
{
  if (!helpOverlayVisible) helpOverlayLoad();
  helpOverlayVisible = !helpOverlayVisible;
  refresh = true;
}

static bool helpOverlayMouseOver()
{
  int left, top, right, bottom, gy = hWin - mPsy;
  if (!helpOverlayVisible) return false;
  helpOverlayBounds(&left, &top, &right, &bottom);
  return ((mPsx >= left) && (mPsx <= right) && (gy >= bottom) && (gy <= top));
}

static void helpOverlayScrollDelta(int delta)
{
  int left, top, right, bottom, visible;
  helpOverlayBounds(&left, &top, &right, &bottom);
  visible = (top - bottom - HELP_OVERLAY_HEADER_H - HELP_OVERLAY_FOOTER_H) / OSD_LINE_H;
  if ((visible < 1) || (helpOverlayLineCount <= (unsigned int) visible)) return;
  if ((delta < 0) && helpOverlayScroll) helpOverlayScroll--;
  else if ((delta > 0) && ((helpOverlayScroll + (unsigned int) visible) < helpOverlayLineCount)) helpOverlayScroll++;
}

static void helpOverlayTrimText(const char *src, char *dst, size_t dstsz, unsigned int maxChars)
{
  size_t len;
  if (!maxChars || !dstsz)
  {
    if (dstsz) *dst = '\0';
    return;
  }
  len = strlen(src);
  if (len <= maxChars)
  {
    strncpy(dst, src, dstsz - 1);
    dst[dstsz - 1] = '\0';
    return;
  }
  if (maxChars <= 3)
  {
    snprintf(dst, dstsz, "%.*s", (int) maxChars, src);
    return;
  }
  snprintf(dst, dstsz, "%.*s...", (int)(maxChars - 3), src);
}

static void drawHelpOverlay()
{
  int left, top, right, bottom, y, visible, row, keyChars, descChars;
  char keybuf[48], descbuf[160], footer[64];
  help_overlay_line_type *line;
  if (!helpOverlayVisible) return;
  helpOverlayClampScroll();
  helpOverlayBounds(&left, &top, &right, &bottom);
  visible = (top - bottom - HELP_OVERLAY_HEADER_H - HELP_OVERLAY_FOOTER_H) / OSD_LINE_H;
  if (visible < 1) visible = 1;
  keyChars = (HELP_OVERLAY_KEY_COL_W - 12) / 6;
  if (keyChars < 6) keyChars = 6;
  descChars = (right - left - HELP_OVERLAY_KEY_COL_W - 18) / 6;
  if (descChars < 12) descChars = 12;

  glColor3ub(dlblue[0], dlblue[1], dlblue[2]);
  glRecti(left, top, right, bottom);
  glColor3ub(brblue[0], brblue[1], brblue[2]);
  glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
  glEnd();

  glColor3ub(white[0], white[1], white[2]);
  glRasterPos2i(left + 10, top - 16);
  GLWin.DrawString((const unsigned char *)"HELP  (H hide, mouse wheel scrolls here)");

  y = top - HELP_OVERLAY_HEADER_H;
  for (row = 0; (row < visible) && ((helpOverlayScroll + row) < helpOverlayLineCount); row++)
  {
    line = &helpOverlayLines[helpOverlayScroll + row];
    helpOverlayTrimText(line->key, keybuf, sizeof(keybuf), keyChars);
    helpOverlayTrimText(line->desc, descbuf, sizeof(descbuf), descChars);
    if (*keybuf)
    {
      glColor3ub(aqua[0], aqua[1], aqua[2]);
      glRasterPos2i(left + 10, y - (row * OSD_LINE_H));
      GLWin.DrawString((const unsigned char *)keybuf);
      glColor3ub(white[0], white[1], white[2]);
      glRasterPos2i(left + HELP_OVERLAY_KEY_COL_W, y - (row * OSD_LINE_H));
      GLWin.DrawString((const unsigned char *)descbuf);
    }
    else
    {
      glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
      glRasterPos2i(left + 22, y - (row * OSD_LINE_H));
      GLWin.DrawString((const unsigned char *)descbuf);
    }
  }

  glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
  snprintf(footer, sizeof(footer), "Lines %u-%u / %u",
           (helpOverlayLineCount ? helpOverlayScroll + 1 : 0),
           ((helpOverlayScroll + (unsigned int) visible) < helpOverlayLineCount ? helpOverlayScroll + (unsigned int) visible : helpOverlayLineCount),
           helpOverlayLineCount);
  glRasterPos2i(left + 10, bottom + 6);
  GLWin.DrawString((const unsigned char *)footer);
}

//destroy packets LL
void pcktsDestroy(bool gh)
{
  if (!goHosts)
  {
    goHosts = 1;
    while (goHosts == 1) usleep(1000);
  }
  pckt_type *pk;
  pktsLL.Start(1);
  while ((pk = (pckt_type *)pktsLL.Read(1)))
  {
    delete pk;
    pktsLL.Next(1);
  }
  pktsLL.Destroy();
  if (gh)
  {
    goHosts = 0;
    osdUpdate();
  }
}

//destroy all LLs
void allDestroy()
{
  seltd = 0;
  pcktsDestroy();
  alrtsDestroy();
  linksDestroy();
  hostsDestroy();
  dynamicHostLastCleanup = 0;
}

static inline GLuint nameFromHostType (host_type *ht)
{
  if (useIpAddrForGlName) {
    return ht->hip.s_addr;
  } else {
    return (GLuint) ht;
  }
}

void hostDrawCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  bool dips = (arg2 == 1l) ? true : false;
  bool anms = (arg3 == 1l) ? true : false;

  if (ht != seltd)  //draw host
  {
    if ((setts.sona != hst) || hostShouldDrawInShowHostMode(ht))
    {
      glPushMatrix();

      glLoadName(nameFromHostType(ht));
      glTranslated(ht->px, ht->py, ht->pz);

      if (ht->col)
	glCallList(objsDraw + 12);
      else if
	(ht->sld) glCallList(objsDraw + 10);
      else
	glCallList(objsDraw + ht->clr);

      glPopMatrix();

      if (dips && !ht->col && hostShouldDrawLabel(ht))  //draw host IP/name
      {
	glColor3ub(white[0], white[1], white[2]);
	glRasterPos3i(ht->px, ht->py + 11, ht->pz);
	GLWin.DrawString((const unsigned char *)hostLabelText(ht));
      }
      if (anms && ht->anm)
      {
	alrtCreate(anom, 248, ht);  //create anomaly alert
	if (setts.mvd < 25)
	  setts.mvd = 25;
      }
      if (animate && ht->vis)
	ht->vis--;
    }
  }
}

//draw host objects
void hostsDraw(GLenum mode)
{
  bool dips = ((mode == GL_RENDER) && (setts.sona != ipn)), anms = (setts.anm && (time(0) - atime) && goAnim);  //every second
  if (anms) time(&atime);
  if (seltd)  //draw selected host
  {
    glPushMatrix();
    glLoadName(0);
    glTranslated(seltd->px, seltd->py, seltd->pz);
    glCallList(objsDraw + (seltd->col ? 13 : 11));
    glPopMatrix();
    if (dips && hostShouldDrawLabel(seltd))  //draw host IP/name
    {
      glColor3ub(white[0], white[1], white[2]);
      glRasterPos3i(seltd->px, seltd->py + 11, seltd->pz);
      GLWin.DrawString((const unsigned char *)hostLabelText(seltd));
    }
    if (anms && seltd->anm)
    {
      alrtCreate(anom, 248, seltd);  //create anomaly alert
      if (setts.mvd < 25) setts.mvd = 25;
    }
    if (animate && seltd->vis) seltd->vis--;
  }

  hstsByIp.forEach(1, hostDrawCb, 0l, dips ? 1l : 0l, anms ? 1l : 0l, 0);
}

//draw link objects
void linksDraw()
{
  glColor3ub(dlgrey[0], dlgrey[1], dlgrey[2]);
  link_type *lk;
  lnksLL.Start(1);
  while ((lk = (link_type *)lnksLL.Read(1)))
  {
    if ((setts.sona != hst) || (hostShouldDrawInShowHostMode(lk->sht) && hostShouldDrawInShowHostMode(lk->dht)))
    {
      glBegin(GL_LINES);
        glVertex3i(lk->sht->px, lk->sht->py + 5, lk->sht->pz);
        glVertex3i(lk->dht->px, lk->dht->py + 5, lk->dht->pz);
      glEnd();
    }
    lnksLL.Next(1);
  }
}

//draw alert objects
void alrtsDraw()
{
  alrt_type *al;
  altsLL.Start(1);
  while ((al = (alrt_type *)altsLL.Read(1)))
  {
    if (al->frm == frame) break;
    switch (al->pr)
    {
      case 248: glColor3ub(brred[0], brred[1], brred[2]); break;  //bright red
      case IPPROTO_ICMP: glColor3ub(red[0], red[1], red[2]); break;  //ICMP red
      case IPPROTO_TCP: glColor3ub(green[0], green[1], green[2]); break;  //TCP green
      case IPPROTO_UDP: glColor3ub(blue[0], blue[1], blue[2]); break;  //UDP blue
      case IPPROTO_ARP: glColor3ub(yellow[0], yellow[1], yellow[2]); break;  //ARP yellow
      default: glColor3ub(brgrey[0], brgrey[1], brgrey[2]);  //other grey
    }
    if (al->ao == text)
    {
      glRasterPos3i(al->ht->px, al->ht->py + 11, al->ht->pz);
      GLWin.DrawString((const unsigned char *)(*al->ht->htnm && setts.sipn ? al->ht->htnm : al->ht->htip));
    }
    else
    {
      glPushMatrix();
      glTranslated(al->ht->px, al->ht->py + 5, al->ht->pz);
      glScalef(al->sz, al->sz, al->sz);
      glCallList(objsDraw + (al->ao == actv ? ALERT_LIST_BASE : (ALERT_LIST_BASE + 1)));
      glPopMatrix();
    }
    if (animate)
    {
      al->sz++;
      if ((al->sz < 16) || ((al->ao == text) && (al->sz < 255)))  //size/duration of alert
      {
        if (al->ao == anom) dAnom = false;
        altsLL.Next(1);
      }
      else
      {
        delete al;
        altsLL.Delete(1);
        if (!altsLL.Num()) refresh = true;
      }
    }
    else altsLL.Next(1);
  }
}

//draw packet objects
void pcktsDraw()
{
  double itrns, dx, dy, dz, yaw, pitch, hyp;
  char sbuf[6];
  pckt_type *pk;
  pktsLL.Start(1);
  while ((pk = (pckt_type *)pktsLL.Read(1)))
  {
    if (pk->frm == frame) break;
    glPushMatrix();
    glTranslated(pk->cu.x, pk->cu.y + PACKET_CENTER_Y, pk->cu.z);
    dx = pk->ht->px - pk->cu.x;
    dy = pk->ht->py - pk->cu.y;
    dz = pk->ht->pz - pk->cu.z;
    hyp = sqrt((dx * dx) + (dz * dz));
    yaw = atan2(dx, dz) * RAD2DEG;
    pitch = -atan2(dy, (hyp ? hyp : 1.0)) * RAD2DEG;
    glRotated(yaw, 0.0, 1.0, 0.0);
    glRotated(pitch, 1.0, 0.0, 0.0);
    glCallList(objsDraw + packetListIndex(pk->clr, pk->shp));
    glPopMatrix();
    if (setts.pdp)
    {
      glColor3ub(white[0], white[1], white[2]);
      sprintf(sbuf, "%u", pk->dstpt);
      glRasterPos3d(pk->cu.x, pk->cu.y + PACKET_LABEL_Y, pk->cu.z);
      GLWin.DrawString((const unsigned char *)sbuf);
    }
    if (animate)
    {
      itrns = sqrt(sqr(pk->cu.x - pk->ht->px) + sqr(pk->cu.y - pk->ht->py) + sqr(pk->cu.z - pk->ht->pz)) / (setts.fsp ? 6.0 : 3.0);  //constant speed for all packets
      if (itrns > 1.0)
      {
        pk->cu.x -= (pk->cu.x - pk->ht->px) / itrns;
        pk->cu.y -= (pk->cu.y - pk->ht->py) / itrns;
        pk->cu.z -= (pk->cu.z - pk->ht->pz) / itrns;
        pktsLL.Next(1);
      }
      else
      {
        if (setts.sona)
        {
          alrtCreate((setts.sona == ipn ? text : actv), pk->pr, pk->ht);  //on-active create alert
          if (setts.sona == hst)
          {
            pk->ht->vis = 254;
            setts.mvd = 255;
          }
          else if (setts.sona == slt) pk->ht->sld = 1;
        }
        delete pk;
        pktsLL.Delete(1);
        if (!pktsLL.Num()) refresh = true;
      }
    }
    else pktsLL.Next(1);
  }
}

//draw 2D objects
void draw2D()
{
  int py = hWin - OSD_MARGIN_Y;
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0.0, (GLdouble)wWin, 0.0, (GLdouble)hWin);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glTranslatef(0.375, 0.375, 0.0);  //rasterisation fix
  if (seltd)  //draw selected host details
  {
    glColor3ub(white[0], white[1], white[2]);
    glRasterPos2i(6, py);
    GLWin.DrawString((const unsigned char *)htdtls);
  }
  if (setts.osd && goSize)
  {
    int px, left, top, right, bottom;
    osdUpdate();
    osdBounds(&left, &top, &right, &bottom);
    px = left;
    drawOsdPanel();
    osdDrawPktLegend(px, py);
  }
  if (ptrc > hlt) drawPacketTrafficStatus();
  if (helpOverlayVisible) drawHelpOverlay();
  if (GLWin.On()) GLWin.Draw(GL_RENDER);  //draw 2D GUI
  else if (mMove)  //draw selection box
  {
    glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
    glBegin(GL_LINE_LOOP);
      glVertex2i(mBxx, mBxy);
      glVertex2i(mPsx, mBxy);
      glVertex2i(mPsx, mPsy);
      glVertex2i(mBxx, mPsy);
    glEnd();
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glEnable(GL_DEPTH_TEST);
}

//draw graphics
void displayGL()
{
  refresh = false;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(setts.vws[0].ee.x, setts.vws[0].ee.y, setts.vws[0].ee.z, setts.vws[0].dr.x, setts.vws[0].dr.y, setts.vws[0].dr.z, 0.0, 1.0, 0.0);
  glCallList(objsDraw + 14);  //draw cross object
  if (hstsByIp.Num() != 0) hostsDraw(GL_RENDER);
  if (lnksLL.Num()) linksDraw();
  if (altsLL.Num()) alrtsDraw();
  if (pktsLL.Num()) pcktsDraw();
  if (setts.osd || seltd || mMove || (ptrc > hlt) || GLWin.On() || helpOverlayVisible) draw2D();
  animate = false;
  glfwSwapBuffers();
}

//glfwSetWindowRefreshCallback
void GLFWCALL refreshGL()
{
  refresh = true;
}

//glfwSetWindowSizeCallback
void GLFWCALL resizeGL(int w, int h)
{
  wWin = w;
  hWin = h;
  goSize = ((wWin >= OSD_MIN_W) && (hWin >= OSD_MIN_H));  //don't draw OSD if window too small for the legend and status text
  glViewport(0, 0, wWin, hWin);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0, (GLdouble)wWin / (GLdouble)hWin, 1.0, DEPTH);
  GLWin.ScreenSize(wWin, hWin);
}

//load selected host details into htdtls
void hostDetails()
{
  char pbuf[16];
  host_runtime_meta_type *meta;
  strcpy(htdtls, "IP: ");  //4
  strcat(htdtls, seltd->htip);  //15
  if (*seltd->htmc && strcmp(seltd->htmc, "00:00:00:00:00:00"))
  {
    strcat(htdtls, "\nMAC: ");  //6
    strcat(htdtls, seltd->htmc);  //17
  }
  if (*seltd->htnm)
  {
    strcat(htdtls, "\nName: ");  //7
    strcat(htdtls, seltd->htnm);  //255
  }
  if (*seltd->htrm)
  {
    strcat(htdtls, "\nRemarks: ");  //10
    strcat(htdtls, seltd->htrm);  //255
  }
  meta = hostRuntimeMeta(seltd, false);
  if (meta && meta->pr)
  {
    strcat(htdtls, "\nLast Protocol: ");
    if (meta->pr == IPPROTO_OTHER) strcat(htdtls, "Other");
    else strcat(htdtls, protoDecode(meta->pr, pbuf));
    if (meta->shp)
    {
      strcat(htdtls, "\nLast Packet Form: ");
      strcat(htdtls, packetShapeFilterLabel(meta->shp));
    }
    if (meta->port)
    {
      sprintf(pbuf, "%u", meta->port);
      strcat(htdtls, "\nLast Important Port: ");
      strcat(htdtls, pbuf);
    }
  }
  if (meta && *meta->dnm)
  {
    strcat(htdtls, "\nLast Discovery Name: ");
    strcat(htdtls, meta->dnm);
  }
  if (hostIsKnownNetpos(seltd)) strcat(htdtls, "\nKnown Host: netpos /32");
}

//2D GUI window "please wait"
void waitShow()
{
  GLWin.CreateWin(-1, -1, 104, 50, "PROCESSING", false);
  GLWin.AddLabel(10, 10, "Please Wait...");
  displayGL();
}

//display message box
void messageBox(const char *ttl, const char *msg)
{
  GLWin.CreateWin(-1, -1, 230, 90, ttl);
  GLWin.AddLabel(10, 10, msg);
  displayGL();
}

void hostSetCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  unsigned char mbr = (unsigned char) arg1;
  unsigned char val = (unsigned char) arg2;
  switch (mbr)
  {
  case 0:
    if (!val && (setts.sona == hst) && ht->sld)
    {
      ht->vis = 254;
      setts.mvd = 255;
    }
    ht->sld = val;
    break;

  case 1:
    ht->pip = val;
    break;

  case 2:
    ht->anm = val;
    break;

  case 3:
    ht->shp = val;
    break;

  case 4:
    ht->alk = val;
    break;
   }
}

//set value for all hosts
void hostsSet(unsigned char mbr, unsigned char val)
{
  hstsByIp.forEach(1, hostSetCb, (long) mbr, (long) val, 0, 0);
}

//return pointer to host from IP address
host_type *hostIP(in_addr ip, bool crt)
{
  HtKeyType key = keyFromIpAddress(ip);
  host_type *ht = (host_type *) hstsByIp.firstThat(key, 2, &isHostIpAddress,
						   (long) ip.s_addr, 0, 0, 0);
  if (ht)
    return ht;

  if (crt)
    return hostCreate(ip);

  return 0;
}

//load network layout
void netLoad(const char *fl)
{
  FILE *net;
  if ((net = fopen(fl, "rb")))
  {
    char hnl[4];
    if (fgets(hnl, 4, net))
    {
      host_type host, *ht, *dht;
      size_t htsz = sizeof(host);
      if (!strcmp(hnl, "HN1"))
      {
        allDestroy();
        unsigned char spr;
        unsigned int hsts;
        in_addr hip;
        size_t ipsz = sizeof(hip);
        if (fread(&hsts, sizeof(hsts), 1, net) == 1)
        {
          for (; hsts; hsts--)
          {
            if (fread(&host, htsz, 1, net) != 1)
            {
              goHosts = 0;
              fclose(net);
              return;
            }
            host.col = 0;
	    ht = new host_type(host);
	    newHostByIp(ht);
            hostSetDynamic(ht, false);
	    hostByPositionPostMove(ht);
            moveCollision(ht);
          }
          while (fread(&hip, ipsz, 1, net) == 1)
          {
            ht = hostIP(hip, false);
            if (fread(&hip, ipsz, 1, net) != 1) break;
            dht = hostIP(hip, false);
            if (fread(&spr, 1, 1, net) != 1) break;
            if (ht && dht) linkCreDel(ht, dht, spr);
          }
        }
        goHosts = 0;
      }
      else if (!strcmp(hnl, "HNL"))
      {
        allDestroy();
        while (fread(&host, htsz, 1, net) == 1)
        {
          host.col = 0;
          ht = new host_type(host);
	  newHostByIp(ht);
          hostSetDynamic(ht, false);
	  hostByPositionPostMove(ht);
          moveCollision(ht);
        }
        goHosts = 0;
      }
    }
    fclose(net);
  }
}

void netSaveCountCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  unsigned int *count = (unsigned int *)(void *) arg1;
  if (hostShouldPersist(ht)) (*count)++;
}

bool netSaveCb(void *data, long arg1, long arg2, long arg3, long arg4)
{
  FILE *fp = (FILE *)(void *) arg1;
  host_type *ht = (host_type *) data;
  if (!hostShouldPersist(ht)) return false;

  if (fwrite(ht, sizeof(host_type), 1, fp) != 1) {
    return true; // Will halt the iteration
  }
  return false;
}

//save network layout
void netSave(const char* fl)
{
  FILE *net;
  if ((net = fopen(fl, "wb")))
  {
    unsigned int hsts = 0;
    hstsByIp.forEach(1, netSaveCountCb, (long)(void *) &hsts, 0, 0, 0);
    fputs("HN1", net);
    if (fwrite(&hsts, sizeof(hsts), 1, net) == 1)
    {
      if (hstsByIp.firstThat(1, &netSaveCb, (long) net, 0, 0, 0)) {
	// fwrite() failed in netSaveCb
	fclose(net);
	remove(fl);
	return;
      }

      link_type *lk;
      lnksLL.Start(1);
      size_t ipsz = sizeof(in_addr);
      while ((lk = (link_type *)lnksLL.Read(1)))
      {
        if (!hostShouldPersist(lk->sht) || !hostShouldPersist(lk->dht))
        {
          lnksLL.Next(1);
          continue;
        }
        if (fwrite(&lk->sht->hip, ipsz, 1, net) != 1) break;
        if (fwrite(&lk->dht->hip, ipsz, 1, net) != 1) break;
        if (fwrite(&lk->spr, 1, 1, net) != 1) break;  //spare (future use)
        lnksLL.Next(1);
      }
    }
    fclose(net);
  }
}

void btnProcessExportCsvCb(void **data, long arg1, long arg2,
			   long arg3, long arg4) {
  host_type *ht = *((host_type **) data);
  FILE *cf = (FILE *)(void *) arg1;
  unsigned char cnt;
  char buf[5], svc[12], row[1097];

  if (ht->sld)
  {
    strcpy(row, "\"");
    quotecsv(ht->htip, row);
    strcat(row, "\",\"");
    quotecsv(ht->htmc, row);
    strcat(row, "\",\"");
    quotecsv(ht->htnm, row);
    strcat(row, "\",\"");
    quotecsv(ht->htrm, row);
    strcat(row, "\",\"");
    for (cnt = 0; cnt < SVCS; cnt++)
    {
      if (ht->svc[cnt] == -1) break;
      sprintf(svc, "%s:%d ", protoDecode((ht->svc[cnt] % 10000) / 10, buf), ht->svc[cnt] / 10000);
      strcat(row, svc);
    }
    strcat(row, "\"\n");
    fputs(row, cf);
  }
}

typedef struct FindHostsCbData_ {
  char *gi1;
  char *gi2;
  char *gi3;
  char *gi4;
  char *gi5;
  bool anet;
  in_addr_t mask, net;
  unsigned int found;
  unsigned short sprt;
} FindHostsCbData;

void btnProcessFindHostsCb(void **data, long arg1, long arg2,
			   long arg3, long arg4) {
  host_type *ht = *((host_type **) data);
  FindHostsCbData *findhostscbdata = (FindHostsCbData *)(void *) arg1;
  bool ipok = true;

  if (*(findhostscbdata->gi1)) {
    if (findhostscbdata->anet) {
      if ((ht->hip.s_addr & findhostscbdata->mask) != findhostscbdata->net) {
	ipok = false;
      }
    } else if (strcmp(ht->htip, findhostscbdata->gi1)) {
      ipok = false;
    }
  }

  if (ipok && strcasestr(ht->htmc, findhostscbdata->gi2) &&
      strcasestr(ht->htnm, findhostscbdata->gi3) &&
      strcasestr(ht->htrm, findhostscbdata->gi4)) {
    if (!*(findhostscbdata->gi5)) {
      ht->sld = 1;
      findhostscbdata->found++;
    } else {
      for (unsigned cnt = 0; cnt < SVCS; cnt++) {
	if (ht->svc[cnt] == -1) {
	  break;
	}

	if ((ht->svc[cnt] / 10000) == findhostscbdata->sprt) {
	  ht->sld = 1;
	  findhostscbdata->found++;
	  break;
	}
      }
    }
  }
}

void btnProcessSlInActCb(void **data, long arg1, long arg2, long arg3,
			 long arg4) {
  host_type *ht = *((host_type **) data);
  time_t itime = (time_t) arg1;
  char *gi1 = (char *)(void *) arg2;
  char *gi2 = (char *)(void *) arg3;
  char *gi3 = (char *)(void *) arg4;

  ht->sld = ((itime - ht->lpk) > ((atoi(gi1) * 86400) + (atoi(gi2) * 360) + (atoi(gi3) * 60)));
}

typedef struct ResolveHostnamesCbData_
{
  unsigned int selected;
  unsigned int resolved;
  unsigned int existing;
  unsigned int failed;
} ResolveHostnamesCbData;

static void resolveSelectedHostnamesCb(void **data, long arg1, long arg2,
                                       long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  ResolveHostnamesCbData *rd = (ResolveHostnamesCbData *)(void *) arg1;

  if (!ht || !ht->sld) return;
  rd->selected++;
  if (*ht->htnm)
  {
    rd->existing++;
    return;
  }
  if (hostResolveName(ht)) rd->resolved++;
  else rd->failed++;
}

//process 2D GUI button selected
void btnProcess(int gs)
{
  int gr = GLResult[0] % 100;  //result identification
  char *gi1 = (GLResult[0] >= 100 ? GLWin.GetInput(GLResult[0] / 100) : 0), *gi2, *gi3, *gi4, *gi5;  //get input text
  if ((gs == GLWIN_CLOSE) || (!gi1 && (gr != HSD_HSENRUN) && (gr != HSD_HSENSTP)))
  {
    GLWin.Close(false);  //close focused 2D GUI window
    return;
  }
  switch (gr)
  {
    case HSD_EDTNAME: case HSD_EDTRMKS:
      strcpy((gr == HSD_EDTNAME ? seltd->htnm : seltd->htrm), gi1);
      hostPromoteStatic(seltd);
      hostDetails();
      GLWin.Close();  //close all 2D GUI windows
      break;
    case HSD_EXPTCSV:
      if (*gi1)
      {
        waitShow();
        extensionAdd(gi1, ".csv");
        FILE *cf;
        if ((cf = fopen(gi1, "w")))
        {
          fputs("\"IP\",\"MAC\",\"Name\",\"Remarks\",\"Services\"\n", cf);
	  hstsByIp.forEach(1, btnProcessExportCsvCb, (long) cf, 0, 0, 0);
          fclose(cf);
        }
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter File Name!");
      break;
    case HSD_FNDHSTS:
      gi2 = GLWin.GetInput(GLResult[1]);  //get input text
      gi3 = GLWin.GetInput(GLResult[2]);
      gi4 = GLWin.GetInput(GLResult[3]);
      gi5 = GLWin.GetInput(GLResult[4]);
      if (*gi1 || *gi2 || *gi3 || *gi4 || *gi5)
      {
        if (gs == GLWIN_DEL)
        {
          GLWin.PutInput("", GLResult[0] / 100);  //put input text
          GLWin.PutInput("", GLResult[1]);
          GLWin.PutInput("", GLResult[2]);
          GLWin.PutInput("", GLResult[3]);
          GLWin.PutInput("", GLResult[4]);
        }
        else
        {
          bool anet = false;
          unsigned short sprt = 0;
          in_addr_t mask = 0, net = 0;
	  FindHostsCbData findhostscbdata;

          char ttmp[19], *cidr;
          if (*gi1)
          {
            strcpy(ttmp, gi1);
            if ((cidr = strchr(ttmp, '/')))
            {
              *cidr = '\0';
              if (atoi(++cidr)) mask = htonl(0xffffffff << (32 - atoi(cidr)));
              net = inet_addr(ttmp) & mask;
              anet = true;
            }
          }
          if (*gi5)
	    sprt = atoi(gi5);

          seltd = 0;

          if (!glfwGetKey(GLFW_KEY_LCTRL) && !glfwGetKey(GLFW_KEY_RCTRL))
	    hostsSet(0, 0);  //ht->sld

	  findhostscbdata.gi1 = gi1;
	  findhostscbdata.gi2 = gi2;
	  findhostscbdata.gi3 = gi3;
	  findhostscbdata.gi4 = gi4;
	  findhostscbdata.gi5 = gi5;
	  findhostscbdata.anet = anet;
	  findhostscbdata.mask = mask;
	  findhostscbdata.net = net;
	  findhostscbdata.found = 0;
	  findhostscbdata.sprt = sprt;

	  hstsByIp.forEach(1, btnProcessFindHostsCb,
			   (long)(void *) &findhostscbdata,
			   0, 0, 0);

          sprintf(ttmp, "Found: %u", findhostscbdata.found);
          GLWin.PutLabel(ttmp, GLResult[5]);
        }
      }
      break;
    case HSD_HNLOPEN: case HSD_HNLSAVE:
      if (*gi1)
      {
        waitShow();
        if (gr == HSD_HNLOPEN)
        {
          netLoad(gi1);
          netposExactHostsSync();
        }
        else
        {
          extensionAdd(gi1, ".hnl");
          netSave(gi1);
        }
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter File Name!");
      break;
    case HSD_HPTOPEN: case HSD_HPTSAVE: case HSD_HPTRPY:
      if (*gi1)
      {
        waitShow();
        if (ptrc == rec) ptrc = hlt;
        else if (ptrc == rpy)
        {
          pcktsDestroy();
          ptrc = hlt;
          goHosts = 0;
        }
        while (ptrc == hlt) usleep(1000);
        if (gr == HSD_HPTOPEN)
        {
          pkrcCopy(gi1, hsddata("traffic.hpt"));
          packetTrafficPathSet(hsddata("traffic.hpt"));
          packetTrafficLabelSet(gi1);
        }
        else if (gr == HSD_HPTRPY)
        {
          char hptname[256], hptpath[512];
          setStringValue(hptname, sizeof(hptname), gi1);
          extensionAdd(hptname, ".hpt");
          packetTrafficResolvePath(hptname, hptpath, sizeof(hptpath));
          if (!packetTrafficReplayStart(hptpath)) messageBox("ERROR", "Unable to replay packet traffic file.");
        }
        else
        {
          extensionAdd(gi1, ".hpt");
          pkrcCopy(packetTrafficPathGet(), gi1);
        }
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter File Name!");
      break;
    case HSD_MAKEHST:
      if (*gi1)
      {
        bool anm = setts.anm;
        in_addr mip;
        mip.s_addr = inet_addr(gi1);
        goHosts = 1;
        while (goHosts == 1) usleep(1000);
        setts.anm = false;  //don't create anomaly
        seltd = hostIP(mip, true);
        hostPromoteStatic(seltd);
        setts.anm = anm;
        goHosts = 0;
        seltd->sld = 1;
        hostDetails();
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter IP Address!");
      break;
    case HSD_NETPOS:
      if (*gi1)
      {
        FILE *nf, *tf;
        if ((nf = fopen(hsddata("netpos.txt"), "r")))
        {
          if ((tf = fopen(hsddata("tmp-netpos-hsd"), "w")))
          {
            switch (gs)
            {
              case GLWIN_ITMUP: GLWin.Scroll(GLWIN_UP, true); break;
              case GLWIN_ITMDN: GLWin.Scroll(GLWIN_DOWN, true); break;
              case GLWIN_DEL: break;  //nothing
              default:
                GLWin.Scroll();  //start
                fputs(gi1, tf);
                fputc('\n', tf);
            }
            bool btmp = false;
            char fbuf[256], tbuf[256], *end;
            while (fgets(fbuf, 256, nf))
            {
              if ((end = strchr(fbuf, '\n'))) *end = '\0';  //remove \n for comparison
              if (strcmp(fbuf, gi1))
              {
                if (gs == GLWIN_ITMUP)
                {
                  if (btmp)
                  {
                    fputs(tbuf, tf);
                    fputc('\n', tf);
                    btmp = false;
                  }
                  strcpy(tbuf, fbuf);
                  btmp = true;
                }
                else
                {
                  fputs(fbuf, tf);
                  fputc('\n', tf);
                  if (btmp)
                  {
                    fputs(tbuf, tf);
                    fputc('\n', tf);
                    btmp = false;
                  }
                }
              }
              else if (gs == GLWIN_ITMUP)
              {
                if (btmp)
                {
                  fputs(fbuf, tf);
                  fputc('\n', tf);
                  fputs(tbuf, tf);
                  fputc('\n', tf);
                  btmp = false;
                }
                else
                {
                  fputs(fbuf, tf);
                  fputc('\n', tf);
                }
              }
              else if (gs == GLWIN_ITMDN)
              {
                strcpy(tbuf, fbuf);
                btmp = true;
              }
            }
            if (btmp)
            {
              fputs(tbuf, tf);
              fputc('\n', tf);
            }
            fclose(tf);
            fclose(nf);
            remove(hsddata("netpos.txt"));
            strcpy(fbuf, hsddata("tmp-netpos-hsd"));
            rename(fbuf, hsddata("netpos.txt"));
          }
          else fclose(nf);
        }
        goHosts = 1;
        while (goHosts == 1) usleep(1000);
        netpsLoad();  //reload netpos
        netposExactHostsSync();
        goHosts = 0;
      }
      break;
    case HSD_PKTSPRO: case HSD_PKTSPRT:
      if (*gi1)
      {
        if (gr == HSD_PKTSPRO)
        {
          packetFilterResetAll();
          osdUpdate();
        }
        else
        {
          packetFilterResetAll();
          packetFilterSetPort((unsigned short)atoi(gi1));
          if (setts.prt) pcktsDestroy(true);
          else osdUpdate();
        }
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter Number!");
      break;
    case HSD_SETCMDS:
      strcpy(setts.cmd[0], gi1);
      strcpy(setts.cmd[1], GLWin.GetInput(GLResult[1]));
      strcpy(setts.cmd[2], GLWin.GetInput(GLResult[2]));
      strcpy(setts.cmd[3], GLWin.GetInput(GLResult[3]));
      GLWin.Close();  //close all 2D GUI windows
      break;
    case HSD_SLINACT:
      gi2 = GLWin.GetInput(GLResult[1]);  //get input text
      gi3 = GLWin.GetInput(GLResult[2]);
      if (*gi1 || *gi2 || *gi3)
      {
        seltd = 0;
        time_t itime = time(0);
	hstsByIp.forEach(1, btnProcessSlInActCb, (long) itime,
			 (long)(void *) gi1,
			 (long)(void *) gi2,
			 (long)(void *) gi3);
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter Days, Hours or Minutes!");
      break;
    case HSD_HSENRUN:
      if (gs == GLWIN_MISC1)
      {
        if (!localHsenSaveUiState()) break;
        settsSave();
        GLWin.Close(false);
        localHsenDialogOpen();
      }
      else if (gs == GLWIN_MISC2)
      {
        localHsenStopManaged(true);
      }
      else
      {
        if (!localHsenSaveUiState()) break;
        settsSave();
        if (localHsenStartSelected(true)) GLWin.Close();
      }
      break;
    case HSD_HSENSTP:
      localHsenStopManaged(true);
      break;
  }
}

bool hostTabFirstThatCb(void *data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = (host_type *) data;
  host_type **identified_hd = (host_type **)(void *) arg1;

  if (*identified_hd != 0) {
    if (ht->sld) {
      return true; // i.e. return the next one after the identified one
    } else {
      return false;
    }
  }

  if (seltd && (seltd == ht)) {
    *identified_hd = ht;
    return false;
  }

  return false;
}

//select next/prev host in selection
void hostTab(bool nx)
{
  host_type *ht;
  host_type *identified_hd = 0;

  ht = (host_type *) hstsByPos.firstThat(1, &hostTabFirstThatCb,
					 (long)(void *) &identified_hd,
					 0, 0, 0);
  if (ht) {
    seltd = ht;
    hostDetails();
  }
}

//generate host info
void infoHost()
{
  FILE *info;
  if ((info = fopen(hsddata("tmp-hinfo-hsd"), "w")))
  {
    char buf[11];
    host_runtime_meta_type *meta = hostRuntimeMeta(seltd, false);
    char pbuf[16];
    fputs(htdtls, info);
    fprintf(info, "\n\nAnomaly: %s\nLast Sensor: %u\nLast Packet: %sShow Packets: %s\nDownloads: %s"
      , (seltd->anm ? "Yes" : "No"), seltd->lsn, ctime(&seltd->lpk), (seltd->shp ? "Yes" : "No"), formatBytes(seltd->dld, buf));
    fprintf(info, "\nUploads: %s\nAuto Link Lines: %s\nLock: %s\nLifetime: %s"
      , formatBytes(seltd->uld, buf), (seltd->alk ? "Yes" : "No"), (seltd->lck ? "On" : "Off"), (hostIsDynamic(seltd) ? "Dynamic" : "Static"));  //reuse buf
    if (hostIsKnownNetpos(seltd)) fprintf(info, "\nKnown Host: netpos /32");
    if (meta && meta->pr)
    {
      fprintf(info, "\nLast Protocol: %s", (meta->pr == IPPROTO_OTHER ? "Other" : protoDecode(meta->pr, pbuf)));
      if (meta->shp) fprintf(info, "\nLast Packet Form: %s", packetShapeFilterLabel(meta->shp));
      if (meta->port) fprintf(info, "\nLast Important Port: %u", meta->port);
    }
    if (meta && *meta->dnm) fprintf(info, "\nLast Discovery Name: %s", meta->dnm);
    if (seltd->svc[0] == -1) fprintf(info, "\n\nNO SERVICES");
    else
    {
      fprintf(info, "\n\nSERVICES\nProtocol\tPort");
      for (unsigned char cnt = 0; cnt < SVCS; cnt++)
      {
        if (seltd->svc[cnt] != -1) fprintf(info, "\n%s\t%d", protoDecode((seltd->svc[cnt] % 10000) / 10, buf), seltd->svc[cnt] / 10000);
        else break;
      }
    }
    fclose(info);
  }
}

//reset packet traffic replay time offset
void offsetReset()
{
  distm = time(0) - pkrp.ptime.tv_sec;
  rpoff = milliTime(0) - milliTime(&pkrp.ptime);
}

void infoSelectionForEachCb(void **data, long arg1, long arg2, long arg3,
			    long arg4) {
  host_type *ht = *((host_type **) data);
  FILE *fp = (FILE *)(void *) arg1;
  if (ht->sld)
    fprintf(fp, "\n%s\t%s", ht->htip, ht->htnm);
}

//generate selection info
void infoSelection()
{
  FILE *info;
  if ((info = fopen(hsddata("tmp-hinfo-hsd"), "w")))
  {
    fprintf(info, "CURRENT SELECTION\n");
    hstsByPos.forEach(1, &infoSelectionForEachCb, (long)(void *) info, 0, 0, 0);
    fclose(info);
  }
}

void keyboardForEachCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  keyact_type key = (keyact_type) arg1;
  int dx, dz;
  double angt;

  if (key == kaInvertSelection) {
    ht->sld = !ht->sld;  //invert selection, ctrl+s
  } else if (key == kaSelectNamed) {
    ht->sld = (*ht->htnm != '\0');  //select all named hosts, ctrl+n
  } else if (ht->sld) {
    switch (key) {

    case kaToggleSelectionPersistant:
      ht->pip = !ht->pip;
      break;  //toggle selection persistant IP

    case kaAutoLinksSelection:
      ht->alk = 1;
      break;  //automatic link lines for selection

    case kaStopAutoLinksSelection:
      ht->alk = 0;
      break;  //stop automatic link lines for selection, ctrl+j

    case kaShowSelectionPackets:
      ht->shp = 1;
      break;  //show packets for selection

    case kaHideSelectionPackets:
      ht->shp = 0;
      break;  //stop showing packets for selection, ctrl+p

    default:
      if (ht->lck)
	break;

      hostByPositionPreMove(ht);

      if (key == kaSelMoveUp) {
	ht->py += SPC;  //move selection up
      } else if (key == kaSelMoveDown) {
	ht->py -= SPC;  //move selection down
      } else {
	if (key == kaSelMoveForward)
	  angt = setts.vws[0].ax * RAD;  //move selection forward
	else if (key == kaSelMoveLeft)
	  angt = (setts.vws[0].ax + 90.0) * RAD;  //move selection left
	else if (key == kaSelMoveRight)
	  angt = (setts.vws[0].ax - 90.0) * RAD;  //move selection right
	else
	  angt = (setts.vws[0].ax + 180.0) * RAD;  //move selection back

	if (cos(angt) >= 0.707) {
	  dx = 0;
	  dz = 1;
	} else if (sin(angt) > 0.707) {
	  dx = 1;
	  dz = 0;
	} else if (sin(angt) < -0.707) {
	  dx = -1;
	  dz = 0;
	} else {
	  dx = 0;
	  dz = -1;
	}

	ht->px += dx * SPC;
	ht->pz += dz * SPC;
      }

      hostByPositionPostMove(ht);
      moveCollision(ht);
    }
  }
}

//glfwSetKeyCallback
void GLFWCALL keyboardGL(int key, int action)
{
  bool ctrlDown = (glfwGetKey(GLFW_KEY_LCTRL) || glfwGetKey(GLFW_KEY_RCTRL));
  bool shiftDown = (glfwGetKey(GLFW_KEY_LSHIFT) || glfwGetKey(GLFW_KEY_RSHIFT));
  int rawkey = key, actionKey = key;
  if (!action) return;
  if (ctrlDown && shiftDown && (key != GLFW_KEY_LCTRL) && (key != GLFW_KEY_RCTRL) && (key != GLFW_KEY_LSHIFT) && (key != GLFW_KEY_RSHIFT)) actionKey = CTRLSHIFTKEY(key);
  else if (ctrlDown && (key != GLFW_KEY_LCTRL) && (key != GLFW_KEY_RCTRL)) actionKey = CTRLKEY(key);
  else if (shiftDown && (key != GLFW_KEY_LSHIFT) && (key != GLFW_KEY_RSHIFT)) actionKey = SHIFTKEY(key);
  keyact_type keyact = keyActionFromInput(actionKey);
  if (GLWin.On())
  {
    char *clip;
    int menuValue = GLWin.MenuValueForKey(actionKey);
    if (menuValue)
    {
      mnuProcess(menuValue);
      refresh = true;
      return;
    }
    if (((GLResult[0] % 100) == HSD_HSTINFO) && ((keyact == kaNextSelectedHost) || (keyact == kaPrevSelectedHost)))
    {
      hostTab((keyact == kaNextSelectedHost));
      if (seltd)
      {
        GLWin.Scroll();  //start
        infoHost();
      }
      refresh = true;
      return;
    }
    switch (actionKey)
    {
      case 600:  //ctrl+x
        if ((clip = GLWin.GetInput()))
        {
          strcpy(setts.clip, clip);  //cut input text to clipboard
          GLWin.PutInput("");
        }
        break;
      case 579: if ((clip = GLWin.GetInput())) strcpy(setts.clip, clip); break;  //copy input text to clipboard, ctrl+c
      case 598: GLWin.PutInput(setts.clip); break;  //paste input text from clipboard, ctrl+v
      case GLFW_KEY_ENTER: btnProcess(GLWin.DefaultButton()); break;  //process 2D GUI default button
      case GLFW_KEY_ESC: GLWin.Close(false); break;  //close focused 2D GUI window
      case GLFW_KEY_TAB: case 805:  //select next/prev host in selection, update host info
        if ((GLResult[0] % 100) == HSD_HSTINFO)
        {
          hostTab((actionKey == GLFW_KEY_TAB));
          if (seltd)
          {
            GLWin.Scroll();  //start
            infoHost();
          }
          break;
        }  //pass tab to 2D GUI
      default: GLWin.Key(rawkey, shiftDown);  //pass key to 2D GUI
    }
  }
  else
  {
    int kymv = (shiftDown ? 3 : 1) * MOV;
    double angx, angy;
    if ((actionKey == GLFW_KEY_ESC) && helpOverlayVisible)
    {
      helpOverlayVisible = false;
      refresh = true;
      return;
    }
    switch (keyact)
    {
      case kaMoveForward:
      case kaMoveBack:
        if (keyact == kaMoveBack) kymv *= -1;
        angx = setts.vws[0].ax * RAD;
        angy = setts.vws[0].ay * RAD;
        setts.vws[0].ee.x += sin(angx) * kymv;
        setts.vws[0].dr.x += sin(angx) * kymv;
        setts.vws[0].ee.y += sin(angy) * kymv;
        setts.vws[0].dr.y += sin(angy) * kymv;
        setts.vws[0].ee.z += cos(angx) * cos(angy) * kymv;
        setts.vws[0].dr.z += cos(angx) * cos(angy) * kymv;
        break;
      case kaMoveLeft:
      case kaMoveRight:  //move left/right
        angx = (setts.vws[0].ax + (keyact == kaMoveLeft ? 90.0 : -90.0)) * RAD;
        setts.vws[0].ee.x += sin(angx) * kymv;
        setts.vws[0].dr.x += sin(angx) * kymv;
        setts.vws[0].ee.z += cos(angx) * kymv;
        setts.vws[0].dr.z += cos(angx) * kymv;
        break;
      case kaViewHome: setts.vws[0] = vwdef[0]; break;  //recall default view
      case kaViewHomeAlt: setts.vws[0] = vwdef[1]; break;  //recall alternate default view
      case kaViewPos1: memcpy(&setts.vws[0], &setts.vws[1], sizeof(view_type)); break;
      case kaViewPos2: memcpy(&setts.vws[0], &setts.vws[2], sizeof(view_type)); break;
      case kaViewPos3: memcpy(&setts.vws[0], &setts.vws[3], sizeof(view_type)); break;
      case kaViewPos4: memcpy(&setts.vws[0], &setts.vws[4], sizeof(view_type)); break;
      case kaViewSave1: memcpy(&setts.vws[1], &setts.vws[0], sizeof(view_type)); break;
      case kaViewSave2: memcpy(&setts.vws[2], &setts.vws[0], sizeof(view_type)); break;
      case kaViewSave3: memcpy(&setts.vws[3], &setts.vws[0], sizeof(view_type)); break;
      case kaViewSave4: memcpy(&setts.vws[4], &setts.vws[0], sizeof(view_type)); break;
      case kaLayoutRestore1: netLoad(hsddata("1network.hnl")); netposExactHostsSync(); break;
      case kaLayoutRestore2: netLoad(hsddata("2network.hnl")); netposExactHostsSync(); break;
      case kaLayoutRestore3: netLoad(hsddata("3network.hnl")); netposExactHostsSync(); break;
      case kaLayoutRestore4: netLoad(hsddata("4network.hnl")); netposExactHostsSync(); break;
      case kaLayoutSave1: netSave(hsddata("1network.hnl")); break;
      case kaLayoutSave2: netSave(hsddata("2network.hnl")); break;
      case kaLayoutSave3: netSave(hsddata("3network.hnl")); break;
      case kaLayoutSave4: netSave(hsddata("4network.hnl")); break;
      case kaSelectAll: hostsSet(0, 1); break;  //select all hosts, ht->sld
      case kaSelMoveUp:
      case kaSelMoveDown:
      case kaSelMoveForward:
      case kaSelMoveBack:
      case kaSelMoveLeft:
      case kaSelMoveRight:
        goHosts = 1;
        while (goHosts == 1) usleep(1000);
      case kaInvertSelection:
      case kaSelectNamed:
      case kaToggleSelectionPersistant:
      case kaAutoLinksSelection:
      case kaStopAutoLinksSelection:
      case kaShowSelectionPackets:
      case kaHideSelectionPackets:
        if ((keyact == kaInvertSelection) || (keyact == kaSelectNamed)) seltd = 0;
	// Since the keyboardForEachCb callback changes the hosts' positions,
	// we cannot iterate (forEach) over the hstsByPos hash-table; instead
	// we walk over the hstsByIp hash-table.
	hstsByIp.forEach(1, &keyboardForEachCb, (long) keyact, 0, 0, 0);
        goHosts = 0;
        break;
      case kaFindHosts:  //2D GUI find hosts
        GLWin.CreateWin(-1, -1, 350, 220, "FIND HOSTS");
        GLWin.AddLabel(10, 15, "IP/Net:");
        GLWin.AddLabel(202, 15, "(CIDR Notation for Net)");
        GLWin.AddLabel(10, 45, "MAC:");
        GLResult[1] = GLWin.AddInput(70, 40, 17, 17, "", true);
        GLWin.AddLabel(10, 75, "Hostname:");
        GLResult[2] = GLWin.AddInput(70, 70, 43, 255, "", true);
        GLWin.AddLabel(10, 105, "Remarks:");
        GLResult[3] = GLWin.AddInput(70, 100, 43, 255, "", true);
        GLWin.AddLabel(10, 135, "Port No.:");
        GLResult[4] = GLWin.AddInput(70, 130, 5, 5, "", true);
        GLResult[5] = GLWin.AddLabel(156, 135, "");
        GLResult[0] = (GLWin.AddInput(70, 10, 18, 18, "", true) * 100) + HSD_FNDHSTS;  //last for focus
        GLWin.AddButton(146, 160, GLWIN_OK, "Find", true, true);
        GLWin.AddButton(210, 160, GLWIN_DEL, "Clear");
        break;
      case kaNextSelectedHost:
      case kaPrevSelectedHost:
        hostTab((keyact == kaNextSelectedHost));
        break;  //select next/prev host in selection
      case kaCycleIpNameDisplay:  //cycle show IP - IP/name - name only
        if (setts.sipn == ips) setts.sipn = ins;
        else if (setts.sipn == ins) setts.sipn = nms;
        else setts.sipn = ips;
        osdUpdate();
        break;
      case kaToggleAddDestinationHosts:  //toggle add destination hosts
        setts.adh = !setts.adh;
        if (setts.adh && setts.bct) setts.bct = 0;
        osdUpdate();
        break;
      case kaMakeHost:  //make host
        GLWin.CreateWin(-1, -1, 230, 100, "MAKE HOST");
        GLWin.AddLabel(10, 15, "Enter IP Address:");
        GLResult[0] = (GLWin.AddInput(118, 10, 15, 15, "") * 100) + HSD_MAKEHST;
        GLWin.AddButton(102, 40, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(154, 40, GLWIN_CLOSE, "Cancel");
        break;
      case kaEditHostname:  //edit selected host hostname
        if (!seltd) break;
        GLWin.CreateWin(-1, -1, 320, 116, seltd->htip);
        GLWin.AddLabel(10, 10, "Edit Hostname:");
        GLResult[0] = (GLWin.AddInput(10, 26, 48, 255, seltd->htnm) * 100) + HSD_EDTNAME;
        GLWin.AddButton(192, 56, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(244, 56, GLWIN_CLOSE, "Cancel");
        break;
      case kaEditRemarks:  //edit selected host remarks
        if (!seltd) break;
        GLWin.CreateWin(-1, -1, 320, 116, seltd->htip);
        GLWin.AddLabel(10, 10, "Edit Remarks:");
        GLResult[0] = (GLWin.AddInput(10, 26, 48, 255, seltd->htrm) * 100) + HSD_EDTRMKS;
        GLWin.AddButton(192, 56, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(244, 56, GLWIN_CLOSE, "Cancel");
        break;
      case kaCreateLinkLine:
      case kaDeleteLinkLine:  //create/delete start/end link line with selected host
        if (seltd)
        {
          if (lnkht)
          {
            if (lnkht != seltd)
            {
              goHosts = 1;
              while (goHosts == 1) usleep(1000);
              linkCreDel(lnkht, seltd, 0, (keyact == kaDeleteLinkLine));
              goHosts = 0;
            }
            lnkht = 0;
          }
          else lnkht = seltd;
        }
        else lnkht = 0;
        break;
      case kaAutoLinksAll:  //automatic link lines for all hosts
        setts.nhl = 1;
        hostsSet(4, 1);  //ht->alk
        osdUpdate();
        break;
      case kaToggleNewHostLinks:  //toggle automatic link lines for new hosts
        setts.nhl = !setts.nhl;
        osdUpdate();
        break;
      case kaDeleteAllLinks:  //delete link lines for all hosts
        setts.nhl = 0;
        hostsSet(4, 0);  //ht->alk
        goHosts = 1;
        while (goHosts == 1) usleep(1000);
        linksDestroy();
        goHosts = 0;
        osdUpdate();
        break;
      case kaShowAllPackets:  //show packets for all hosts
        setts.nhp = 1;
        hostsSet(3, 1);  //ht->shp
        osdUpdate();
        break;
      case kaToggleNewHostPackets:  //toggle show packets for new hosts
        setts.nhp = !setts.nhp;
        osdUpdate();
        break;
      case kaShowSensor1Packets:
        packetFilterSetSensor(1);
        pcktsDestroy(true);
        break;
      case kaShowSensor2Packets:
        packetFilterSetSensor(2);
        pcktsDestroy(true);
        break;
      case kaShowSensor3Packets:
        packetFilterSetSensor(3);
        pcktsDestroy(true);
        break;
      case kaShowSensor4Packets:
        packetFilterSetSensor(4);
        pcktsDestroy(true);
        break;
      case kaShowSensor5Packets:
        packetFilterSetSensor(5);
        pcktsDestroy(true);
        break;
      case kaShowSensor6Packets:
        packetFilterSetSensor(6);
        pcktsDestroy(true);
        break;
      case kaShowSensor7Packets:
        packetFilterSetSensor(7);
        pcktsDestroy(true);
        break;
      case kaShowSensor8Packets:
        packetFilterSetSensor(8);
        pcktsDestroy(true);
        break;
      case kaShowSensor9Packets:
        packetFilterSetSensor(9);
        pcktsDestroy(true);
        break;
      case kaShowAllSensorPackets:  //show packets from all hsen
        packetFilterResetAll();
        osdUpdate();
        break;
      case kaPrevSensorPackets:  //decrement hsen to show packets from
        {
          unsigned char sen = setts.sen;
          packetFilterResetAll();
          setts.sen = (sen > 1 ? (unsigned char)(sen - 1) : 9);
        }
        pcktsDestroy(true);
        break;
      case kaNextSensorPackets:  //increment hsen to show packets from
        {
          unsigned char sen = setts.sen;
          packetFilterResetAll();
          setts.sen = ((sen >= 1) && (sen < 9) ? (unsigned char)(sen + 1) : 1);
        }
        pcktsDestroy(true);
        break;
      case kaToggleBroadcasts:  //toggle broadcasts
        setts.bct = !setts.bct;
        if (setts.bct && setts.adh) setts.adh = 0;
        osdUpdate();
        break;
      case kaDecreasePacketLimit:  //decrement number of packets allowed on screen before drop
        if (!setts.pks) break;
        setts.pks -= 100;
        osdUpdate();
        break;
      case kaIncreasePacketLimit:  //increment number of packets allowed on screen before drop
        if (setts.pks < 1000000)
        {
          setts.pks += 100;
          osdUpdate();
        }
        break;
      case kaTogglePacketDestPort: setts.pdp = !setts.pdp; break;  //toggle packet destination port
      case kaTogglePacketSpeed:  //toggle packet speed
        setts.fsp = !setts.fsp;
        osdUpdate();
        break;
      case kaPacketsOff:  //packets off
        setts.nhp = 0;
        hostsSet(3, 0);  //ht->shp
        pcktsDestroy(true);
        break;
      case kaRecordPacketTraffic:  //packet traffic record
      {
        char recordPath[512], recordLabel[128];
        if (ptrc > hlt) ptrc = hlt;
        while (ptrc == hlt) usleep(1000);
        if (!packetTrafficRecordPathNext(recordPath, sizeof(recordPath), recordLabel, sizeof(recordLabel)))
        {
          messageBox("ERROR", "Unable to create next packet traffic file name.");
          break;
        }
        if ((pfile = fopen(recordPath, "wb")))
        {
          fputs("HP2", pfile);
          distm = 0;
          packetTrafficPathSet(recordPath);
          packetTrafficLabelSet(recordLabel);
          packetTrafficStarted = time(0);
          ptrc = rec;
        }
        else messageBox("ERROR", "Unable to create packet traffic file.");
        break;
      }
      case kaReplayPacketTraffic:  //packet traffic replay
      {
        char firstReplay[128], replayName[256];
        unsigned int files = packetTrafficReplayListCreate(hsddata("tmp-flist-hsd"), firstReplay, sizeof(firstReplay));
        if (!files)
        {
          messageBox("INFO", "No packet traffic recordings found in hsd-data.");
          break;
        }
        if (fileExists(hsddata(packetTrafficLabel))) setStringValue(replayName, sizeof(replayName), packetTrafficLabel);
        else setStringValue(replayName, sizeof(replayName), firstReplay);
        GLWin.CreateWin(-1, -1, 332, 234, "REPLAY PACKET TRAFFIC FILE...", true, true);
        GLWin.AddLabel(10, 10, "Choose Recording:");
        GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, replayName) * 100) + HSD_HPTRPY;
        GLWin.AddList(10, 52, 10, 50, hsddata("tmp-flist-hsd"));
        GLWin.AddButton(128, 60, GLWIN_OK, "Replay", false, true);
        GLWin.AddButton(70, 60, GLWIN_CLOSE, "Cancel", false, false);
        break;
      }
      case kaSkipReplayPacket: if (ptrc == rpy) offsetReset(); break;  //packet traffic replay - jump to next packet
      case kaStopPacketTraffic:  //packet traffic record/replay stop
        if (ptrc == rec) ptrc = hlt;
        else if (ptrc == rpy)
        {
          pcktsDestroy();
          ptrc = hlt;
          goHosts = 0;
        }
        break;
      case kaOpenPacketTrafficFile:
      case kaSavePacketTrafficFile:  //2D GUI open/save packet traffic file
      {
        char saveName[256];
        filelistCreate(hsddata("tmp-flist-hsd"), ".hpt");
        if (keyact == kaOpenPacketTrafficFile)
        {
          GLWin.CreateWin(-1, -1, 332, 234, "OPEN PACKET TRAFFIC FILE...", true, true);
          GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, "") * 100) + HSD_HPTOPEN;
        }
        else
        {
          GLWin.CreateWin(-1, -1, 332, 234, "SAVE PACKET TRAFFIC FILE AS...", true, true);
          setStringValue(saveName, sizeof(saveName), packetTrafficLabel);
          GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, saveName) * 100) + HSD_HPTSAVE;
        }
        GLWin.AddLabel(10, 10, "Enter File Name:");
        GLWin.AddList(10, 52, 10, 50, hsddata("tmp-flist-hsd"));
        GLWin.AddButton(128, 60, GLWIN_OK, "OK", false, true);
        GLWin.AddButton(76, 60, GLWIN_CLOSE, "Cancel", false, false);
        break;
      }
      case kaToggleAnimation: goAnim = !goAnim; break;  //toggle animation
      case kaAcknowledgeAllAnomalies: hostsSet(2, 0); break;  //acknowledge all anomalies
      case kaToggleOsd: setts.osd = !setts.osd; break;  //toggle OSD
      case kaExportSelectionCsv:  //export selection details in CSV file
        filelistCreate(hsddata("tmp-flist-hsd"), ".csv");
        GLWin.CreateWin(-1, -1, 332, 234, "EXPORT SELECTION DETAILS IN CSV FILE AS...", true, true);
        GLWin.AddLabel(10, 10, "Enter File Name:");
        GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, "") * 100) + HSD_EXPTCSV;
        GLWin.AddList(10, 52, 10, 50, hsddata("tmp-flist-hsd"));
        GLWin.AddButton(128, 60, GLWIN_OK, "OK", false, true);
        GLWin.AddButton(76, 60, GLWIN_CLOSE, "Cancel", false, false);
        break;
      case kaShowHostInformation:  //2D GUI selected host info
        if (!seltd) break;
        infoHost();
        GLWin.CreateWin(-2, -2, 352, 277, "HOST INFORMATION", true, true);
        GLWin.AddView(10, 10, 10, 10, 10, hsddata("tmp-hinfo-hsd"));
        GLResult[0] = HSD_HSTINFO;
        break;
      case kaShowSelectionInformation:  //2D GUI selection info
        infoSelection();
        GLWin.CreateWin(-2, -2, 352, 270, "SELECTION INFORMATION", true, true);
        GLWin.AddButton(10, 6, GLWIN_MISC1, "Selection");
        GLWin.AddButton(104, 6, GLWIN_MISC2, "General");
        GLWin.AddView(10, 42, 10, 10, 17, hsddata("tmp-hinfo-hsd"));
        GLResult[0] = 0;
        break;
      case kaShowHelp: helpOverlayToggle(); break;
      default:
        break;
    }
  }
  refresh = true;
}

static void triggerKeyAction(keyact_type action)
{
  if (keybinds[action].key) keyboardGL(keybinds[action].key, 2);
}

static keyact_type menuActionForValue(int value)
{
  switch (value)
  {
    case 40: return kaShowHostInformation;
    case 44: return kaEditHostname;
    case 45: return kaEditRemarks;
    case 46: return kaShowSelectionInformation;
    case 47: return kaAcknowledgeAllAnomalies;
    case 60: return kaShowAllPackets;
    case 69: return kaPacketsOff;
    case 70: return kaViewHome;
    case 71: return kaViewPos1;
    case 72: return kaViewPos2;
    case 73: return kaViewPos3;
    case 74: return kaViewPos4;
    case 75: return kaViewSave1;
    case 76: return kaViewSave2;
    case 77: return kaViewSave3;
    case 78: return kaViewSave4;
    case 81: return kaLayoutRestore1;
    case 82: return kaLayoutRestore2;
    case 83: return kaLayoutRestore3;
    case 84: return kaLayoutRestore4;
    case 86: return kaLayoutSave1;
    case 87: return kaLayoutSave2;
    case 88: return kaLayoutSave3;
    case 89: return kaLayoutSave4;
    case 79: return kaViewHomeAlt;
    case 93: return kaFindHosts;
    case 97: return kaShowHelp;
    default: return kaCount;
  }
}

static unsigned char hostZone(host_type *ht)
{
  if (!ht) return 0;
  if (ht->px < 0) return (ht->pz < 0 ? 3 : 2);
  return (ht->pz < 0 ? 4 : 1);
}

static void menuSelectionStateCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  menu_selection_state_type *state = (menu_selection_state_type *)(void *) arg1;
  unsigned char zone;
  (void) arg2;
  (void) arg3;
  (void) arg4;
  if (!ht->sld) return;
  if (!state->any)
  {
    state->any = true;
    state->lockOn = ht->lck;
    state->colour = ht->clr;
    state->zone = hostZone(ht);
    return;
  }
  if (state->lockOn != ((bool) ht->lck)) state->mixedLock = true;
  if (state->colour != ht->clr) state->mixedColour = true;
  zone = hostZone(ht);
  if (state->zone != zone) state->mixedZone = true;
}

static menu_selection_state_type menuSelectionState()
{
  menu_selection_state_type state = {0};
  hstsByIp.forEach(1, &menuSelectionStateCb, (long)(void *)&state, 0, 0, 0);
  return state;
}

static void hostDeleteManaged(host_type *ht)
{
  pckt_type *pk;
  alrt_type *al;
  if (!ht) return;
  if (seltd == ht) seltd = 0;
  if (lnkht == ht) lnkht = 0;
  pktsLL.Start(1);
  while ((pk = (pckt_type *)pktsLL.Read(1)))
  {
    if (pk->ht == ht)
    {
      delete pk;
      pktsLL.Delete(1);
    }
    else pktsLL.Next(1);
  }
  altsLL.Start(1);
  while ((al = (alrt_type *)altsLL.Read(1)))
  {
    if (al->ht == ht)
    {
      delete al;
      altsLL.Delete(1);
    }
    else altsLL.Next(1);
  }
  linkCreDel(ht, 0, 0, true);
  hostByPositionPreMove(ht);
  hostCollisionDetach(ht);
  hostDynamicStateByIp.erase(hostDynamicStateKey(ht->hip));
  hostRuntimeMetaByIp.erase(hostDynamicStateKey(ht->hip));
  delete ht;
  refresh = true;
}

typedef struct DynamicHostCleanupCbData_
{
  time_t now;
  bool deleted;
} DynamicHostCleanupCbData;

static void dynamicHostsCleanupCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  DynamicHostCleanupCbData *cleanup = (DynamicHostCleanupCbData *)(void *) arg1;

  if (!hostIsDynamic(ht) || ht->lck || ht->sld || !ht->lpk) return;
  if ((cleanup->now - ht->lpk) < (time_t) dynamicHostTtlSeconds) return;

  hostDeleteManaged(ht);
  *data = 0;
  cleanup->deleted = true;
}

static void dynamicHostsCleanupMaybe()
{
  DynamicHostCleanupCbData cleanup;
  time_t now;

  if (!dynamicHostsEnabled || goHosts) return;
  if (!dynamicHostTtlSeconds) dynamicHostTtlSeconds = DEFAULT_DYNAMIC_HOST_TTL_SECONDS;
  if (!dynamicHostCleanupIntervalSeconds) dynamicHostCleanupIntervalSeconds = DEFAULT_DYNAMIC_HOST_CLEANUP_INTERVAL_SECONDS;

  time(&now);
  if (dynamicHostLastCleanup && ((unsigned int)(now - dynamicHostLastCleanup) < dynamicHostCleanupIntervalSeconds)) return;
  dynamicHostLastCleanup = now;

  goHosts = 1;
  while (goHosts == 1) usleep(1000);

  cleanup.now = now;
  cleanup.deleted = false;
  hstsByIp.forEach(1, dynamicHostsCleanupCb, (long)(void *) &cleanup, 0, 0, 0);
  goHosts = 0;

  if (cleanup.deleted) osdUpdate();
}

void mnuKeyProcessLe9Cb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  int m = (int) arg1;
  bool *ptr_init = (bool *)(void *) arg2;
  int spx = 0, spy = 0, spz = 0;

  if (ht->sld && !ht->lck)
  {
    switch (m)
    {
    case 1:  //move selection to grey zone
      hostByPositionPreMove(ht);
      if (ht->px < 0) ht->px *= -1;
      if (ht->pz < 0) ht->pz *= -1;
      hostPos(ht, HPR, 1);
      hostByPositionPostMove(ht);
      moveCollision(ht);
      break;

    case 2:  //move selection to blue zone
      hostByPositionPreMove(ht);
      if (ht->px > 0) ht->px *= -1;
      if (ht->pz < 0) ht->pz *= -1;
      hostPos(ht, HPR, 1);
      hostByPositionPostMove(ht);
      moveCollision(ht);
      break;

    case 3:  //move selection to green zone
      hostByPositionPreMove(ht);
      if (ht->px > 0) ht->px *= -1;
      if (ht->pz > 0) ht->pz *= -1;
      hostPos(ht, HPR, 1);
      hostByPositionPostMove(ht);
      moveCollision(ht);
      break;

    case 4:  //move selection to red zone
      hostByPositionPreMove(ht);
      if (ht->px < 0) ht->px *= -1;
      if (ht->pz > 0) ht->pz *= -1;
      hostPos(ht, HPR, 1);
      hostByPositionPostMove(ht);
      moveCollision(ht);
      break;

    case 5: case 6: case 7:  //auto arrange
      if (*ptr_init)
      {
	hostByPositionPreMove(ht);
	hostPos(ht, (m == 5 ? HPR : 10), (m == 7 ? 2 : 1));
	hostByPositionPostMove(ht);
	moveCollision(ht);
	spx = ht->px;
	spy = ht->py;
	spz = ht->pz;
	*ptr_init = false;
      }
      else
      {
	hostByPositionPreMove(ht);
	ht->px = spx;
	ht->py = spy;
	ht->pz = spz;
	hostPos(ht, (m == 5 ? HPR : 10), (m == 7 ? 2 : 1));
	hostByPositionPostMove(ht);
	moveCollision(ht);
      }
      break;

    case 8:  //arrange into nets
      if (!(spx = hostNet(ht)))
	break;

      if (spx == 1) {
	hostByPositionPreMove(ht);
	hostPos(ht, HPR, 1);
	hostByPositionPostMove(ht);
      }
      moveCollision(ht);
      break;

    case 9:  //delete selection
      hostDeleteManaged(ht);
      *data = 0;
      break;
    }
  }
}

void mnuKeyProcessLe36Cb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  int m = (int) arg1;
  time_t itime = (time_t) arg2;

  unsigned char cnt;
  char scmd[274];

  switch (m)
  {
  case 31:
    ht->sld = ht->anm;
    break;  //select all anomalies

  case 32:
    ht->sld = ht->shp;
    break;  //select all showing packets

  case 33:
    ht->sld = ((itime - ht->lpk) > 300);
    break;  //5 minutes

  case 34:
    ht->sld = ((itime - ht->lpk) > 3600);
    break;  //1 hour

  case 35:
    ht->sld = ((itime - ht->lpk) > 86400);
    break;  //1 day

  case 36:
    ht->sld = ((itime - ht->lpk) > 604800);
    break;  //1 week

  default:
    if (!ht->sld)
      break;

    switch (m)
      {
      case 10:
      case 11:
      case 12:
      case 13:
      case 14:
      case 15:
      case 16:
      case 17:
      case 18:
      case 19:
	ht->clr = m % 10;
	break;  //colour

      case 21:
	ht->lck = 1;
	break;  //lock on

      case 22:
	ht->lck = 0;
	break;  //lock off

      case 23:
	ht->dld = 0;
	break;  //reset downloads

      case 24:
	ht->uld = 0;
	break;  //reset uploads

      case 25:
	for (cnt = 0; cnt < SVCS; cnt++)
	  ht->svc[cnt] = -1;
	break;  //reset services list

      case 26:
	ht->anm = 0;
	break;  //acknowledge anomaly

      case 27:
      case 28:
      case 29:
      case 30:
	if (!*setts.cmd[m - 27])
	  break;

	sprintf(scmd, "%s %s &", setts.cmd[m - 27], ht->htip);
	if (system(scmd)) break;  //nothing
	break;
      }
  }
}

//process 2D GUI menu item selected
void mnuProcess(int m)
{
  GLWin.Close();  //close all 2D GUI windows
  GLResult[0] = 0;
  if (!m) return;
  menuBuildDepth = menuDepthForId(m);
  if (m == 100) menuBaseX = mPsx;
  if (m <= 9)
  {
    waitShow();
    bool init = true;
    goHosts = 1;
    while (goHosts == 1) usleep(1000);
    if (m == 9)
    {
      seltd = 0;
      pcktsDestroy();
      alrtsDestroy();
    }
    hstsByIp.forEach(1, &mnuKeyProcessLe9Cb, (long) m, (long)(void *)&init, 0, 0);
    goHosts = 0;
    GLWin.Close();  //close all 2D GUI windows
  }
  else if (m <= 36)
  {
    if (m >= 31) seltd = 0;
    time_t itime = time(0);
    hstsByIp.forEach(1, &mnuKeyProcessLe36Cb, (long) m, (long) itime, 0, 0);
  }
  else
  {
    char sbuf[256] = "";
    switch (m)
    {
      case 37:
        GLWin.CreateWin(-1, -1, 254, 100, "SELECT INACTIVE >");
        GLWin.AddLabel(10, 15, "Days:");
        GLWin.AddLabel(88, 15, "Hours:");
        GLResult[1] = GLWin.AddInput(130, 10, 2, 2, "");
        GLWin.AddLabel(166, 15, "Minutes:");
        GLResult[2] = GLWin.AddInput(220, 10, 2, 2, "");
        GLResult[0] = (GLWin.AddInput(46, 10, 3, 3, "") * 100) + HSD_SLINACT;
        GLWin.AddButton(126, 40, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(178, 40, GLWIN_CLOSE, "Cancel");
        break;
      case 38:
        GLWin.CreateWin(-1, -1, 320, 210, "SET COMMANDS");
        GLWin.AddLabel(10, 10, "Host IP Address will be Appended to End of Command");
        GLWin.AddLabel(10, 35, "1:");
        GLWin.AddLabel(10, 65, "2:");
        GLResult[1] = GLWin.AddInput(28, 60, 45, 255, setts.cmd[1]);
        GLWin.AddLabel(10, 95, "3:");
        GLResult[2] = GLWin.AddInput(28, 90, 45, 255, setts.cmd[2]);
        GLWin.AddLabel(10, 125, "4:");
        GLResult[3] = GLWin.AddInput(28, 120, 45, 255, setts.cmd[3]);
        GLResult[0] = (GLWin.AddInput(28, 30, 45, 255, setts.cmd[0]) * 100) + HSD_SETCMDS;  //last for focus
        GLWin.AddButton(186, 150, GLWIN_OK, "Set", true, true);
        GLWin.AddButton(244, 150, GLWIN_CLOSE, "Cancel");
        break;
      case 39:  //reset selection link lines
        goHosts = 1;
        while (goHosts == 1) usleep(1000);
        lnkht = 0;
        link_type *lk;
        lnksLL.Start(1);
        while ((lk = (link_type *)lnksLL.Read(1)))
        {
          if (lk->sht->sld || lk->dht->sld)
          {
            delete lk;
            lnksLL.Delete(1);
          }
          else lnksLL.Next(1);
        }
        goHosts = 0;
        break;
      case 40: triggerKeyAction(kaShowHostInformation); break;  //2D GUI selected host info
      case 41:  //show selected host packets only
        if (!seltd) break;
        triggerKeyAction(kaPacketsOff);  //packets off
        seltd->shp = 1;  //show packets
        break;
      case 42:  //move selected host here
        if (!seltd) break;
        if (seltd->lck) break;
        goHosts = 1;
        while (goHosts == 1) usleep(1000);
	hostByPositionPreMove(seltd);
        seltd->px += (int)((setts.vws[0].ee.x - seltd->px) / SPC) * SPC;
        seltd->py += (int)((setts.vws[0].ee.y - seltd->py) / SPC) * SPC;
        seltd->pz += (int)((setts.vws[0].ee.z - seltd->pz) / SPC) * SPC;
	hostByPositionPostMove(seltd);
        moveCollision(seltd);
        goHosts = 0;
        break;
      case 43:  //go to selected host
        if (seltd)
        {
          view_type tview = {(seltd->pz < 0 ? 0.0 : 180.0),
			     0.0,
			     {(double)seltd->px,
			      seltd->py + 5.0,
			      (double)(seltd->pz + ((seltd->pz < 0 ? -16 : 16) * MOV))
			     },
			     {(double)seltd->px,
			      seltd->py + 5.0,
			      (double)(seltd->pz + ((seltd->pz < 0 ? -15 : 15) * MOV))
			     }
	  };
          setts.vws[0] = tview;
        }
        break;
      case 44: triggerKeyAction(kaEditHostname); break;  //edit selected host hostname
      case 45: triggerKeyAction(kaEditRemarks); break;  //edit selected host remarks
      case 46: triggerKeyAction(kaShowSelectionInformation); break;  //2D GUI selection info
      case 47: triggerKeyAction(kaAcknowledgeAllAnomalies); break;  //acknowledge all anomalies
      case 48:  //toggle anomaly detection
        setts.anm = !setts.anm;
        dAnom = setts.anm;
        break;
      case 50:  //show selection IPs/names
        setts.sips = sel;
        if (setts.sona == ipn) setts.sona = don;
        osdUpdate();
        break;
      case 51:  //show all IPs/names
        setts.sips = all;
        if (setts.sona == ipn) setts.sona = don;
        osdUpdate();
        break;
      case 52:  //on-active show IP/name
        setts.sona = ipn;
        osdUpdate();
        break;
      case 54: hostsSet(1, 0);  //show no IPs/names, including persistant, ht->pip
      case 53:  //show IPs/names off
        setts.sips = off;
        if (setts.sona == ipn) setts.sona = don;
        osdUpdate();
        break;
      case 55:  //on-active alert
        setts.sona = alt;
        osdUpdate();
        break;
      case 56:  //on-active show host
        setts.sona = hst;
        osdUpdate();
        break;
      case 57:  //on-active select host
        setts.sona = slt;
        osdUpdate();
        break;
      case 58:  //on-active do nothing
        setts.sona = don;
        osdUpdate();
        break;
      case 60: triggerKeyAction(kaShowAllPackets); break;  //show packets for all hosts
      case 61:  //legacy all protocols / show all tree filters
      case 139: packetFilterResetAll(); osdUpdate(); break;
      case 140: packetFilterSetSensor(1); pcktsDestroy(true); break;
      case 141: packetFilterSetSensor(2); pcktsDestroy(true); break;
      case 142: packetFilterSetSensor(3); pcktsDestroy(true); break;
      case 143: packetFilterSetSensor(4); pcktsDestroy(true); break;
      case 144: packetFilterSetSensor(5); pcktsDestroy(true); break;
      case 145: packetFilterSetSensor(6); pcktsDestroy(true); break;
      case 146: packetFilterSetSensor(7); pcktsDestroy(true); break;
      case 147: packetFilterSetSensor(8); pcktsDestroy(true); break;
      case 148: packetFilterSetSensor(9); pcktsDestroy(true); break;
      case 62: packetFilterSetTree(pfIcmpAll); pcktsDestroy(true); break;  //legacy ICMP
      case 63: packetFilterSetTree(pfTcpAll); pcktsDestroy(true); break;  //legacy TCP
      case 64: packetFilterSetTree(pfUdpAll); pcktsDestroy(true); break;  //legacy UDP
      case 65: packetFilterSetTree(pfArpAll); pcktsDestroy(true); break;  //legacy ARP
      case 133: packetFilterSetTree(pfOtherAll); pcktsDestroy(true); break;  //legacy OTHER
      case 67:  //show all ports packets
        packetFilterResetAll();
        osdUpdate();
        break;
      case 68:  //enter port to show packets for
        GLWin.CreateWin(-1, -1, 158, 100, "PACKETS PORT");
        GLWin.AddLabel(10, 15, "Enter Port No.:");
        GLResult[0] = (GLWin.AddInput(106, 10, 5, 5, "") * 100) + HSD_PKTSPRT;
        GLWin.AddButton(30, 40, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(82, 40, GLWIN_CLOSE, "Cancel");
        break;
      case 127: packetFilterResetAll(); osdUpdate(); break;  //legacy form all
      case 128: packetFilterSetTree(pfTcpControl); pcktsDestroy(true); break;  //legacy cube
      case 129: packetFilterSetTree(pfTcpPayload); pcktsDestroy(true); break;  //legacy double
      case 130: packetFilterSetTree(pfTcpLargePayload); pcktsDestroy(true); break;  //legacy triple
      case 131: packetFilterSetTree(pfUdpDiscovery); pcktsDestroy(true); break;  //legacy sphere
      case 132: packetFilterSetTree(pfIcmpAll); pcktsDestroy(true); break;  //legacy pyramid
      case 135: packetFilterResetAll(); osdUpdate(); break;  //legacy tcp control all
      case 136: packetFilterSetTree(pfTcpSetupFinish); pcktsDestroy(true); break;
      case 137: packetFilterSetTree(pfTcpReset); pcktsDestroy(true); break;
      case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156:
      case 157: case 158: case 159: case 160: case 161: case 162: case 163: case 164:
      case 165: case 166: case 167:
      {
        unsigned int rowsCount = 0;
        const packet_tree_row_type *rows = packetTreeRows(&rowsCount);
        unsigned int idx = (unsigned int)(m - 149);
        if (idx < rowsCount)
        {
          if (rows[idx].filter == pfAll)
          {
            packetFilterResetAll();
            osdUpdate();
          }
          else
          {
            packetFilterSetTree(rows[idx].filter);
            pcktsDestroy(true);
          }
        }
        break;
      }
      case 69: triggerKeyAction(kaPacketsOff); break;  //packets off
      case 70: triggerKeyAction(kaViewHome); break;  //recall home view
      case 71: triggerKeyAction(kaViewPos1); break;  //recall view position 1
      case 72: triggerKeyAction(kaViewPos2); break;  //recall view position 2
      case 73: triggerKeyAction(kaViewPos3); break;  //recall view position 3
      case 74: triggerKeyAction(kaViewPos4); break;  //recall view position 4
      case 75: case 76: case 77: case 78: memcpy(&setts.vws[m - 74], &setts.vws[0], sizeof(view_type)); break;  //save current view as view position 1-4
      case 79: triggerKeyAction(kaViewHomeAlt); break;  //recall alternate home view
      case 80: case 85:  //2D GUI open/save network layout file
        filelistCreate(hsddata("tmp-flist-hsd"), ".hnl");
        if (m == 80)
        {
          GLWin.CreateWin(-1, -1, 332, 234, "OPEN NETWORK LAYOUT FILE...", true, true);
          GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, "") * 100) + HSD_HNLOPEN;
        }
        else
        {
          GLWin.CreateWin(-1, -1, 332, 234, "SAVE NETWORK LAYOUT FILE AS...", true, true);
          GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, "") * 100) + HSD_HNLSAVE;
        }
        GLWin.AddLabel(10, 10, "Enter File Name:");
        GLWin.AddList(10, 52, 10, 50, hsddata("tmp-flist-hsd"));
        GLWin.AddButton(128, 60, GLWIN_OK, "OK", false, true);
        GLWin.AddButton(76, 60, GLWIN_CLOSE, "Cancel", false, false);
        break;
      case 81: netLoad(hsddata("1network.hnl")); netposExactHostsSync(); break;  //restore network layout 1
      case 82: netLoad(hsddata("2network.hnl")); netposExactHostsSync(); break;  //restore network layout 2
      case 83: netLoad(hsddata("3network.hnl")); netposExactHostsSync(); break;  //restore network layout 3
      case 84: netLoad(hsddata("4network.hnl")); netposExactHostsSync(); break;  //restore network layout 4
      case 86: netSave(hsddata("1network.hnl")); break;  //save current network layout as network layout 1
      case 87: netSave(hsddata("2network.hnl")); break;  //save current network layout as network layout 2
      case 88: netSave(hsddata("3network.hnl")); break;  //save current network layout as network layout 3
      case 89: netSave(hsddata("4network.hnl")); break;  //save current network layout as network layout 4
      case 90: case 91:  //2D GUI netpos editor
        if ((m == 91) && seltd)
	  sprintf(sbuf, "pos %s/32 %d %d %d %s",
		  seltd->htip,
		  seltd->px / SPC,
		  seltd->py / SPC,
		  seltd->pz / SPC,
		  tclr[seltd->clr]);  //add selected host net pos
        GLWin.CreateWin(-1, -1, 332, 234, "NET POSITIONS", true, true);
        GLWin.AddLabel(10, 10, "Line:");
        GLResult[0] = (GLWin.AddInput(10, 26, 50, 255, sbuf) * 100) + HSD_NETPOS;
        GLWin.AddList(10, 52, 10, 50, hsddata("netpos.txt"));
        GLWin.AddButton(292, 60, GLWIN_ITMUP, "!u", false, false);  //item up
        GLWin.AddButton(263, 60, GLWIN_ITMDN, "!d", false, false);  //item down
        GLWin.AddButton(204, 60, GLWIN_OK, "Add", false, true);
        GLWin.AddButton(146, 60, GLWIN_DEL, "Delete", false, false);
        break;
      case 92:  //clear network layout
        allDestroy();
        goHosts = 0;
        break;
      case 93: triggerKeyAction(kaFindHosts); break;  //2D GUI find hosts
      case 94:  //start local hsen
        localHsenDialogOpen();
        break;
      case 95:  //stop local hsen
        localHsenStopManaged(true);
        break;
      case 96:  //resolve hostnames for selection
      {
        ResolveHostnamesCbData rd = {0, 0, 0, 0};
        char mbuf[160];
        waitShow();
        hstsByIp.forEach(1, &resolveSelectedHostnamesCb, (long)(void *)&rd, 0, 0, 0);
        GLWin.Close();
        if (!rd.selected) messageBox("HOSTNAMES", "No hosts are selected.");
        else
        {
          snprintf(mbuf, sizeof(mbuf), "Selected: %u  Resolved: %u  Already named: %u  Unresolved: %u",
                   rd.selected, rd.resolved, rd.existing, rd.failed);
          messageBox("HOSTNAMES", mbuf);
        }
        break;
      }
      case 97:
        GLWin.Close();
        helpOverlayToggle();
        break;
      case 98:  //about
        GLWin.CreateWin(-1, -1, 230, 138, "ABOUT");
        GLWin.AddLabel(10, 10, "Hosts3D " HSD_VERSION_STR);
        GLWin.AddLabel(10, 26, "Copyright (c) 2006-2011  Del Castle");
        GLWin.AddLabel(10, 42, "http://hosts3d.sourceforge.net");
        GLWin.AddLabel(10, 58, "Community-maintained continuation");
        GLWin.AddBitmap(11, 76, red[0], red[1], red[2], bitmaps[0]);
        GLWin.AddBitmap(19, 76, red[0], red[1], red[2], bitmaps[1]);
        GLWin.AddBitmap(11, 84, red[0], red[1], red[2], bitmaps[2]);
        GLWin.AddBitmap(19, 84, red[0], red[1], red[2], bitmaps[3]);
        GLWin.AddBitmap(11, 92, red[0], red[1], red[2], bitmaps[4]);
        GLWin.AddBitmap(11, 100, red[0], red[1], red[2], bitmaps[6]);
        GLWin.AddBitmap(11, 76, black[0], black[1], black[2], bitmaps[8]);
        GLWin.AddBitmap(19, 76, black[0], black[1], black[2], bitmaps[9]);
        GLWin.AddBitmap(11, 84, black[0], black[1], black[2], bitmaps[10]);
        GLWin.AddBitmap(19, 84, black[0], black[1], black[2], bitmaps[11]);
        GLWin.AddBitmap(10, 92, black[0], black[1], black[2], bitmaps[12]);
        GLWin.AddBitmap(11, 100, black[0], black[1], black[2], bitmaps[14]);
        GLWin.AddBitmap(25, 76, green[0], green[1], green[2], bitmaps[0]);
        GLWin.AddBitmap(33, 76, green[0], green[1], green[2], bitmaps[1]);
        GLWin.AddBitmap(25, 84, green[0], green[1], green[2], bitmaps[2]);
        GLWin.AddBitmap(33, 84, green[0], green[1], green[2], bitmaps[3]);
        GLWin.AddBitmap(33, 92, green[0], green[1], green[2], bitmaps[5]);
        GLWin.AddBitmap(33, 100, green[0], green[1], green[2], bitmaps[7]);
        GLWin.AddBitmap(25, 76, black[0], black[1], black[2], bitmaps[8]);
        GLWin.AddBitmap(33, 76, black[0], black[1], black[2], bitmaps[9]);
        GLWin.AddBitmap(25, 84, black[0], black[1], black[2], bitmaps[10]);
        GLWin.AddBitmap(33, 84, black[0], black[1], black[2], bitmaps[11]);
        GLWin.AddBitmap(34, 92, black[0], black[1], black[2], bitmaps[13]);
        GLWin.AddBitmap(33, 100, black[0], black[1], black[2], bitmaps[15]);
        GLWin.AddBitmap(18, 76, yellow[0], yellow[1], yellow[2], bitmaps[0]);
        GLWin.AddBitmap(26, 76, yellow[0], yellow[1], yellow[2], bitmaps[1]);
        GLWin.AddBitmap(18, 84, yellow[0], yellow[1], yellow[2], bitmaps[2]);
        GLWin.AddBitmap(26, 84, yellow[0], yellow[1], yellow[2], bitmaps[3]);
        GLWin.AddBitmap(18, 92, yellow[0], yellow[1], yellow[2], bitmaps[4]);
        GLWin.AddBitmap(26, 92, yellow[0], yellow[1], yellow[2], bitmaps[5]);
        GLWin.AddBitmap(18, 100, yellow[0], yellow[1], yellow[2], bitmaps[6]);
        GLWin.AddBitmap(26, 100, yellow[0], yellow[1], yellow[2], bitmaps[7]);
        GLWin.AddBitmap(18, 76, black[0], black[1], black[2], bitmaps[8]);
        GLWin.AddBitmap(26, 76, black[0], black[1], black[2], bitmaps[9]);
        GLWin.AddBitmap(18, 84, black[0], black[1], black[2], bitmaps[10]);
        GLWin.AddBitmap(26, 84, black[0], black[1], black[2], bitmaps[11]);
        GLWin.AddBitmap(17, 92, black[0], black[1], black[2], bitmaps[12]);
        GLWin.AddBitmap(27, 92, black[0], black[1], black[2], bitmaps[13]);
        GLWin.AddBitmap(18, 100, black[0], black[1], black[2], bitmaps[14]);
        GLWin.AddBitmap(26, 100, black[0], black[1], black[2], bitmaps[15]);
        GLResult[0] = 0;
        break;
      case 99: goRun = false; break;  //exit
      case 100:
        addMenuItem("MAIN", 12, 0, 0);
        if (seltd) addMenuItem("Selected", 12, 1, 101, 'D');
        addMenuItem("Selection", 12, 1, 102, 'S');
        addMenuItem("Anomalies", 12, 1, 103, 'A');
        addMenuItem("IP/Name", 12, 1, 104, 'N');
        addMenuItem("Packets", 12, 1, 105, 'P');
        addMenuItem("On-Active", 12, 1, 106, 'O');
        addMenuItem("View", 12, 1, 107, 'V');
        addMenuItem("Layout", 12, 1, 108, 'L');
        addMenuItem("Other", 12, 1, 109, 'T');
        addMenuItem("Local hsen", 12, 0, 94, 'H');
        if (fullscn) addMenuItem("Exit", 12, 0, 99, 'X');
        break;
      case 101:
        addMenuItem("SELECTED", 8, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Information", 8, 0, 40, 0, kaShowHostInformation);
        addMenuItem("Show Packets Only", 8, 0, 41, 'P');
        addMenuItem("Move Here", 8, 0, 42, 'M');
        addMenuItem("Go To", 8, 0, 43, 'G');
        addMenuItem("Hostname", 8, 0, 44, 0, kaEditHostname);
        addMenuItem("Remarks", 8, 0, 45, 0, kaEditRemarks);
        addMenuItem("Add Net Position", 8, 0, 91, 'A');
        break;
      case 102:
        addMenuItem("SELECTION", 10, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Information", 10, 0, 46, 0, kaShowSelectionInformation);
        addMenuItem("Resolve Hostnames", 10, 0, 96, 'H');
        addMenuItem("Colour", 10, 1, 110, 'C');
        addMenuItem("Lock", 10, 1, 111, 'L');
        addMenuItem("Move To Zone", 10, 1, 112, 'M');
        addMenuItem("Arrange", 10, 1, 113, 'A');
        addMenuItem("Commands", 10, 1, 114, 'O');
        addMenuItem("Reset", 10, 1, 115, 'R');
        addMenuItem("Delete", 10, 1, 116, 'X');
        break;
      case 103:
        addMenuItem("ANOMALIES", 4, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Select All", 4, 0, 31, 'S');
        addMenuItem("Acknowledge", 4, 1, 117, 'K');
        addMenuItem("Toggle Detection", 4, 0, 48, 'T', kaCount, msToggle, setts.anm);
        break;
      case 104:
      {
        addMenuItem("IP/NAME", 6, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Show Selection", 6, 0, 50, 'S', kaCount, msChoice, (setts.sips == sel));
        addMenuItem("Show All", 6, 0, 51, 'A', kaCount, msChoice, (setts.sips == all));
        addMenuItem("Show On-Active", 6, 0, 52, 'O', kaCount, msChoice, (setts.sona == ipn));
        addMenuItem("Show Off", 6, 0, 53, 'F', kaCount, msChoice, ((setts.sips == off) && (setts.sona != ipn)));
        addMenuItem("All Off", 6, 0, 54, 'X');
        break;
      }
      case 105:
        addMenuItem("PACKETS", 9, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Show All", 7, 0, 60, 0, kaShowAllPackets);
        addMenuItem("Sensor", 9, 1, 138, 'N', kaCount, msChoice, (setts.sen != 0));
        addMenuItem("Filter", 9, 1, 118, 'F', kaCount, msChoice, (packetTreeFilter != pfAll));
        addMenuItem("Port", 9, 1, 119, 'T', kaCount, msChoice, (setts.prt != 0));
        addMenuItem("Select Showing", 9, 0, 32, 'S');
        addMenuItem("Off", 9, 0, 69, 0, kaPacketsOff);
        break;
      case 106:
        addMenuItem("ON-ACTIVE", 6, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Alert", 6, 0, 55, 'A', kaCount, msChoice, (setts.sona == alt));
        addMenuItem("Show IP/Name", 6, 0, 52, 'N', kaCount, msChoice, (setts.sona == ipn));
        addMenuItem("Show Host", 6, 0, 56, 'H', kaCount, msChoice, (setts.sona == hst));
        addMenuItem("Select", 6, 0, 57, 'S', kaCount, msChoice, (setts.sona == slt));
        addMenuItem("Do Nothing", 6, 0, 58, 'D', kaCount, msChoice, (setts.sona == don));
        break;
      case 107:
        addMenuItem("VIEW", 3, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Recall", 3, 1, 120, 'R');
        addMenuItem("Save", 3, 1, 121, 'S');
        break;
      case 108:
        addMenuItem("LAYOUT", 5, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Restore", 5, 1, 122, 'R');
        addMenuItem("Save", 5, 1, 123, 'S');
        addMenuItem("Net Positions", 5, 0, 90, 'N');
        addMenuItem("Clear", 5, 1, 124, 'C');
        break;
      case 109:
        addMenuItem("OTHER", 5, 2, 100, GLFW_KEY_BACKSPACE);
        addMenuItem("Find Hosts", 5, 0, 93, 0, kaFindHosts);
        addMenuItem("Select Inactive", 5, 1, 125, 'I');
        addMenuItem("Help", 5, 0, 97, 0, kaShowHelp);
        addMenuItem("About", 5, 0, 98, 'A');
        break;
      case 110:
      {
        menu_selection_state_type state = menuSelectionState();
        addMenuItem("COLOUR", 11, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("Grey", 11, 0, 10, 'G', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 0)));
        addMenuItem("Orange", 11, 0, 11, 'O', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 1)));
        addMenuItem("Yellow", 11, 0, 12, 'Y', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 2)));
        addMenuItem("Fluro", 11, 0, 13, 'F', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 3)));
        addMenuItem("Green", 11, 0, 14, 'E', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 4)));
        addMenuItem("Mint", 11, 0, 15, 'M', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 5)));
        addMenuItem("Aqua", 11, 0, 16, 'A', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 6)));
        addMenuItem("Blue", 11, 0, 17, 'B', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 7)));
        addMenuItem("Purple", 11, 0, 18, 'P', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 8)));
        addMenuItem("Violet", 11, 0, 19, 'V', kaCount, msChoice, (state.any && !state.mixedColour && (state.colour == 9)));
        break;
      }
      case 111:
      {
        menu_selection_state_type state = menuSelectionState();
        addMenuItem("LOCK", 3, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("On", 3, 0, 21, 'O', kaCount, msChoice, (state.any && !state.mixedLock && state.lockOn));
        addMenuItem("Off", 3, 0, 22, 'F', kaCount, msChoice, (state.any && !state.mixedLock && !state.lockOn));
        break;
      }
      case 112:
      {
        menu_selection_state_type state = menuSelectionState();
        addMenuItem("MOVE TO ZONE", 5, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("Grey", 5, 0, 1, 'G', kaCount, msChoice, (state.any && !state.mixedZone && (state.zone == 1)));
        addMenuItem("Blue", 5, 0, 2, 'B', kaCount, msChoice, (state.any && !state.mixedZone && (state.zone == 2)));
        addMenuItem("Green", 5, 0, 3, 'N', kaCount, msChoice, (state.any && !state.mixedZone && (state.zone == 3)));
        addMenuItem("Red", 5, 0, 4, 'R', kaCount, msChoice, (state.any && !state.mixedZone && (state.zone == 4)));
        break;
      }
      case 113:
        addMenuItem("ARRANGE", 5, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("Default", 5, 0, 5, 'D');
        addMenuItem("10x10", 5, 0, 6, '1');
        addMenuItem("10x10 2xSpc", 5, 0, 7, '2');
        addMenuItem("Into Nets", 5, 0, 8, 'N');
        break;
      case 114:
        addMenuItem("COMMANDS", 6, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("Command 1", 6, 0, 27, '1');
        addMenuItem("Command 2", 6, 0, 28, '2');
        addMenuItem("Command 3", 6, 0, 29, '3');
        addMenuItem("Command 4", 6, 0, 30, '4');
        addMenuItem("Set", 6, 0, 38, 'S');
        break;
      case 115:
        addMenuItem("RESET", 5, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("Link Lines", 5, 0, 39, 'L');
        addMenuItem("Downloads", 5, 0, 23, 'D');
        addMenuItem("Uploads", 5, 0, 24, 'U');
        addMenuItem("Services", 5, 0, 25, 'S');
        break;
      case 116:
        addMenuItem("DELETE", 2, 2, 102, GLFW_KEY_BACKSPACE);
        addMenuItem("Confirm", 2, 0, 9, 'C');
        break;
      case 117:
        addMenuItem("ACKNOWLEDGE", 3, 2, 103, GLFW_KEY_BACKSPACE);
        addMenuItem("Selection", 3, 0, 26, 'S');
        addMenuItem("All", 3, 0, 47, 0, kaAcknowledgeAllAnomalies);
        break;
      case 118:
      {
        unsigned int rowsCount = 0;
        const packet_tree_row_type *rows = packetTreeRows(&rowsCount);
        addMenuItem("FILTER", (int)rowsCount + 1, 2, 105, GLFW_KEY_BACKSPACE);
        for (unsigned int cnt = 0; cnt < rowsCount; cnt++)
        {
          char label[64];
          snprintf(label, sizeof(label), "%s%s", (rows[cnt].indent ? " " : ""), rows[cnt].label);
          addMenuItem(label, (int)rowsCount + 1, 0, 149 + (int)cnt, 0, kaCount, msChoice,
                      ((rows[cnt].filter == pfAll) ? packetFilterAllActive() : packetTreeFilterActive(rows[cnt].filter)));
        }
        break;
      }
      case 119:
      {
        char portLabel[32];
        if (setts.prt) snprintf(portLabel, sizeof(portLabel), "Enter... [%u]", setts.prt);
        else strcpy(portLabel, "Enter...");
        addMenuItem("PORT", 3, 2, 105, GLFW_KEY_BACKSPACE);
        addMenuItem("All", 3, 0, 67, 'A', kaCount, msChoice, packetFilterAllActive());
        addMenuItem(portLabel, 3, 0, 68, 'E', kaCount, msChoice, (setts.prt != 0) && !setts.sen && (packetTreeFilter == pfAll));
        break;
      }
      case 138:
        addMenuItem("SENSOR", 11, 2, 105, GLFW_KEY_BACKSPACE);
        addMenuItem("All", 11, 0, 139, 'A', kaCount, msChoice, packetFilterAllActive());
        addMenuItem("Sensor 1", 11, 0, 140, '1', kaCount, msChoice, (setts.sen == 1) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 2", 11, 0, 141, '2', kaCount, msChoice, (setts.sen == 2) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 3", 11, 0, 142, '3', kaCount, msChoice, (setts.sen == 3) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 4", 11, 0, 143, '4', kaCount, msChoice, (setts.sen == 4) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 5", 11, 0, 144, '5', kaCount, msChoice, (setts.sen == 5) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 6", 11, 0, 145, '6', kaCount, msChoice, (setts.sen == 6) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 7", 11, 0, 146, '7', kaCount, msChoice, (setts.sen == 7) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 8", 11, 0, 147, '8', kaCount, msChoice, (setts.sen == 8) && !setts.prt && (packetTreeFilter == pfAll));
        addMenuItem("Sensor 9", 11, 0, 148, '9', kaCount, msChoice, (setts.sen == 9) && !setts.prt && (packetTreeFilter == pfAll));
        break;
      case 120:
        addMenuItem("RECALL", 7, 2, 107, GLFW_KEY_BACKSPACE);
        addMenuItem("Home", 7, 0, 70, 0, kaViewHome);
        addMenuItem("Alternate Home", 7, 0, 79, 0, kaViewHomeAlt);
        addMenuItem("Pos 1", 7, 0, 71, 0, kaViewPos1);
        addMenuItem("Pos 2", 7, 0, 72, 0, kaViewPos2);
        addMenuItem("Pos 3", 7, 0, 73, 0, kaViewPos3);
        addMenuItem("Pos 4", 7, 0, 74, 0, kaViewPos4);
        break;
      case 121:
        addMenuItem("SAVE", 5, 2, 107, GLFW_KEY_BACKSPACE);
        addMenuItem("Pos 1", 5, 0, 75, '1');
        addMenuItem("Pos 2", 5, 0, 76, '2');
        addMenuItem("Pos 3", 5, 0, 77, '3');
        addMenuItem("Pos 4", 5, 0, 78, '4');
        break;
      case 122:
        addMenuItem("RESTORE", 6, 2, 108, GLFW_KEY_BACKSPACE);
        addMenuItem("File", 6, 0, 80, 'F');
        addMenuItem("Net 1", 6, 0, 81, '1');
        addMenuItem("Net 2", 6, 0, 82, '2');
        addMenuItem("Net 3", 6, 0, 83, '3');
        addMenuItem("Net 4", 6, 0, 84, '4');
        break;
      case 123:
        addMenuItem("SAVE", 6, 2, 108, GLFW_KEY_BACKSPACE);
        addMenuItem("File", 6, 0, 85, 'F');
        addMenuItem("Net 1", 6, 0, 86, '1');
        addMenuItem("Net 2", 6, 0, 87, '2');
        addMenuItem("Net 3", 6, 0, 88, '3');
        addMenuItem("Net 4", 6, 0, 89, '4');
        break;
      case 124:
        addMenuItem("CLEAR", 2, 2, 108, GLFW_KEY_BACKSPACE);
        addMenuItem("Confirm", 2, 0, 92, 'C');
        break;
      case 125:
        addMenuItem("SELECT INACTIVE", 6, 2, 109, GLFW_KEY_BACKSPACE);
        addMenuItem("> 5 Minutes", 6, 0, 33, '5');
        addMenuItem("> 1 Hour", 6, 0, 34, 'H');
        addMenuItem("> 1 Day", 6, 0, 35, 'D');
        addMenuItem("> 1 Week", 6, 0, 36, 'W');
        addMenuItem("> Other", 6, 0, 37, 'O');
        break;
    }
  }
}

void infoGeneralCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *ht = *((host_type **) data);
  unsigned int *ptrcnt = (unsigned int *)(void *) arg1;
  unsigned long long *ptrsdld = (unsigned long long *) arg2;
  unsigned long long *ptrsuld = (unsigned long long *) arg3;

  if (ht->sld)
  {
    (*ptrsdld) += ht->dld;
    (*ptrsuld) += ht->uld;
    (*ptrcnt)++;
  }
}

//generate general info
void infoGeneral()
{
  FILE *info;
  if ((info = fopen(hsddata("tmp-hinfo-hsd"), "w")))
  {
    unsigned int cnt = 0;
    unsigned long long sdld = 0, suld = 0;
    char buf[11];
    hstsByIp.forEach(1, &infoGeneralCb, (long)(void *) &cnt,
		     (long)(void *) &sdld, (long)(void *) &suld, 0);
    fprintf(info, "GENERAL\n\nHosts: %u\nDownloads: %s", cnt, formatBytes(sdld, buf));
    fprintf(info, "\nUploads: %s", formatBytes(suld, buf));  //reuse buf
    fclose(info);
  }
}

static inline host_type * hostTypeFromName (GLuint name)
{
  if (useIpAddrForGlName) {
    in_addr ip;
    ip.s_addr = name;
    return hostIP(ip, false);
  } else {
    return (host_type *) name;
  }
}

//glfwSetMouseButtonCallback
void GLFWCALL clickGL(int button, int action)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT)
  {
    if (GLWin.On())  //if 2D GUI displayed
    {
      int gs = GLWin.Select(!action);  //2D GUI object clicked
      if (action) mMove = ((gs >= GLWIN_TITLE) && (gs <= GLWIN_RBAR));  //2D GUI move
      else
      {
        mMove = false;
        switch (gs)
        {
          case GLWIN_OK: case GLWIN_DEL: case GLWIN_CLOSE: case GLWIN_ITMUP: case GLWIN_ITMDN: btnProcess(gs); break;  //process 2D GUI button selected
          case GLWIN_MNUITM: mnuProcess(GLWin.GetSelected()); break;
          case GLWIN_MISC1:
          case GLWIN_MISC2:
            if ((GLResult[0] % 100) == HSD_HSENRUN) btnProcess(gs);
            else
            {
              GLWin.Scroll();  //start
              if (gs == GLWIN_MISC1) infoSelection();  //generate selection info
              else infoGeneral();  //generate general info
            }
            break;
        }
      }
    }
    else if (action && setts.osd && (osdRowHitProcess(mPsx, hWin - mPsy) || osdPacketHitProcess(mPsx, hWin - mPsy)))
    {
      mMove = false;
    }
    else if (action)  //start selection box
    {
      mPsy = hWin - mPsy;
      mBxx = mPsx;
      mBxy = mPsy;
      mMove = true;
    }
    else
    {
      mMove = false;
      if (hstsByIp.Num())
      {
        if (!glfwGetKey(GLFW_KEY_LCTRL) && !glfwGetKey(GLFW_KEY_RCTRL)) hostsSet(0, 0);  //ht->sld
        int sx = mBxx - mPsx, sy = mBxy - mPsy, asx = abs(sx), asy = abs(sy);
        GLint hits, vpt[4];
        GLuint selbuf[SELBUF];
        glGetIntegerv(GL_VIEWPORT, vpt);
        glSelectBuffer(SELBUF, selbuf);
        glRenderMode(GL_SELECT);
        glInitNames();
        glPushName(0);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPickMatrix((GLdouble)mBxx - ((GLdouble)sx / 2.0), (GLdouble)mBxy - ((GLdouble)sy / 2.0), (asx < 3 ? 3 : asx), (asy < 3 ? 3 : asy), vpt);
        gluPerspective(45.0, (GLdouble)wWin / (GLdouble)hWin, 1.0, DEPTH);
        glMatrixMode(GL_MODELVIEW);
        hostsDraw(GL_SELECT);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        if ((hits = glRenderMode(GL_RENDER)))
        {
          host_type *ht;
          GLuint names, minZ = 0xffffffff, *ptr = (GLuint *)selbuf, *closest = 0, *ptrnames;
          for (; hits; hits--)
          {
            names = *ptr;
            ptr++;
            if (*ptr < minZ)
            {
              minZ = *ptr;
              closest = ptr + 2;
            }
            if ((asx >= 3) || (asy >= 3))  //multi-select
            {
              ptrnames = ptr + 2;
              if (*ptrnames)
              {
		ht = hostTypeFromName(*ptrnames);
                ht->sld = 1;
              }
              else seltd->sld = 1;
            }
            ptr += names + 2;
          }
          if ((asx < 3) && (asy < 3))  //single-select
          {
            if (*closest)
            {
	      ht = hostTypeFromName(*closest);
              if (ht->sld)
              {
                ht->sld = 0;
                if (ht->col)  //deselect all in cluster
                {
                  host_type *tht = ht->col;
                  while (tht != ht)
                  {
                    tht->sld = 0;
                    tht = tht->col;
                  }
                }
                seltd = 0;
              }
              else
              {
                seltd = ht;
                seltd->sld = 1;
                hostDetails();
              }
            }
            else if ((ht = seltd->col))
            {
              seltd->sld = 0;
              seltd = ht;  //cycle thru hosts in cluster
              seltd->sld = 1;
              hostDetails();
            }
            else if (seltd->pip)
            {
              seltd->sld = 0;
              seltd->pip = 0;
              seltd = 0;
            }
            else
            {
              seltd->sld = 1;
              seltd->pip = 1;  //activate persistant IP
            }
          }
          else seltd = 0;
        }
        else seltd = 0;
      }
    }
  }
  else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mView = action;  //start move view
  else if (!GLWin.On()) mnuProcess(100);
  refresh = true;
}

//glfwSetMouseWheelCallback
void GLFWCALL wheelGL(int pos)
{
  if (GLWin.On()) GLWin.Scroll((pos > mWhl ? GLWIN_UP : GLWIN_DOWN), false);  //scroll text in 2D GUI
  else if (helpOverlayMouseOver()) helpOverlayScrollDelta((pos > mWhl ? -1 : 1));
  else  //move view up/down
  {
    setts.vws[0].ee.y += (pos > mWhl ? 1 : -1) * MOV;
    setts.vws[0].dr.y += (pos > mWhl ? 1 : -1) * MOV;
  }
  mWhl = pos;
  refresh = true;
}

//glfwSetMousePosCallback
void GLFWCALL motionGL(int x, int y)
{
  int gy = hWin - y;
  GLWin.MousePos(x, gy);
  if (mMove)
  {
    if (GLWin.On()) GLWin.Motion(x - mPsx, y - mPsy);  //2D GUI motion
    else y = gy;  //resize selection box
  }
  else if (mView)  //move view
  {
    setts.vws[0].ax -= (x - mPsx) * 0.2;
    if (setts.vws[0].ax < 0.0) setts.vws[0].ax = 359.9;
    else if (setts.vws[0].ax > 359.9) setts.vws[0].ax = 0.0;
    setts.vws[0].ay -= (y - mPsy) * 0.2;
    if (setts.vws[0].ay < -90.0) setts.vws[0].ay = -90.0;
    else if (setts.vws[0].ay > 90.0) setts.vws[0].ay = 90.0;
    setts.vws[0].dr.x = (sin(setts.vws[0].ax * RAD) * MOV) + setts.vws[0].ee.x;
    setts.vws[0].dr.y = (sin(setts.vws[0].ay * RAD) * MOV) + setts.vws[0].ee.y;
    setts.vws[0].dr.z = (cos(setts.vws[0].ax * RAD) * cos(setts.vws[0].ay * RAD) * MOV) + setts.vws[0].ee.z;
  }
  mPsx = x;
  mPsy = y;
  refresh = true;
}

//load settings
static bool strEqNoCase(const char *left, const char *right)
{
  while (*left && *right)
  {
    if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) return false;
    left++;
    right++;
  }
  return (!*left && !*right);
}

static char *trimWs(char *txt)
{
  char *end;
  while (*txt && isspace((unsigned char)*txt)) txt++;
  if (!*txt) return txt;
  end = txt + strlen(txt) - 1;
  while ((end >= txt) && isspace((unsigned char)*end))
  {
    *end = '\0';
    end--;
  }
  return txt;
}

static bool parseBoolValue(const char *value, bool *out)
{
  if (strEqNoCase(value, "1") || strEqNoCase(value, "true") || strEqNoCase(value, "yes") || strEqNoCase(value, "on"))
  {
    *out = true;
    return true;
  }
  if (strEqNoCase(value, "0") || strEqNoCase(value, "false") || strEqNoCase(value, "no") || strEqNoCase(value, "off"))
  {
    *out = false;
    return true;
  }
  return false;
}

static bool parseUnsignedCharValue(const char *value, unsigned char *out)
{
  char *end;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value) return false;
  end = trimWs(end);
  if (*end || (parsed > 255)) return false;
  *out = (unsigned char) parsed;
  return true;
}

static bool parseUnsignedShortValue(const char *value, unsigned short *out)
{
  char *end;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value) return false;
  end = trimWs(end);
  if (*end || (parsed > 65535)) return false;
  *out = (unsigned short) parsed;
  return true;
}

static bool parseUnsignedIntValue(const char *value, unsigned int *out)
{
  char *end;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value) return false;
  end = trimWs(end);
  if (*end) return false;
  *out = (unsigned int) parsed;
  return true;
}

static bool parseDoubleValue(const char *value, double *out)
{
  char *end;
  double parsed = strtod(value, &end);
  if (end == value) return false;
  end = trimWs(end);
  if (*end) return false;
  *out = parsed;
  return true;
}

static bool parsePosValue(const char *value, pos_type *out)
{
  double x, y, z;
  if (sscanf(value, " %lf , %lf , %lf ", &x, &y, &z) != 3) return false;
  out->x = x;
  out->y = y;
  out->z = z;
  return true;
}

static void setStringValue(char *dst, size_t dstsz, const char *src)
{
  strncpy(dst, src, dstsz - 1);
  dst[dstsz - 1] = '\0';
}

static const char *sipnToString(sipn_type mode)
{
  switch (mode)
  {
  case ips: return "ip";
  case ins: return "ip_and_name";
  default: return "name_only";
  }
}

static bool parseSipnValue(const char *value, sipn_type *out)
{
  if (strEqNoCase(value, "ip")) *out = ips;
  else if (strEqNoCase(value, "ip_and_name")) *out = ins;
  else if (strEqNoCase(value, "name_only")) *out = nms;
  else return false;
  return true;
}

static const char *sipsToString(sips_type mode)
{
  switch (mode)
  {
  case sel: return "selection";
  case all: return "all";
  default: return "off";
  }
}

static bool parseSipsValue(const char *value, sips_type *out)
{
  if (strEqNoCase(value, "off")) *out = off;
  else if (strEqNoCase(value, "selection")) *out = sel;
  else if (strEqNoCase(value, "all")) *out = all;
  else return false;
  return true;
}

static const char *sonaToString(sona_type mode)
{
  switch (mode)
  {
  case alt: return "alert";
  case ipn: return "show_ip_name";
  case hst: return "show_host";
  case slt: return "select_host";
  default: return "do_nothing";
  }
}

static bool parseSonaValue(const char *value, sona_type *out)
{
  if (strEqNoCase(value, "do_nothing")) *out = don;
  else if (strEqNoCase(value, "alert")) *out = alt;
  else if (strEqNoCase(value, "show_ip_name")) *out = ipn;
  else if (strEqNoCase(value, "show_host")) *out = hst;
  else if (strEqNoCase(value, "select_host")) *out = slt;
  else return false;
  return true;
}

static void ensureHsdDataDir()
{
  char datapath[256];
  strcpy(datapath, hsddata(""));
  if (fileExists(datapath)) return;
#ifdef __MINGW32__
  mkdir(datapath);
#else
  mkdir(datapath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);  //drwxr-xr-x
#endif
}

static bool textContainsNoCase(const char *text, const char *pattern)
{
  if (!text || !pattern || !*pattern) return false;
  while (*text)
  {
    const char *t = text, *p = pattern;
    while (*t && *p && (tolower((unsigned char)*t) == tolower((unsigned char)*p)))
    {
      t++;
      p++;
    }
    if (!*p) return true;
    text++;
  }
  return false;
}

static void localHsenResetUiNames()
{
  localHsenUiAutostart = -1;
  localHsenUiPromisc = -1;
  for (unsigned char cnt = 0; cnt < LOCAL_HSEN_MAX_INTERFACES; cnt++)
  {
    localHsenUiIfaceChecks[cnt] = -1;
    localHsenUiIfaceSensors[cnt] = -1;
  }
}

static const char *localHsenClassify(const char *id, const char *name)
{
  char txt[544];
  snprintf(txt, sizeof(txt), "%s %s", (id ? id : ""), (name ? name : ""));

  if (textContainsNoCase(txt, "wi-fi") || textContainsNoCase(txt, "wifi") ||
      textContainsNoCase(txt, "wireless") || textContainsNoCase(txt, "wlan") ||
      textContainsNoCase(name, "802.11")) return "wlan";

  if ((id && !strcmp(id, "lo")) || textContainsNoCase(txt, "wan miniport") ||
      textContainsNoCase(txt, "loopback") || textContainsNoCase(txt, "bluetooth") ||
      textContainsNoCase(txt, "virtual") || textContainsNoCase(txt, "vpn") ||
      textContainsNoCase(txt, "tunnel") || textContainsNoCase(txt, "hyper-v") ||
      textContainsNoCase(txt, "vmware") || textContainsNoCase(txt, "virtualbox") ||
      textContainsNoCase(txt, "npcap") || textContainsNoCase(txt, "miniport")) return "other";

  if (textContainsNoCase(txt, "ethernet") || textContainsNoCase(txt, "gigabit") ||
      textContainsNoCase(txt, "gbe")) return "ethernet";
  if (id && (!strncmp(id, "eth", 3) || !strncmp(id, "en", 2) || !strncmp(id, "em", 2) ||
             !strncmp(id, "bond", 4))) return "ethernet";

  return "other";
}

static const char *localHsenClassLabel(const char *klass)
{
  if (!strcmp(klass, "ethernet")) return "Ethernet";
  if (!strcmp(klass, "wlan")) return "WLAN";
  return "Other";
}

static bool localHsenDefaultSelected(const char *klass)
{
  return !strcmp(klass, "ethernet");
}

static int localHsenFindById(const localhsen_if_type *arr, unsigned char count, const char *id)
{
  for (unsigned char cnt = 0; cnt < count; cnt++)
  {
    if (!strcmp(arr[cnt].id, id)) return (int) cnt;
  }
  return -1;
}

static unsigned char localHsenNextSensorId(const localhsen_if_type *arr, unsigned char count)
{
  bool used[256] = {false};
  for (unsigned char cnt = 0; cnt < count; cnt++)
  {
    if (arr[cnt].sensorId) used[arr[cnt].sensorId] = true;
  }
  for (unsigned short sen = 1; sen < 256; sen++)
  {
    if (!used[sen]) return (unsigned char) sen;
  }
  return 1;
}

static void localHsenStatePath(char *path, size_t pathsz)
{
#ifdef __MINGW32__
  setStringValue(path, pathsz, hsddata("local-hsen-windows.state"));
#else
  setStringValue(path, pathsz, hsddata("local-hsen.state"));
#endif
}

static void localHsenLegacyStatePath(char *path, size_t pathsz)
{
#ifdef __MINGW32__
  setStringValue(path, pathsz, hsddata("local-hsen.state"));
#else
  if (pathsz) *path = '\0';
#endif
}

static void localHsenStateClear()
{
  char path[256], legacy[256];
  localHsenStatePath(path, sizeof(path));
  if (fileExists(path)) remove(path);
  localHsenLegacyStatePath(legacy, sizeof(legacy));
  if (*legacy && fileExists(legacy)) remove(legacy);
}

#ifdef __MINGW32__
static unsigned long long localHsenProcessCreateStamp(HANDLE proc)
{
  FILETIME created, exited, kernel, user;
  ULARGE_INTEGER stamp;
  if (!GetProcessTimes(proc, &created, &exited, &kernel, &user)) return 0;
  stamp.LowPart = created.dwLowDateTime;
  stamp.HighPart = created.dwHighDateTime;
  return stamp.QuadPart;
}
#else
static unsigned long long localHsenProcessCreateStamp(unsigned long pid)
{
#ifdef __linux__
  FILE *fp;
  char path[64], line[4096], *tok, *rp;
  unsigned int field = 3;
  snprintf(path, sizeof(path), "/proc/%lu/stat", pid);
  if (!(fp = fopen(path, "r"))) return 0;
  if (!fgets(line, sizeof(line), fp))
  {
    fclose(fp);
    return 0;
  }
  fclose(fp);
  rp = strrchr(line, ')');
  if (!rp || (rp[1] != ' ')) return 0;
  tok = strtok(rp + 2, " ");
  while (tok)
  {
    if (field == 22) return strtoull(tok, 0, 10);
    tok = strtok(0, " ");
    field++;
  }
#endif
  return 0;
}
#endif

static unsigned char localHsenStateLoadPath(const char *path, localhsen_pid_type *pids, unsigned char maxpids)
{
  FILE *fp;
  char line[128], *txt, *end;
  unsigned char count = 0;
  if (!(fp = fopen(path, "r"))) return 0;
  while ((count < maxpids) && fgets(line, sizeof(line), fp))
  {
    char *stampTxt;
    txt = trimWs(line);
    if (!*txt) continue;
    stampTxt = strchr(txt, ' ');
    if (!stampTxt) continue;
    *stampTxt++ = '\0';
    unsigned long pid = strtoul(txt, &end, 10);
    end = trimWs(end);
    if (*end || !pid) continue;
    unsigned long long stamp = strtoull(trimWs(stampTxt), &end, 10);
    end = trimWs(end);
    if (*end) continue;
    pids[count].pid = pid;
    pids[count].created = stamp;
    count++;
  }
  fclose(fp);
  return count;
}

static unsigned char localHsenStateLoad(localhsen_pid_type *pids, unsigned char maxpids)
{
  char path[256], legacy[256];
  unsigned char count;
  localHsenStatePath(path, sizeof(path));
  count = localHsenStateLoadPath(path, pids, maxpids);
  if (count) return count;
  localHsenLegacyStatePath(legacy, sizeof(legacy));
  if (*legacy && strcmp(path, legacy)) return localHsenStateLoadPath(legacy, pids, maxpids);
  return 0;
}

static bool localHsenStateSave(const localhsen_pid_type *pids, unsigned char count)
{
  FILE *fp;
  char path[256];
  localHsenStatePath(path, sizeof(path));
  if (!(fp = fopen(path, "w"))) return false;
  for (unsigned char cnt = 0; cnt < count; cnt++) fprintf(fp, "%lu %llu\n", (unsigned long) pids[cnt].pid, pids[cnt].created);
  if (fclose(fp) == EOF)
  {
    remove(path);
    return false;
  }
  return true;
}

static bool localHsenExeInfo(char *exepath, size_t exepathsz, char *workdir, size_t workdirsz)
{
#ifdef __MINGW32__
  char modpath[MAX_PATH];
  DWORD got = GetModuleFileNameA(0, modpath, sizeof(modpath));
  char *slash;
  if (!got || (got >= sizeof(modpath))) return false;
  slash = strrchr(modpath, '\\');
  if (!slash) return false;
  *slash = '\0';
  if (workdir) setStringValue(workdir, workdirsz, modpath);
  snprintf(exepath, exepathsz, "%s\\hsen.exe", modpath);
  return fileExists(exepath);
#else
  char modpath[PATH_MAX];
  char *slash;
#ifdef __linux__
  ssize_t got = readlink("/proc/self/exe", modpath, sizeof(modpath) - 1);
  if ((got > 0) && (got < (ssize_t)sizeof(modpath)))
  {
    modpath[got] = '\0';
    slash = strrchr(modpath, '/');
    if (slash)
    {
      *slash = '\0';
      if (workdir) setStringValue(workdir, workdirsz, modpath);
      snprintf(exepath, exepathsz, "%s/hsen", modpath);
      if (fileExists(exepath)) return true;
    }
  }
#endif
  if (workdir)
  {
    if (!getcwd(modpath, sizeof(modpath))) setStringValue(modpath, sizeof(modpath), ".");
    setStringValue(workdir, workdirsz, modpath);
  }
  setStringValue(exepath, exepathsz, "hsen");
  return true;
#endif
}

static bool localHsenProcessLooksManaged(
#ifdef __MINGW32__
  HANDLE proc
#else
  unsigned long pid
#endif
)
{
#ifdef __MINGW32__
  char path[MAX_PATH], *base;
  DWORD pathsz = sizeof(path);
  if (!QueryFullProcessImageNameA(proc, 0, path, &pathsz)) return false;
  base = strrchr(path, '\\');
  base = (base ? (base + 1) : path);
  return strEqNoCase(base, "hsen.exe");
#else
  return (pid > 0);
#endif
}

static bool localHsenRunAndCaptureDiscovery(char *output, size_t outsz)
{
#ifdef __MINGW32__
  SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  HANDLE rd = 0, wr = 0;
  char exepath[512], workdir[512], cmd[1024];
  DWORD got = 0;
  BOOL ok;
  output[0] = '\0';
  if (!localHsenExeInfo(exepath, sizeof(exepath), workdir, sizeof(workdir))) return false;
  if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput = wr;
  si.hStdError = wr;
  si.wShowWindow = SW_HIDE;
  snprintf(cmd, sizeof(cmd), "\"%s\" -l", exepath);
  ok = CreateProcessA(0, cmd, 0, 0, TRUE, CREATE_NO_WINDOW, 0, workdir, &si, &pi);
  CloseHandle(wr);
  if (!ok)
  {
    CloseHandle(rd);
    return false;
  }
  while ((got + 1) < outsz)
  {
    DWORD chunk = 0;
    if (!ReadFile(rd, output + got, (DWORD)(outsz - got - 1), &chunk, 0) || !chunk) break;
    got += chunk;
  }
  output[got] = '\0';
  WaitForSingleObject(pi.hProcess, 5000);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(rd);
  return true;
#else
  FILE *pp;
  char exepath[512], workdir[512], cmd[1024], line[512];
  size_t got = 0, len;
  output[0] = '\0';
  if (!localHsenExeInfo(exepath, sizeof(exepath), workdir, sizeof(workdir))) return false;
  snprintf(cmd, sizeof(cmd), "\"%s\" -l 2>&1", exepath);
  if (!(pp = popen(cmd, "r"))) return false;
  while (fgets(line, sizeof(line), pp) && ((got + 1) < outsz))
  {
    len = strlen(line);
    if ((got + len) >= outsz) len = outsz - got - 1;
    memcpy(output + got, line, len);
    got += len;
  }
  output[got] = '\0';
  pclose(pp);
  return true;
#endif
}

static bool localHsenDiscoverInterfaces(bool keepSelections)
{
  localhsen_if_type saved[LOCAL_HSEN_MAX_INTERFACES];
  unsigned char savedCount = localHsenIfCount;
  char output[8192], *line, *sep, *name;
  if (savedCount) memcpy(saved, localHsenIfs, sizeof(saved));
  if (!localHsenRunAndCaptureDiscovery(output, sizeof(output))) return false;
  localHsenIfCount = 0;
  line = strtok(output, "\r\n");
  while (line && (localHsenIfCount < LOCAL_HSEN_MAX_INTERFACES))
  {
    int savedIdx;
    localhsen_if_type *dst;
    char *id = trimWs(line);
    if (!*id)
    {
      line = strtok(0, "\r\n");
      continue;
    }
    sep = strstr(id, "  :");
    if (sep)
    {
      *sep = '\0';
      name = trimWs(sep + 3);
    }
    else name = id;
    id = trimWs(id);
    if (localHsenFindById(localHsenIfs, localHsenIfCount, id) != -1)
    {
      line = strtok(0, "\r\n");
      continue;
    }
    dst = &localHsenIfs[localHsenIfCount];
    memset(dst, 0, sizeof(*dst));
    setStringValue(dst->id, sizeof(dst->id), id);
    setStringValue(dst->name, sizeof(dst->name), (*name ? name : id));
    setStringValue(dst->klass, sizeof(dst->klass), localHsenClassify(dst->id, dst->name));
    savedIdx = (keepSelections ? localHsenFindById(saved, savedCount, dst->id) : -1);
    if (savedIdx != -1)
    {
      dst->selected = saved[savedIdx].selected;
      dst->sensorId = (saved[savedIdx].sensorId ? saved[savedIdx].sensorId : localHsenNextSensorId(localHsenIfs, localHsenIfCount));
    }
    else
    {
      dst->selected = localHsenDefaultSelected(dst->klass);
      dst->sensorId = localHsenNextSensorId(localHsenIfs, localHsenIfCount);
    }
    localHsenIfCount++;
    line = strtok(0, "\r\n");
  }
  return true;
}

static bool localHsenSaveUiState()
{
  unsigned char sen;
  bool used[256] = {false};
  localHsenWindowsAutostart = (localHsenUiAutostart != -1 ? GLWin.GetCheck(localHsenUiAutostart) : false);
  localHsenWindowsPromisc = (localHsenUiPromisc != -1 ? GLWin.GetCheck(localHsenUiPromisc) : false);
  for (unsigned char cnt = 0; cnt < localHsenIfCount; cnt++)
  {
    char *txt = GLWin.GetInput(localHsenUiIfaceSensors[cnt]);
    if (!txt || !parseUnsignedCharValue(txt, &sen) || !sen)
    {
      messageBox("ERROR", "Each local hsen interface needs a sensor ID from 1 to 255.");
      return false;
    }
    localHsenIfs[cnt].selected = GLWin.GetCheck(localHsenUiIfaceChecks[cnt]);
    localHsenIfs[cnt].sensorId = sen;
    if (localHsenIfs[cnt].selected)
    {
      if (used[sen])
      {
        messageBox("ERROR", "Selected local hsen interfaces must not share the same sensor ID.");
        return false;
      }
      used[sen] = true;
    }
  }
  return true;
}

static bool localHsenStopManaged(bool showmsg)
{
  localhsen_pid_type pids[LOCAL_HSEN_MAX_INTERFACES];
  unsigned char count = localHsenStateLoad(pids, LOCAL_HSEN_MAX_INTERFACES);
  bool stopped = false, failed = false;
  for (unsigned char cnt = 0; cnt < count; cnt++)
  {
#ifdef __MINGW32__
    HANDLE proc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pids[cnt].pid);
    if (!proc) continue;
    if (!localHsenProcessLooksManaged(proc))
    {
      CloseHandle(proc);
      continue;
    }
    if (localHsenProcessCreateStamp(proc) != pids[cnt].created)
    {
      CloseHandle(proc);
      continue;
    }
    DWORD code = 0;
    if (GetExitCodeProcess(proc, &code) && (code != STILL_ACTIVE))
    {
      CloseHandle(proc);
      continue;
    }
    if (!TerminateProcess(proc, 0)) failed = true;
    else
    {
      WaitForSingleObject(proc, 3000);
      stopped = true;
    }
    CloseHandle(proc);
#else
    pid_t pid = (pid_t) pids[cnt].pid;
    unsigned long long stamp = localHsenProcessCreateStamp(pids[cnt].pid);
    if (!localHsenProcessLooksManaged(pids[cnt].pid)) continue;
    if (pids[cnt].created && stamp && (stamp != pids[cnt].created)) continue;
    if ((kill(pid, 0) == -1) && (errno == ESRCH)) continue;
    if (kill(pid, SIGTERM) == -1) failed = true;
    else
    {
      for (unsigned char tries = 0; tries < 30; tries++)
      {
        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if ((rc == pid) || ((rc == -1) && (errno == ECHILD))) break;
        usleep(100000);
      }
      stopped = true;
    }
#endif
  }
  localHsenStateClear();
  if (showmsg)
  {
    if (failed) messageBox("ERROR", "Stopping one or more managed local hsen processes failed.");
    else if (stopped) messageBox("LOCAL hsen", "Managed local hsen processes stopped.");
    else messageBox("LOCAL hsen", "No managed local hsen processes were running.");
  }
  return !failed;
}

static bool localHsenLaunchOne(const localhsen_if_type *iface, unsigned long *pidOut)
{
#ifdef __MINGW32__
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  char exepath[512], workdir[512], cmd[1400];
  if (!localHsenExeInfo(exepath, sizeof(exepath), workdir, sizeof(workdir))) return false;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  snprintf(cmd, sizeof(cmd), "\"%s\" %u \"%s\" 127.0.0.1%s", exepath, iface->sensorId, iface->id, (localHsenWindowsPromisc ? " -p" : ""));
  if (!CreateProcessA(0, cmd, 0, 0, FALSE, CREATE_NO_WINDOW, 0, workdir, &si, &pi)) return false;
  *pidOut = (unsigned long) pi.dwProcessId;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
#else
  pid_t pid;
  char exepath[512], workdir[512], cmd[1600];
  const char *basecmd;
  if (!localHsenExeInfo(exepath, sizeof(exepath), workdir, sizeof(workdir))) return false;
  if (*setts.hsst && !textContainsNoCase(setts.hsst, "<sudo command>")) basecmd = setts.hsst;
  else basecmd = exepath;
  snprintf(cmd, sizeof(cmd), "%s %u \"%s\" 127.0.0.1%s", basecmd, iface->sensorId, iface->id, (localHsenWindowsPromisc ? " -p" : ""));
  pid = fork();
  if (pid == -1) return false;
  if (!pid)
  {
    int nullfd = open("/dev/null", O_RDWR);
    if (nullfd != -1)
    {
      dup2(nullfd, 0);
      dup2(nullfd, 1);
      dup2(nullfd, 2);
      if (nullfd > 2) close(nullfd);
    }
    if (*workdir) chdir(workdir);
    setsid();
    execl("/bin/sh", "sh", "-c", cmd, (char *) 0);
    _exit(127);
  }
  *pidOut = (unsigned long) pid;
  return true;
#endif
}

static bool localHsenStartSelected(bool showmsg)
{
  localhsen_pid_type pids[LOCAL_HSEN_MAX_INTERFACES];
  unsigned char count = 0;
  bool failed = false, anySelected = false;
  localHsenStopManaged(false);
  for (unsigned char cnt = 0; cnt < localHsenIfCount; cnt++)
  {
    unsigned long pid;
    if (!localHsenIfs[cnt].selected) continue;
    anySelected = true;
    if (!localHsenLaunchOne(&localHsenIfs[cnt], &pid)) failed = true;
    else
    {
      pids[count].pid = pid;
#ifdef __MINGW32__
      HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid);
      pids[count].created = (proc ? localHsenProcessCreateStamp(proc) : 0);
      if (proc) CloseHandle(proc);
#else
      pids[count].created = localHsenProcessCreateStamp(pid);
#endif
      count++;
    }
  }
  if (count) localHsenStateSave(pids, count);
  else localHsenStateClear();
  if (showmsg)
  {
    if (failed) messageBox("ERROR", "One or more local hsen processes failed to start.");
    else if (anySelected) messageBox("LOCAL hsen", "Selected local hsen interfaces started.");
    else messageBox("LOCAL hsen", "Settings saved. No local hsen interfaces are selected.");
  }
  return !failed;
}

static void localHsenDialogOpen()
{
  int btnTop, rowTop;
  unsigned char rows;
  char sbuf[64], lbuf[64];
  bool discovered = localHsenDiscoverInterfaces(true);
  localHsenResetUiNames();
  rows = (localHsenIfCount ? localHsenIfCount : 1);
  GLWin.CreateWin(-1, -1, 520, 148 + (rows * 22), "LOCAL hsen", true, false);
  GLWin.AddLabel(10, 10, "Target Host: 127.0.0.1");
  GLWin.AddLabel(10, 34, "Autostart:");
  localHsenUiAutostart = GLWin.AddCheck(88, 29, localHsenWindowsAutostart);
  GLWin.AddLabel(138, 34, "Promiscuous:");
  localHsenUiPromisc = GLWin.AddCheck(236, 29, localHsenWindowsPromisc);
  GLWin.AddLabel(286, 34, "Ethernet preselected, WLAN/other visible.");
  GLWin.AddLabel(10, 58, "Use");
  GLWin.AddLabel(48, 58, "Sensor");
  GLWin.AddLabel(104, 58, "Type / Interface");
  rowTop = 74;
  if (!localHsenIfCount)
  {
    GLWin.AddLabel(10, rowTop, (discovered ? "No interfaces found." : "Interface scan failed. Check hsen."));
  }
  for (unsigned char cnt = 0; cnt < localHsenIfCount; cnt++)
  {
    localHsenUiIfaceChecks[cnt] = GLWin.AddCheck(10, rowTop - 5, localHsenIfs[cnt].selected);
    snprintf(sbuf, sizeof(sbuf), "%u", localHsenIfs[cnt].sensorId);
    localHsenUiIfaceSensors[cnt] = GLWin.AddInput(48, rowTop - 5, 3, 3, sbuf);
    snprintf(lbuf, sizeof(lbuf), "%s  %.50s", localHsenClassLabel(localHsenIfs[cnt].klass), localHsenIfs[cnt].name);
    GLWin.AddLabel(104, rowTop, lbuf);
    rowTop += 22;
  }
  btnTop = 94 + (rows * 22);
  GLResult[0] = HSD_HSENRUN;
  GLWin.AddButton(10, btnTop, GLWIN_MISC1, "Refresh");
  GLWin.AddButton(120, btnTop, GLWIN_OK, "Save + Start", true, true);
  GLWin.AddButton(256, btnTop, GLWIN_MISC2, "Stop");
}

static void localHsenAutostartMaybe()
{
  if (!localHsenWindowsAutostart) return;
  if (!localHsenDiscoverInterfaces(true)) return;
  localHsenStartSelected(false);
}

static void compactBindingToken(const char *src, char *dst, size_t dstsz)
{
  size_t pos = 0;
  while (*src && (pos < (dstsz - 1)))
  {
    if (!isspace((unsigned char)*src) && (*src != '_') && (*src != '-')) dst[pos++] = toupper((unsigned char)*src);
    src++;
  }
  dst[pos] = '\0';
}

static int keyCodeFromToken(const char *token)
{
  if (!token || !*token) return 0;
  if ((strlen(token) == 1) && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ[]=-,.", token[0])) return token[0];
  if ((strlen(token) == 1) && isdigit((unsigned char)token[0])) return token[0];
  if (!strcmp(token, "UP")) return GLFW_KEY_UP;
  if (!strcmp(token, "DOWN")) return GLFW_KEY_DOWN;
  if (!strcmp(token, "LEFT")) return GLFW_KEY_LEFT;
  if (!strcmp(token, "RIGHT")) return GLFW_KEY_RIGHT;
  if (!strcmp(token, "HOME")) return GLFW_KEY_HOME;
  if (!strcmp(token, "BACKSPACE")) return GLFW_KEY_BACKSPACE;
  if (!strcmp(token, "TAB")) return GLFW_KEY_TAB;
  if (!strcmp(token, "SPACE")) return GLFW_KEY_SPACE;
  if (!strcmp(token, "INSERT")) return GLFW_KEY_INSERT;
  if (!strcmp(token, "END")) return GLFW_KEY_END;
  if (!strcmp(token, "PAGEUP") || !strcmp(token, "PGUP")) return GLFW_KEY_PAGEUP;
  if (!strcmp(token, "PAGEDOWN") || !strcmp(token, "PGDOWN") || !strcmp(token, "PGDN")) return GLFW_KEY_PAGEDOWN;
  if (!strcmp(token, "PLUS") || !strcmp(token, "EQUAL") || !strcmp(token, "EQUALS")) return '=';
  if (!strcmp(token, "MINUS") || !strcmp(token, "DASH")) return '-';
  if (!strcmp(token, "COMMA")) return ',';
  if (!strcmp(token, "PERIOD") || !strcmp(token, "DOT") || !strcmp(token, "FULLSTOP")) return '.';
  if (!strcmp(token, "LEFTBRACKET") || !strcmp(token, "OPENBRACKET")) return '[';
  if (!strcmp(token, "RIGHTBRACKET") || !strcmp(token, "CLOSEBRACKET")) return ']';
  if ((token[0] == 'F') && isdigit((unsigned char)token[1]))
  {
    char *end = 0;
    long fnum = strtol(token + 1, &end, 10);
    if (*end) return 0;
    switch (fnum)
    {
    case 1: return GLFW_KEY_F1;
    case 2: return GLFW_KEY_F2;
    case 3: return GLFW_KEY_F3;
    case 4: return GLFW_KEY_F4;
    case 5: return GLFW_KEY_F5;
    case 6: return GLFW_KEY_F6;
    case 7: return GLFW_KEY_F7;
    case 8: return GLFW_KEY_F8;
    case 9: return GLFW_KEY_F9;
    case 10: return GLFW_KEY_F10;
    case 11: return GLFW_KEY_F11;
    case 12: return GLFW_KEY_F12;
    }
  }
  return 0;
}

static bool parseBindingValue(const char *value, int *out)
{
  char compact[64], *keytxt = compact;
  bool ctrl = false, shift = false;
  compactBindingToken(value, compact, sizeof(compact));
  if (!*keytxt) return false;
  if (!strcmp(keytxt, "NONE") || !strcmp(keytxt, "DISABLED"))
  {
    *out = 0;
    return true;
  }
  while (true)
  {
    if (!strncmp(keytxt, "CTRL+", 5))
    {
      ctrl = true;
      keytxt += 5;
      continue;
    }
    if (!strncmp(keytxt, "SHIFT+", 6))
    {
      shift = true;
      keytxt += 6;
      continue;
    }
    break;
  }
  int keycode = keyCodeFromToken(keytxt);
  if (!keycode) return false;
  *out = (ctrl && shift ? CTRLSHIFTKEY(keycode) : (ctrl ? CTRLKEY(keycode) : (shift ? SHIFTKEY(keycode) : keycode)));
  return true;
}

static const char *keyCodeName(int key)
{
  static char single[2];
  switch (key)
  {
  case GLFW_KEY_UP: return "Up";
  case GLFW_KEY_DOWN: return "Down";
  case GLFW_KEY_LEFT: return "Left";
  case GLFW_KEY_RIGHT: return "Right";
  case GLFW_KEY_HOME: return "Home";
  case GLFW_KEY_BACKSPACE: return "Backspace";
  case GLFW_KEY_TAB: return "Tab";
  case GLFW_KEY_SPACE: return "Space";
  case GLFW_KEY_INSERT: return "Insert";
  case GLFW_KEY_END: return "End";
  case GLFW_KEY_PAGEUP: return "Page Up";
  case GLFW_KEY_PAGEDOWN: return "Page Down";
  case GLFW_KEY_F1: return "F1";
  case GLFW_KEY_F2: return "F2";
  case GLFW_KEY_F3: return "F3";
  case GLFW_KEY_F4: return "F4";
  case GLFW_KEY_F5: return "F5";
  case GLFW_KEY_F6: return "F6";
  case GLFW_KEY_F7: return "F7";
  case GLFW_KEY_F8: return "F8";
  case GLFW_KEY_F9: return "F9";
  case GLFW_KEY_F10: return "F10";
  case GLFW_KEY_F11: return "F11";
  case GLFW_KEY_F12: return "F12";
  case ',': return "Comma";
  case '.': return "Period";
  case '[': return "[";
  case ']': return "]";
  case '-': return "Minus";
  case '=': return "Plus";
  default:
    if (((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')))
    {
      single[0] = (char) key;
      single[1] = '\0';
      return single;
    }
    return "Unknown";
  }
}

static void bindingLabel(int encoded, char *buf, size_t bufsz)
{
  if (!encoded)
  {
    strncpy(buf, "Disabled", bufsz - 1);
    buf[bufsz - 1] = '\0';
    return;
  }
  if (encoded >= 1536)
  {
    snprintf(buf, bufsz, "Ctrl + Shift + %s", keyCodeName(encoded - 1536));
    return;
  }
  if (encoded >= 1024)
  {
    snprintf(buf, bufsz, "Shift + %s", keyCodeName(encoded - 1024));
    return;
  }
  if (encoded >= 512)
  {
    snprintf(buf, bufsz, "Ctrl + %s", keyCodeName(encoded - 512));
    return;
  }
  snprintf(buf, bufsz, "%s", keyCodeName(encoded));
}

static const char *menuLabelWithHint(const char *title, int hotkey, menustate_type state, bool active)
{
  static char labels[12][128];
  static unsigned char idx = 0;
  char binding[32];
  const char *prefix = "";
  idx = (idx + 1) % 12;
  switch (state)
  {
    case msToggle: prefix = (active ? "[X] " : "[ ] "); break;
    case msChoice: prefix = (active ? "(*) " : "( ) "); break;
    default: break;
  }
  if (!hotkey)
  {
    snprintf(labels[idx], sizeof(labels[idx]), "%s%s", prefix, title);
    return labels[idx];
  }
  bindingLabel(hotkey, binding, sizeof(binding));
  snprintf(labels[idx], sizeof(labels[idx]), "%s%s (%s)", prefix, title, binding);
  return labels[idx];
}

static int menuDepthForId(int menuid)
{
  switch (menuid)
  {
    case 100:
      return 0;
    case 101: case 102: case 103: case 104: case 105:
    case 106: case 107: case 108: case 109:
      return 1;
    case 110: case 111: case 112: case 113: case 114:
    case 115: case 116: case 117: case 118: case 119:
    case 120: case 121: case 122: case 123: case 124:
    case 125: case 126: case 134: case 138:
      return 2;
    default:
      return 0;
  }
}

static int menuItemHotkey(int value, keyact_type action)
{
  keyact_type resolved = (action < kaCount ? action : menuActionForValue(value));
  if ((resolved < kaCount) && keybinds[resolved].key) return keybinds[resolved].key;
  return 0;
}

static int menuItemWidth(const char *label, bool submenu)
{
  return 12 + ((int)strlen(label) * 6) + (submenu ? 16 : 6);
}

static void addMenuItem(const char *title, int items, int sub, int value, int mnemonic, keyact_type action, menustate_type state, bool active)
{
  (void) mnemonic;
  int hotkey = menuItemHotkey(value, action);
  const char *label = menuLabelWithHint(title, hotkey, state, active);
  GLWin.AddMenu(menuItemWidth(label, (sub != 0)), label, items, sub, value, hotkey,
                (menuBaseX - mPsx) + (menuBuildDepth * MENU_LEVEL_INDENT));
}

static keyact_type keyActionFromInput(int encoded)
{
  for (unsigned char cnt = 0; cnt < kaCount; cnt++)
  {
    if (keybinds[cnt].key && (keybinds[cnt].key == encoded)) return (keyact_type) cnt;
  }
  if (encoded >= 1536)
  {
    int ctrlEncoded = encoded - 1024;
    int shiftEncoded = encoded - 512;
    int baseEncoded = encoded - 1536;
    for (unsigned char cnt = 0; cnt < kaCount; cnt++)
    {
      if (keybinds[cnt].key && ((keybinds[cnt].key == ctrlEncoded) || (keybinds[cnt].key == shiftEncoded) || (keybinds[cnt].key == baseEncoded))) return (keyact_type) cnt;
    }
  }
  if (encoded >= 1024)
  {
    encoded -= 1024;
    for (unsigned char cnt = 0; cnt < kaCount; cnt++)
    {
      if (keybinds[cnt].key && (keybinds[cnt].key == encoded)) return (keyact_type) cnt;
    }
  }
  return kaCount;
}

static bool settsLoadIni(const char *path)
{
  FILE *sts;
  char line[1024];
  bool packetTreeLoaded = false;
  if (!(sts = fopen(path, "r"))) return false;
  while (fgets(line, sizeof(line), sts))
  {
    char *entry = trimWs(line);
    if (!*entry || (*entry == '#') || (*entry == ';') || (*entry == '[')) continue;
    char *sep = strchr(entry, '=');
    if (!sep) continue;
    *sep = '\0';
    char *key = trimWs(entry), *value = trimWs(sep + 1);
    bool flag;
    unsigned char uc;
    unsigned short us;
    unsigned int ui;
    double dbl;
    int binding;
    pos_type pos;
    sipn_type sipn;
    sips_type sips;
    sona_type sona;
    if (!strcmp(key, "show_osd"))
    {
      if (parseBoolValue(value, &flag)) setts.osd = flag;
    }
    else if (!strcmp(key, "show_broadcast_hosts"))
    {
      if (parseBoolValue(value, &flag)) setts.bct = flag;
    }
    else if (!strcmp(key, "fast_packets"))
    {
      if (parseBoolValue(value, &flag)) setts.fsp = flag;
    }
    else if (!strcmp(key, "add_destination_hosts"))
    {
      if (parseBoolValue(value, &flag)) setts.adh = flag;
    }
    else if (!strcmp(key, "anomaly_detection"))
    {
      if (parseBoolValue(value, &flag)) setts.anm = flag;
    }
    else if (!strcmp(key, "new_host_links"))
    {
      if (parseBoolValue(value, &flag)) setts.nhl = flag;
    }
    else if (!strcmp(key, "new_host_packets"))
    {
      if (parseBoolValue(value, &flag)) setts.nhp = flag;
    }
    else if (!strcmp(key, "show_packet_destination_port"))
    {
      if (parseBoolValue(value, &flag)) setts.pdp = flag;
    }
    else if (!strcmp(key, "start_hsen_promiscuous"))
    {
      if (parseBoolValue(value, &flag)) setts.hspm = flag;
    }
    else if (!strcmp(key, "sensor_filter"))
    {
      if (parseUnsignedCharValue(value, &uc)) setts.sen = uc;
    }
    else if (!strcmp(key, "packet_tree_filter"))
    {
      if (parsePacketTreeFilterValue(value, &uc))
      {
        packetTreeFilter = uc;
        packetTreeLoaded = true;
      }
    }
    else if (!strcmp(key, "protocol_filter"))
    {
      if (parseUnsignedCharValue(value, &uc)) setts.pr = uc;
    }
    else if (!strcmp(key, "packet_form_filter"))
    {
      if (parsePacketShapeFilterValue(value, &uc)) setts.psf = uc;
    }
    else if (!strcmp(key, "packet_tcp_filter"))
    {
      if (parsePacketTcpFilterValue(value, &uc)) setts.ptf = uc;
    }
    else if (!strcmp(key, "port_filter"))
    {
      if (parseUnsignedShortValue(value, &us)) setts.prt = us;
    }
    else if (!strcmp(key, "packet_limit"))
    {
      if (parseUnsignedIntValue(value, &ui)) setts.pks = ui;
    }
    else if (!strcmp(key, "display_mode"))
    {
      if (parseSipnValue(value, &sipn)) setts.sipn = sipn;
    }
    else if (!strcmp(key, "display_scope"))
    {
      if (parseSipsValue(value, &sips)) setts.sips = sips;
    }
    else if (!strcmp(key, "on_active_action"))
    {
      if (parseSonaValue(value, &sona)) setts.sona = sona;
    }
    else if (!strcmp(key, "hsen_start_command")) setStringValue(setts.hsst, sizeof(setts.hsst), value);
    else if (!strcmp(key, "hsen_sensor_id")) setStringValue(setts.hsid, sizeof(setts.hsid), value);
    else if (!strcmp(key, "hsen_interface")) setStringValue(setts.hsif, sizeof(setts.hsif), value);
    else if (!strcmp(key, "hsen_host")) setStringValue(setts.hshd, sizeof(setts.hshd), value);
    else if (!strcmp(key, "hsen_stop_command")) setStringValue(setts.hssp, sizeof(setts.hssp), value);
    else if (!strcmp(key, "command1")) setStringValue(setts.cmd[0], sizeof(setts.cmd[0]), value);
    else if (!strcmp(key, "command2")) setStringValue(setts.cmd[1], sizeof(setts.cmd[1]), value);
    else if (!strcmp(key, "command3")) setStringValue(setts.cmd[2], sizeof(setts.cmd[2]), value);
    else if (!strcmp(key, "command4")) setStringValue(setts.cmd[3], sizeof(setts.cmd[3]), value);
    else if (!strcmp(key, "local_hsen_autostart") || !strcmp(key, "local_hsen_windows_autostart"))
    {
      if (parseBoolValue(value, &flag)) localHsenWindowsAutostart = flag;
    }
    else if (!strcmp(key, "local_hsen_promiscuous") || !strcmp(key, "local_hsen_windows_promiscuous"))
    {
      if (parseBoolValue(value, &flag)) localHsenWindowsPromisc = flag;
    }
    else if (!strcmp(key, "local_hsen_interface_count") || !strcmp(key, "local_hsen_windows_interface_count"))
    {
      if (parseUnsignedCharValue(value, &uc) && (uc <= LOCAL_HSEN_MAX_INTERFACES)) localHsenIfCount = uc;
    }
    else if (!strncmp(key, "local_hsen_interface", 20) || !strncmp(key, "local_hsen_windows_interface", 27))
    {
      unsigned int idx;
      char field[32];
      int matched = sscanf(key, "local_hsen_interface%u_%31s", &idx, field);
      if ((matched != 2) && (sscanf(key, "local_hsen_windows_interface%u_%31s", &idx, field) == 2)) matched = 2;
      if ((matched == 2) && (idx < LOCAL_HSEN_MAX_INTERFACES))
      {
        localhsen_if_type *iface = &localHsenIfs[idx];
        if ((idx + 1) > localHsenIfCount) localHsenIfCount = (unsigned char) idx + 1;
        if (!strcmp(field, "id")) setStringValue(iface->id, sizeof(iface->id), value);
        else if (!strcmp(field, "name")) setStringValue(iface->name, sizeof(iface->name), value);
        else if (!strcmp(field, "class")) setStringValue(iface->klass, sizeof(iface->klass), value);
        else if (!strcmp(field, "selected"))
        {
          if (parseBoolValue(value, &flag)) iface->selected = flag;
        }
        else if (!strcmp(field, "sensor_id"))
        {
          if (parseUnsignedCharValue(value, &uc) && uc) iface->sensorId = uc;
        }
      }
    }
    else if (!strcmp(key, "dynamic_hosts_enabled"))
    {
      if (parseBoolValue(value, &flag)) dynamicHostsEnabled = flag;
    }
    else if (!strcmp(key, "dynamic_host_ttl_seconds"))
    {
      if (parseUnsignedIntValue(value, &ui) && ui) dynamicHostTtlSeconds = ui;
    }
    else if (!strcmp(key, "dynamic_host_cleanup_interval_seconds"))
    {
      if (parseUnsignedIntValue(value, &ui) && ui) dynamicHostCleanupIntervalSeconds = ui;
    }
    else
    {
      bool matched = false;
      for (unsigned char cnt = 0; cnt < kaCount; cnt++)
      {
        if (!strcmp(key, keybinds[cnt].name))
        {
          if (parseBindingValue(value, &binding)) keybinds[cnt].key = binding;
          matched = true;
          break;
        }
      }
      if (matched) continue;
      unsigned char vwn;
      if (!strncmp(key, "view", 4) && isdigit((unsigned char)key[4]) && (key[5] == '.'))
      {
        vwn = (unsigned char)(key[4] - '0');
        if (vwn < 5)
        {
          const char *field = key + 6;
          if (!strcmp(field, "ax"))
          {
            if (parseDoubleValue(value, &dbl)) setts.vws[vwn].ax = dbl;
          }
          else if (!strcmp(field, "ay"))
          {
            if (parseDoubleValue(value, &dbl)) setts.vws[vwn].ay = dbl;
          }
          else if (!strcmp(field, "eye"))
          {
            if (parsePosValue(value, &pos)) setts.vws[vwn].ee = pos;
          }
          else if (!strcmp(field, "look_at"))
          {
            if (parsePosValue(value, &pos)) setts.vws[vwn].dr = pos;
          }
        }
      }
    }
  }
  fclose(sts);
  if (!packetTreeLoaded) packetTreeFilterMigrateLegacy();
  else
  {
    setts.pr = 0;
    setts.psf = 0;
    setts.ptf = 0;
  }
  return true;
}

static bool settsLoadLegacy(const char *path)
{
  FILE *sts;
  if (!(sts = fopen(path, "rb"))) return false;
  fseek(sts, 0, SEEK_END);
  if (ftell(sts) == sizeof(setts))
  {
    rewind(sts);
    if (fread(&setts, sizeof(setts), 1, sts) == 1)
    {
      fclose(sts);
      return true;
    }
  }
  fclose(sts);
  return false;
}

static bool settsSaveIni(const char *path)
{
  FILE *sts;
  if (!(sts = fopen(path, "w"))) return false;
  fputs("# Hosts3D settings\n", sts);
  fputs("# Generated automatically. Edit with Hosts3D closed.\n\n", sts);
  fputs("[display]\n", sts);
  fprintf(sts, "show_osd=%d\n", (setts.osd ? 1 : 0));
  fprintf(sts, "show_broadcast_hosts=%d\n", (setts.bct ? 1 : 0));
  fprintf(sts, "fast_packets=%d\n", (setts.fsp ? 1 : 0));
  fprintf(sts, "add_destination_hosts=%d\n", (setts.adh ? 1 : 0));
  fprintf(sts, "anomaly_detection=%d\n", (setts.anm ? 1 : 0));
  fprintf(sts, "new_host_links=%d\n", (setts.nhl ? 1 : 0));
  fprintf(sts, "new_host_packets=%d\n", (setts.nhp ? 1 : 0));
  fprintf(sts, "show_packet_destination_port=%d\n", (setts.pdp ? 1 : 0));
  fprintf(sts, "packet_limit=%u\n", setts.pks);
  fprintf(sts, "display_mode=%s\n", sipnToString(setts.sipn));
  fprintf(sts, "display_scope=%s\n", sipsToString(setts.sips));
  fprintf(sts, "on_active_action=%s\n\n", sonaToString(setts.sona));

  fputs("[filters]\n", sts);
  fprintf(sts, "sensor_filter=%u\n", setts.sen);
  fprintf(sts, "packet_tree_filter=%s\n", packetTreeFilterString(packetTreeFilter));
  fprintf(sts, "port_filter=%u\n\n", setts.prt);

  fputs("[hsen]\n", sts);
  fprintf(sts, "start_hsen_promiscuous=%d\n", (setts.hspm ? 1 : 0));
  fprintf(sts, "hsen_start_command=%s\n", setts.hsst);
  fprintf(sts, "hsen_sensor_id=%s\n", setts.hsid);
  fprintf(sts, "hsen_interface=%s\n", setts.hsif);
  fprintf(sts, "hsen_host=%s\n", setts.hshd);
  fprintf(sts, "hsen_stop_command=%s\n\n", setts.hssp);

  fputs("[commands]\n", sts);
  fprintf(sts, "command1=%s\n", setts.cmd[0]);
  fprintf(sts, "command2=%s\n", setts.cmd[1]);
  fprintf(sts, "command3=%s\n", setts.cmd[2]);
  fprintf(sts, "command4=%s\n\n", setts.cmd[3]);

  fputs("[dynamic_hosts]\n", sts);
  fprintf(sts, "dynamic_hosts_enabled=%d\n", (dynamicHostsEnabled ? 1 : 0));
  fprintf(sts, "dynamic_host_ttl_seconds=%u\n", dynamicHostTtlSeconds);
  fprintf(sts, "dynamic_host_cleanup_interval_seconds=%u\n\n", dynamicHostCleanupIntervalSeconds);

  fputs("[local_hsen]\n", sts);
  fprintf(sts, "local_hsen_autostart=%d\n", (localHsenWindowsAutostart ? 1 : 0));
  fprintf(sts, "local_hsen_promiscuous=%d\n", (localHsenWindowsPromisc ? 1 : 0));
  fprintf(sts, "local_hsen_interface_count=%u\n", localHsenIfCount);
  for (unsigned char cnt = 0; cnt < localHsenIfCount; cnt++)
  {
    fprintf(sts, "local_hsen_interface%u_id=%s\n", cnt, localHsenIfs[cnt].id);
    fprintf(sts, "local_hsen_interface%u_name=%s\n", cnt, localHsenIfs[cnt].name);
    fprintf(sts, "local_hsen_interface%u_class=%s\n", cnt, localHsenIfs[cnt].klass);
    fprintf(sts, "local_hsen_interface%u_selected=%d\n", cnt, (localHsenIfs[cnt].selected ? 1 : 0));
    fprintf(sts, "local_hsen_interface%u_sensor_id=%u\n", cnt, localHsenIfs[cnt].sensorId);
  }
  fputc('\n', sts);

  fputs("[keybindings]\n", sts);
  for (unsigned char cnt = 0; cnt < kaCount; cnt++)
  {
    char label[32];
    bindingLabel(keybinds[cnt].key, label, sizeof(label));
    fprintf(sts, "%s=%s\n", keybinds[cnt].name, label);
  }
  fputc('\n', sts);

  fputs("[views]\n", sts);
  for (unsigned char cnt = 0; cnt < 5; cnt++)
  {
    fprintf(sts, "view%u.ax=%.6f\n", cnt, setts.vws[cnt].ax);
    fprintf(sts, "view%u.ay=%.6f\n", cnt, setts.vws[cnt].ay);
    fprintf(sts, "view%u.eye=%.6f, %.6f, %.6f\n", cnt, setts.vws[cnt].ee.x, setts.vws[cnt].ee.y, setts.vws[cnt].ee.z);
    fprintf(sts, "view%u.look_at=%.6f, %.6f, %.6f\n", cnt, setts.vws[cnt].dr.x, setts.vws[cnt].dr.y, setts.vws[cnt].dr.z);
  }
  if (fclose(sts) == EOF)
  {
    remove(path);
    return false;
  }
  return true;
}

void settsLoad()
{
  char inipath[256], legacypath[256];
  ensureHsdDataDir();
  localHsenIfCount = 0;
  localHsenWindowsAutostart = false;
  localHsenWindowsPromisc = false;
  memset(localHsenIfs, 0, sizeof(localHsenIfs));
  strcpy(inipath, hsddata("settings.ini"));
  strcpy(legacypath, hsddata("settings-hsd"));

  if (settsLoadIni(inipath))
  {
    packetTreeFilterMigrateLegacy();
    settsSaveIni(inipath);
    if (fileExists(legacypath)) remove(legacypath);
    return;
  }
  if (settsLoadLegacy(legacypath))
  {
    packetTreeFilterMigrateLegacy();
    if (settsSaveIni(inipath)) remove(legacypath);
    return;
  }
  packetFilterResetAll();
  if (fileExists(legacypath)) remove(legacypath);
  if (!fileExists(inipath)) settsSaveIni(inipath);
}

//save settings
void settsSave()
{
  char inipath[256], legacypath[256];
  ensureHsdDataDir();
  strcpy(inipath, hsddata("settings.ini"));
  strcpy(legacypath, hsddata("settings-hsd"));
  if (settsSaveIni(inipath) && fileExists(legacypath)) remove(legacypath);
}

static void controlLine(FILE *ctls, const char *binding, const char *desc)
{
  fprintf(ctls, "%s\t%s\n", binding, desc);
}

static void controlLineKey(FILE *ctls, keyact_type action, const char *desc)
{
  char label[32];
  bindingLabel(keybinds[action].key, label, sizeof(label));
  controlLine(ctls, label, desc);
}

static void controlLinePair(FILE *ctls, keyact_type first, keyact_type second, const char *desc)
{
  char left[32], right[32], binding[72];
  bindingLabel(keybinds[first].key, left, sizeof(left));
  bindingLabel(keybinds[second].key, right, sizeof(right));
  snprintf(binding, sizeof(binding), "%s/%s", left, right);
  controlLine(ctls, binding, desc);
}

//check for controls file, create it
void checkControls()
{
  FILE *ctls;
  char movef[32], moveb[32], movel[32], mover[32], shiftmv[160];
  ensureHsdDataDir();
  if ((ctls = fopen(hsddata("controls.txt"), "w")))
  {
    controlLine(ctls, "Left Mouse Button", "Select Host");
    controlLine(ctls, "", "Click-and-Drag to Select Multiple Hosts");
    controlLine(ctls, "", "Click Selected Host to Toggle Persistant IP/Name");
    controlLine(ctls, "Middle Mouse Button", "Click-and-Drag to Change View");
    controlLine(ctls, "Right Mouse Button", "Show Menu");
    controlLine(ctls, "Left Mouse Button on OSD Row", "Cycle Clicked OSD Setting or Toggle");
    controlLine(ctls, "Left Mouse Button on OSD Packet Example", "Apply Matching Packet Filter");
    controlLine(ctls, "Esc", "Close Open Menu or Dialog");
    controlLine(ctls, "", "Menu state markers: [X]/[ ] = toggle, (*)/( ) = current mode");
    controlLine(ctls, "Mouse Wheel", "Move Up/Down");
    controlLinePair(ctls, kaMoveForward, kaMoveBack, "Move Forward/Back");
    controlLinePair(ctls, kaMoveLeft, kaMoveRight, "Move Left/Right");
    bindingLabel(keybinds[kaMoveForward].key, movef, sizeof(movef));
    bindingLabel(keybinds[kaMoveBack].key, moveb, sizeof(moveb));
    bindingLabel(keybinds[kaMoveLeft].key, movel, sizeof(movel));
    bindingLabel(keybinds[kaMoveRight].key, mover, sizeof(mover));
    snprintf(shiftmv, sizeof(shiftmv), "Shift + %s/%s/%s/%s", movef, moveb, movel, mover);
    controlLine(ctls, shiftmv, "Move at Triple Speed");
    controlLineKey(ctls, kaViewHome, "Recall Home View");
    controlLineKey(ctls, kaViewHomeAlt, "Recall Alternate Home View");
    controlLineKey(ctls, kaViewPos1, "Recall View Position 1");
    controlLineKey(ctls, kaViewPos2, "Recall View Position 2");
    controlLineKey(ctls, kaViewPos3, "Recall View Position 3");
    controlLineKey(ctls, kaViewPos4, "Recall View Position 4");
    controlLineKey(ctls, kaViewSave1, "Save View Position 1");
    controlLineKey(ctls, kaViewSave2, "Save View Position 2");
    controlLineKey(ctls, kaViewSave3, "Save View Position 3");
    controlLineKey(ctls, kaViewSave4, "Save View Position 4");
    controlLineKey(ctls, kaLayoutRestore1, "Restore Network Layout 1");
    controlLineKey(ctls, kaLayoutRestore2, "Restore Network Layout 2");
    controlLineKey(ctls, kaLayoutRestore3, "Restore Network Layout 3");
    controlLineKey(ctls, kaLayoutRestore4, "Restore Network Layout 4");
    controlLineKey(ctls, kaLayoutSave1, "Save Network Layout 1");
    controlLineKey(ctls, kaLayoutSave2, "Save Network Layout 2");
    controlLineKey(ctls, kaLayoutSave3, "Save Network Layout 3");
    controlLineKey(ctls, kaLayoutSave4, "Save Network Layout 4");
    controlLine(ctls, "Ctrl", "Multi-Select");
    controlLineKey(ctls, kaSelectAll, "Select All Hosts");
    controlLineKey(ctls, kaInvertSelection, "Invert Selection");
    controlLinePair(ctls, kaSelMoveUp, kaSelMoveDown, "Move Selection Up/Down");
    controlLinePair(ctls, kaSelMoveForward, kaSelMoveBack, "Move Selection Forward/Back");
    controlLinePair(ctls, kaSelMoveLeft, kaSelMoveRight, "Move Selection Left/Right");
    controlLineKey(ctls, kaFindHosts, "Find Hosts");
    controlLineKey(ctls, kaNextSelectedHost, "Select Next Host in Selection");
    controlLineKey(ctls, kaPrevSelectedHost, "Select Previous Host in Selection");
    controlLineKey(ctls, kaToggleSelectionPersistant, "Toggle Persistant IP/Name for Selection");
    controlLineKey(ctls, kaCycleIpNameDisplay, "Cycle Show IP - IP/Name - Name Only");
    controlLineKey(ctls, kaToggleAddDestinationHosts, "Toggle Add Destination Hosts [D]");
    controlLineKey(ctls, kaMakeHost, "Make Host");
    controlLineKey(ctls, kaEditHostname, "Edit Hostname for Selected Host");
    controlLineKey(ctls, kaSelectNamed, "Select All Named Hosts");
    controlLineKey(ctls, kaEditRemarks, "Edit Remarks for Selected Host");
    controlLineKey(ctls, kaCreateLinkLine, "Press Twice with Different Selected Hosts for Link Line");
    controlLineKey(ctls, kaDeleteLinkLine, "Delete Link Line (2nd Selected Host, Press Link on 1st)");
    controlLineKey(ctls, kaAutoLinksAll, "Automatic Link Lines for All Hosts");
    controlLineKey(ctls, kaToggleNewHostLinks, "Toggle Automatic Link Lines for New Hosts [L]");
    controlLineKey(ctls, kaAutoLinksSelection, "Automatic Link Lines for Selection");
    controlLineKey(ctls, kaStopAutoLinksSelection, "Stop Automatic Link Lines for Selection");
    controlLineKey(ctls, kaDeleteAllLinks, "Delete Link Lines for All Hosts");
    controlLineKey(ctls, kaShowSelectionPackets, "Show Packets for Selection");
    controlLineKey(ctls, kaHideSelectionPackets, "Stop Showing Packets for Selection");
    controlLineKey(ctls, kaShowAllPackets, "Show Packets for All Hosts");
    controlLineKey(ctls, kaToggleNewHostPackets, "Toggle Show Packets for New Hosts [P]");
    controlLineKey(ctls, kaShowSensor1Packets, "Show Packets from Sensor 1");
    controlLineKey(ctls, kaShowSensor2Packets, "Show Packets from Sensor 2");
    controlLineKey(ctls, kaShowSensor3Packets, "Show Packets from Sensor 3");
    controlLineKey(ctls, kaShowSensor4Packets, "Show Packets from Sensor 4");
    controlLineKey(ctls, kaShowSensor5Packets, "Show Packets from Sensor 5");
    controlLineKey(ctls, kaShowSensor6Packets, "Show Packets from Sensor 6");
    controlLineKey(ctls, kaShowSensor7Packets, "Show Packets from Sensor 7");
    controlLineKey(ctls, kaShowSensor8Packets, "Show Packets from Sensor 8");
    controlLineKey(ctls, kaShowSensor9Packets, "Show Packets from Sensor 9");
    controlLineKey(ctls, kaShowAllSensorPackets, "Show Packets from All Sensors");
    controlLinePair(ctls, kaPrevSensorPackets, kaNextSensorPackets, "Change Sensor to Show Packets from");
    controlLineKey(ctls, kaToggleBroadcasts, "Toggle Show Simulated Broadcasts [B]");
    controlLineKey(ctls, kaDecreasePacketLimit, "Decrease Allowed Packets");
    controlLineKey(ctls, kaIncreasePacketLimit, "Increase Allowed Packets");
    controlLineKey(ctls, kaTogglePacketDestPort, "Toggle Show Packet Destination Port");
    controlLineKey(ctls, kaTogglePacketSpeed, "Toggle Double Speed Packets [F]");
    controlLineKey(ctls, kaPacketsOff, "Packets Off");
    controlLineKey(ctls, kaRecordPacketTraffic, "Record Packet Traffic");
    controlLineKey(ctls, kaReplayPacketTraffic, "Replay Recorded Packet Traffic");
    controlLineKey(ctls, kaSkipReplayPacket, "Skip to Next Packet during Replay Traffic");
    controlLineKey(ctls, kaStopPacketTraffic, "Stop Record/Replay of Packet Traffic");
    controlLineKey(ctls, kaOpenPacketTrafficFile, "Open Packet Traffic File...");
    controlLineKey(ctls, kaSavePacketTrafficFile, "Save Packet Traffic File As...");
    controlLineKey(ctls, kaToggleAnimation, "Toggle Pause Animation");
    controlLine(ctls, "Ctrl + X", "Cut Input Box Text");
    controlLine(ctls, "Ctrl + C", "Copy Input Box Text");
    controlLine(ctls, "Ctrl + V", "Paste Input Box Text");
    controlLineKey(ctls, kaAcknowledgeAllAnomalies, "Acknowledge All Anomalies");
    controlLineKey(ctls, kaToggleOsd, "Toggle Show OSD");
    controlLineKey(ctls, kaExportSelectionCsv, "Export Selection Details in CSV File As...");
    controlLineKey(ctls, kaShowHostInformation, "Show Selected Host Information");
    controlLineKey(ctls, kaShowSelectionInformation, "Show Selection Information");
    controlLineKey(ctls, kaShowHelp, "Toggle Help Overlay");
    fclose(ctls);
  }
}

//compile GL objects
void initObjsGL()
{
  objsDraw = glGenLists(OBJ_LIST_COUNT);
  glNewList(objsDraw, GL_COMPILE);  //grey host object
    hobjDraw(brgrey[0], brgrey[1], brgrey[2]);
  glEndList();
  glNewList(objsDraw + 1, GL_COMPILE);  //orange host object
    hobjDraw(orange[0], orange[1], orange[2]);
  glEndList();
  glNewList(objsDraw + 2, GL_COMPILE);  //yellow host object
    hobjDraw(yellow[0], yellow[1], yellow[2]);
  glEndList();
  glNewList(objsDraw + 3, GL_COMPILE);  //fluro host object
    hobjDraw(fluro[0], fluro[1], fluro[2]);
  glEndList();
  glNewList(objsDraw + 4, GL_COMPILE);  //green host object
    hobjDraw(green[0], green[1], green[2]);
  glEndList();
  glNewList(objsDraw + 5, GL_COMPILE);  //mint host object
    hobjDraw(mint[0], mint[1], mint[2]);
  glEndList();
  glNewList(objsDraw + 6, GL_COMPILE);  //aqua host object
    hobjDraw(aqua[0], aqua[1], aqua[2]);
  glEndList();
  glNewList(objsDraw + 7, GL_COMPILE);  //blue host object
    hobjDraw(sky[0], sky[1], sky[2]);
  glEndList();
  glNewList(objsDraw + 8, GL_COMPILE);  //purple host object
    hobjDraw(purple[0], purple[1], purple[2]);
  glEndList();
  glNewList(objsDraw + 9, GL_COMPILE);  //violet host object
    hobjDraw(violet[0], violet[1], violet[2]);
  glEndList();
  glNewList(objsDraw + 10, GL_COMPILE);  //red host object
    hobjDraw(red[0], red[1], red[2]);
  glEndList();
  glNewList(objsDraw + 11, GL_COMPILE);  //br red selected host object
    hobjDraw(brred[0], brred[1], brred[2]);
  glEndList();
  glNewList(objsDraw + 12, GL_COMPILE);  //multiple-hosts object
    mobjDraw(false);
  glEndList();
  glNewList(objsDraw + 13, GL_COMPILE);  //br red selected multiple-hosts object
    mobjDraw(true);
  glEndList();
  glNewList(objsDraw + 14, GL_COMPILE);  //cross object
    cobjDraw();
  glEndList();
  for (unsigned char clr = 0; clr < PACKET_COLOR_COUNT; clr++)
  {
    glNewList(objsDraw + packetListIndex(clr, pCube), GL_COMPILE);
      pobjDraw(clr);
    glEndList();
    glNewList(objsDraw + packetListIndex(clr, pDouble), GL_COMPILE);
      pobj2Draw(clr);
    glEndList();
    glNewList(objsDraw + packetListIndex(clr, pTriple), GL_COMPILE);
      pobj3Draw(clr);
    glEndList();
    glNewList(objsDraw + packetListIndex(clr, pSphere), GL_COMPILE);
      psobjDraw(clr);
    glEndList();
    glNewList(objsDraw + packetListIndex(clr, pPyramid), GL_COMPILE);
      ppobjDraw(clr);
    glEndList();
  }
  glNewList(objsDraw + ALERT_LIST_BASE, GL_COMPILE);  //active alert object
    aobjDraw(false);
  glEndList();
  glNewList(objsDraw + ALERT_LIST_BASE + 1, GL_COMPILE);  //anomaly alert object
    aobjDraw(true);
  glEndList();
}

void pktProcessCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *dh = *((host_type **) data);
  host_type *sh = (host_type *)(void *) arg1;
  in_addr_t mask = (in_addr_t) arg2;
  in_addr_t dstip = (in_addr_t) arg3;
  pkex_type *pkex = (pkex_type *)(void *) arg4;

  if ((dh != sh) && ((dh->hip.s_addr & mask) == dstip))
    pcktCreate(pkex, sh, dh, false);
}

//glfwCreateThread
void GLFWCALL pktProcess(void *arg)
{
#ifdef __MINGW32__
  WORD wVersionRequested = MAKEWORD(2, 0);  //WSAStartup parameter
  WSADATA wsaData;  //WSAStartup parameter
  WSAStartup(wVersionRequested, &wsaData);  //initiate use of the Winsock DLL
  SOCKET psock;
#else
  int psock;
#endif
  sockaddr_in padr;  //hsen address
  padr.sin_family = AF_INET;
  padr.sin_addr.s_addr = INADDR_ANY;
  padr.sin_port = htons(HOST3D_PORT);
#ifdef __MINGW32__
  if ((psock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
  {
    MessageBoxA(0, "Socket create failed, continuing", "Hosts3D", MB_ICONWARNING | MB_OK);
#else
  if ((psock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    syslog(LOG_WARNING, "socket create failed, continuing\n");
#endif
  }
  else if (bind(psock, (sockaddr *)&padr, sizeof(padr)) == -1)
  {
#ifdef __MINGW32__
    MessageBoxA(0, "Socket bind failed, continuing", "Hosts3D", MB_ICONWARNING | MB_OK);
    closesocket(psock);
    WSACleanup();
#else
    syslog(LOG_WARNING, "socket bind failed, continuing\n");
    close(psock);
#endif
  }
#ifdef __MINGW32__
  unsigned long sopt = 1;
  ioctlsocket(psock, FIONBIO, &sopt);
#else
  fcntl(psock, F_SETFL, O_NONBLOCK);
#endif
  enum pksc_type { none, pkfl, sckt };  //packet source
  pksc_type pksc;
  pkif_type *pkif;
  pkex_type *pkex = 0;  //packet info from hsen
  pdns_type *pdns;  //DNS info from hsen
  timeval ptm;  //time of packet
  int rcsz, prsz = sizeof(pkrp), pr1sz = sizeof(pkrc1_type), pesz = sizeof(pkex_type), dnsz = sizeof(pdns_type), ptsz = sizeof(ptm);
  in_addr_t mask, dstip;
  unsigned long long rpytm;
  char pbuf[dnsz];
  host_type *sh, *dh;
  pkrc1_type pkr1;
  while (goRun)
  {
    if (!goHosts)
    {
      pksc = none;
      if (ptrc == hlt)
      {
        fclose(pfile);
        packetTrafficStarted = 0;
        ptrc = stp;
      }
      if (ptrc == rpy)
      {
        rpytm = milliTime(0) - rpoff;
        if (!feof(pfile) && (rpytm >= milliTime(&pkrp.ptime)))
        {
          pkex = &pkrp.pk;
          pkif = &pkrp.pk.pk;
          pksc = pkfl;
        }
        else if (feof(pfile) && !pktsLL.Num())  //stop replay when last packet ends
        {
          ptrc = hlt;
          refresh = true;
        }
      }
      else if ((rcsz = recv(psock, pbuf, dnsz, 0)) > 0)  //received UDP packet
      {
        if ((pbuf[0] == 85) && (rcsz == pesz))  //85 used to identify packet info
        {
          pkex = (pkex_type *)pbuf;
          pkif = (pkif_type *)&pkex->pk;
          pksc = sckt;
          if (ptrc == rec)  //record packet
          {
            if (!distm) time(&distm);
            gettimeofday(&ptm, 0);
            if (fwrite(&ptm, ptsz, 1, pfile) != 1) ptrc = hlt;
            else if (fwrite(pkex, pesz, 1, pfile) != 1) ptrc = hlt;
          }
        }
        else if ((pbuf[0] == 42) && (rcsz == dnsz))  //42 used to identify DNS info
        {
          pdns = (pdns_type *)pbuf;
          if ((sh = hostIP(pdns->dnsip, false)))
          {
            hostRuntimeNoteDiscoveryName(sh, pdns->htnm);
            if (!*sh->htnm) strcpy(sh->htnm, pdns->htnm);
            if (seltd == sh) hostDetails();
          }
        }
      }
      if (pksc)
      {
        unsigned short importantPort = packetImportantPort(pkif);
        sh = hostIP(pkif->srcip, true);
        hostRuntimeNotePacket(sh, pkex, importantPort);
        if (seltd == sh) hostDetails();
        if (pksc == sckt)
        {
          sh->lsn = pkif->sen;
          sh->uld += pkex->sz;
          time(&sh->lpk);
          if (!*sh->htmc) sprintf(sh->htmc, "%02X:%02X:%02X:%02X:%02X:%02X", pkex->srcmc[0], pkex->srcmc[1], pkex->srcmc[2], pkex->srcmc[3], pkex->srcmc[4], pkex->srcmc[5]);
          if (!(((pkif->pr == IPPROTO_TCP) && !(pkex->syn && pkex->ack)) || ((pkif->pr == IPPROTO_UDP) && !pkex->ack)))  //true if other protocol
          {
            svcAdd(sh, (pkif->srcpt * 10000) + (pkif->pr * 10), setts.anm);
            refresh = true;
          }
        }
        if ((dh = hostIP(pkif->dstip, setts.adh)))
        {
          hostRuntimeNotePacket(dh, pkex, importantPort);
          if (seltd == dh) hostDetails();
          if (pksc == sckt)
          {
            dh->lsn = pkif->sen;
            dh->dld += pkex->sz;
            time(&dh->lpk);
          }
          pcktCreate(pkex, sh, dh, true);
        }
        else if (setts.bct && (mask = isBroadcast(pkif->dstip)))
        {
          dstip = pkif->dstip.s_addr & mask;
	  hstsByIp.forEach(2, &pktProcessCb, (long)(void *) sh, (long) mask,
			   (long) dstip, (long)(void *) pkex);
        }
        if (pksc == pkfl)
        {
          if (pkrLegacy)
          {
            if (fread(&pkr1, pr1sz, 1, pfile) == 1)
            {
              memset(&pkrp, 0, sizeof(pkrp));
              pkrp.ptime = pkr1.ptime;
              pkrp.pk.id = 85;
              pkrp.pk.pk = pkr1.pk;
            }
            else ptrc = hlt;
          }
          else if (fread(&pkrp, prsz, 1, pfile) != 1) ptrc = hlt;
        }
      }
      else usleep(10000);
    }
    else
    {
      if (goHosts == 1) goHosts = 2;
      usleep(10000);
    }
  }
#ifdef __MINGW32__
  closesocket(psock);
  WSACleanup();
#else
  close(psock);
#endif
  if (ptrc) fclose(pfile);
}

int main(int argc, char *argv[])
{
  if (getopt(argc, argv, "f") == 'f') fullscn = true;
  else if (argc != 1)
  {
    fprintf(stderr, "hosts3d " HSD_VERSION_STR " usage: %s [-f]\n  -f : display full screen\n", argv[0]);
    return 1;
  }

#ifndef __MINGW32__
  syslog(LOG_INFO, "started\n");
#endif

  // The following code decides what we use as the 'name' for the host GL
  // object.
  // The 'name' is a GLuint type, and is used by the glSelectBuffer()
  // API to identify the host objects selected by the user.
  // GLuint is always 32 bits wide, irrespective of the CPU architecture.
  // On 32-bit systems, pointer sizes are also 32 bits, and we exploit this
  // coincidence to just use the (host_type *) as the name. So the name->host
  // lookup is an O(1) operation on 32-bit systems.
  // We can't do this on 64-bit systems, so we use the host's IP address
  // (IPv4 address = 32 bits) as the name. The name->host lookup will use the
  // hstsByIp hash table and is a O(log n) operation.
  if (sizeof(GLuint) < sizeof(host_type *)) {
    useIpAddrForGlName = true;
  } else {
    useIpAddrForGlName = false;
  }

  if (!glfwInit())
  {
    fprintf(stderr, "hosts3d error: glfwInit() failed\n");
#ifndef __MINGW32__
    syslog(LOG_ERR, "glfwInit() failed\n");
#endif
    return 1;
  }
  if (fullscn)
  {
    GLFWvidmode vid;
    glfwGetDesktopMode(&vid);
    wWin = vid.Width;
    hWin = vid.Height;
  }
  if (!glfwOpenWindow(wWin, hWin, 0, 0, 0, 0, 24, 0, (fullscn ? GLFW_FULLSCREEN : GLFW_WINDOW)))
  {
    fprintf(stderr, "hosts3d error: glfwOpenWindow() failed\n");
#ifndef __MINGW32__
    syslog(LOG_ERR, "glfwOpenWindow() failed\n");
#endif
    glfwTerminate();
    return 1;
  }
  settsLoad();  //load settings
  checkControls();  //write controls file from current settings
  helpOverlayLoad();
  netpsLoad();  //load netpos
  osdUpdate();
  netLoad(hsddata("0network.hnl"));  //load network layout 0
  netposExactHostsSync();
  if (goHosts) goHosts = 0;
  glfwSetWindowTitle("Hosts3D");  //window title
  glfwSetWindowRefreshCallback(refreshGL);
  glfwSetWindowSizeCallback(resizeGL);
  glfwSetKeyCallback(keyboardGL);
  glfwSetMouseButtonCallback(clickGL);
  glfwSetMouseWheelCallback(wheelGL);
  glfwSetMousePosCallback(motionGL);
  glfwDisable(GLFW_AUTO_POLL_EVENTS);
  glfwEnable(GLFW_KEY_REPEAT);
  glfwEnable(GLFW_MOUSE_CURSOR);
  glEnable(GL_DEPTH_TEST);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  initObjsGL();
  GLWin.InitFont();
  GLFWthread gthrd = glfwCreateThread(pktProcess, 0);  //packet gatherer
  localHsenAutostartMaybe();
  signal(SIGINT, hsdStop);  //capture ctrl+c
  signal(SIGTERM, hsdStop);  //capture kill
  while (goRun)
  {
    if ((milliTime(0) - fps) > 40)  //25 FPS
    {
      if (setts.anm) dAnom = true;
      dynamicHostsCleanupMaybe();
      if ((ptrc > hlt) || GLWin.On()) refresh = true;
      if (goAnim)
      {
        if (pktsLL.Num() || altsLL.Num() || setts.mvd) animate = true;
        if (setts.mvd) setts.mvd--;
      }
      frame++;
      fps = milliTime(0);
    }
    if (refresh || animate) displayGL();
    else usleep(10000);
    glfwPollEvents();
    goRun = (goRun && glfwGetWindowParam(GLFW_OPENED));
  }
#ifndef __MINGW32__
  syslog(LOG_INFO, "stopping...\n");
#endif
  localHsenStopManaged(false);
  goHosts = 2;
  glfwWaitThread(gthrd, GLFW_WAIT);
  settsSave();  //save settings
  netSave(hsddata("0network.hnl"));  //save network layout 0
  allDestroy();
  netpsDestroy();
  glDeleteLists(objsDraw, OBJ_LIST_COUNT);
  glfwTerminate();
#ifndef __MINGW32__
  syslog(LOG_INFO, "stopped\n");
#endif
  return 0;
}
