/*
** Winamp for Linux - MPRIS 2 (D-Bus) interface, so desktop media keys,
** sound menus and widgets can control the player.
*/
#pragma once

void mpris_init();
void mpris_shutdown();
void mpris_update(); // call when the play state, song, volume, shuffle or repeat changed
