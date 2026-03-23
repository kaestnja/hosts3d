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
#include <map>
#include <math.h>  //cos(), sin(), sqrt()
#include <signal.h>  //signal()
#include <stdio.h>
#include <stdlib.h>  //abs(), atoi(), system()
#include <string.h>
#include <sys/stat.h>  //mkdir()
#include <time.h>
#ifdef __MINGW32__
#include <getopt.h>  //getopt()
#include <winsock2.h>
#else
#include <arpa/inet.h>  //inet_addr(), inet_ntoa()
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
static const int OSD_MIN_W = 300;
static const int OSD_MIN_H = 220;
static const int OSD_LINE_H = 13;
static const int OSD_PKT_GAP_H = 13;
static const int OSD_PKT_TEXT_LINES = 7;
int wWin = DEFAULT_WIN_W, hWin = DEFAULT_WIN_H, mBxx, mBxy, mPsx = 0, mPsy = 0, mWhl = 0, GLResult[6] = {0, 0, 0, 0, 0, 0};  //2D GUI result identification
time_t atime = 0, distm;  //packet traffic display time offset
ptrc_type ptrc = stp;  //packet traffic state
pkrc_type pkrp;  //packet traffic replay packet
unsigned short frame = 0;
unsigned long long fps = 0, rpoff;  //packet traffic replay time offset
char goHosts = 2, osdtxt[83], htdtls[570];
view_type vwdef[2] = {{0.0, 0.0, {0.0, 75.0, -360.0}, {0.0, 75.0, MOV - 360.0}}, {90.0, 0.0, {-360.0, 75.0, 0.0}, {MOV - 360.0, 75.0, 0.0}}};  //default views
sett_type setts = {false, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 5000, "", "<sudo command> hsen", "1", "eth0", "", "<sudo command> killall hsen"
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

enum keyact_type
{
  kaMoveForward, kaMoveBack, kaMoveLeft, kaMoveRight,
  kaViewHome, kaViewHomeAlt, kaViewPos1, kaViewPos2, kaViewPos3, kaViewPos4,
  kaSelectAll, kaInvertSelection, kaSelectNamed,
  kaSelMoveUp, kaSelMoveDown, kaSelMoveForward, kaSelMoveBack, kaSelMoveLeft, kaSelMoveRight,
  kaFindHosts, kaNextSelectedHost, kaPrevSelectedHost, kaToggleSelectionPersistant, kaCycleIpNameDisplay,
  kaToggleAddDestinationHosts, kaMakeHost, kaEditHostname, kaEditRemarks,
  kaCreateLinkLine, kaDeleteLinkLine, kaAutoLinksAll, kaToggleNewHostLinks, kaAutoLinksSelection, kaStopAutoLinksSelection, kaDeleteAllLinks,
  kaShowSelectionPackets, kaHideSelectionPackets, kaShowAllPackets, kaToggleNewHostPackets,
  kaShowSensor1Packets, kaShowSensor2Packets, kaShowSensor3Packets, kaShowSensor4Packets, kaShowAllSensorPackets, kaPrevSensorPackets, kaNextSensorPackets,
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
  {"view_position_1", CTRLKEY(GLFW_KEY_F1)},
  {"view_position_2", CTRLKEY(GLFW_KEY_F2)},
  {"view_position_3", CTRLKEY(GLFW_KEY_F3)},
  {"view_position_4", CTRLKEY(GLFW_KEY_F4)},
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
  {"show_sensor_1_packets", GLFW_KEY_F1},
  {"show_sensor_2_packets", GLFW_KEY_F2},
  {"show_sensor_3_packets", GLFW_KEY_F3},
  {"show_sensor_4_packets", GLFW_KEY_F4},
  {"show_all_sensor_packets", GLFW_KEY_F5},
  {"show_previous_sensor_packets", '['},
  {"show_next_sensor_packets", ']'},
  {"toggle_broadcasts", 'B'},
  {"decrease_packet_limit", '-'},
  {"increase_packet_limit", '='},
  {"toggle_show_packet_destination_port", CTRLKEY('T')},
  {"toggle_packet_speed", 'Z'},
  {"packets_off", 'K'},
  {"record_packet_traffic", GLFW_KEY_PAGEDOWN},
  {"replay_packet_traffic", GLFW_KEY_INSERT},
  {"skip_replay_packet", GLFW_KEY_PAGEUP},
  {"stop_packet_traffic", GLFW_KEY_END},
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

static void ensureHsdDataDir();
static bool parseBindingValue(const char *value, int *out);
static void bindingLabel(int encoded, char *buf, size_t bufsz);
static const char *menuLabelWithBinding(const char *title, keyact_type action);
static keyact_type keyActionFromInput(int encoded);
static void triggerKeyAction(keyact_type action);
static bool hostIsDynamic(host_type *ht);
static void hostSetDynamic(host_type *ht, bool dynamic);
static void hostPromoteStatic(host_type *ht);
static bool hostShouldPersist(host_type *ht);
void hostDetails();
static void hostDeleteManaged(host_type *ht);
static void dynamicHostsCleanupMaybe();
static void settsSave();
static void localHsenDialogOpen();
static bool localHsenStopManaged(bool showmsg = false);
static bool localHsenSaveUiState();
static bool localHsenStartSelected(bool showmsg = false);
static bool localHsenDiscoverInterfaces(bool keepSelections = true);
static void localHsenAutostartMaybe();

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
  void *ret = hstsByIp.newEntryAtBucket(key, (void *) ht);
}

// remove an entry from the hstsByPos hashtable
void hostByPositionPreMove(host_type *ht) {
  bool success;
  HtKeyType key = keyFromCoOrdinates(ht->px, ht->py, ht->pz);
  success = hstsByPos.deleteEntryAtBucket(0, key, (void *) ht);
}

// add an entry to the hstsByPos hashtable
void hostByPositionPostMove(host_type *ht) {
  HtKeyType key = keyFromCoOrdinates(ht->px, ht->py, ht->pz);
  void *ret = hstsByPos.newEntryAtBucket(key, (void *) ht);
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

//create packet
void pcktCreate(pkif_type *pkt, host_type *sht, host_type *dht, bool lk)
{
  if ((pktsLL.Num() < setts.pks) && (sht->shp || dht->shp) && (!setts.sen || (pkt->sen == setts.sen))
    && (!setts.pr || (pkt->pr == setts.pr)) && (!setts.prt || (pkt->srcpt == setts.prt) || (pkt->dstpt == setts.prt)) && goAnim)
  {
    pckt_type pckt = {pkt->pr, pkt->dstpt, frame, {sht->px, sht->py, sht->pz}, dht}, *pk;
    pktsLL.End(2);
    while ((pk = (pckt_type *)pktsLL.Read(2)))
    {
      if (pk->frm != frame) break;
      if ((pk->cu.x == pckt.cu.x) && (pk->cu.y == pckt.cu.y) && (pk->cu.z == pckt.cu.z) && (pk->ht == pckt.ht)) return;
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

//update osd text
void osdUpdate()
{
  char sbuf[34];
  strcpy(osdtxt, "Sen: ");  //5
  if (setts.sen) sprintf(sbuf, "%u", setts.sen);  //3
  else strcpy(sbuf, "All");
  strcat(sbuf, "\nPro: ");  //6
  strcat(osdtxt, sbuf);
  if (setts.pr) sprintf(sbuf, "%u", setts.pr);  //3
  else strcpy(sbuf, "All");
  strcat(sbuf, "\nPrt: ");  //6
  strcat(osdtxt, sbuf);
  if (setts.prt) sprintf(sbuf, "%u", setts.prt);  //5
  else strcpy(sbuf, "All");
  strcat(osdtxt, sbuf);
  sprintf(sbuf, "\n%s: %s\nAct: %s\nPkts: ", tipn[setts.sipn], (setts.sona == ipn ? "Act" : tips[setts.sips]), tona[setts.sona]);  //29
  if (setts.fsp) strcat(sbuf, "F");  //1
  if (setts.adh) strcat(sbuf, "D");  //1
  else if (setts.bct) strcat(sbuf, "B");
  if (setts.nhp) strcat(sbuf, "P");  //1
  if (setts.nhl) strcat(sbuf, "L");  //1
  strcat(osdtxt, sbuf);
  sprintf(sbuf, "\n%u", setts.pks);  //11
  strcat(osdtxt, sbuf);
}

struct pktlg_type
{
  unsigned char red, green, blue;
  const char *name;
};

static void osdDrawPktLegend(int px, int py)
{
  static const pktlg_type legend[] =
  {
    {red[0], red[1], red[2], "ICMP"},
    {green[0], green[1], green[2], "TCP"},
    {blue[0], blue[1], blue[2], "UDP"},
    {yellow[0], yellow[1], yellow[2], "ARP"},
    {brgrey[0], brgrey[1], brgrey[2], "OTHER"}
  };

  py -= (OSD_PKT_TEXT_LINES * OSD_LINE_H) + OSD_PKT_GAP_H;
  for (unsigned int cnt = 0; cnt < (sizeof(legend) / sizeof(legend[0])); cnt++)
  {
    glColor3ub(legend[cnt].red, legend[cnt].green, legend[cnt].blue);
    glRasterPos2i(px, py - (cnt * OSD_LINE_H));
    glBitmap(8, 8, 0.0, 0.0, 0.0, 0.0, bitmaps[16]);
    glColor3ub(white[0], white[1], white[2]);
    glRasterPos2i(px + 14, py - (cnt * OSD_LINE_H));
    GLWin.DrawString((const unsigned char *)legend[cnt].name);
  }
}

//destroy packets LL
void pcktsDestroy(bool gh = false)
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
    if ((setts.sona != hst) || ht->vis || ht->sld || ht->anm)
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

      if (dips && !ht->col && (ht->pip || (setts.sips == all) || ((setts.sips == sel) && ht->sld)) && ((setts.sipn != nms) || *ht->htnm))  //draw host IP/name
      {
	glColor3ub(white[0], white[1], white[2]);
	glRasterPos3i(ht->px, ht->py + 11, ht->pz);
	GLWin.DrawString((const unsigned char *)(*ht->htnm && setts.sipn ? ht->htnm : ht->htip));
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
    if (dips && (seltd->pip || setts.sips) && ((setts.sipn != nms) || *seltd->htnm))  //draw host IP/name
    {
      glColor3ub(white[0], white[1], white[2]);
      glRasterPos3i(seltd->px, seltd->py + 11, seltd->pz);
      GLWin.DrawString((const unsigned char *)(*seltd->htnm && setts.sipn ? seltd->htnm : seltd->htip));
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
    if ((setts.sona != hst) || ((lk->sht->vis || lk->sht->sld || lk->sht->anm) && (lk->dht->vis || lk->dht->sld || lk->dht->anm)))
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
      glCallList(objsDraw + (al->ao == actv ? 20 : 21));
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
  double itrns;
  char sbuf[6];
  pckt_type *pk;
  pktsLL.Start(1);
  while ((pk = (pckt_type *)pktsLL.Read(1)))
  {
    if (pk->frm == frame) break;
    glPushMatrix();
    glTranslated(pk->cu.x, pk->cu.y, pk->cu.z);
    switch (pk->pr)
    {
      case IPPROTO_ICMP: glCallList(objsDraw + 16); break;  //ICMP red
      case IPPROTO_TCP: glCallList(objsDraw + 17); break;  //TCP green
      case IPPROTO_UDP: glCallList(objsDraw + 18); break;  //UDP blue
      case IPPROTO_ARP: glCallList(objsDraw + 19); break;  //ARP yellow
      default: glCallList(objsDraw + 15);  //other grey
    }
    glPopMatrix();
    if (setts.pdp)
    {
      glColor3ub(white[0], white[1], white[2]);
      sprintf(sbuf, "%u", pk->dstpt);
      glRasterPos3d(pk->cu.x, pk->cu.y + 7.0, pk->cu.z);
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
  int px = wWin - 66, py = hWin - 16;
  char sbuf[25];
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
    glColor3ub(white[0], white[1], white[2]);
    glRasterPos2i(px, py);
    GLWin.DrawString((const unsigned char *)osdtxt);
    osdDrawPktLegend(px, py);
    if (lnkht)
    {
      strcpy(sbuf, "Link Line");
      glRasterPos2i(px, 19);
      GLWin.DrawString((const unsigned char *)sbuf);
    }
    if (goAnim) sprintf(sbuf, "%u", pktsLL.Num());
    else strcpy(sbuf, "PAUSED");
    glRasterPos2i(px, 6);
    GLWin.DrawString((const unsigned char *)sbuf);
    if (dAnom)
    {
      glColor3ub(brred[0], brred[1], brred[2]);
      glRasterPos2i(px + 54, py);
      GLWin.DrawChar('A');
    }
  }
  if (ptrc > hlt)  //draw packet traffic record/replay status
  {
    time_t dt = time(0) - distm;
    if (ptrc == rec)
    {
      glColor3ub(red[0], red[1], red[2]);
      if (distm) strftime(sbuf, 17, "REC %j %H:%M:%S", gmtime(&dt));
      else strcpy(sbuf, "REC");
    }
    else
    {
      glColor3ub(green[0], green[1], green[2]);
      strftime(sbuf, 25, "REPLAY %d-%m-%y %H:%M:%S", localtime(&dt));
    }
    glRasterPos2i(6, 6);
    GLWin.DrawString((const unsigned char *)sbuf);
  }
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
  if (setts.osd || seltd || mMove || (ptrc > hlt) || GLWin.On()) draw2D();
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
  GLWin.AddButton(178, 30, GLWIN_CLOSE, "OK", true, true);
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
          bool anet = false, ipok;
          unsigned char cnt;
          unsigned short sprt = 0;
          unsigned int found = 0;
          in_addr_t mask = 0, net = 0;
	  FindHostsCbData findhostscbdata;

          char ttmp[19], lmc[18], lnm[256], lrm[256], *cidr;  //lowercase MAC, name, remarks
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
        if (gr == HSD_HNLOPEN) netLoad(gi1);
        else
        {
          extensionAdd(gi1, ".hnl");
          netSave(gi1);
        }
        GLWin.Close();  //close all 2D GUI windows
      }
      else messageBox("ERROR", "Enter File Name!");
      break;
    case HSD_HPTOPEN: case HSD_HPTSAVE:
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
        if (gr == HSD_HPTOPEN) pkrcCopy(gi1, hsddata("traffic.hpt"));
        else
        {
          extensionAdd(gi1, ".hpt");
          pkrcCopy(hsddata("traffic.hpt"), gi1);
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
        goHosts = 0;
      }
      break;
    case HSD_PKTSPRO: case HSD_PKTSPRT:
      if (*gi1)
      {
        if (gr == HSD_PKTSPRO)
        {
          setts.pr = atoi(gi1);
          if (setts.pr) pcktsDestroy(true);
          else osdUpdate();
        }
        else
        {
          setts.prt = atoi(gi1);
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
    fputs(htdtls, info);
    fprintf(info, "\n\nAnomaly: %s\nLast Sensor: %u\nLast Packet: %sShow Packets: %s\nDownloads: %s"
      , (seltd->anm ? "Yes" : "No"), seltd->lsn, ctime(&seltd->lpk), (seltd->shp ? "Yes" : "No"), formatBytes(seltd->dld, buf));
    fprintf(info, "\nUploads: %s\nAuto Link Lines: %s\nLock: %s\nLifetime: %s"
      , formatBytes(seltd->uld, buf), (seltd->alk ? "Yes" : "No"), (seltd->lck ? "On" : "Off"), (hostIsDynamic(seltd) ? "Dynamic" : "Static"));  //reuse buf
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
  if (!action) return;
  if ((glfwGetKey(GLFW_KEY_LCTRL) || glfwGetKey(GLFW_KEY_RCTRL)) && (key != GLFW_KEY_LCTRL) && (key != GLFW_KEY_RCTRL)) key += 512;
  keyact_type keyact = keyActionFromInput(key);
  if (GLWin.On())
  {
    char *clip;
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
    switch (key)
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
          hostTab((key == GLFW_KEY_TAB));
          if (seltd)
          {
            GLWin.Scroll();  //start
            infoHost();
          }
          break;
        }  //pass tab to 2D GUI
      default: GLWin.Key(key, (glfwGetKey(GLFW_KEY_LSHIFT) || glfwGetKey(GLFW_KEY_RSHIFT)));  //pass key to 2D GUI
    }
  }
  else
  {
    int kymv = (glfwGetKey(GLFW_KEY_LSHIFT) || glfwGetKey(GLFW_KEY_RSHIFT) ? 3 : 1) * MOV;
    double angx, angy;
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
        GLWin.AddButton(280, 160, GLWIN_CLOSE, "Close");
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
        setts.sen = 1;
        pcktsDestroy(true);
        break;
      case kaShowSensor2Packets:
        setts.sen = 2;
        pcktsDestroy(true);
        break;
      case kaShowSensor3Packets:
        setts.sen = 3;
        pcktsDestroy(true);
        break;
      case kaShowSensor4Packets:
        setts.sen = 4;
        pcktsDestroy(true);
        break;
      case kaShowAllSensorPackets:  //show packets from all hsen
        setts.sen = 0;
        osdUpdate();
        break;
      case kaPrevSensorPackets:  //decrement hsen to show packets from
        setts.sen--;
        pcktsDestroy(true);
        break;
      case kaNextSensorPackets:  //increment hsen to show packets from
        setts.sen++;
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
        if (ptrc > hlt) ptrc = hlt;
        while (ptrc == hlt) usleep(1000);
        if ((pfile = fopen(hsddata("traffic.hpt"), "wb")))
        {
          fputs("HPT", pfile);
          distm = 0;
          ptrc = rec;
        }
        break;
      case kaReplayPacketTraffic:  //packet traffic replay
        if (ptrc > hlt) ptrc = hlt;
        while (ptrc == hlt) usleep(1000);
        if ((pfile = fopen(hsddata("traffic.hpt"), "rb")))
        {
          fseek(pfile, 3, SEEK_SET);
          if (fread(&pkrp, sizeof(pkrp), 1, pfile) == 1)
          {
            pcktsDestroy();
            ptrc = rpy;
            offsetReset();
            goHosts = 0;
          }
          else fclose(pfile);
        }
        break;
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
        filelistCreate(hsddata("tmp-flist-hsd"), ".hpt");
        if (keyact == kaOpenPacketTrafficFile)
        {
          GLWin.CreateWin(-1, -1, 332, 234, "OPEN PACKET TRAFFIC FILE...", true, true);
          GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, "") * 100) + HSD_HPTOPEN;
        }
        else
        {
          GLWin.CreateWin(-1, -1, 332, 234, "SAVE PACKET TRAFFIC FILE AS...", true, true);
          GLResult[0] = (GLWin.AddInput(10, 26, 50, 251, "") * 100) + HSD_HPTSAVE;
        }
        GLWin.AddLabel(10, 10, "Enter File Name:");
        GLWin.AddList(10, 52, 10, 50, hsddata("tmp-flist-hsd"));
        GLWin.AddButton(128, 60, GLWIN_OK, "OK", false, true);
        GLWin.AddButton(76, 60, GLWIN_CLOSE, "Cancel", false, false);
        break;
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
        GLWin.AddButton(186, 6, GLWIN_CLOSE, "Close", true, true);
        GLWin.AddView(10, 42, 10, 10, 17, hsddata("tmp-hinfo-hsd"));
        GLResult[0] = 0;
        break;
      case kaShowHelp:  //2D GUI help
        GLWin.CreateWin(-2, -2, 352, 271, "HELP", true, true);
        GLWin.AddBitmap(10, 10, red[0], red[1], red[2], bitmaps[16]);
        GLWin.AddLabel(23, 10, "ICMP");
        GLWin.AddBitmap(59, 10, green[0], green[1], green[2], bitmaps[16]);
        GLWin.AddLabel(72, 10, "TCP");
        GLWin.AddBitmap(102, 10, blue[0], blue[1], blue[2], bitmaps[16]);
        GLWin.AddLabel(115, 10, "UDP");
        GLWin.AddBitmap(145, 10, yellow[0], yellow[1], yellow[2], bitmaps[16]);
        GLWin.AddLabel(158, 10, "ARP");
        GLWin.AddBitmap(188, 10, brgrey[0], brgrey[1], brgrey[2], bitmaps[16]);
        GLWin.AddLabel(201, 10, "OTHER");
        GLWin.AddView(10, 30, 10, 10, 28, hsddata("controls.txt"));
        GLResult[0] = 0;
        break;
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
			     {seltd->px,
			      seltd->py + 5.0,
			      seltd->pz + ((seltd->pz < 0 ? -16 : 16) * MOV)
			     },
			     {seltd->px,
			      seltd->py + 5.0,
			      seltd->pz + ((seltd->pz < 0 ? -15 : 15) * MOV)
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
      case 61:  //show all protocols packets
        setts.pr = 0;
        osdUpdate();
        break;
      case 62:  //show ICMP packets
        setts.pr = IPPROTO_ICMP;
        pcktsDestroy(true);
        break;
      case 63:  //show TCP packets
        setts.pr = IPPROTO_TCP;
        pcktsDestroy(true);
        break;
      case 64:  //show UDP packets
        setts.pr = IPPROTO_UDP;
        pcktsDestroy(true);
        break;
      case 65:  //show ARP packets
        setts.pr = IPPROTO_ARP;
        pcktsDestroy(true);
        break;
      case 66:  //enter protocol to show packets for
        GLWin.CreateWin(-1, -1, 170, 100, "PACKETS PROTOCOL");
        GLWin.AddLabel(10, 15, "Enter Protocol No.:");
        GLResult[0] = (GLWin.AddInput(130, 10, 3, 3, "") * 100) + HSD_PKTSPRO;
        GLWin.AddButton(42, 40, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(94, 40, GLWIN_CLOSE, "Cancel");
        break;
      case 67:  //show all ports packets
        setts.prt = 0;
        osdUpdate();
        break;
      case 68:  //enter port to show packets for
        GLWin.CreateWin(-1, -1, 158, 100, "PACKETS PORT");
        GLWin.AddLabel(10, 15, "Enter Port No.:");
        GLResult[0] = (GLWin.AddInput(106, 10, 5, 5, "") * 100) + HSD_PKTSPRT;
        GLWin.AddButton(30, 40, GLWIN_OK, "OK", true, true);
        GLWin.AddButton(82, 40, GLWIN_CLOSE, "Cancel");
        break;
      case 69: triggerKeyAction(kaPacketsOff); break;  //packets off
      case 70: triggerKeyAction(kaViewHomeAlt); break;  //recall alternate home view
      case 71: triggerKeyAction(kaViewPos1); break;  //recall view position 1
      case 72: triggerKeyAction(kaViewPos2); break;  //recall view position 2
      case 73: triggerKeyAction(kaViewPos3); break;  //recall view position 3
      case 74: triggerKeyAction(kaViewPos4); break;  //recall view position 4
      case 75: case 76: case 77: case 78: memcpy(&setts.vws[m - 74], &setts.vws[0], sizeof(view_type)); break;  //save current view as view position 1-4
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
      case 81: netLoad(hsddata("1network.hnl")); break;  //restore network layout 1
      case 82: netLoad(hsddata("2network.hnl")); break;  //restore network layout 2
      case 83: netLoad(hsddata("3network.hnl")); break;  //restore network layout 3
      case 84: netLoad(hsddata("4network.hnl")); break;  //restore network layout 4
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
        GLWin.AddButton(70, 60, GLWIN_CLOSE, "Close", false, false);
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
      case 97: triggerKeyAction(kaShowHelp); break;  //2D GUI help
      case 98:  //about
        GLWin.CreateWin(-1, -1, 230, 138, "ABOUT");
        GLWin.AddLabel(10, 10, "Hosts3D 1.16");
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
        GLWin.AddButton(178, 78, GLWIN_CLOSE, "OK", true, true);
        GLResult[0] = 0;
        break;
      case 99: goRun = false; break;  //exit
      case 100:
        GLWin.AddMenu(88, "MAIN", 12, 0, 0);  //nothing
        if (seltd) GLWin.AddMenu(88, "Selected", 12, 1, 101);
        GLWin.AddMenu(88, "Selection", 12, 1, 102);  //6+(10x6)+12+10=88
        GLWin.AddMenu(88, "Anomalies", 12, 1, 103);
        GLWin.AddMenu(88, "IP/Name", 12, 1, 104);
        GLWin.AddMenu(88, "Packets", 12, 1, 105);
        GLWin.AddMenu(88, "On-Active", 12, 1, 106);
        GLWin.AddMenu(88, "View", 12, 1, 107);
        GLWin.AddMenu(88, "Layout", 12, 1, 108);
        GLWin.AddMenu(88, "Other", 12, 1, 109);
        GLWin.AddMenu(88, "Local hsen", 12, 0, 94);
        if (fullscn) GLWin.AddMenu(88, "Exit", 12, 0, 99);
        break;
      case 101:
        GLWin.AddMenu(114, "SELECTED", 8, 2, 100);
        GLWin.AddMenu(114, menuLabelWithBinding("Information", kaShowHostInformation), 8, 0, 40);
        GLWin.AddMenu(114, "Show Packets Only", 8, 0, 41);  //6+(17x6)+6=114
        GLWin.AddMenu(114, "Move Here", 8, 0, 42);
        GLWin.AddMenu(114, "Go To", 8, 0, 43);
        GLWin.AddMenu(114, menuLabelWithBinding("Hostname", kaEditHostname), 8, 0, 44);
        GLWin.AddMenu(114, menuLabelWithBinding("Remarks", kaEditRemarks), 8, 0, 45);
        GLWin.AddMenu(114, "Add Net Position", 8, 0, 91);
        break;
      case 102:
        GLWin.AddMenu(114, "SELECTION", 10, 2, 100);
        GLWin.AddMenu(114, menuLabelWithBinding("Information", kaShowSelectionInformation), 10, 0, 46);
        GLWin.AddMenu(114, "Resolve Hostnames", 10, 0, 96);
        GLWin.AddMenu(114, "Colour", 10, 1, 110);
        GLWin.AddMenu(114, "Lock", 10, 1, 111);
        GLWin.AddMenu(114, "Move To Zone", 10, 1, 112);
        GLWin.AddMenu(114, "Arrange", 10, 1, 113);
        GLWin.AddMenu(114, "Commands", 10, 1, 114);
        GLWin.AddMenu(114, "Reset", 10, 1, 115);
        GLWin.AddMenu(114, "Delete", 10, 1, 116);
        break;
      case 103:
        GLWin.AddMenu(108, "ANOMALIES", 4, 2, 100);
        GLWin.AddMenu(108, "Select All", 4, 0, 31);
        GLWin.AddMenu(108, "Acknowledge", 4, 1, 117);
        GLWin.AddMenu(108, "Toggle Detection", 4, 0, 48);  //6+(16x6)+6=108
        break;
      case 104:
        GLWin.AddMenu(96, "IP/NAME", 6, 2, 100);
        if (setts.sips != sel) GLWin.AddMenu(96, "Show Selection", 6, 0, 50);  //6+(14x6)+6=96
        if (setts.sips != all) GLWin.AddMenu(96, "Show All", 6, 0, 51);
        if (setts.sona != ipn) GLWin.AddMenu(96, "Show On-Active", 6, 0, 52);
        if (setts.sips != off) GLWin.AddMenu(96, "Show Off", 6, 0, 53);
        GLWin.AddMenu(96, "All Off", 6, 0, 54);
        break;
      case 105:
        GLWin.AddMenu(96, "PACKETS", 6, 2, 100);
        GLWin.AddMenu(96, menuLabelWithBinding("Show All", kaShowAllPackets), 6, 0, 60);
        GLWin.AddMenu(96, "Protocol", 6, 1, 118);
        GLWin.AddMenu(96, "Port", 6, (setts.prt ? 1 : 0), (setts.prt ? 119 : 68));
        GLWin.AddMenu(96, "Select Showing", 6, 0, 32);  //6+(14x6)+6=96
        GLWin.AddMenu(96, menuLabelWithBinding("Off", kaPacketsOff), 6, 0, 69);
        break;
      case 106:
        GLWin.AddMenu(84, "ON-ACTIVE", 6, 2, 100);
        if (setts.sona != alt) GLWin.AddMenu(84, "Alert", 6, 0, 55);
        if (setts.sona != ipn) GLWin.AddMenu(84, "Show IP/Name", 6, 0, 52);  //6+(12x6)+6=84
        if (setts.sona != hst) GLWin.AddMenu(84, "Show Host", 6, 0, 56);
        if (setts.sona != slt) GLWin.AddMenu(84, "Select", 6, 0, 57);
        if (setts.sona != don) GLWin.AddMenu(84, "Do Nothing", 6, 0, 58);
        break;
      case 107:
        GLWin.AddMenu(64, "VIEW", 3, 2, 100);
        GLWin.AddMenu(64, "Recall", 3, 1, 120);  //6+(6x6)+12+10=64
        GLWin.AddMenu(64, "Save", 3, 1, 121);
        break;
      case 108:
        GLWin.AddMenu(90, "LAYOUT", 5, 2, 100);
        GLWin.AddMenu(90, "Restore", 5, 1, 122);
        GLWin.AddMenu(90, "Save", 5, 1, 123);
        GLWin.AddMenu(90, "Net Positions", 5, 0, 90);  //6+(13x6)+6=90
        GLWin.AddMenu(90, "Clear", 5, 1, 124);
        break;
      case 109:
        GLWin.AddMenu(118, "OTHER", 5, 2, 100);
        GLWin.AddMenu(118, menuLabelWithBinding("Find Hosts", kaFindHosts), 5, 0, 93);
        GLWin.AddMenu(118, "Select Inactive", 5, 1, 125);  //6+(15x6)+12+10=118
        GLWin.AddMenu(118, menuLabelWithBinding("Help", kaShowHelp), 5, 0, 97);
        GLWin.AddMenu(118, "About", 5, 0, 98);
        break;
      case 110:
        GLWin.AddMenu(64, "COLOUR", 11, 2, 102);  //6+(6x6)+12+10=64
        GLWin.AddMenu(64, "Grey", 11, 0, 10);
        GLWin.AddMenu(64, "Orange", 11, 0, 11);
        GLWin.AddMenu(64, "Yellow", 11, 0, 12);
        GLWin.AddMenu(64, "Fluro", 11, 0, 13);
        GLWin.AddMenu(64, "Green", 11, 0, 14);
        GLWin.AddMenu(64, "Mint", 11, 0, 15);
        GLWin.AddMenu(64, "Aqua", 11, 0, 16);
        GLWin.AddMenu(64, "Blue", 11, 0, 17);
        GLWin.AddMenu(64, "Purple", 11, 0, 18);
        GLWin.AddMenu(64, "Violet", 11, 0, 19);
        break;
      case 111:
        GLWin.AddMenu(52, "LOCK", 3, 2, 102);  //6+(4x6)+12+10=52
        GLWin.AddMenu(52, "On", 3, 0, 21);
        GLWin.AddMenu(52, "Off", 3, 0, 22);
        break;
      case 112:
        GLWin.AddMenu(100, "MOVE TO ZONE", 5, 2, 102);  //6+(12x6)+12+10=100
        GLWin.AddMenu(100, "Grey", 5, 0, 1);
        GLWin.AddMenu(100, "Blue", 5, 0, 2);
        GLWin.AddMenu(100, "Green", 5, 0, 3);
        GLWin.AddMenu(100, "Red", 5, 0, 4);
        break;
      case 113:
        GLWin.AddMenu(78, "ARRANGE", 5, 2, 102);
        GLWin.AddMenu(78, "Default", 5, 0, 5);
        GLWin.AddMenu(78, "10x10", 5, 0, 6);
        GLWin.AddMenu(78, "10x10 2xSpc", 5, 0, 7);  //6+(11x6)+6=78
        GLWin.AddMenu(78, "Into Nets", 5, 0, 8);
        break;
      case 114:
        GLWin.AddMenu(76, "COMMANDS", 6, 2, 102);  //6+(8x6)+12+10=76
        GLWin.AddMenu(76, "Command 1", 6, 0, 27);
        GLWin.AddMenu(76, "Command 2", 6, 0, 28);
        GLWin.AddMenu(76, "Command 3", 6, 0, 29);
        GLWin.AddMenu(76, "Command 4", 6, 0, 30);
        GLWin.AddMenu(76, "Set", 6, 0, 38);
        break;
      case 115:
        GLWin.AddMenu(72, "RESET", 5, 2, 102);
        GLWin.AddMenu(72, "Link Lines", 5, 0, 39);  //6+(10x6)+6=72
        GLWin.AddMenu(72, "Downloads", 5, 0, 23);
        GLWin.AddMenu(72, "Uploads", 5, 0, 24);
        GLWin.AddMenu(72, "Services", 5, 0, 25);
        break;
      case 116:
        GLWin.AddMenu(64, "DELETE", 2, 2, 102);  //6+(6x6)+12+10=64
        GLWin.AddMenu(64, "Confirm", 2, 0, 9);
        break;
      case 117:
        GLWin.AddMenu(94, "ACKNOWLEDGE", 3, 2, 103);  //6+(11x6)+12+10=94
        GLWin.AddMenu(94, "Selection", 3, 0, 26);
        GLWin.AddMenu(94, menuLabelWithBinding("All", kaAcknowledgeAllAnomalies), 3, 0, 47);
        break;
      case 118:
        GLWin.AddMenu(76, "PROTOCOL", 7, 2, 105);  //6+(8x6)+12+10=76
        if (setts.pr) GLWin.AddMenu(76, "All", 7, 0, 61);
        if (setts.pr != IPPROTO_ICMP) GLWin.AddMenu(76, "ICMP", 7, 0, 62);
        if (setts.pr != IPPROTO_TCP) GLWin.AddMenu(76, "TCP", 7, 0, 63);
        if (setts.pr != IPPROTO_UDP) GLWin.AddMenu(76, "UDP", 7, 0, 64);
        if (setts.pr != IPPROTO_ARP) GLWin.AddMenu(76, "ARP", 7, 0, 65);
        GLWin.AddMenu(76, "Other", 7, 0, 66);
        break;
      case 119:
        GLWin.AddMenu(52, "PORT", 3, 2, 105);  //6+(4x6)+12+10=52
        if (setts.prt) GLWin.AddMenu(52, "All", 3, 0, 67);
        GLWin.AddMenu(52, "Enter", 3, 0, 68);
        break;
      case 120:
        GLWin.AddMenu(108, "RECALL", 6, 2, 107);
        GLWin.AddMenu(108, menuLabelWithBinding("Home", kaViewHomeAlt), 6, 0, 70);  //6+(16x6)+6=108
        GLWin.AddMenu(108, menuLabelWithBinding("Pos 1", kaViewPos1), 6, 0, 71);
        GLWin.AddMenu(108, menuLabelWithBinding("Pos 2", kaViewPos2), 6, 0, 72);
        GLWin.AddMenu(108, menuLabelWithBinding("Pos 3", kaViewPos3), 6, 0, 73);
        GLWin.AddMenu(108, menuLabelWithBinding("Pos 4", kaViewPos4), 6, 0, 74);
        break;
      case 121:
        GLWin.AddMenu(52, "SAVE", 5, 2, 107);  //6+(4x6)+12+10=52
        GLWin.AddMenu(52, "Pos 1", 5, 0, 75);
        GLWin.AddMenu(52, "Pos 2", 5, 0, 76);
        GLWin.AddMenu(52, "Pos 3", 5, 0, 77);
        GLWin.AddMenu(52, "Pos 4", 5, 0, 78);
        break;
      case 122:
        GLWin.AddMenu(70, "RESTORE", 6, 2, 108);  //6+(7x6)+12+10=70
        GLWin.AddMenu(70, "File", 6, 0, 80);
        GLWin.AddMenu(70, "Net 1", 6, 0, 81);
        GLWin.AddMenu(70, "Net 2", 6, 0, 82);
        GLWin.AddMenu(70, "Net 3", 6, 0, 83);
        GLWin.AddMenu(70, "Net 4", 6, 0, 84);
        break;
      case 123:
        GLWin.AddMenu(52, "SAVE", 6, 2, 108);  //6+(4x6)+12+10=52
        GLWin.AddMenu(52, "File", 6, 0, 85);
        GLWin.AddMenu(52, "Net 1", 6, 0, 86);
        GLWin.AddMenu(52, "Net 2", 6, 0, 87);
        GLWin.AddMenu(52, "Net 3", 6, 0, 88);
        GLWin.AddMenu(52, "Net 4", 6, 0, 89);
        break;
      case 124:
        GLWin.AddMenu(58, "CLEAR", 2, 2, 108);  //6+(5x6)+12+10=58
        GLWin.AddMenu(58, "Confirm", 2, 0, 92);
        break;
      case 125:
        GLWin.AddMenu(118, "SELECT INACTIVE", 6, 2, 109);  //6+(15x6)+12+10=118
        GLWin.AddMenu(118, "> 5 Minutes", 6, 0, 33);
        GLWin.AddMenu(118, "> 1 Hour", 6, 0, 34);
        GLWin.AddMenu(118, "> 1 Day", 6, 0, 35);
        GLWin.AddMenu(118, "> 1 Week", 6, 0, 36);
        GLWin.AddMenu(118, "> Other", 6, 0, 37);
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
  GLWin.AddButton(340, btnTop, GLWIN_CLOSE, "Close");
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
  if ((strlen(token) == 1) && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ[]=-", token[0])) return token[0];
  if ((strlen(token) == 1) && isdigit((unsigned char)token[0])) return token[0];
  if (!strcmp(token, "UP")) return GLFW_KEY_UP;
  if (!strcmp(token, "DOWN")) return GLFW_KEY_DOWN;
  if (!strcmp(token, "LEFT")) return GLFW_KEY_LEFT;
  if (!strcmp(token, "RIGHT")) return GLFW_KEY_RIGHT;
  if (!strcmp(token, "HOME")) return GLFW_KEY_HOME;
  if (!strcmp(token, "TAB")) return GLFW_KEY_TAB;
  if (!strcmp(token, "SPACE")) return GLFW_KEY_SPACE;
  if (!strcmp(token, "INSERT")) return GLFW_KEY_INSERT;
  if (!strcmp(token, "END")) return GLFW_KEY_END;
  if (!strcmp(token, "PAGEUP") || !strcmp(token, "PGUP")) return GLFW_KEY_PAGEUP;
  if (!strcmp(token, "PAGEDOWN") || !strcmp(token, "PGDOWN") || !strcmp(token, "PGDN")) return GLFW_KEY_PAGEDOWN;
  if (!strcmp(token, "PLUS") || !strcmp(token, "EQUAL") || !strcmp(token, "EQUALS")) return '=';
  if (!strcmp(token, "MINUS") || !strcmp(token, "DASH")) return '-';
  if (!strcmp(token, "LEFTBRACKET") || !strcmp(token, "OPENBRACKET")) return '[';
  if (!strcmp(token, "RIGHTBRACKET") || !strcmp(token, "CLOSEBRACKET")) return ']';
  if ((token[0] == 'F') && isdigit((unsigned char)token[1]) && !token[2])
  {
    switch (token[1])
    {
    case '1': return GLFW_KEY_F1;
    case '2': return GLFW_KEY_F2;
    case '3': return GLFW_KEY_F3;
    case '4': return GLFW_KEY_F4;
    case '5': return GLFW_KEY_F5;
    case '6': return GLFW_KEY_F6;
    case '7': return GLFW_KEY_F7;
    case '8': return GLFW_KEY_F8;
    }
  }
  return 0;
}

static bool parseBindingValue(const char *value, int *out)
{
  char compact[64], *keytxt = compact;
  bool ctrl = false;
  compactBindingToken(value, compact, sizeof(compact));
  if (!*keytxt) return false;
  if (!strcmp(keytxt, "NONE") || !strcmp(keytxt, "DISABLED"))
  {
    *out = 0;
    return true;
  }
  if (!strncmp(keytxt, "CTRL+", 5))
  {
    ctrl = true;
    keytxt += 5;
  }
  int keycode = keyCodeFromToken(keytxt);
  if (!keycode) return false;
  *out = (ctrl ? CTRLKEY(keycode) : keycode);
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
  if (encoded >= 512)
  {
    snprintf(buf, bufsz, "Ctrl + %s", keyCodeName(encoded - 512));
    return;
  }
  snprintf(buf, bufsz, "%s", keyCodeName(encoded));
}

static const char *menuLabelWithBinding(const char *title, keyact_type action)
{
  static char labels[8][96];
  static unsigned char idx = 0;
  char binding[32];
  idx = (idx + 1) % 8;
  if (!keybinds[action].key)
  {
    snprintf(labels[idx], sizeof(labels[idx]), "%s", title);
    return labels[idx];
  }
  bindingLabel(keybinds[action].key, binding, sizeof(binding));
  snprintf(labels[idx], sizeof(labels[idx]), "%s (%s)", title, binding);
  return labels[idx];
}

static keyact_type keyActionFromInput(int encoded)
{
  for (unsigned char cnt = 0; cnt < kaCount; cnt++)
  {
    if (keybinds[cnt].key && (keybinds[cnt].key == encoded)) return (keyact_type) cnt;
  }
  return kaCount;
}

static bool settsLoadIni(const char *path)
{
  FILE *sts;
  char line[1024];
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
    else if (!strcmp(key, "protocol_filter"))
    {
      if (parseUnsignedCharValue(value, &uc)) setts.pr = uc;
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
  fprintf(sts, "protocol_filter=%u\n", setts.pr);
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
    settsSaveIni(inipath);
    if (fileExists(legacypath)) remove(legacypath);
    return;
  }
  if (settsLoadLegacy(legacypath))
  {
    if (settsSaveIni(inipath)) remove(legacypath);
    return;
  }
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
    controlLineKey(ctls, kaShowHelp, "Show Help");
    fclose(ctls);
  }
}

//compile GL objects
void initObjsGL()
{
  objsDraw = glGenLists(22);
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
  glNewList(objsDraw + 15, GL_COMPILE);  //grey packet object
    pobjDraw(0);
  glEndList();
  glNewList(objsDraw + 16, GL_COMPILE);  //red packet object
    pobjDraw(1);
  glEndList();
  glNewList(objsDraw + 17, GL_COMPILE);  //green packet object
    pobjDraw(2);
  glEndList();
  glNewList(objsDraw + 18, GL_COMPILE);  //blue packet object
    pobjDraw(3);
  glEndList();
  glNewList(objsDraw + 19, GL_COMPILE);  //yellow packet object
    pobjDraw(4);
  glEndList();
  glNewList(objsDraw + 20, GL_COMPILE);  //active alert object
    aobjDraw(false);
  glEndList();
  glNewList(objsDraw + 21, GL_COMPILE);  //anomaly alert object
    aobjDraw(true);
  glEndList();
}

void pktProcessCb(void **data, long arg1, long arg2, long arg3, long arg4)
{
  host_type *dh = *((host_type **) data);
  host_type *sh = (host_type *)(void *) arg1;
  in_addr_t mask = (in_addr_t) arg2;
  in_addr_t dstip = (in_addr_t) arg3;
  pkif_type *pkif = (pkif_type *)(void *) arg4;

  if ((dh != sh) && ((dh->hip.s_addr & mask) == dstip))
    pcktCreate(pkif, sh, dh, false);
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
  int rcsz, prsz = sizeof(pkrp), pisz = sizeof(pkif_type), pesz = sizeof(pkex_type), dnsz = sizeof(pdns_type), ptsz = sizeof(ptm);
  in_addr_t mask, dstip;
  unsigned long long rpytm;
  char pbuf[dnsz];
  host_type *sh, *dh;
  while (goRun)
  {
    if (!goHosts)
    {
      pksc = none;
      if (ptrc == hlt)
      {
        fclose(pfile);
        ptrc = stp;
      }
      if (ptrc == rpy)
      {
        rpytm = milliTime(0) - rpoff;
        if (!feof(pfile) && (rpytm >= milliTime(&pkrp.ptime)))
        {
          pkif = &pkrp.pk;
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
            else if (fwrite(pkif, pisz, 1, pfile) != 1) ptrc = hlt;
          }
        }
        else if ((pbuf[0] == 42) && (rcsz == dnsz))  //42 used to identify DNS info
        {
          pdns = (pdns_type *)pbuf;
          if ((sh = hostIP(pdns->dnsip, false)))
          {
            if (!*sh->htnm) strcpy(sh->htnm, pdns->htnm);
          }
        }
      }
      if (pksc)
      {
        sh = hostIP(pkif->srcip, true);
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
          if (pksc == sckt)
          {
            dh->lsn = pkif->sen;
            dh->dld += pkex->sz;
            time(&dh->lpk);
          }
          pcktCreate(pkif, sh, dh, true);
        }
        else if (setts.bct && (mask = isBroadcast(pkif->dstip)))
        {
          dstip = pkif->dstip.s_addr & mask;
	  hstsByIp.forEach(2, &pktProcessCb, (long)(void *) sh, (long) mask,
			   (long) dstip, (long)(void *) pkif);
        }
        if (pksc == pkfl)
        {
          if (fread(&pkrp, prsz, 1, pfile) != 1) ptrc = hlt;
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
    fprintf(stderr, "hosts3d 1.16 usage: %s [-f]\n  -f : display full screen\n", argv[0]);
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
  osdUpdate();
  netLoad(hsddata("0network.hnl"));  //load network layout 0
  if (goHosts) goHosts = 0;
  netpsLoad();  //load netpos
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
  glDeleteLists(objsDraw, 22);
  glfwTerminate();
#ifndef __MINGW32__
  syslog(LOG_INFO, "stopped\n");
#endif
  return 0;
}
