/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/


#include <config.h>

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "common.h"
#include "utils.h"
#include "core.h"
#include "rb.h"
#include "cdda.h"
#include "gui_main.h"
#include "music_browser.h"
#include "file_info.h"
#include "decoder/dec_mod.h"
#include "decoder/file_decoder.h"
#include "meta_decoder.h"
#include "volume.h"
#include "options.h"
#include "i18n.h"
#include "search_playlist.h"
#include "playlist.h"
#include "ifp_device.h"

extern options_t options;

extern void record_addlist_iter(GtkTreeIter iter_record, playlist_t * pl,
				GtkTreeIter * dest, int album_mode);

extern void assign_audio_fc_filters(GtkFileChooser *fc);
extern void assign_playlist_fc_filters(GtkFileChooser *fc);

extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern GtkTooltips * aqualung_tooltips;

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

extern PangoFontDescription *fd_playlist;
extern PangoFontDescription *fd_statusbar;

GtkWidget * playlist_window;

extern GtkWidget * main_window;
extern GtkWidget * info_window;
extern GtkWidget * vol_window;

extern GtkTreeSelection * music_select;

gint playlist_color_is_set;

gint playlist_drag_y;
gint playlist_scroll_up_tag = -1;
gint playlist_scroll_dn_tag = -1;


extern gulong play_id;
extern gulong pause_id;
extern GtkWidget * play_button;
extern GtkWidget * pause_button;

extern int vol_finished;
extern int vol_index;
gint vol_n_tracks;
gint vol_is_average;
vol_queue_t * pl_vol_queue;

/* used to store array of tree iters of tracks selected for RVA calc */
GtkTreeIter * vol_iters;


GtkWidget * play_list;
GtkTreeStore * play_store = 0;
GtkTreeSelection * play_select;

GtkWidget * statusbar_total;
GtkWidget * statusbar_selected;
GtkWidget * statusbar_total_label;
GtkWidget * statusbar_selected_label;

volatile int playlist_thread_stop; 
volatile int playlist_data_written;
int pl_progress_bar_semaphore;

AQUALUNG_THREAD_DECLARE(playlist_thread_id)
AQUALUNG_MUTEX_DECLARE(playlist_thread_mutex)
AQUALUNG_MUTEX_DECLARE(playlist_wait_mutex)
AQUALUNG_COND_DECLARE_INIT(playlist_thread_wait)

/* popup menus */
GtkWidget * add_menu;
GtkWidget * add__files;
GtkWidget * add__dir;
GtkWidget * add__url;
GtkWidget * sel_menu;
GtkWidget * sel__none;
GtkWidget * sel__all;
GtkWidget * sel__inv;
GtkWidget * rem_menu;
GtkWidget * cut__sel;
GtkWidget * rem__all;
GtkWidget * rem__dead;
GtkWidget * rem__sel;
GtkWidget * plist_menu;
GtkWidget * plist__save;
GtkWidget * plist__save_all;
GtkWidget * plist__load;
GtkWidget * plist__enqueue;
GtkWidget * plist__load_tab;
GtkWidget * plist__rva;
GtkWidget * plist__rva_menu;
GtkWidget * plist__rva_separate;
GtkWidget * plist__rva_average;
GtkWidget * plist__reread_file_meta;
GtkWidget * plist__fileinfo;
GtkWidget * plist__search;
#ifdef HAVE_IFP
GtkWidget * plist__send_songs_to_iriver;
#endif /* HAVE_IFP */

gchar command[RB_CONTROL_SIZE];

gchar fileinfo_name[MAXLEN];
gchar fileinfo_file[MAXLEN];

extern int is_file_loaded;
extern int is_paused;
extern int allow_seeks;

extern char current_file[MAXLEN];

extern rb_t * rb_gui2disk;

extern GtkWidget * playlist_toggle;


typedef struct _playlist_filemeta {
        char * title;
	char * filename;
        float duration;
        float voladj;
	int active;
	int level;
} playlist_filemeta;


playlist_t * playlist_tab_new(char * name);
void playlist_tab_close(GtkButton * button, gpointer data);
void playlist_progress_bar_hide(playlist_t * pl);

playlist_filemeta * playlist_filemeta_get(char * physical_name, char * alt_name, int composit);
void playlist_filemeta_free(playlist_filemeta * plfm);

void remove_files(playlist_t * pl);
void set_cursor_in_playlist(playlist_t * pl, GtkTreeIter *iter, gboolean scroll);
void select_active_position_in_playlist(playlist_t * pl);

void playlist_selection_changed(playlist_t * pl);
void playlist_selection_changed_cb(GtkTreeSelection * select, gpointer data);

void sel__all_cb(gpointer data);
void sel__none_cb(gpointer data);
void rem__all_cb(gpointer data);
void rem__sel_cb(gpointer data);
void cut__sel_cb(gpointer data);
void plist__search_cb(gpointer data);
void rem_all(playlist_t * pl);
void add_files(GtkWidget * widget, gpointer data);
void recalc_album_node(playlist_t * pl, GtkTreeIter * iter);

void add_directory(GtkWidget * widget, gpointer data);
void init_plist_menu(GtkWidget *append_menu);
void show_active_position_in_playlist(playlist_t * pl);
void show_active_position_in_playlist_toggle(playlist_t * pl);
void expand_collapse_album_node(playlist_t * pl);

void playlist_save(playlist_t * pl, char * filename);

void playlist_load_xml(char * filename, int mode, playlist_transfer_t * ptrans);
void load_m3u(char * filename, int enqueue);
void load_pls(char * filename, int enqueue);
gint is_playlist(char * filename);


GtkWidget * playlist_notebook;

GList * playlists;


playlist_t *
playlist_new(char * name) {

	playlist_t * pl = NULL;

	if ((pl = (playlist_t *)calloc(1, sizeof(playlist_t))) == NULL) {
		fprintf(stderr, "playlist_new(): calloc error\n");
		return NULL;
	}

	playlists = g_list_append(playlists, pl);

	AQUALUNG_COND_INIT(pl->thread_wait);

#ifdef _WIN32
	pl->thread_mutex = g_mutex_new();
	pl->wait_mutex = g_mutex_new();
	pl->thread_wait = g_cond_new();
#endif

	strncpy(pl->name, name, MAXLEN-1);

	pl->store = gtk_tree_store_new(NUMBER_OF_COLUMNS,
				       G_TYPE_STRING,    /* track name */
				       G_TYPE_STRING,    /* physical filename */
				       G_TYPE_STRING,    /* color for selections */
				       G_TYPE_FLOAT,     /* volume adjustment [dB] */
				       G_TYPE_STRING,    /* volume adj. displayed */
				       G_TYPE_FLOAT,     /* duration (in secs) */
				       G_TYPE_STRING,    /* duration displayed */
				       G_TYPE_INT);      /* font weight */
	return pl;
}

void
playlist_free(playlist_t * pl) {

	playlists = g_list_remove(playlists, pl);

#ifdef _WIN32
	g_mutex_free(pl->thread_mutex);
	g_mutex_free(pl->wait_mutex);
	g_cond_free(pl->thread_wait);
#endif

	free(pl);
}


gint
playlist_compare_widget(gconstpointer list, gconstpointer widget) {

	return ((playlist_t *)list)->widget != widget;
}

gint
playlist_compare_playing(gconstpointer list, gconstpointer dummy) {

	return ((playlist_t *)list)->playing == 0;
}

gint
playlist_compare_name(gconstpointer list, gconstpointer name) {

	return strcmp(((playlist_t *)list)->name, (char *)name);
}

playlist_t *
playlist_find(gconstpointer data, GCompareFunc func) {

	GList * find = g_list_find_custom(playlists, data, func);

	if (find != NULL) {
		return (playlist_t *)(find->data);
	}

	return NULL;
}


playlist_t *
playlist_get_current() {

	int idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(playlist_notebook));
	GtkWidget * widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), idx);

	playlist_t * pl = playlist_find(widget, playlist_compare_widget);

	if (pl == NULL) {
		printf("WARNING: playlist_get_current() == NULL\n");
	}

	return pl;
}

playlist_t *
playlist_get_playing() {

	return playlist_find(NULL/*dummy*/, playlist_compare_playing);
}

playlist_t *
playlist_get_by_page_num(int num) {

	GtkWidget * widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), num);

	playlist_t * pl = playlist_find(widget, playlist_compare_widget);

	if (pl == NULL) {
		printf("WARNING: playlist_get_by_page_num() == NULL\n");
	}

	return pl;
}


playlist_t *
playlist_get_by_name(char * name) {

	return playlist_find(name, playlist_compare_name);
}


playlist_transfer_t *
playlist_transfer_new(playlist_t * pl) {

	playlist_transfer_t * pt = NULL;

	if ((pt = (playlist_transfer_t *)calloc(1, sizeof(playlist_transfer_t))) == NULL) {
		fprintf(stderr, "playlist_transfer_new(): calloc error\n");
		return NULL;
	}

	if (pl == NULL) {
		if ((pt->pl = playlist_get_current()) == NULL) {
			free(pt);
			return NULL;
		}
	} else {
		pt->pl = pl;
	}

	pt->index = -1;

	return pt;
}


void
playlist_disable_bold_font_foreach(gpointer data, gpointer user_data) {

        GtkTreeIter iter;
	gint i = 0;

	GtkTreeStore * store = ((playlist_t *)data)->store;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i++)) {
		gint j = 0;
		GtkTreeIter iter_child;

		gtk_tree_store_set(store, &iter, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, &iter, j++)) {
			gtk_tree_store_set(store, &iter_child, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
		}
	}
}

void
playlist_disable_bold_font() {

	g_list_foreach(playlists, playlist_disable_bold_font_foreach, NULL);
}


void
adjust_playlist_item_color(GtkTreeStore * store, GtkTreeIter * piter,
			   char * active, char * inactive) {

	gchar * str;

	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, COLUMN_SELECTION_COLOR, &str, -1);
	
	if (strcmp(str, pl_color_active) == 0) {
		gtk_tree_store_set(store, piter, COLUMN_SELECTION_COLOR, active, -1);
		if (options.show_active_track_name_in_bold) {
			gtk_tree_store_set(store, piter, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_BOLD, -1);
		}
	}
	
	if (strcmp(str, pl_color_inactive) == 0) {
		gtk_tree_store_set(store, piter,
				   COLUMN_SELECTION_COLOR, inactive,
				   COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
	}

	g_free(str);
}


void
set_playlist_color_foreach(gpointer data, gpointer user_data) {

	
        GdkColor color;
	GtkTreeIter iter;
	gchar active[14];
	gchar inactive[14];
	gint i = 0;

	playlist_t * pl = (playlist_t *)data;

        if (options.override_skin_settings &&
	    (gdk_color_parse(options.activesong_color, &color) == TRUE)) {

                if (!color.red && !color.green && !color.blue) {
                        color.red++;
		}

                pl->view->style->fg[SELECTED].red = color.red;
                pl->view->style->fg[SELECTED].green = color.green;
                pl->view->style->fg[SELECTED].blue = color.blue;
        }

        sprintf(active, "#%04X%04X%04X",
		pl->view->style->fg[SELECTED].red,
		pl->view->style->fg[SELECTED].green,
		pl->view->style->fg[SELECTED].blue);

	sprintf(inactive, "#%04X%04X%04X",
		pl->view->style->fg[INSENSITIVE].red,
		pl->view->style->fg[INSENSITIVE].green,
		pl->view->style->fg[INSENSITIVE].blue);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {
		gint j = 0;
		GtkTreeIter iter_child;

		adjust_playlist_item_color(pl->store, &iter, active, inactive);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
			adjust_playlist_item_color(pl->store, &iter_child, active, inactive);
		}
        }

        strcpy(pl_color_active, active);
	strcpy(pl_color_inactive, inactive);
}

void
set_playlist_color(void) {
	
	g_list_foreach(playlists, set_playlist_color_foreach, NULL);
}



void
playlist_foreach_selected(playlist_t * pl, void (* foreach)(playlist_t *, GtkTreeIter *, void *),
			  void * data) {

	GtkTreeIter iter_top;
	GtkTreeIter iter;
	gint i;
	gint j;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_top, NULL, i++)) {

		gboolean topsel = gtk_tree_selection_iter_is_selected(pl->select, &iter_top);

		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter_top)) {

			j = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, &iter_top, j++)) {
				if (topsel || gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
					(*foreach)(pl, &iter, data);
				}
			}

		} else if (topsel) {
			(*foreach)(pl, &iter_top, data);
		}
	}
}


GtkTreePath *
playlist_get_playing_path(playlist_t * pl) {

	GtkTreeIter iter;
	gchar * color;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, COLUMN_SELECTION_COLOR, &color, -1);

		if (strcmp(color, pl_color_active) == 0) {
			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {

				GtkTreeIter iter_child;
				gchar * color_child;
				int j = 0;

				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter_child, &iter, j++)) {

					gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child,
							   COLUMN_SELECTION_COLOR, &color_child, -1);

					if (strcmp(color_child, pl_color_active) == 0) {
						g_free(color);
						g_free(color_child);
						return gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), &iter_child);
					}

					g_free(color_child);
				}
			} else {
				g_free(color);
				return gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), &iter);
			}
		}

		g_free(color);
	}

	return NULL;
}


void
mark_track(GtkTreeStore * store, GtkTreeIter * piter) {

        int j, n;
        char * track_name;
	char *str;
        char counter[MAXLEN];
	char tmptrackname[MAXLEN];

	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, COLUMN_SELECTION_COLOR, &str, -1);
	if (strcmp(str, pl_color_active) == 0) {
		g_free(str);
		return;
	}
	g_free(str);

        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), piter);

        if (n) {
		GtkTreeIter iter_child;

                gtk_tree_model_get(GTK_TREE_MODEL(store), piter,
				   COLUMN_TRACK_NAME, &track_name, -1);
                strncpy(tmptrackname, track_name, MAXLEN-1);

                j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, piter, j++)) {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter_child,
					   COLUMN_SELECTION_COLOR, &str, -1);
			if (strcmp(str, pl_color_active) == 0) {
				g_free(str);
                                break;
			}
			g_free(str);
		}

		if (j > n) {
			return;
		}

                sprintf(counter, _(" (%d/%d)"), j, n);
                strncat(tmptrackname, counter, MAXLEN-1);
		gtk_tree_store_set(store, piter, COLUMN_TRACK_NAME, tmptrackname, -1);
                g_free(track_name);
        }

	gtk_tree_store_set(store, piter, COLUMN_SELECTION_COLOR, pl_color_active, -1);
	if (options.show_active_track_name_in_bold) {
		gtk_tree_store_set(store, piter, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_BOLD, -1);
	}

	if (gtk_tree_store_iter_depth(store, piter)) { /* track node of album */
		GtkTreeIter iter_parent;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &iter_parent, piter);
		mark_track(store, &iter_parent);
	}
}


