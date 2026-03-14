/*
 * menu.h — Menu bar and status bar interface
 * Handles the top-level menu bar, dropdown navigation, and command dispatch.
 * Dialog boxes are declared in dialog.h (included below for convenience).
 */
#pragma once

#include <stdbool.h>
#include "types.h"
#include "editor.h"
#include "dialog.h"

/* Menu system state — passed by pointer to all menu functions. */
typedef struct {
    bool active;         /* true while menu bar has keyboard focus */
    int  top_item;       /* highlighted top-level item index (-1 = none) */
    int  sub_item;       /* highlighted sub-item index (-1 = none) */
    bool dropdown_open;  /* true when a dropdown is visible */
} MenuState;

void menu_init(MenuState *ms);

/* Render menu bar (call every frame before display_flush). */
void menu_render_bar(const MenuState *ms);

/* Render open dropdown on top of the edit area. */
void menu_render_dropdown(const MenuState *ms);

/* Feed an input event to the menu system.
 * Returns true when the event was consumed (editor should not process it). */
bool menu_handle_event(MenuState *ms, Editor *ed, const InputEvent *ev);

/* Returns true after the user selected Exit or the exit shortcut. */
bool menu_should_exit(void);

/* Render the status bar at the bottom of the screen (call every frame). */
void render_status_bar(const Editor *ed);
