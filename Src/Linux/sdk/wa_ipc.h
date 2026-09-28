/*
** Winamp for Linux - subset of Src/Winamp/wa_ipc.h understood by the player.
** Values are identical to the Windows ones so plug-in code can be shared.
*/
#ifndef NULLSOFT_WINAMP_LINUX_WA_IPC_H
#define NULLSOFT_WINAMP_LINUX_WA_IPC_H

#include "wa_linux.h"

#define WM_WA_IPC WM_USER
#define WM_WA_MPEG_EOF (WM_USER + 2)

#define IPC_GETVERSION 0
#define IPC_STARTPLAY 102
#define IPC_ISPLAYING 104
#define IPC_GETOUTPUTTIME 105
#define IPC_JUMPTOTIME 106
#define IPC_SETPLAYLISTPOS 121
#define IPC_SETVOLUME 122
#define IPC_SETPANNING 123
#define IPC_GETLISTLENGTH 124
#define IPC_GETLISTPOS 125
#define IPC_GETINFO 126
#define IPC_GETPLAYLISTFILE 211
#define IPC_GETPLAYLISTTITLE 212
#define IPC_UPDTITLE 243
#define IPC_GET_SHUFFLE 250
#define IPC_GET_REPEAT 251
#define IPC_GETINIFILE 334
#define IPC_GETINIDIRECTORY 335

#endif