void
unmark_track(GtkTreeStore * store, GtkTreeIter * piter) {

	int n;

	gtk_tree_store_set(store, piter, 2, pl_color_inactive, -1);
	gtk_tree_store_set(store, piter, 7, PANGO_WEIGHT_NORMAL, -1);

        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), piter);

        if (n) {
		/* unmarking an album node: cut the counter string from the end */

		char * name;
		char * pack;
		int len1, len2;

		gtk_tree_model_get(GTK_TREE_MODEL(store), piter, 0, &name, 1, &pack, -1);

		sscanf(pack, "%04X%04X", &len1, &len2);

		/* the +2 in the index below is the length of the ": "
		   string which is put between the artist and album
		   names in music_browser.c: record_addlist_iter() */
		name[len1 + len2 + 2] = '\0';

		gtk_tree_store_set(store, piter, 0, name, -1);

		g_free(pack);
		g_free(name);
	}

	if (gtk_tree_store_iter_depth(store, piter)) { /* track node of album */
		GtkTreeIter iter_parent;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &iter_parent, piter);
		unmark_track(store, &iter_parent);
	}
}


void
playlist_start_playback_at_path(playlist_t * pl, GtkTreePath * path) {

	GtkTreeIter iter;
	GtkTreePath * p;
	gchar cmd;
	cue_t cue;

	playlist_t * plist = NULL;


	if (!allow_seeks) {
		return;
	}

	while ((plist = playlist_get_playing()) != NULL) {
		plist->playing = 0;
	}

	pl->playing = 1;

	p = playlist_get_playing_path(pl);
 	if (p != NULL) {
 		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
 		gtk_tree_path_free(p);
		unmark_track(pl->store, &iter);
 	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path);

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {
		GtkTreeIter iter_child;
		gtk_tree_model_iter_children(GTK_TREE_MODEL(pl->store), &iter_child, &iter);
		mark_track(pl->store, &iter_child);
	} else {
		mark_track(pl->store, &iter);
	}
	
	p = playlist_get_playing_path(pl);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
	gtk_tree_path_free(p);
	cue_track_for_playback(pl->store, &iter, &cue);

        if (options.show_sn_title) {
		refresh_displays();
        }

	toggle_noeffect(PLAY, TRUE);

	if (is_paused) {
		is_paused = 0;
		toggle_noeffect(PAUSE, FALSE);
	}
		
	cmd = CMD_CUE;
	rb_write(rb_gui2disk, &cmd, sizeof(char));
	rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
	try_waking_disk_thread();
	
	is_file_loaded = 1;
}


static gboolean
playlist_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);

	return TRUE;
}


gint
playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	gchar * pname;
	gchar * pfile;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return TRUE;
	}

        switch (kevent->keyval) {

        case GDK_Insert:
	case GDK_KP_Insert:
                if (kevent->state & GDK_SHIFT_MASK) {  /* SHIFT + Insert */
                        add_directory(NULL, NULL);
                } else {
                        add_files(NULL, NULL);
                }
                return TRUE;
        case GDK_q:
	case GDK_Q:
	case GDK_Escape:
                if (!options.playlist_is_embedded) {
                        playlist_window_close(NULL, NULL, NULL);
		}
		return TRUE;
	case GDK_F1:
	case GDK_i:
	case GDK_I:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, &column);

		if (path &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path) &&
		    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {

			GtkTreeIter dummy;
			
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   COLUMN_TRACK_NAME, &pname,
					   COLUMN_PHYSICAL_FILENAME, &pfile, -1);
			
			strncpy(fileinfo_name, pname, MAXLEN-1);
			strncpy(fileinfo_file, pfile, MAXLEN-1);
			free(pname);
			free(pfile);
			show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy);
		}
		return TRUE;
	case GDK_Return:
	case GDK_KP_Enter:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, &column);

		if (path) {
			playlist_start_playback_at_path(pl, path);
		}		
		return TRUE;
	case GDK_u:
	case GDK_U:
		cut__sel_cb(NULL);
		return TRUE;
	case GDK_f:
	case GDK_F:
		plist__search_cb(pl);
		return TRUE;
        case GDK_a:
        case GDK_A:
		if (kevent->state & GDK_CONTROL_MASK) {
			if (kevent->state & GDK_SHIFT_MASK) {
				sel__none_cb(NULL);
			} else {
				sel__all_cb(NULL);
			}
		} else {
			show_active_position_in_playlist_toggle(pl);
		}
                return TRUE;
        case GDK_w:
        case GDK_W:
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_tab_close(NULL, pl);
		} else {
			gtk_tree_view_collapse_all(GTK_TREE_VIEW(pl->view));
			show_active_position_in_playlist(pl);
		}
                return TRUE;
        case GDK_Delete:
	case GDK_KP_Delete:
                if (kevent->state & GDK_SHIFT_MASK) {  /* SHIFT + Delete */
			rem__all_cb(NULL);
                } else if (kevent->state & GDK_CONTROL_MASK) {  /* CTRL + Delete */
                        remove_files(NULL);
                } else {
			rem__sel_cb(NULL);
                }
                return TRUE;
	case GDK_t:
	case GDK_T:
		if (kevent->state & GDK_CONTROL_MASK) {  /* CTRL + T */
			playlist_tab_new(NULL);
		}
#ifdef HAVE_IFP
		else {
			aifp_transfer_files();
		}
#endif /* HAVE_IFP */
		return TRUE;
		break;
        case GDK_grave:
                expand_collapse_album_node(pl);
                return TRUE;
	case GDK_Page_Up:
		if (kevent->state & GDK_CONTROL_MASK) {
			gtk_notebook_prev_page(GTK_NOTEBOOK(playlist_notebook));
			return FALSE;
		}
		return TRUE;
	case GDK_Page_Down:
		if (kevent->state & GDK_CONTROL_MASK) {
			gtk_notebook_next_page(GTK_NOTEBOOK(playlist_notebook));
			return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}


gint
doubleclick_handler(GtkWidget * widget, GdkEventButton * event, gpointer data) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	gchar * pname;
	gchar * pfile;
        gboolean http;
	playlist_t * pl = (playlist_t *)data;

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(pl->view), event->x, event->y,
						  &path, &column, NULL, NULL)) {
			playlist_start_playback_at_path(pl, path);
		}
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(pl->view), event->x, event->y,
						  &path, &column, NULL, NULL) &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path) &&
		    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {

			if (gtk_tree_selection_count_selected_rows(pl->select) == 0)
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(pl->view), path, NULL, FALSE);

			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   COLUMN_TRACK_NAME, &pname, COLUMN_PHYSICAL_FILENAME, &pfile, -1);

                        http = httpc_is_url(pfile);
	                gtk_widget_set_sensitive(plist__rva_separate, !http);
	                gtk_widget_set_sensitive(plist__rva_average, !http);
#ifdef HAVE_IFP
		        gtk_widget_set_sensitive(plist__send_songs_to_iriver, !http);
#endif  /* HAVE_IFP */

                        strncpy(fileinfo_name, pname, MAXLEN-1);
			strncpy(fileinfo_file, pfile, MAXLEN-1);
			free(pname);
			free(pfile);
					
			gtk_widget_set_sensitive(plist__fileinfo, TRUE);
		} else {
			gtk_widget_set_sensitive(plist__fileinfo, FALSE);
		}

		gtk_widget_set_sensitive(plist__rva, (vol_window == NULL) ? TRUE : FALSE);

		gtk_menu_popup(GTK_MENU(plist_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
		return TRUE;
	}
	return FALSE;
}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
}


gboolean
finalize_add_to_playlist(gpointer data) {

	playlist_transfer_t * pt = (playlist_transfer_t *)data;

	pt->pl->progbar_semaphore--;

	if (playlist_window != NULL && pt->pl->progbar_semaphore == 0) {
		playlist_content_changed(pt->pl);
		playlist_progress_bar_hide(pt->pl);
		delayed_playlist_rearrange();
		select_active_position_in_playlist(pt->pl);
	}

	/*
	if (pt->xml_doc != NULL) {
		xmlFreeDoc((xmlDocPtr)pt->xml_doc);
	}
	*/

	free(pt);

	return FALSE;
}



gboolean
add_file_to_playlist(gpointer data) {

	playlist_transfer_t * pt = (playlist_transfer_t *)data;
	playlist_filemeta * plfm = (playlist_filemeta *)pt->plfm;

	AQUALUNG_MUTEX_LOCK(pt->pl->wait_mutex);

	if (pt->clear) {
		rem_all(pt->pl);
		pt->clear = 0;
	}

	if (plfm != NULL) {

		GtkTreeIter iter;
		GtkTreePath * path;
		gchar voladj_str[32];
		gchar duration_str[MAXLEN];


		voladj2str(plfm->voladj, voladj_str);
		time2time_na(plfm->duration, duration_str);

		if (plfm->level == 0) {

			/* deactivate toplevel item if playing path already exists */
			if (plfm->active &&
			    (path = playlist_get_playing_path(pt->pl)) != NULL) {
				plfm->active = 0;
				gtk_tree_path_free(path);
			}

			gtk_tree_store_append(pt->pl->store, &iter, NULL);
		} else {
			GtkTreeIter parent;
			int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pt->pl->store), NULL);

			if (n == 0) {
				/* someone viciously cleared the list while adding tracks to album node;
				   ignore further tracks to this node */
				goto finish;
			}

			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pt->pl->store), &parent, NULL, n-1);
			gtk_tree_store_append(pt->pl->store, &iter, &parent);
		}

		gtk_tree_store_set(pt->pl->store, &iter,
			   COLUMN_TRACK_NAME, plfm->title,
			   COLUMN_PHYSICAL_FILENAME, plfm->filename,
			   COLUMN_SELECTION_COLOR, plfm->active ? pl_color_active : pl_color_inactive,
			   COLUMN_VOLUME_ADJUSTMENT, plfm->voladj,
			   COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
			   COLUMN_DURATION, plfm->duration,
			   COLUMN_DURATION_DISP, duration_str,
			   COLUMN_FONT_WEIGHT, (plfm->active && options.show_active_track_name_in_bold) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, 
                           -1);
	}

 finish:
	if (plfm != NULL) {
		playlist_filemeta_free(plfm);
	}

	pt->pl->data_written--;

	if (pt->pl->data_written == 0) {
		AQUALUNG_COND_SIGNAL(pt->pl->thread_wait)
	}

	AQUALUNG_MUTEX_UNLOCK(pt->pl->wait_mutex)

	return FALSE;
}


void
playlist_thread_add_to_list(playlist_transfer_t * pt) {

	AQUALUNG_MUTEX_LOCK(pt->pl->wait_mutex);
	pt->pl->data_written++;

	g_idle_add(add_file_to_playlist, pt);

	if (pt->pl->data_written > 0) {
		AQUALUNG_COND_WAIT(pt->pl->thread_wait, pt->pl->wait_mutex);
	}

	AQUALUNG_MUTEX_UNLOCK(pt->pl->wait_mutex);
}

void *
add_files_to_playlist_thread(void * arg) {

	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
	GSList * node = NULL;

	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	printf("--> add_files_to_playlist_thread\n");

	for (node = pt->list; node; node = node->next) {

		if (!pt->pl->thread_stop) {
			playlist_filemeta * plfm = NULL;

			if ((plfm = playlist_filemeta_get((char *)node->data, NULL, 1)) != NULL) {
				pt->plfm = plfm;
				playlist_thread_add_to_list(pt);
			}
		}

		g_free(node->data);
	}

	g_slist_free(pt->list);

	g_idle_add(finalize_add_to_playlist, pt);

	printf("<-- add_files_to_playlist_thread\n");

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	return NULL;
}


void
add_files(GtkWidget * widget, gpointer data) {

        GtkWidget *dialog;

        dialog = gtk_file_chooser_dialog_new(_("Select files"), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		playlist_transfer_t * pt = playlist_transfer_get(PLAYLIST_ENQUEUE, NULL);

		if (pt != NULL) {

			strncpy(options.currdir,
				gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
				MAXLEN-1);

			pt->list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

			playlist_progress_bar_show(pt->pl);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       add_files_to_playlist_thread,
					       pt);
		}
        }

        gtk_widget_destroy(dialog);
}


void
add_dir_to_playlist(playlist_transfer_t * pt, char * dirname) {

	gint i, n;
	struct dirent ** ent;
	struct stat st_file;
	gchar path[MAXLEN];


	if (pt->pl->thread_stop) {
		return;
	}

	n = scandir(dirname, &ent, filter, alphasort);
	for (i = 0; i < n; i++) {

		if (pt->pl->thread_stop) {
			break;
		}

		snprintf(path, MAXLEN-1, "%s/%s", dirname, ent[i]->d_name);

		if (stat(path, &st_file) == -1) {
			free(ent[i]);
			continue;
		}

		if (S_ISDIR(st_file.st_mode)) {
			add_dir_to_playlist(pt, path);
		} else {
			playlist_filemeta * plfm = NULL;

			if ((plfm = playlist_filemeta_get(path, NULL, 1)) != NULL) {

				pt->plfm = plfm;
				playlist_thread_add_to_list(pt);
			}
		}

		free(ent[i]);
	}

	while (i < n) {
		free(ent[i]);
		++i;
	}

	if (n > 0) {
		free(ent);
	}
}


void *
add_dir_to_playlist_thread(void * arg) {


	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
	GSList * node = NULL;

	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	printf("--> add_dir_to_playlist_thread\n");

	for (node = pt->list; node; node = node->next) {

		add_dir_to_playlist(pt, (char *)node->data);
		g_free(node->data);
	}

	g_slist_free(pt->list);

	g_idle_add(finalize_add_to_playlist, pt);

	printf("<-- add_dir_to_playlist_thread\n");

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex)

	return NULL;
}


void
add_directory(GtkWidget * widget, gpointer data) {

        GtkWidget *dialog;

        dialog = gtk_file_chooser_dialog_new(_("Select directory"), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
					     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		playlist_transfer_t * pt = playlist_transfer_get(PLAYLIST_ENQUEUE, NULL);

		if (pt != NULL) {
			strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
				MAXLEN-1);

			pt->list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

			playlist_progress_bar_show(pt->pl);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       add_dir_to_playlist_thread, pt);
		}
        }

        gtk_widget_destroy(dialog);
}


void
add_url(GtkWidget * widget, gpointer data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * hbox2;
	GtkWidget * url_entry;
	GtkWidget * url_label;
	char url[MAXLEN];

        dialog = gtk_dialog_new_with_buttons(_("Add URL"),
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, -1);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	url_label = gtk_label_new(_("URL:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), url_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

        url_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(url_entry), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(url_entry), "http://");
        gtk_box_pack_start(GTK_BOX(hbox2), url_entry, TRUE, TRUE, 0);

	gtk_widget_grab_focus(url_entry);

	gtk_widget_show_all(dialog);

 display:
	url[0] = '\0';
        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		
		GtkTreeIter iter;
		float duration = 0.0f;
		float voladj = options.rva_no_rva_voladj;
		gchar voladj_str[32];
		gchar duration_str[MAXLEN];
		playlist_t * pl = playlist_get_current();

                strncpy(url, gtk_entry_get_text(GTK_ENTRY(url_entry)), MAXLEN-1);
		
		if (url[0] == '\0' || strstr(url, "http://") != url || strlen(url) <= strlen("http://")) {
			gtk_widget_grab_focus(url_entry);
			goto display;
		}

		voladj2str(voladj, voladj_str);
		time2time_na(0.0f, duration_str);
		gtk_tree_store_append(pl->store, &iter, NULL);
		gtk_tree_store_set(pl->store, &iter,
			   COLUMN_TRACK_NAME, url,
			   COLUMN_PHYSICAL_FILENAME, url,
			   COLUMN_SELECTION_COLOR, pl_color_inactive,
			   COLUMN_VOLUME_ADJUSTMENT, voladj,
			   COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
			   COLUMN_DURATION, duration,
			   COLUMN_DURATION_DISP, duration_str,
			   COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL,
                           -1);
        }

        gtk_widget_destroy(dialog);
        return;
}


void
plist__save_cb(gpointer data) {

        GtkWidget * dialog;
        gchar * selected_filename;

	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to save the playlist to."),
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window),
                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                             GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

        deflicker();
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "playlist.xml");

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                strncpy(options.currdir, selected_filename, MAXLEN-1);

		playlist_save(pl, selected_filename);

                g_free(selected_filename);
        }

        gtk_widget_destroy(dialog);
}


void
plist__save_all_cb(gpointer data) {

        GtkWidget * dialog;
        gchar * selected_filename;

	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to save the playlist to."),
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window),
                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                             GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

        deflicker();
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "playlist.xml");

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                strncpy(options.currdir, selected_filename, MAXLEN-1);

		playlist_save_all(selected_filename);

                g_free(selected_filename);
        }

        gtk_widget_destroy(dialog);
}


playlist_transfer_t *
playlist_transfer_get(int mode, char * name) {

	playlist_t * pl = NULL;
	playlist_transfer_t * pt = NULL;

	if (mode == PLAYLIST_LOAD_TAB) {
		pl = playlist_tab_new(name);
		pt = playlist_transfer_new(pl);
		pt->on_the_fly = 1;
	} else {
		if (name == NULL) {
			pl = playlist_get_current();
		} else {
			pl = playlist_get_by_name(name);
		}
		if (pl == NULL) {
			pl = playlist_tab_new(name);
		}
		pt = playlist_transfer_new(pl);
		pt->on_the_fly = 1;
	}

	if (mode == PLAYLIST_LOAD) {
		pt->clear = 1;
	}

	return pt;
}


/* if (ptrans == NULL) then loads file/dir/playlist according to mode,
   else ignore the mode and use the supplied ptrans as destination */
void
playlist_load_entity(const char * filename, int mode, playlist_transfer_t * ptrans) {

	playlist_transfer_t * pt = NULL;
	char fullname[MAXLEN];

	normalize_filename(filename, fullname);

	switch (is_playlist(fullname)) {
	case 0:
		if (ptrans != NULL) {
			pt = ptrans;
		} else {
			pt = playlist_transfer_get(mode, NULL);
		}
		pt->list = g_slist_append(NULL, strdup(fullname));
		playlist_progress_bar_show(pt->pl);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       add_dir_to_playlist_thread, pt);
		} else {
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       add_files_to_playlist_thread, pt);
		}

		break;
	case 1:
		playlist_load_xml(fullname, mode, ptrans);
		break;
	case 2:
		//playlist_load_m3u(fullname, mode, ptrans);
		break;
	case 3:
		//playlist_load_pls(fullname, mode, ptrans);
		break;
	}
}

void
playlist_load_dialog(int mode) {

        GtkWidget * dialog;
        const gchar * selected_filename;


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_playlist_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                strncpy(options.currdir, selected_filename, MAXLEN-1);

		playlist_load_entity(selected_filename, mode, NULL);
        }

        gtk_widget_destroy(dialog);
}

void
plist__load_cb(gpointer data) {

	playlist_load_dialog(PLAYLIST_LOAD);
}

void
plist__enqueue_cb(gpointer data) {

	playlist_load_dialog(PLAYLIST_ENQUEUE);
}

void
plist__load_tab_cb(gpointer data) {

	playlist_load_dialog(PLAYLIST_LOAD_TAB);
}

gint
watch_vol_calc(gpointer data) {

        gfloat * volumes = (gfloat *)data;

        if (!vol_finished) {
                return TRUE;
        }

	if (vol_index != vol_n_tracks) {
		free(volumes);
		volumes = NULL;
		return FALSE;
	}

	if (vol_is_average) {
		gchar voladj_str[32];
		gfloat voladj = rva_from_multiple_volumes(vol_n_tracks, volumes,
							 options.rva_use_linear_thresh,
							 options.rva_avg_linear_thresh,
							 options.rva_avg_stddev_thresh,
							 options.rva_refvol,
							 options.rva_steepness);
		gint i;

		voladj2str(voladj, voladj_str);

		for (i = 0; i < vol_n_tracks; i++) {

			if (gtk_tree_store_iter_is_valid(play_store, &vol_iters[i])) {
				gtk_tree_store_set(play_store, &vol_iters[i],
						   COLUMN_VOLUME_ADJUSTMENT, voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str, -1);
			}
		}
	} else {
		gfloat voladj;
		gchar voladj_str[32];

		gint i;

		for (i = 0; i < vol_n_tracks; i++) {
			if (gtk_tree_store_iter_is_valid(play_store, &vol_iters[i])) {
				voladj = rva_from_volume(volumes[i],
							 options.rva_refvol,
							 options.rva_steepness);
				voladj2str(voladj, voladj_str);
				gtk_tree_store_set(play_store, &vol_iters[i], COLUMN_VOLUME_ADJUSTMENT, voladj,
						   COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str, -1);
			}
		}
	}

	free(volumes);
	volumes = NULL;
	free(vol_iters);
	vol_iters = NULL;
        return FALSE;
}


void
plist_setup_vol_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

        gchar * pfile;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, 1, &pfile, -1);

	if (pl_vol_queue == NULL) {
		pl_vol_queue = vol_queue_push(NULL, pfile, *iter/*dummy*/);
	} else {
		vol_queue_push(pl_vol_queue, pfile, *iter/*dummy*/);
	}
	++vol_n_tracks;

	vol_iters = (GtkTreeIter *)realloc(vol_iters, vol_n_tracks * sizeof(GtkTreeIter));
	if (!vol_iters) {
		fprintf(stderr, "realloc error in plist_setup_vol_foreach()\n");
		return;
	}
	vol_iters[vol_n_tracks-1] = *iter;

	g_free(pfile);
}

void
plist_setup_vol_calc(playlist_t * pl) {

	gfloat * volumes = NULL;

	pl_vol_queue = NULL;

        if (vol_window != NULL) {
                return;
        }

	vol_n_tracks = 0;

	playlist_foreach_selected(pl, plist_setup_vol_foreach, NULL);

	if (vol_n_tracks == 0)
		return;

	if (!options.rva_is_enabled) {

		GtkWidget * dialog = gtk_dialog_new_with_buttons(
				 _("Warning"),
				 options.playlist_is_embedded ?
				 GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window),
				 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				 GTK_STOCK_YES, GTK_RESPONSE_ACCEPT,
				 GTK_STOCK_NO, GTK_RESPONSE_REJECT,
				 NULL);

		GtkWidget * label =  gtk_label_new(_("Playback RVA is currently disabled.\n"
						     "Do you want to enable it now?"));

		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, TRUE, 10);
		gtk_widget_show(label);

		if (aqualung_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
			free(vol_iters);
			vol_iters = NULL;
			gtk_widget_destroy(dialog);

			return;
		} else {
			options.rva_is_enabled = 1;
			gtk_widget_destroy(dialog);
		}
	}

	if ((volumes = calloc(vol_n_tracks, sizeof(float))) == NULL) {
		fprintf(stderr, "calloc error in plist__rva_separate_cb()\n");
		free(vol_iters);
		vol_iters = NULL;
		return;
	}


	calculate_volume(pl_vol_queue, volumes);
	g_timeout_add(200, watch_vol_calc, (gpointer)volumes);
}


void
plist__rva_separate_cb(gpointer data) {

	playlist_t * pl = playlist_get_current();

	if (pl != NULL) {
		vol_is_average = 0;
		plist_setup_vol_calc(pl);
	}
}


void
plist__rva_average_cb(gpointer data) {

	playlist_t * pl = playlist_get_current();

	if (pl != NULL) {
		vol_is_average = 1;
		plist_setup_vol_calc(pl);
	}
}


void
plist__reread_file_meta_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

	gchar * title;
	gchar * fullname;
	gchar voladj_str[32];
	gchar duration_str[MAXLEN];
	gint composit;
	playlist_filemeta * plfm = NULL;
	GtkTreeIter dummy;


	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter,
			   COLUMN_TRACK_NAME, &title,
			   COLUMN_PHYSICAL_FILENAME, &fullname,
			   -1);

	if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &dummy, iter)) {
		composit = 0;
	} else {
		composit = 1;
	}

	plfm = playlist_filemeta_get(fullname, title, composit);
	if (plfm == NULL) {
		fprintf(stderr, "plist__reread_file_meta_foreach(): "
			"playlist_filemeta_get() returned NULL\n");
		g_free(title);
		g_free(fullname);
		return;
	}
			
	voladj2str(plfm->voladj, voladj_str);
	time2time_na(plfm->duration, duration_str);
	
	gtk_tree_store_set(pl->store, iter,
			   COLUMN_TRACK_NAME, plfm->title,
			   COLUMN_PHYSICAL_FILENAME, fullname,
			   COLUMN_VOLUME_ADJUSTMENT, plfm->voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
			   COLUMN_DURATION, plfm->duration, COLUMN_DURATION_DISP, duration_str,
			   -1);
			
	playlist_filemeta_free(plfm);
	plfm = NULL;
	g_free(title);
	g_free(fullname);
}


void
plist__reread_file_meta_cb(gpointer data) {
	
	GtkTreeIter iter;
	GtkTreeIter iter_child;
	gint i = 0;
	gint j = 0;
	gint reread = 0;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	playlist_foreach_selected(pl, plist__reread_file_meta_foreach, NULL);
	
	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {
		
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {
			
			reread = 0;
			
			if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
				reread = 1;
			} else {
				j = 0;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter_child, &iter, j++)) {
					if (gtk_tree_selection_iter_is_selected(pl->select,
										&iter_child)) {
						reread = 1;
						break;
					}
				}
			}
			
			if (reread) {
				recalc_album_node(pl, &iter);
			}
		}
	}
	
	delayed_playlist_rearrange();
}


#ifdef HAVE_IFP
void
plist__send_songs_to_iriver_cb(gpointer data) {

        aifp_transfer_files();

}
#endif /* HAVE_IFP */

void
plist__fileinfo_cb(gpointer data) {

	GtkTreeIter dummy;

	show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy);
}

void
plist__search_cb(gpointer data) {
	
	search_playlist_dialog();
}


static gboolean
add_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			gtk_menu_popup(GTK_MENU(add_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);

			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


static gboolean
sel_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			gtk_menu_popup(GTK_MENU(sel_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);

			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


static gboolean
rem_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			gtk_menu_popup(GTK_MENU(rem_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);

			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


/* if alt_name != NULL, it will be used as title if no meta is found */
playlist_filemeta *
playlist_filemeta_get(char * physical_name, char * alt_name, int composit) {

	gchar display_name[MAXLEN];
	gchar artist_name[MAXLEN];
	gchar record_name[MAXLEN];
	gchar track_name[MAXLEN];
	metadata * meta = NULL;
	gint use_meta = 0;
	gchar * substr;

#if defined(HAVE_MOD) && (defined(HAVE_LIBZ) || defined(HAV_LIBBZ2))
        gchar * pos;
#endif /* (HAVE_MOD && (HAVE_LIBZ || HAVE_LIBBZ2)) */

#ifdef HAVE_MOD
        if (is_valid_mod_extension(physical_name)) {
                composit = 0;
        }

#ifdef HAVE_LIBZ	
        if ((pos = strrchr(physical_name, '.')) != NULL) {
                pos++;

                if (!strcasecmp(pos, "gz")) {
                    composit = 0;
                }

#ifdef HAVE_LIBBZ2
                if (!strcasecmp(pos, "bz2")) {
                    composit = 0;
                }
#endif /* HAVE LIBBZ2 */

        }
#endif /* HAVE LIBZ */

#endif /* HAVE_MOD */

	playlist_filemeta * plfm = calloc(1, sizeof(playlist_filemeta));
	if (!plfm) {
		fprintf(stderr, "calloc error in playlist_filemeta_get()\n");
		return NULL;
	}

	if ((plfm->duration = get_file_duration(physical_name)) < 0.0f) {
		return NULL;
	}

	if (options.rva_is_enabled) {
		meta = meta_new();
		if (meta != NULL && meta_read(meta, physical_name)) {
			if (!meta_get_rva(meta, &(plfm->voladj))) {
				plfm->voladj = options.rva_no_rva_voladj;
			}
		} else {
			plfm->voladj = options.rva_no_rva_voladj;
		}
		if (meta != NULL) {
			meta_free(meta);
			meta = NULL;
		}
	} else {
		plfm->voladj = 0.0f;
	}
	
	artist_name[0] = '\0';
	record_name[0] = '\0';
	track_name[0] = '\0';

	if (options.auto_use_ext_meta_artist ||
	    options.auto_use_ext_meta_record ||
	    options.auto_use_ext_meta_track) {
		
		meta = meta_new();
		if (meta != NULL && !meta_read(meta, physical_name)) {
			meta_free(meta);
			meta = NULL;
		}
	}
	
	use_meta = 0;
	if ((meta != NULL) && options.auto_use_ext_meta_artist) {
		meta_get_artist(meta, artist_name);
		if (artist_name[0] != '\0') {
			use_meta = 1;
		}
	}
	
	if ((meta != NULL) && options.auto_use_ext_meta_record) {
		meta_get_record(meta, record_name);
		if (record_name[0] != '\0') {
			use_meta = 1;
		}
	}
	
	if ((meta != NULL) && options.auto_use_ext_meta_track) {
		meta_get_title(meta, track_name);
		if (track_name[0] != '\0') {
			use_meta = 1;
		}
	}
	
	if ((artist_name[0] != '\0') ||
	    (record_name[0] != '\0') ||
	    (track_name[0] != '\0')) {
		
		if (artist_name[0] == '\0') {
			strcpy(artist_name, _("Unknown"));
		}
		if (record_name[0] == '\0') {
			strcpy(record_name, _("Unknown"));
		}
		if (track_name[0] == '\0') {
			strcpy(track_name, _("Unknown"));
		}
	} else {
		use_meta = 0;
	}
	
	if (use_meta && (meta != NULL)) {
		if (composit) {
			make_title_string(display_name, options.title_format,
					  artist_name, record_name, track_name);
		} else {
			strncpy(display_name, track_name, MAXLEN-1);

		}
	} else {
		if (alt_name != NULL) {
			strcpy(display_name, alt_name);
		} else {
    			if ((substr = strrchr(physical_name, '/')) == NULL) {
            			substr = physical_name;
			} else {
            			++substr;
			}
			substr = g_filename_display_name(substr);
			strcpy(display_name, substr);
			g_free(substr);
		} 
	}
	if (meta != NULL) {
		meta_free(meta);
		meta = NULL;
	}

	plfm->filename = g_strdup(physical_name);
	plfm->title = g_strdup(display_name);

	return plfm;
}


void
playlist_filemeta_free(playlist_filemeta * plfm) {

	free(plfm->filename);
	free(plfm->title);
	free(plfm);
}


void
add__files_cb(gpointer data) {

	add_files(NULL, NULL);
}


void
add__dir_cb(gpointer data) {

	add_directory(NULL, NULL);
}


void
add__url_cb(gpointer data) {

	add_url(NULL, NULL);
}


void
select_all(GtkWidget * widget, gpointer data) {

	playlist_t * pl = playlist_get_current();

	if (pl != NULL) {
		gtk_tree_selection_select_all(pl->select);
	}
}


void
sel__all_cb(gpointer data) {

	select_all(NULL, data);
}


void
sel__none_cb(gpointer data) {

	playlist_t * pl;

	if ((pl = playlist_get_current()) != NULL) {
		gtk_tree_selection_unselect_all(pl->select);
	}
}


void
sel__inv_foreach(GtkTreePath * path, gpointer data) {

	gtk_tree_selection_unselect_path((GtkTreeSelection *)data, path);
	gtk_tree_path_free(path);
}


void
sel__inv_cb(gpointer data) {


	GList * list = NULL;
	playlist_t * pl = NULL;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	list = gtk_tree_selection_get_selected_rows(pl->select, NULL);

	g_signal_handlers_block_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	gtk_tree_selection_select_all(pl->select);
	g_list_foreach(list, (GFunc)sel__inv_foreach, pl->select);
	g_list_free(list);

	g_signal_handlers_unblock_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	playlist_selection_changed(pl);
}


void
rem_all(playlist_t * pl) {

	g_signal_handlers_block_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	gtk_tree_store_clear(pl->store);

	g_signal_handlers_unblock_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

        playlist_selection_changed(pl);
        playlist_content_changed(pl);
}

void
rem__all_cb(gpointer data) {

	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	rem_all(pl);
}

int
all_tracks_selected(playlist_t * pl, GtkTreeIter * piter) {

	gint j = 0;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, piter, j++)) {
		if (!gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
			return 0;
		}
	}
	return 1;
}

void
rem__sel_cb(gpointer data) {

	GtkTreeIter iter;
	gint i = 0;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	g_signal_handlers_block_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
			gtk_tree_store_remove(pl->store, &iter);
			i--;
		} else {
			int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

			if (n > 0) {
				if (all_tracks_selected(pl, &iter)) {
					gtk_tree_store_remove(pl->store, &iter);
					i--;
				} else {
					int j = 0;
					int recalc = 0;
					GtkTreeIter iter_child;

					while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
						if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
							gtk_tree_store_remove(pl->store, &iter_child);
							j--;
							recalc = 1;
						}
					}

					if (recalc) {
						recalc_album_node(pl, &iter);
					}
				}
			}
		}
	}

	g_signal_handlers_unblock_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	gtk_tree_selection_unselect_all(pl->select);
	playlist_content_changed(pl);
}


int
rem__dead_skip(char * filename) {

	return (strstr(filename, "CDDA") != filename &&
		strstr(filename, "http") != filename &&
		(g_file_test(filename, G_FILE_TEST_IS_REGULAR) == FALSE));
}

void
rem__dead_cb(gpointer data) {

	GtkTreeIter iter;
	gint i = 0;
        gchar *filename;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		GtkTreeIter iter_child;
		gint j = 0;


                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
				   COLUMN_PHYSICAL_FILENAME, &filename, -1);

                gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

                if (n == 0 && rem__dead_skip(filename)) {
                        g_free(filename);
                        gtk_tree_store_remove(pl->store, &iter);
                        --i;
                        continue;
                }

                g_free(filename);

                if (n) {

                        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
                                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child,
						   COLUMN_PHYSICAL_FILENAME, &filename, -1);

                                if (rem__dead_skip(filename)) {
                                        gtk_tree_store_remove(pl->store, &iter_child);
                                        --j;
                                }

                                g_free(filename);
                        }

                        /* remove album node if empty */
                        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) == 0) {
				gtk_tree_store_remove(pl->store, &iter);
                                --i;
                        } else {
                                recalc_album_node(pl, &iter);
                        }
                }

        }
	playlist_content_changed(pl);
}

void
remove_sel(GtkWidget * widget, gpointer data) {

	rem__sel_cb(data);
}


/* playlist item is selected -> keep, else -> remove
 * ret: 0 if kept, 1 if removed track
 */
int
cut_track_item(playlist_t * pl, GtkTreeIter * piter) {

	if (!gtk_tree_selection_iter_is_selected(pl->select, piter)) {
		gtk_tree_store_remove(pl->store, piter);
		return 1;
	}
	return 0;
}


/* ret: 1 if at least one of album node's children are selected; else 0 */
int
any_tracks_selected(playlist_t * pl, GtkTreeIter * piter) {

	gint j = 0;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, piter, j++)) {
		if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
			return 1;
		}
	}
	return 0;
}


/* cut selected callback */
void
cut__sel_cb(gpointer data) {

	GtkTreeIter iter;
	gint i = 0;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

		if (n) { /* album node */
			if (any_tracks_selected(pl, &iter)) {
				gint j = 0;
				GtkTreeIter iter_child;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter_child, &iter, j++)) {
					j -= cut_track_item(pl, &iter_child);
				}

				recalc_album_node(pl, &iter);
			} else {
				i -= cut_track_item(pl, &iter);
			}
		} else { /* track node */
			i -= cut_track_item(pl, &iter);
		}
	}
	gtk_tree_selection_unselect_all(pl->select);
	playlist_content_changed(pl);
}


void
playlist_reorder_columns_foreach(gpointer data, gpointer user_data) {

	playlist_t * pl = (playlist_t *)data;
	int * order = (int *)user_data;
	GtkTreeViewColumn * cols[3];
	int i;

	cols[0] = pl->track_column;
	cols[1] = pl->rva_column;
	cols[2] = pl->length_column;

        for (i = 2; i >= 0; i--) {
		gtk_tree_view_move_column_after(GTK_TREE_VIEW(pl->view),
						cols[order[i]], NULL);
	}

	gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column),
					 options.show_rva_in_playlist);
	gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->length_column),
					 options.show_length_in_playlist);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pl->view), options.enable_pl_rules_hint);
}

void
playlist_reorder_columns_all(int * order) {

	g_list_foreach(playlists, playlist_reorder_columns_foreach, order);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook),
					   options.playlist_always_show_tabs);
	}
}



gint
playlist_size_allocate(GtkWidget * widget, GdkEventConfigure * event, gpointer data) {

	gint avail;
	gint track_width;
	gint rva_width;
	gint length_width;

	playlist_t * pl = (playlist_t *)data;

	if (pl == NULL) {
		return TRUE;
	}

	if (main_window == NULL || playlist_window == NULL) {
		return TRUE;
	}

	avail = pl->view->allocation.width;

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column))) {
		gtk_tree_view_column_cell_get_size(GTK_TREE_VIEW_COLUMN(pl->rva_column),
						   NULL, NULL, NULL, &rva_width, NULL);
		rva_width += 5;
	} else {
		rva_width = 1;
	}

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(pl->length_column))) {
		gtk_tree_view_column_cell_get_size(GTK_TREE_VIEW_COLUMN(pl->length_column),
						   NULL, NULL, NULL, &length_width, NULL);
		length_width += 5;
	} else {
		length_width = 1;
	}

	track_width = avail - rva_width - length_width;
	if (track_width < 1)
		track_width = 1;

	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->track_column), track_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->rva_column), rva_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->length_column), length_width);


	if (options.playlist_is_embedded) {
		if (main_window->window != NULL) {
			gtk_widget_queue_draw(main_window);
		}
	} else {
		if (playlist_window->window != NULL) {
			gtk_widget_queue_draw(playlist_window);
		}
	}

        return TRUE;
}

void
playlist_size_allocate_all_foreach(gpointer data, gpointer user_data) {

	playlist_size_allocate(NULL, NULL, data);
}

// This is the function to call from outside
void
playlist_size_allocate_all() {

	g_list_foreach(playlists, playlist_size_allocate_all_foreach, NULL);
}


gint
playlist_rearrange_timeout_cb(gpointer data) {   

	playlist_size_allocate_all();

	return FALSE;
}

void
delayed_playlist_rearrange() {

	g_timeout_add(100, playlist_rearrange_timeout_cb, NULL);
}


void
playlist_stats_set_busy() {

	gtk_label_set_text(GTK_LABEL(statusbar_total), _("counting..."));
}


void
playlist_child_stats(playlist_t * pl, GtkTreeIter * iter,
		     int * count, float * duration, float * songs_size, int selected) {

	gint j = 0;
	gchar * tstr;
	struct stat statbuf;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, iter, j++)) {
		
		if (!selected || gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
			
			float len = 0;

			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child, COLUMN_DURATION, &len, 
                                           COLUMN_PHYSICAL_FILENAME, &tstr, -1);
			*duration += len;
			(*count)++;
			if (stat(tstr, &statbuf) != -1) {
				*songs_size += statbuf.st_size / 1024.0;
			}
			g_free(tstr);
		}
	}
}


/* if selected == true -> stats for selected tracks; else: all tracks */
void
playlist_stats(playlist_t * pl, int selected) {

	GtkTreeIter iter;
	gint i = 0;

	gint count = 0;
	gfloat duration = 0;
	gfloat len = 0;
	gchar str[MAXLEN];
	gchar time[MAXLEN];
        gchar * tstr;
        gfloat songs_size, m_size;
        struct stat statbuf;

	if (!options.enable_playlist_statusbar) {
		return;
	}

	if (main_window == NULL || playlist_window == NULL ||
	    statusbar_total == NULL || statusbar_selected == NULL) {
		return;
	}

        songs_size = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);
		if (n > 0) { /* album node -- count children tracks */
			if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
				playlist_child_stats(pl, &iter, &count, &duration, &songs_size, 0/*false*/);
			} else {
				playlist_child_stats(pl, &iter, &count, &duration, &songs_size, selected);
			}
		} else {
			if (!selected || gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, COLUMN_DURATION, &len, 
                                                   COLUMN_PHYSICAL_FILENAME, &tstr, -1);
				duration += len;
				count++;
				if (stat(tstr, &statbuf) != -1) {
					songs_size += statbuf.st_size / 1024.0;
				}
				g_free(tstr);
			}
                }
	}


	time2time(duration, time);
        m_size = songs_size / 1024.0;

	if (count == 1) {
                if (options.pl_statusbar_show_size && (m_size > 0)) {
                        if (m_size < 1024) {
                                sprintf(str, _("%d track [%s] (%.1f MB)"), count, time, m_size);
                        } else {
                                sprintf(str, _("%d track [%s] (%.1f GB)"), count, time, m_size / 1024.0);
                        }
                } else {
                        if(fabsf(duration) < 1e-8) {
                                sprintf(str, _("%d track "), count);
                        } else {
                                sprintf(str, _("%d track [%s] "), count, time);
                        }
                }
	} else {
                if (options.pl_statusbar_show_size && (m_size > 0)) {
                        if (m_size < 1024) {
                                sprintf(str, _("%d tracks [%s] (%.1f MB)"), count, time, m_size);
                        } else {
                                sprintf(str, _("%d tracks [%s] (%.1f GB)"), count, time, m_size / 1024.0);
                        }
                } else {
                        if(fabsf(duration) < 1e-8) {
                                sprintf(str, _("%d tracks "), count);
                        } else {
                                sprintf(str, _("%d tracks [%s] "), count, time);
                        }
                }
	}

	if (selected) {
		gtk_label_set_text(GTK_LABEL(statusbar_selected), str);
	} else {
		gtk_label_set_text(GTK_LABEL(statusbar_total), str);
	}
}


void
recalc_album_node(playlist_t * pl, GtkTreeIter * iter) {

	gint count = 0;
	gfloat duration = 0;
	gfloat songs_size;
	gchar time[MAXLEN];
	gchar * color;

	playlist_child_stats(pl, iter, &count, &duration, &songs_size, 0/*false*/);
	time2time(duration, time);
	gtk_tree_store_set(pl->store, iter,
			   COLUMN_DURATION, duration,
			   COLUMN_DURATION_DISP, time, -1);

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, COLUMN_SELECTION_COLOR, &color, -1);
	if (strcmp(color, pl_color_active) == 0) {
		unmark_track(pl->store, iter);
		mark_track(pl->store, iter);
	}
	g_free(color);
}


void
playlist_selection_changed(playlist_t * pl) {

	playlist_stats(pl, 1/* true */);
}


void
playlist_content_changed(playlist_t * pl) {

	if (pl == NULL) { // TODO: this check won't be needed
		return;
	}

	if (pl->progbar_semaphore == 0) {
		playlist_stats(pl, 0/*false*/);
	} else {
		playlist_stats_set_busy();
	}
}

void
playlist_selection_changed_cb(GtkTreeSelection * select, gpointer data) {

	playlist_selection_changed((playlist_t *)data);
}

void
playlist_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, data->target, 8, (const guchar *) "list\0", 5);
}


void
playlist_perform_drag(playlist_t * pl,
		      GtkTreeIter * sel_iter, GtkTreePath * sel_path,
		      GtkTreeIter * pos_iter, GtkTreePath * pos_path) {

	gint cmp = gtk_tree_path_compare(sel_path, pos_path);
	gint sel_depth = gtk_tree_path_get_depth(sel_path);
	gint pos_depth = gtk_tree_path_get_depth(pos_path);

	gint * sel_idx = gtk_tree_path_get_indices(sel_path);
	gint * pos_idx = gtk_tree_path_get_indices(pos_path);

	if (cmp == 0) {
		return;
	}

	if (sel_depth == pos_depth && (sel_depth == 1 /* top */ || sel_idx[0] == pos_idx[0])) {

		GtkTreeIter parent;

		if (cmp == 1) {
			gtk_tree_store_move_before(pl->store, sel_iter, pos_iter);
		} else {
			gtk_tree_store_move_after(pl->store, sel_iter, pos_iter);
		}

		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &parent, sel_iter)) {
			unmark_track(pl->store, &parent);
			mark_track(pl->store, &parent);
		}
	} else {

		GtkTreeIter iter;
		GtkTreeIter sel_parent;
		GtkTreeIter pos_parent;
		gint recalc_sel_parent = 0;
		gint recalc_pos_parent = 0;
		gchar * tname;
		gchar * fname;
		gchar * color;
		gfloat voladj;
		gchar * voldisp;
		gfloat duration;
		gchar * durdisp;
		gint fontw;

		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), sel_iter)) {
			return;
		}

		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &sel_parent, sel_iter)) {
			recalc_sel_parent = 1;
		}

		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &pos_parent, pos_iter)) {
			recalc_pos_parent = 1;
		}

		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), sel_iter,
				   COLUMN_TRACK_NAME, &tname, 
                                   COLUMN_PHYSICAL_FILENAME, &fname,
				   COLUMN_SELECTION_COLOR, &color,
				   COLUMN_VOLUME_ADJUSTMENT, &voladj,
				   COLUMN_VOLUME_ADJUSTMENT_DISP, &voldisp, 
                                   COLUMN_DURATION, &duration,
				   COLUMN_DURATION_DISP, &durdisp, 
                                   COLUMN_FONT_WEIGHT, &fontw, -1);

		if (cmp == 1) {
			gtk_tree_store_insert_before(pl->store, &iter, NULL, pos_iter);
		} else {
			gtk_tree_store_insert_after(pl->store, &iter, NULL, pos_iter);
		}

		gtk_tree_store_set(pl->store, &iter,
				   COLUMN_TRACK_NAME, tname,
				   COLUMN_PHYSICAL_FILENAME, fname,
                                   COLUMN_SELECTION_COLOR, color,
				   COLUMN_VOLUME_ADJUSTMENT, voladj, 
                                   COLUMN_VOLUME_ADJUSTMENT_DISP, voldisp,
				   COLUMN_DURATION, duration,
				   COLUMN_DURATION_DISP, durdisp,
				   COLUMN_FONT_WEIGHT, fontw, -1);

		gtk_tree_store_remove(pl->store, sel_iter);

		if (recalc_sel_parent) {
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &sel_parent)) {
				recalc_album_node(pl, &sel_parent);
				unmark_track(pl->store, &sel_parent);
				mark_track(pl->store, &sel_parent);
			} else {
				gtk_tree_store_remove(pl->store, &sel_parent);
			}
		}

		if (recalc_pos_parent) {
			recalc_album_node(pl, &pos_parent);
			unmark_track(pl->store, &pos_parent);
			mark_track(pl->store, &pos_parent);
		}

		g_free(tname);
		g_free(fname);
		g_free(color);
		g_free(voldisp);
		g_free(durdisp);
	}
}


void
playlist_remove_scroll_tags() {

	if (playlist_scroll_up_tag > 0) {	
		g_source_remove(playlist_scroll_up_tag);
		playlist_scroll_up_tag = -1;
	}
	if (playlist_scroll_dn_tag > 0) {	
		g_source_remove(playlist_scroll_dn_tag);
		playlist_scroll_dn_tag = -1;
	}
}

gint
playlist_drag_data_received(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, 
			    GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	GtkTreeViewColumn * column;
	playlist_t * pl = (playlist_t *)user_data;

	if (info == 0) { /* drag and drop inside Aqualung */

		if (!strcmp((gchar *)data->data, "store")) {

			GtkTreePath * path = NULL;
			GtkTreeIter * piter = NULL;
			GtkTreeIter iter;
			GtkTreeIter parent;
			GtkTreeModel * model;
			int depth;

			if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
				depth = gtk_tree_path_get_depth(gtk_tree_model_get_path(model, &iter));
#ifdef HAVE_CDDA
				if (is_store_iter_cdda(&iter))
					depth += 1;
#endif /* HAVE_CDDA */
			} else {
				return FALSE;
			}

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(pl->view),
							  x, y, &path, &column, NULL, NULL)) {

				if (depth != 4) { /* dragging store, artist or record */
					while (gtk_tree_path_get_depth(path) > 1) {
						gtk_tree_path_up(path);
					}
				}

				gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path);
				piter = &iter;
			}

			switch (depth) {
			case 1:
				store__addlist_defmode(piter);
				break;
			case 2:
				artist__addlist_defmode(piter);
				break;
			case 3:
				record__addlist_defmode(piter);
				break;
			case 4:
				track__addlist_cb(piter);

				if (piter && gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store),
									&parent, piter)) {
					recalc_album_node(pl, &parent);
					unmark_track(pl->store, &parent);
					mark_track(pl->store, &parent);
				}

				break;
			}

			if (path) {
				gtk_tree_path_free(path);
			}
			
		} else if (!strcmp((gchar *)data->data, "list")) {

			GtkTreeModel * model;
			GtkTreeIter sel_iter;
			GtkTreeIter pos_iter;
			GtkTreePath * sel_path = NULL;
			GtkTreePath * pos_path = NULL;

			gtk_tree_selection_set_mode(pl->select, GTK_SELECTION_SINGLE);

			if (gtk_tree_selection_get_selected(pl->select, &model, &sel_iter)) {

				sel_path = gtk_tree_model_get_path(model, &sel_iter);

				if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(pl->view),
								  x, y, &pos_path, &column, NULL, NULL)) {

					gtk_tree_model_get_iter(model, &pos_iter, pos_path);

					playlist_perform_drag(pl,
							      &sel_iter, sel_path,
							      &pos_iter, pos_path);
				}
			}

			if (sel_path) {
				gtk_tree_path_free(sel_path);
			}

			if (pos_path) {
				gtk_tree_path_free(pos_path);
			}

			gtk_tree_selection_set_mode(pl->select, GTK_SELECTION_MULTIPLE);
		}

	} else { /* drag and drop from external app */

		gchar ** uri_list;
		gchar * str = NULL;
		int i;
		char file[MAXLEN];
		struct stat st_file;

		for (i = 0; *((gchar *)data->data + i); i++) {
			if (*((gchar *)data->data + i) == '\r') {
				*((gchar *)data->data + i) = '\n';
			}
		}

		uri_list = g_strsplit((gchar *)data->data, "\n", 0);

		for (i = 0; uri_list[i]; i++) {

			if (*(uri_list[i]) == '\0') {
				continue;
			}

			if ((str = g_filename_from_uri(uri_list[i], NULL, NULL)) != NULL) {
				strncpy(file, str, MAXLEN-1);
				g_free(str);
			} else {
				int off = 0;

				if (strstr(uri_list[i], "file:") == uri_list[i] ||
				    strstr(uri_list[i], "FILE:") == uri_list[i]) {
					off = 5;
				}

				while (uri_list[i][off] == '/' && uri_list[i][off+1] == '/') {
					off++;
				}

				strncpy(file, uri_list[i] + off, MAXLEN-1);
			}

			if ((str = g_locale_from_utf8(file, -1, NULL, NULL, NULL)) != NULL) {
				strncpy(file, str, MAXLEN-1);
				g_free(str);
			}

			if (stat(file, &st_file) == 0) {

				playlist_transfer_t * pt = playlist_transfer_new(pl);

				pt->list = g_slist_append(NULL, strdup(file));

				playlist_progress_bar_show(pt->pl);
				if (S_ISDIR(st_file.st_mode)) {
					AQUALUNG_THREAD_CREATE(pt->pl->thread_id, NULL,
							       add_dir_to_playlist_thread, pt);
				} else {
					AQUALUNG_THREAD_CREATE(pt->pl->thread_id, NULL,
							       add_files_to_playlist_thread, pt);
				}
			}
		}

		g_strfreev(uri_list);

		playlist_remove_scroll_tags();
	}

	return FALSE;
}

gint
playlist_scroll_up(gpointer data) {

	g_signal_emit_by_name(G_OBJECT(((playlist_t *)data)->scroll), "scroll-child",
			      GTK_SCROLL_STEP_BACKWARD, FALSE/*vertical*/, NULL);

	return TRUE;
}

gint
playlist_scroll_dn(gpointer data) {

	g_signal_emit_by_name(G_OBJECT(((playlist_t *)data)->scroll), "scroll-child",
			      GTK_SCROLL_STEP_FORWARD, FALSE/*vertical*/, NULL);

	return TRUE;
}


void
playlist_drag_leave(GtkWidget * widget, GdkDragContext * drag_context,
		    guint time, gpointer data) {

	if (playlist_drag_y < widget->allocation.height / 2) {
		if (playlist_scroll_up_tag == -1) {
			playlist_scroll_up_tag = g_timeout_add(100, playlist_scroll_up, data);
		}
	} else {
		if (playlist_scroll_dn_tag == -1) {
			playlist_scroll_dn_tag = g_timeout_add(100, playlist_scroll_dn, data);
		}
	}
}


gboolean
playlist_drag_motion(GtkWidget * widget, GdkDragContext * context,
		     gint x, gint y, guint time, gpointer data) {

	playlist_drag_y = y;
	playlist_remove_scroll_tags();

	return TRUE;
}


void
playlist_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	playlist_remove_scroll_tags();
}


#ifdef HAVE_CDDA

void
playlist_add_cdda(GtkTreeIter * iter_drive, unsigned long hash) {

	int i = 0;
	GtkTreeIter iter;

	int target_found = 0;
	GtkTreeIter target_iter;

	playlist_t * pl; // TODO

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {

			int j = 0;
			int has_cdda = 0;
			GtkTreeIter child;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &child, &iter, j++)) {

				char * pfile;
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &child,
						   COLUMN_PHYSICAL_FILENAME, &pfile, -1);

				if (!target_found && strstr(pfile, "CDDA") == pfile) {
					has_cdda = 1;
				}

				if (cdda_hash_matches(pfile, hash)) {
					g_free(pfile);
					return;
				}

				g_free(pfile);
			}

			if (!target_found && !has_cdda) {
				target_iter = iter;
				target_found = 1;
			}
		} else {

			char * pfile;
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   COLUMN_PHYSICAL_FILENAME, &pfile, -1);

			if (!target_found && strstr(pfile, "CDDA") != pfile) {
				target_iter = iter;
				target_found = 1;
			}

			if (cdda_hash_matches(pfile, hash)) {
				g_free(pfile);
				return;
			}
				
			g_free(pfile);
		}
	}

	if (target_found) {
		record_addlist_iter(*iter_drive, pl, &target_iter, options.playlist_is_tree);
	} else {
		record_addlist_iter(*iter_drive, pl, NULL, options.playlist_is_tree);
	}
}

void
playlist_remove_cdda(char * device_path) {

	int i = 0;
	GtkTreeIter iter;
	unsigned long hash;

	cdda_drive_t * drive = cdda_get_drive_by_device_path(device_path);

	playlist_t * pl; // TODO

	if (drive == NULL) {
		return;
	}

	if (drive->disc.hash == 0) {
		hash = drive->disc.hash_prev;
	} else {
		hash = drive->disc.hash;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {

			int j = 0;
			GtkTreeIter child;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &child, &iter, j++)) {

				char * pfile;
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &child,
						   COLUMN_PHYSICAL_FILENAME, &pfile, -1);

				if (cdda_hash_matches(pfile, hash)) {
					gtk_tree_store_remove(pl->store, &child);
					--j;
				}
				
				g_free(pfile);
			}

			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) == 0) {
				gtk_tree_store_remove(pl->store, &iter);
				--i;
			} else {
				recalc_album_node(pl, &iter);
			}

		} else {

			char * pfile;
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   COLUMN_PHYSICAL_FILENAME, &pfile, -1);
			
			if (cdda_hash_matches(pfile, hash)) {
				gtk_tree_store_remove(pl->store, &iter);
				--i;
			}

			g_free(pfile);
		}
	}

	playlist_content_changed(NULL); // TODO
}

#endif /* HAVE_CDDA */


void
playlist_rename(playlist_t * pl, char * name) {

	if (name != NULL) {
		strncpy(pl->name, name, MAXLEN-1);
	}

	gtk_label_set_text(GTK_LABEL(pl->label), pl->name);
	gtk_notebook_set_menu_label_text(GTK_NOTEBOOK(playlist_notebook), pl->widget, pl->name);
}

void
playlist_tab_close(GtkButton * button, gpointer data) {

	playlist_t * pl = (playlist_t *)data;
	int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook));
	int i;

	if (pl->progbar_semaphore > 0 || pl->ms_semaphore > 0) {
		return;
	}

	if (n == 1) {
		rem_all(pl);
		playlist_rename(pl, _("(Untitled)"));
		return;
	}

	for (i = 0; i < n; i++) {
		if (pl->widget == gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), i)) {
			gtk_notebook_remove_page(GTK_NOTEBOOK(playlist_notebook), i);
			playlist_free(pl);

			if (!options.playlist_always_show_tabs &&
			    gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 1) {
				gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook), FALSE);
			}

			return;
		}
	}

	printf("WARNING: unable to locate playlist\n");
}



GtkWidget *
create_playlist_tab_label(playlist_t * pl) {

	GtkWidget * hbox;
	GtkWidget * button;
        GtkWidget * image;
        GdkPixbuf * pixbuf;

	char path[MAXLEN];

	hbox = gtk_hbox_new(FALSE, 4);
	pl->label = gtk_label_new(pl->name);
	button = gtk_button_new();

	sprintf(path, "%s/tab-close.png", AQUALUNG_DATADIR);

        pixbuf = gdk_pixbuf_new_from_file(path, NULL);

	if (pixbuf) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_container_add(GTK_CONTAINER(button), image);
	}

	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
        GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(button, 16, 16);

        gtk_box_pack_start(GTK_BOX(hbox), pl->label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

	gtk_widget_show_all(hbox);

        g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(playlist_tab_close), pl);


	return hbox;
}

void
create_playlist_gui(playlist_t * pl) {

	GtkWidget * viewport;
	GtkCellRenderer * track_renderer;
	GtkCellRenderer * rva_renderer;
	GtkCellRenderer * length_renderer;

	int i;

	GdkPixbuf * pixbuf;
	gchar path[MAXLEN];

	GtkTargetEntry source_table[] = {
		{ "STRING", GTK_TARGET_SAME_APP, 0 }
	};

	GtkTargetEntry target_table[] = {
		{ "STRING", GTK_TARGET_SAME_APP, 0 },
		{ "STRING", 0, 1 },
		{ "text/plain", 0, 1 },
		{ "text/uri-list", 0, 1 }
	};


        pl->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pl->store));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(pl->view), FALSE);
	gtk_widget_set_name(pl->view, "play_list");
	gtk_widget_set_size_request(pl->view, 100, 100);

        if (options.override_skin_settings) {
                gtk_widget_modify_font(pl->view, fd_playlist);
        }

        if (options.enable_pl_rules_hint) {
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pl->view), TRUE);
        }

	for (i = 0; i < 3; i++) {
		switch (options.plcol_idx[i]) {
		case 0:
			track_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)track_renderer, "xalign", 0.0, NULL);
			pl->track_column = gtk_tree_view_column_new_with_attributes("Tracks",
										track_renderer,
										"text", 0,
										"foreground", 2,
                                                                                "weight", 7,
										NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->track_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(pl->track_column), 3);
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(pl->track_column), FALSE);
			gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->track_column), 10);
                        g_object_set(G_OBJECT(track_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
                        gtk_cell_renderer_set_fixed_size(track_renderer, 10, -1);
			gtk_tree_view_append_column(GTK_TREE_VIEW(pl->view), pl->track_column);
			break;

		case 1:
			rva_renderer = gtk_cell_renderer_text_new();
			pl->rva_column = gtk_tree_view_column_new_with_attributes("RVA",
									      rva_renderer,
									      "text", 4,
									      "foreground", 2,
                                                                              "weight", 7,
									      NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->rva_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(pl->rva_column), 3);
			if (options.show_rva_in_playlist) {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column), TRUE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->rva_column), 50);
			} else {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column), FALSE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->rva_column), 1);
			}
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(pl->rva_column), FALSE);
			gtk_tree_view_append_column(GTK_TREE_VIEW(pl->view), pl->rva_column);
			break;

		case 2:
			length_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)length_renderer, "xalign", 1.0, NULL);
			pl->length_column = gtk_tree_view_column_new_with_attributes("Length",
										 length_renderer,
										 "text", 6,
										 "foreground", 2,
                                                                                 "weight", 7,
										 NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->length_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(pl->length_column), 3);
			if (options.show_length_in_playlist) {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->length_column), TRUE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->length_column), 50);
			} else {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->length_column), FALSE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->length_column), 1);
			}
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(pl->length_column), FALSE);
			gtk_tree_view_append_column(GTK_TREE_VIEW(pl->view), pl->length_column);
		}
	}

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pl->view), FALSE);

	g_signal_connect(G_OBJECT(pl->view), "button_press_event",
			 G_CALLBACK(doubleclick_handler), pl);


	/* setup drag and drop */
	
	gtk_drag_source_set(pl->view,
			    GDK_BUTTON1_MASK,
			    source_table,
			    sizeof(source_table) / sizeof(GtkTargetEntry),
			    GDK_ACTION_MOVE);
	
	gtk_drag_dest_set(pl->view,
			  GTK_DEST_DEFAULT_ALL,
			  target_table,
			  sizeof(target_table) / sizeof(GtkTargetEntry),
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);

	snprintf(path, MAXLEN-1, "%s/drag.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(pl->view, pixbuf);
	}	

	g_signal_connect(G_OBJECT(pl->view), "drag_end",
			 G_CALLBACK(playlist_drag_end), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_data_get",
			 G_CALLBACK(playlist_drag_data_get), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_leave",
			 G_CALLBACK(playlist_drag_leave), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_motion",
			 G_CALLBACK(playlist_drag_motion), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_data_received",
			 G_CALLBACK(playlist_drag_data_received), pl);
	

        pl->select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pl->view));
        gtk_tree_selection_set_mode(pl->select, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(pl->select), "changed",
			 G_CALLBACK(playlist_selection_changed_cb), pl);


        pl->widget = gtk_vbox_new(FALSE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(pl->widget), viewport, TRUE, TRUE, 0);

	pl->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pl->scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), pl->scroll);

	gtk_container_add(GTK_CONTAINER(pl->scroll), pl->view);


	pl->progbar_container = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(pl->widget), pl->progbar_container, FALSE, FALSE, 0);

	if (pl->progbar_semaphore > 0) {
		pl->progbar_semaphore--;
		playlist_progress_bar_show(pl);
	}

        g_signal_connect(G_OBJECT(pl->widget), "size_allocate",
			 G_CALLBACK(playlist_size_allocate), pl);

	gtk_widget_show_all(pl->widget);

	gtk_notebook_append_page(GTK_NOTEBOOK(playlist_notebook),
				 pl->widget,
				 create_playlist_tab_label(pl));

	gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), -1);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) > 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook), TRUE);
	}

	playlist_rename(pl, NULL);
}

playlist_t *
playlist_tab_new(char * name) {

	playlist_t * pl = playlist_new(name == NULL ? _("(Untitled)") : name);
	create_playlist_gui(pl);

	return pl;
}

void
notebook_switch_page(GtkNotebook * notebook, GtkNotebookPage * page,
		     guint page_num, gpointer user_data) {

	playlist_t * pl = playlist_get_by_page_num(page_num);

	if (pl == NULL) {
		return;
	}

	playlist_content_changed(pl);
	playlist_selection_changed(pl);
}

void
create_playlist(void) {

	GtkWidget * vbox;

	GtkWidget * statusbar;
	GtkWidget * statusbar_scrolledwin;
	GtkWidget * statusbar_viewport;

	GtkWidget * hbox_bottom;
	GtkWidget * add_button;
	GtkWidget * sel_button;
	GtkWidget * rem_button;


        playlist_color_is_set = 0;

	if (pl_color_active[0] == '\0') {
		strcpy(pl_color_active, "#ffffff");
	}

	if (pl_color_inactive[0] == '\0') {
		strcpy(pl_color_inactive, "#010101");
	}


        /* window creating stuff */
	if (!options.playlist_is_embedded) {
		playlist_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(playlist_window), _("Playlist"));
		gtk_container_set_border_width(GTK_CONTAINER(playlist_window), 2);
		g_signal_connect(G_OBJECT(playlist_window), "delete_event",
				 G_CALLBACK(playlist_window_close), NULL);
		gtk_widget_set_size_request(playlist_window, 300, 200);
	} else {
		gtk_widget_set_size_request(playlist_window, 200, 200);
	}

        if (!options.playlist_is_embedded) {
                g_signal_connect(G_OBJECT(playlist_window), "key_press_event",
                                 G_CALLBACK(playlist_window_key_pressed), NULL);
        }

	gtk_widget_set_events(playlist_window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);


        if (!options.playlist_is_embedded) {
                plist_menu = gtk_menu_new();
                init_plist_menu(plist_menu);
        }

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

	/* create notebook */
	playlist_notebook = gtk_notebook_new();
	gtk_notebook_set_show_border(GTK_NOTEBOOK(playlist_notebook), FALSE);
	gtk_notebook_popup_enable(GTK_NOTEBOOK(playlist_notebook));
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(playlist_notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook),
				   options.playlist_always_show_tabs);
        gtk_box_pack_start(GTK_BOX(vbox), playlist_notebook, TRUE, TRUE, 0);

	if (playlists != NULL) {
		GList * list;
		for (list = playlists; list; list = list->next) {
			create_playlist_gui((playlist_t *)list->data);
		}
	}

	/* should only be connected after creating playlist tabs */
	g_signal_connect(G_OBJECT(playlist_notebook), "switch_page",
			 G_CALLBACK(notebook_switch_page), NULL);


	/* statusbar */
	if (options.enable_playlist_statusbar) {

		statusbar_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
                GTK_WIDGET_UNSET_FLAGS(statusbar_scrolledwin, GTK_CAN_FOCUS);
		gtk_widget_set_size_request(statusbar_scrolledwin, 1, -1);    /* MAGIC */
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(statusbar_scrolledwin),
					       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

		statusbar_viewport = gtk_viewport_new(NULL, NULL);
		gtk_widget_set_name(statusbar_viewport, "info_viewport");

		gtk_container_add(GTK_CONTAINER(statusbar_scrolledwin), statusbar_viewport);
		gtk_box_pack_start(GTK_BOX(vbox), statusbar_scrolledwin, FALSE, TRUE, 2);

		gtk_widget_set_events(statusbar_viewport,
				      GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

		g_signal_connect(G_OBJECT(statusbar_viewport), "button_press_event",
				 G_CALLBACK(scroll_btn_pressed), NULL);
		g_signal_connect(G_OBJECT(statusbar_viewport), "button_release_event",
				 G_CALLBACK(scroll_btn_released), (gpointer)statusbar_scrolledwin);
		g_signal_connect(G_OBJECT(statusbar_viewport), "motion_notify_event",
				 G_CALLBACK(scroll_motion_notify), (gpointer)statusbar_scrolledwin);
		
		statusbar = gtk_hbox_new(FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(statusbar), 1);
		gtk_container_add(GTK_CONTAINER(statusbar_viewport), statusbar);
		
		statusbar_selected = gtk_label_new("");
		gtk_widget_set_name(statusbar_selected, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected, FALSE, TRUE, 0);
		
		statusbar_selected_label = gtk_label_new(_("Selected: "));
		gtk_widget_set_name(statusbar_selected_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected_label, FALSE, TRUE, 0);
		
		gtk_box_pack_end(GTK_BOX(statusbar), gtk_vseparator_new(), FALSE, TRUE, 5);
		
		statusbar_total = gtk_label_new("");
		gtk_widget_set_name(statusbar_total, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total, FALSE, TRUE, 0);
		
		statusbar_total_label = gtk_label_new(_("Total: "));
		gtk_widget_set_name(statusbar_total_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total_label, FALSE, TRUE, 0);

                if (options.override_skin_settings) {
                        gtk_widget_modify_font (statusbar_selected, fd_statusbar);
                        gtk_widget_modify_font (statusbar_selected_label, fd_statusbar);
                        gtk_widget_modify_font (statusbar_total, fd_statusbar);
                        gtk_widget_modify_font (statusbar_total_label, fd_statusbar);
                }

		/*		
		if ((pl = playlist_get_current()) != NULL) {
			playlist_selection_changed(pl);
			playlist_content_changed(pl);
		}
		*/
	}

	/* bottom area of playlist window */
        hbox_bottom = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_bottom, FALSE, TRUE, 0);

	add_button = gtk_button_new_with_label(_("Add files"));
        GTK_WIDGET_UNSET_FLAGS(add_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), add_button, _("Add files to playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), add_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(add_files), NULL);

	sel_button = gtk_button_new_with_label(_("Select all"));
        GTK_WIDGET_UNSET_FLAGS(sel_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), sel_button, _("Select all songs in playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), sel_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(sel_button), "clicked", G_CALLBACK(select_all), NULL);
	
	rem_button = gtk_button_new_with_label(_("Remove selected"));
        GTK_WIDGET_UNSET_FLAGS(rem_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), rem_button, _("Remove selected songs from playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), rem_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(rem_button), "clicked", G_CALLBACK(remove_sel), NULL);


        add_menu = gtk_menu_new();

        add__dir = gtk_menu_item_new_with_label(_("Add directory"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__dir);
        g_signal_connect_swapped(G_OBJECT(add__dir), "activate", G_CALLBACK(add__dir_cb), NULL);
	gtk_widget_show(add__dir);

        add__url = gtk_menu_item_new_with_label(_("Add URL"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__url);
        g_signal_connect_swapped(G_OBJECT(add__url), "activate", G_CALLBACK(add__url_cb), NULL);
	gtk_widget_show(add__url);

        add__files = gtk_menu_item_new_with_label(_("Add files"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__files);
        g_signal_connect_swapped(G_OBJECT(add__files), "activate", G_CALLBACK(add__files_cb), NULL);
	gtk_widget_show(add__files);

        g_signal_connect_swapped(G_OBJECT(add_button), "event", G_CALLBACK(add_cb), NULL);


        sel_menu = gtk_menu_new();

        sel__none = gtk_menu_item_new_with_label(_("Select none"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__none);
        g_signal_connect_swapped(G_OBJECT(sel__none), "activate", G_CALLBACK(sel__none_cb), NULL);
	gtk_widget_show(sel__none);

        sel__inv = gtk_menu_item_new_with_label(_("Invert selection"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__inv);
        g_signal_connect_swapped(G_OBJECT(sel__inv), "activate", G_CALLBACK(sel__inv_cb), NULL);
	gtk_widget_show(sel__inv);

        sel__all = gtk_menu_item_new_with_label(_("Select all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__all);
        g_signal_connect_swapped(G_OBJECT(sel__all), "activate", G_CALLBACK(sel__all_cb), NULL);
	gtk_widget_show(sel__all);

        g_signal_connect_swapped(G_OBJECT(sel_button), "event", G_CALLBACK(sel_cb), NULL);


        rem_menu = gtk_menu_new();

        rem__all = gtk_menu_item_new_with_label(_("Remove all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__all);
        g_signal_connect_swapped(G_OBJECT(rem__all), "activate", G_CALLBACK(rem__all_cb), NULL);
    	gtk_widget_show(rem__all);

        rem__dead = gtk_menu_item_new_with_label(_("Remove dead"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__dead);
        g_signal_connect_swapped(G_OBJECT(rem__dead), "activate", G_CALLBACK(rem__dead_cb), NULL);
    	gtk_widget_show(rem__dead);

        cut__sel = gtk_menu_item_new_with_label(_("Cut selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), cut__sel);
        g_signal_connect_swapped(G_OBJECT(cut__sel), "activate", G_CALLBACK(cut__sel_cb), NULL);
    	gtk_widget_show(cut__sel);

        rem__sel = gtk_menu_item_new_with_label(_("Remove selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__sel);
        g_signal_connect_swapped(G_OBJECT(rem__sel), "activate", G_CALLBACK(rem__sel_cb), NULL);
    	gtk_widget_show(rem__sel);

        g_signal_connect_swapped(G_OBJECT(rem_button), "event", G_CALLBACK(rem_cb), NULL);

	/*
        g_signal_connect(G_OBJECT(playlist_window), "size_allocate",
			 G_CALLBACK(playlist_size_allocate_all), NULL);
	*/
}
       

void
playlist_ensure_tab_exists() {

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 0) {
		playlist_tab_new(NULL);
	}
}


void
show_playlist(void) {

	options.playlist_on = 1;

	if (!options.playlist_is_embedded) {
		gtk_window_move(GTK_WINDOW(playlist_window), options.playlist_pos_x, options.playlist_pos_y);
		gtk_window_resize(GTK_WINDOW(playlist_window), options.playlist_size_x, options.playlist_size_y);
	} else {
		gtk_window_get_size(GTK_WINDOW(main_window), &options.main_size_x, &options.main_size_y);
		gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x, options.main_size_y + options.playlist_size_y + 6);
	}

        gtk_widget_show_all(playlist_window);

	if (!playlist_color_is_set) {
        	set_playlist_color();
		playlist_color_is_set = 1;
	}
}


void
hide_playlist(void) {

	options.playlist_on = 0;
	if (!options.playlist_is_embedded) {
		gtk_window_get_position(GTK_WINDOW(playlist_window), &options.playlist_pos_x, &options.playlist_pos_y);
		gtk_window_get_size(GTK_WINDOW(playlist_window), &options.playlist_size_x, &options.playlist_size_y);
	} else {
		options.playlist_size_x = playlist_window->allocation.width;
		options.playlist_size_y = playlist_window->allocation.height;
		gtk_window_get_size(GTK_WINDOW(main_window), &options.main_size_x, &options.main_size_y);
	}
        gtk_widget_hide(playlist_window);

	if (options.playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x, options.main_size_y - options.playlist_size_y - 6);
	}

}


xmlNodePtr
save_track_node(playlist_t * pl, GtkTreeIter * piter, xmlNodePtr root, char * nodeID) {

	gchar * track_name;
	gchar * phys_name;
	gchar *converted_temp;
	gchar * color;
	gfloat voladj;
	gfloat duration;
        gchar str[32];
        xmlNodePtr node;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), piter,
			   COLUMN_TRACK_NAME, &track_name, COLUMN_PHYSICAL_FILENAME, &phys_name, 
                           COLUMN_SELECTION_COLOR, &color,
			   COLUMN_VOLUME_ADJUSTMENT, &voladj, COLUMN_DURATION, &duration, -1);
	
	node = xmlNewTextChild(root, NULL, (const xmlChar*) nodeID, NULL);
	
	xmlNewTextChild(node, NULL, (const xmlChar*) "track_name", (const xmlChar*) track_name);
	if (!strcmp(nodeID,"record")) {
		xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) phys_name);
	} else {
		if ((strlen(phys_name) > 4) &&
		    (phys_name[0] == 'C') &&
		    (phys_name[1] == 'D') &&
		    (phys_name[2] == 'D') &&
		    (phys_name[3] == 'A')) {
			converted_temp = strdup(phys_name);
		} else if (httpc_is_url(phys_name)) {
			converted_temp = strdup(phys_name);
		} else {
			converted_temp = g_filename_to_uri(phys_name, NULL, NULL);
		}
		xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) converted_temp);
		g_free(converted_temp);
	};

	/* FIXME: don't use #000000 (black) color for active song */
	
	if (strcmp(color, pl_color_active) == 0) {
		strcpy(str, "yes");
	} else {
		strcpy(str, "no");
	}
	xmlNewTextChild(node, NULL, (const xmlChar*) "is_active", (const xmlChar*) str);
	
	snprintf(str, 31, "%f", voladj);
	xmlNewTextChild(node, NULL, (const xmlChar*) "voladj", (const xmlChar*) str);
	
	snprintf(str, 31, "%f", duration);
	xmlNewTextChild(node, NULL, (const xmlChar*) "duration", (const xmlChar*) str);
	
	g_free(track_name);
	g_free(phys_name);
	g_free(color);

	return node;
}

void
playlist_save_data(playlist_t * pl, xmlNodePtr dest) {

        gint i = 0;
        GtkTreeIter iter;

	xmlNewTextChild(dest, NULL, (const xmlChar*) "name", (const xmlChar*) pl->name);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

		if (n) { /* album node */
			gint j = 0;
			GtkTreeIter iter_child;
			xmlNodePtr node;

			node = save_track_node(pl, &iter, dest, "record");
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
							     &iter_child, &iter, j++)) {
				save_track_node(pl, &iter_child, node, "track");
			}
		} else { /* track node */
			save_track_node(pl, &iter, dest, "track");
		}
        }
}


void
playlist_save(playlist_t * pl, char * filename) {

        xmlDocPtr doc;
        xmlNodePtr root;

        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

	playlist_save_data(pl, root);

        xmlSaveFormatFile(filename, doc, 1);
	xmlFreeDoc(doc);
}

void
playlist_save_all(char * filename) {

        xmlDocPtr doc;
        xmlNodePtr root;
	xmlNodePtr tab;

    	GList * list;

        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist_multi");
        xmlDocSetRootElement(doc, root);

	for (list = playlists; list; list = list->next) {
		playlist_t * pl = (playlist_t *)list->data;
		tab = xmlNewTextChild(root, NULL, (const xmlChar*) "tab", NULL);
		playlist_save_data(pl, tab);
	}

        xmlSaveFormatFile(filename, doc, 1);
	xmlFreeDoc(doc);
}


xmlChar *
parse_playlist_name(xmlDocPtr doc, xmlNodePtr _cur) {

	xmlNodePtr cur;

        for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

                if (!xmlStrcmp(cur->name, (const xmlChar *)"name")) {
                        return xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                }
	}

	return NULL;
}

void
parse_playlist_track(playlist_transfer_t * pt, xmlDocPtr doc, xmlNodePtr _cur, int level) {

        xmlChar * key;
	gchar *converted_temp;
	gint is_record_node;

	xmlNodePtr cur;
	playlist_filemeta * plfm = NULL;

	
	is_record_node = !xmlStrcmp(_cur->name, (const xmlChar *)"record");

	if ((plfm = (playlist_filemeta *)calloc(1, sizeof(playlist_filemeta))) == NULL) {
		fprintf(stderr, "calloc error in parse_playlist_track()\n");
		return;
	}

	plfm->level = level;

        for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

                if (!xmlStrcmp(cur->name, (const xmlChar *)"track_name")) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				plfm->title = strndup((char *)key, MAXLEN-1);
			}
                        xmlFree(key);
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"phys_name")) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if (is_record_node) {
					/* "record" node keeps special data here */
					plfm->filename = strndup((char *)key, MAXLEN-1);
				} else if ((converted_temp = g_filename_from_uri((char *) key, NULL, NULL))) {
					plfm->filename = strndup(converted_temp, MAXLEN-1);
					g_free(converted_temp);
				} else {
					/* try to read utf8 filename from outdated file  */
					converted_temp = g_locale_from_utf8((char *) key, -1, NULL, NULL, NULL);
					if (converted_temp != NULL) {
						plfm->filename = strndup(converted_temp, MAXLEN-1);
						g_free(converted_temp);
					} else {
						/* last try - maybe it's plain locale filename */
						plfm->filename = strndup((char *)key, MAXLEN-1);
					}
				}
			}
                        xmlFree(key);
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"is_active")) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if (!xmlStrcmp(key, (const xmlChar *)"yes")) {
					plfm->active = 1;
				} else {
					plfm->active = 0;
				}
			} else {
				plfm->active = 0;
			}
                        xmlFree(key);
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"voladj")) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				plfm->voladj = convf((char *)key);
                        }
                        xmlFree(key);
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"duration")) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				plfm->duration = convf((char *)key);
                        }
                        xmlFree(key);
		}
	}

	pt->plfm = plfm;
	playlist_thread_add_to_list(pt);

	if (is_record_node) {
		for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
			if (!xmlStrcmp(cur->name, (const xmlChar *)"track")) {
				parse_playlist_track(pt, doc, cur, 1);
			}
		}
	}
}

void *
playlist_load_xml_thread(void * arg) {

	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
        xmlDocPtr doc = (xmlDocPtr)pt->xml_doc;
        xmlNodePtr cur = (xmlNodePtr)pt->xml_node;

	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	printf("--> playlist_load_xml_thread\n");

	for (cur = cur->xmlChildrenNode; cur != NULL && !pt->pl->thread_stop; cur = cur->next) {

		if (!xmlStrcmp(cur->name, (const xmlChar *)"track") ||
		    !xmlStrcmp(cur->name, (const xmlChar *)"record")) {
			parse_playlist_track(pt, doc, cur, 0);
		}
	}

	/*xmlFreeNode((xmlNodePtr)pt->xml_node);
	  xmlFreeDoc((xmlDocPtr)pt->xml_doc);*/

 	g_idle_add(finalize_add_to_playlist, pt);

	printf("<-- playlist_load_xml_thread\n");

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	return NULL;
}


void
playlist_load_xml(char * filename, int mode, playlist_transfer_t * ptrans) {

        xmlDocPtr doc;
        xmlNodePtr cur;
	FILE * f;

        if ((f = fopen(filename, "rt")) == NULL) {
                return;
        }
        fclose(f);

        doc = xmlParseFile(filename);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", filename);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_playlist: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

	if (!xmlStrcmp(cur->name, (const xmlChar *)"aqualung_playlist_multi")) {

		printf("multi playlist detected\n");

		if (ptrans != NULL && ptrans->on_the_fly) {
			/* unnecessary tab for multitab playlist */
			playlist_tab_close(NULL, ptrans->pl);
			free(ptrans);
		}

		for (cur = cur->xmlChildrenNode;
		     cur != NULL && !playlist_thread_stop; cur = cur->next) {

			if (!xmlStrcmp(cur->name, (const xmlChar *)"tab")) {

				/* ignore mode: multitab playlists are
				   always loaded in separate tabs */
				playlist_t * pl = playlist_tab_new(NULL);
				playlist_transfer_t * pt = playlist_transfer_new(pl);

				xmlChar * name = parse_playlist_name(doc, cur);

				if (name != NULL) {
					playlist_rename(pt->pl, (char *)name);
					xmlFree(name);
				}

				pt->xml_doc = doc; /*(void *)xmlCopyDoc(doc, 1);*/
				pt->xml_node = cur; /*(void *)xmlDocCopyNode(cur, doc, 1);*/
				  
				playlist_progress_bar_show(pt->pl);
				AQUALUNG_THREAD_CREATE(pt->pl->thread_id, NULL,
						       playlist_load_xml_thread, pt);
			}
		}

	} else if (!xmlStrcmp(cur->name, (const xmlChar *)"aqualung_playlist")) {

		playlist_transfer_t * pt = NULL;
		xmlChar * name = parse_playlist_name(doc, cur);

		if (ptrans != NULL) {
			pt = ptrans;
		} else {
			pt = playlist_transfer_get(mode, NULL);
		}

		if (name != NULL) {
			printf("name = %s\n", name);
			if (ptrans == NULL || !strcmp(ptrans->pl->name, _("(Untitled)"))) {
				playlist_rename(pt->pl, (char *)name);
			}
			xmlFree(name);
		}

		pt->xml_doc = doc;
		pt->xml_node = cur;

		playlist_progress_bar_show(pt->pl);
		AQUALUNG_THREAD_CREATE(pt->pl->thread_id, NULL,
				       playlist_load_xml_thread, pt);

	} else {
		fprintf(stderr, "load_playlist: XML document of the wrong type, "
			"root node != aqualung_playlist\n");
		xmlFreeDoc(doc);
		return;
        }

        /*xmlFreeDoc(doc);*/
}

void
playlist_load_m3u(char * filename, int enqueue) {

	FILE * f;
	gint c;
	gint i = 0;
	gint n;
	gchar * str;
	gchar pl_dir[MAXLEN];
	gchar line[MAXLEN];
	gchar name[MAXLEN];
	gchar path[MAXLEN];
	gchar tmp[MAXLEN];
	gint have_name = 0;
	playlist_filemeta * plfm = NULL;


	if ((str = strrchr(filename, '/')) == NULL) {
		printf("load_m3u(): programmer error: playlist path is not absolute\n");
	}
	for (i = 0; (i < (str - filename)) && (i < MAXLEN-1); i++) {
		pl_dir[i] = filename[i];
	}
	pl_dir[i] = '\0';

	if ((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open .m3u playlist: %s\n", filename);
		return;
	}

	if (!enqueue) {
		playlist_thread_add_to_list(NULL);
	}

	i = 0;
	while ((c = fgetc(f)) != EOF && !playlist_thread_stop) {
		if ((c != '\n') && (c != '\r') && (i < MAXLEN)) {
			if ((i > 0) || ((c != ' ') && (c != '\t'))) {
				line[i++] = c;
			}
		} else {
			line[i] = '\0';
			if (i == 0) {
				continue;
			}
			i = 0;

			if (strstr(line, "#EXTM3U") == line) {
				continue;
			}
			
			if (strstr(line, "#EXTINF:") == line) {

				char str_duration[64];
				int cnt = 0;
				
				/* We parse the timing, but throw it away. 
				   This may change in the future. */
				while ((line[cnt+8] >= '0') && (line[cnt+8] <= '9') && cnt < 63) {
					str_duration[cnt] = line[cnt+8];
					++cnt;
				}
				str_duration[cnt] = '\0';
				snprintf(name, MAXLEN-1, "%s", line+cnt+9);
				have_name = 1;

			} else {
				/* safeguard against C:\ stuff */
				if ((line[1] == ':') && (line[2] == '\\')) {
					fprintf(stderr, "Ignoring playlist item: %s\n", line);
					i = 0;
					have_name = 0;
					continue;
				}

				snprintf(path, MAXLEN-1, "%s", line);

				/* path curing: turn \-s into /-s */
				for (n = 0; n < strlen(path); n++) {
					if (path[n] == '\\')
						path[n] = '/';
				}
				
				normalize_filename(path, tmp);
				strncpy(path, tmp, MAXLEN-1);

				if (!have_name) {
					gchar * ch;
					if ((ch = strrchr(path, '/')) != NULL) {
						++ch;
						snprintf(name, MAXLEN-1, "%s", ch);
					} else {
						fprintf(stderr,
							"warning: ain't this a directory? : %s\n", path);
						snprintf(name, MAXLEN-1, "%s", path);
					}
				}
				have_name = 0;

				
				plfm = playlist_filemeta_get(path,
							     have_name ? name : NULL,
							     1);
				if (plfm == NULL) {
					fprintf(stderr, "load_m3u(): playlist_filemeta_get() returned NULL\n");
				} else {
					//playlist_thread_add_to_list(plfm); // TODO
				}
			}
		}
	}
}


void
load_pls_load(char * file, char * title, gint * have_file, gint * have_title) {

	playlist_filemeta * plfm = NULL;

	if (*have_file == 0) {
		return;
	}

	plfm = playlist_filemeta_get(file,
				     *have_title ? title : NULL,
				     1);

	if (plfm == NULL) {
		fprintf(stderr, "load_pls_load(): playlist_filemeta_get() returned NULL\n");
	} else {
		//playlist_thread_add_to_list(plfm); // TODO
	}
	
	*have_file = *have_title = 0;
}

void
load_pls(char * filename, int enqueue) {

	FILE * f;
	gint c;
	gint i = 0;
	gint n;
	gchar * str;
	gchar pl_dir[MAXLEN];
	gchar line[MAXLEN];
	gchar file[MAXLEN];
	gchar title[MAXLEN];
	gchar tmp[MAXLEN];
	gint have_file = 0;
	gint have_title = 0;
	gchar numstr_file[10];
	gchar numstr_title[10];


	if ((str = strrchr(filename, '/')) == NULL) {
		printf("load_pls(): programmer error: playlist path is not absolute\n");
	}
	for (i = 0; (i < (str - filename)) && (i < MAXLEN-1); i++) {
		pl_dir[i] = filename[i];
	}
	pl_dir[i] = '\0';

	if ((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open .pls playlist: %s\n", filename);
		return;
	}

	if (!enqueue) {
		playlist_thread_add_to_list(NULL);
	}

	i = 0;
	while ((c = fgetc(f)) != EOF && !playlist_thread_stop) {
		if ((c != '\n') && (c != '\r') && (i < MAXLEN)) {
			if ((i > 0) || ((c != ' ') && (c != '\t'))) {
				line[i++] = c;
			}
		} else {
			line[i] = '\0';
			if (i == 0)
				continue;
			i = 0;

			if (strstr(line, "[playlist]") == line) {
				continue;
			}
			if (strcasestr(line, "NumberOfEntries") == line) {
				continue;
			}
			if (strstr(line, "Version") == line) {
				continue;
			}
			
			if (strstr(line, "File") == line) {

				char numstr[10];
				char * ch;
				int m;

				if (have_file) {
					load_pls_load(file, title, &have_file, &have_title);
				}

				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", filename);
					return;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

				m = 0;
				for (n = 0; (line[n+4] != '=') && (m < sizeof(numstr)); n++) {
					if ((line[n+4] != ' ') && (line[n+4] != '\t'))
						numstr[m++] = line[n+4];
				}
				numstr[m] = '\0';
				strncpy(numstr_file, numstr, sizeof(numstr_file));

				/* safeguard against C:\ stuff */
				if ((ch[1] == ':') && (ch[2] == '\\')) {
					fprintf(stderr, "Ignoring playlist item: %s\n", ch);
					i = 0;
					have_file = have_title = 0;
					continue;
				}

				snprintf(file, MAXLEN-1, "%s", ch);

				/* path curing: turn \-s into /-s */

				for (n = 0; n < strlen(file); n++) {
					if (file[n] == '\\')
						file[n] = '/';
				}

				normalize_filename(file, tmp);
				strncpy(file, tmp, MAXLEN-1);

				have_file = 1;
				

			} else if (strstr(line, "Title") == line) {

				char numstr[10];
				char * ch;
				int m;

				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", filename);
					return;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

				m = 0;
				for (n = 0; (line[n+5] != '=') && (m < sizeof(numstr)); n++) {
					if ((line[n+5] != ' ') && (line[n+5] != '\t'))
						numstr[m++] = line[n+5];
				}
				numstr[m] = '\0';
				strncpy(numstr_title, numstr, sizeof(numstr_title));

				snprintf(title, MAXLEN-1, "%s", ch);
				have_title = 1;


			} else if (strstr(line, "Length") == line) {

				/* We parse the timing, but throw it away. 
				   This may change in the future. */

				char * ch;
				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", filename);
					return;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

			} else {
				fprintf(stderr, 
					"Syntax error: invalid line in playlist: %s\n", line);
				return;
			}

			if (!have_file || !have_title) {
				continue;
			}
			
			if (strcmp(numstr_file, numstr_title) != 0) {
				continue;
			}

			load_pls_load(file, title, &have_file, &have_title);
		}
	}
	load_pls_load(file, title, &have_file, &have_title);
}


/* return values: 
 *   0: not a playlist 
 *   1: native aqualung (XML)
 *   2: .m3u
 *   3: .pls
 */
int
is_playlist(char * filename) {

	FILE * f;
	gchar buf[] = "<?xml version=\"1.0\"?>\n<aqualung_playlist";
	gchar inbuf[64];
	gint len;

	if ((f = fopen(filename, "rb")) == NULL) {
		return 0;
	}
	if (fread(inbuf, 1, strlen(buf)+1, f) != strlen(buf)+1) {
		fclose(f);
		goto _readext;
	}
	fclose(f);
	inbuf[strlen(buf)] = '\0';

	if (strcmp(buf, inbuf) == 0) {
		return 1;
	}

 _readext:
	len = strlen(filename);
	if (len < 5) {
		return 0;
	}

	if ((filename[len-4] == '.') &&
	    ((filename[len-3] == 'm') || (filename[len-3] == 'M')) &&
	    (filename[len-2] == '3') &&
	    ((filename[len-1] == 'u') || (filename[len-1] == 'U'))) {
		return 2;
	}

	if ((filename[len-4] == '.') &&
	    ((filename[len-3] == 'p') || (filename[len-3] == 'P')) &&
	    ((filename[len-2] == 'l') || (filename[len-2] == 'L')) &&
	    ((filename[len-1] == 's') || (filename[len-1] == 'S'))) {
		return 3;
	}

	return 0;
}


void
init_plist_menu(GtkWidget *append_menu) {

	GtkWidget * separator;

        plist__save = gtk_menu_item_new_with_label(_("Save playlist"));
        plist__save_all = gtk_menu_item_new_with_label(_("Save all playlists"));
        plist__load_tab = gtk_menu_item_new_with_label(_("Load playlist in new tab"));
        plist__load = gtk_menu_item_new_with_label(_("Load playlist"));
        plist__enqueue = gtk_menu_item_new_with_label(_("Enqueue playlist"));
#ifdef HAVE_IFP
        plist__send_songs_to_iriver = gtk_menu_item_new_with_label(_("Send to iFP device"));
#endif  /* HAVE_IFP */
        plist__rva = gtk_menu_item_new_with_label(_("Calculate RVA"));
        plist__rva_menu = gtk_menu_new();
        plist__rva_separate = gtk_menu_item_new_with_label(_("Separate"));
        plist__rva_average = gtk_menu_item_new_with_label(_("Average"));
        plist__reread_file_meta = gtk_menu_item_new_with_label(_("Reread file metadata"));
        plist__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
        plist__search = gtk_menu_item_new_with_label(_("Search..."));

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__save);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__save_all);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__load_tab);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__load);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__enqueue);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

#ifdef HAVE_IFP
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__send_songs_to_iriver);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);
#endif  /* HAVE_IFP */

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__rva);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(plist__rva), plist__rva_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_separate);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_average);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__reread_file_meta);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__fileinfo);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__search);

        g_signal_connect_swapped(G_OBJECT(plist__save), "activate", G_CALLBACK(plist__save_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__save_all), "activate", G_CALLBACK(plist__save_all_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__load_tab), "activate", G_CALLBACK(plist__load_tab_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__load), "activate", G_CALLBACK(plist__load_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__enqueue), "activate", G_CALLBACK(plist__enqueue_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__rva_separate), "activate", G_CALLBACK(plist__rva_separate_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__rva_average), "activate", G_CALLBACK(plist__rva_average_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__reread_file_meta), "activate", G_CALLBACK(plist__reread_file_meta_cb), NULL);

#ifdef HAVE_IFP
        g_signal_connect_swapped(G_OBJECT(plist__send_songs_to_iriver), "activate", G_CALLBACK(plist__send_songs_to_iriver_cb), NULL);
#endif  /* HAVE_IFP */

        g_signal_connect_swapped(G_OBJECT(plist__fileinfo), "activate", G_CALLBACK(plist__fileinfo_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__search), "activate", G_CALLBACK(plist__search_cb), NULL);

        gtk_widget_show(plist__save);
        gtk_widget_show(plist__save_all);
        gtk_widget_show(plist__load_tab);
        gtk_widget_show(plist__load);
        gtk_widget_show(plist__enqueue);
        gtk_widget_show(plist__rva);
        gtk_widget_show(plist__rva_separate);
        gtk_widget_show(plist__rva_average);
        gtk_widget_show(plist__reread_file_meta);
#ifdef HAVE_IFP
        gtk_widget_show(plist__send_songs_to_iriver);
#endif  /* HAVE_IFP */
        gtk_widget_show(plist__fileinfo);
        gtk_widget_show(plist__search);
}


void
show_active_position_in_playlist(playlist_t * pl) {

        GtkTreeIter iter;
	gchar * str;
        gint flag;
        GtkTreePath * visible_path;
        GtkTreeViewColumn * visible_column;


        flag = 0;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   COLUMN_SELECTION_COLOR, &str, -1);
		        if (strcmp(str, pl_color_active) == 0) {
                                flag = 1;
		                if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
                                    gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &visible_path, &visible_column);
                                }
                                break;
                        }
			g_free(str);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &iter));

                if (!flag) {
                        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, 0);
                }

                set_cursor_in_playlist(pl, &iter, TRUE);
        }
}

void
show_active_position_in_playlist_toggle(playlist_t * pl) {

        GtkTreeIter iter, iter_child;
	gchar * str;
        gint flag, cflag, j;
        GtkTreePath * visible_path;
        GtkTreeViewColumn * visible_column;


        flag = cflag = 0;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, COLUMN_SELECTION_COLOR, &str, -1);
		        if (strcmp(str, pl_color_active) == 0) {
                                flag = 1;
		                if (gtk_tree_selection_iter_is_selected(pl->select, &iter) &&
                                    gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter)) {

                                        gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &visible_path, &visible_column);

                                        if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(pl->view), visible_path)) {
                                                gtk_tree_view_expand_row(GTK_TREE_VIEW(pl->view), visible_path, FALSE);
                                        }

                                        j = 0;

                                        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
                                                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child, COLUMN_SELECTION_COLOR, &str, -1);
                                                if (strcmp(str, pl_color_active) == 0) {
                                                        cflag = 1;
                                                        break;
                                                }
                                        }
                                }
                                break;
                        }

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &iter));

                if (flag) {
                        if (cflag) {
                                set_cursor_in_playlist(pl, &iter_child, TRUE);
                        } else {
                                set_cursor_in_playlist(pl, &iter, TRUE);
                        }
                }
        }
}


void
expand_collapse_album_node(playlist_t * pl) {

	GtkTreeIter iter, iter_child;
	gint i, j;
        GtkTreePath *path;
        GtkTreeViewColumn *column;


        i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(pl->select, &iter) &&
                    gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter)) {

                        gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, &column);

                        if (path && gtk_tree_view_row_expanded(GTK_TREE_VIEW(pl->view), path)) {
                                gtk_tree_view_collapse_row(GTK_TREE_VIEW(pl->view), path);
                        } else {
                                gtk_tree_view_expand_row(GTK_TREE_VIEW(pl->view), path, FALSE);
                        }
                        
                        continue;
                }

                j = 0;

                while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {

                        if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child) &&
                            gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &iter, &iter_child)) {

                                path = gtk_tree_model_get_path (GTK_TREE_MODEL(pl->store), &iter);

                                if (path) {
                                        gtk_tree_view_collapse_row(GTK_TREE_VIEW(pl->view), path);
                                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (pl->view), path, NULL, TRUE);
                                }
                        }
                }
        }
}


static gboolean
pl_progress_bar_update(gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	if (pl->progbar != NULL) {
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(pl->progbar));
		return TRUE;
	}

	return FALSE;
}

static void
pl_progress_bar_stop_clicked(GtkWidget * widget, gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	pl->thread_stop = 1;
}

void
playlist_progress_bar_show(playlist_t * pl) {

	pl->progbar_semaphore++;

	if (pl->progbar != NULL) {
		return;
	}

	pl->thread_stop = 0;

	playlist_stats_set_busy();

	pl->progbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(pl->progbar_container), pl->progbar, TRUE, TRUE, 0);

	pl->progbar_stop_button = gtk_button_new_with_label(_("Stop adding files"));
        gtk_box_pack_start(GTK_BOX(pl->progbar_container), pl->progbar_stop_button, FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(pl->progbar_stop_button), "clicked",
			 G_CALLBACK(pl_progress_bar_stop_clicked), pl);

	gtk_widget_show_all(pl->progbar_container);

	g_timeout_add(200, pl_progress_bar_update, pl);
}


void
playlist_progress_bar_hide(playlist_t * pl) {

	if (pl->progbar != NULL) {
		gtk_widget_destroy(pl->progbar);
		pl->progbar = NULL;
	}

	if (pl->progbar_stop_button != NULL) {
		gtk_widget_destroy(pl->progbar_stop_button);
		pl->progbar_stop_button = NULL;
	}
}

void
playlist_progress_bar_hide_all() {

	GList * list;

	for (list = playlists; list; list = list->next) {
		playlist_progress_bar_hide((playlist_t *)list->data);
	}
}


void
remove_files(playlist_t * pl) {

        GtkWidget *info_dialog;
        gint response, n = 0, i = 0, j, files = 0, last_pos;
	GtkTreeIter iter, iter_child;
        gchar temp[MAXLEN], *filename;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

                if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
		        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);
                        files++;
                }

                j = 0;
                while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
                        if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
                                n = 1;
                        }
                }
                if (n) break;
        }
        
        if (!n) {

                if (files == 1) {
                        sprintf(temp, _("The selected file will be deleted from the filesystem. "
					"No recovery will be possible after this operation.\n\n"
					"Are you sure?"));
                } else {
                        sprintf(temp, _("All selected files will be deleted from the filesystem. "
					"No recovery will be possible after this operation.\n\n"
					"Are you sure?"));
                }

                info_dialog = gtk_message_dialog_new (options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                                      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, temp);

                gtk_window_set_title(GTK_WINDOW(info_dialog), _("Remove file"));
                gtk_widget_show (info_dialog);

                response = aqualung_dialog_run (GTK_DIALOG (info_dialog));     

                gtk_widget_destroy(info_dialog);

                if (response == GTK_RESPONSE_YES) {   

                        i = last_pos = 0;
                	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {
                                if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {

					char * filename_locale;

                                        gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, COLUMN_PHYSICAL_FILENAME, &filename, -1);

					if ((filename_locale = g_locale_from_utf8(filename, -1, NULL, NULL, NULL)) == NULL) {
						g_free(filename);
						continue;
					}

                                        n = unlink(filename_locale);
                                        if (n != -1) {
                                                gtk_tree_store_remove(pl->store, &iter);
                                                --i;
                                                last_pos = (i-1) < 0 ? 0 : i-1;
                                        }

                                        g_free(filename_locale);
                                        g_free(filename);
                                }
                        }

                        for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i); i++);

                        if (i) {
                                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, last_pos);
                                set_cursor_in_playlist(pl, &iter, FALSE);
                        }
                        playlist_content_changed(pl);
                }
        }
}

void
set_cursor_in_playlist(playlist_t * pl, GtkTreeIter *iter, gboolean scroll) {

        GtkTreePath * visible_path;

        visible_path = gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), iter);

        if (visible_path) {
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(pl->view), visible_path, NULL, TRUE);
                if (scroll == TRUE) {
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW (pl->view), visible_path, 
						     NULL, TRUE, 0.3, 0.0);
                }
                gtk_tree_path_free(visible_path);
        }
}

void
select_active_position_in_playlist(playlist_t * pl) {

	GtkTreeIter iter;
	gchar * str;

	if (pl == NULL) { // TODO: this check won't be needed
		return;
	}

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   COLUMN_SELECTION_COLOR, &str, -1);
		        if (strcmp(str, pl_color_active) == 0) {
                                set_cursor_in_playlist(pl, &iter, FALSE);
				g_free(str);
				break;
                        }
			g_free(str);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &iter));
        }
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

