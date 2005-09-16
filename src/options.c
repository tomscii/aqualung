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

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#include "common.h"
#include "gui_main.h"
#include "playlist.h"
#include "i18n.h"
#include "options.h"


extern pthread_mutex_t output_thread_lock;

char title_format[MAXLEN];
char default_param[MAXLEN];

#ifdef HAVE_SRC
extern int src_type;
GtkWidget * optmenu_src;
#endif /* HAVE_SRC */

extern int ladspa_is_postfader;
extern GtkWidget * main_window;
extern GtkTooltips * aqualung_tooltips;
extern int auto_save_playlist;
extern int show_rva_in_playlist;
extern int show_length_in_playlist;
extern int show_track_name_using_bold;
extern int plcol_idx[3];
extern int rva_is_enabled;
extern int rva_env;
extern float rva_refvol;
extern float rva_steepness;
extern int rva_use_averaging;
extern int rva_use_linear_thresh;
extern float rva_avg_linear_thresh;
extern float rva_avg_stddev_thresh;
extern int auto_use_meta_artist;
extern int auto_use_meta_record;
extern int auto_use_meta_track;
extern int auto_use_ext_meta_artist;
extern int auto_use_ext_meta_record;
extern int auto_use_ext_meta_track;
extern int replaygain_tag_to_use;
extern int enable_tooltips;
extern int hide_comment_pane_shadow;
extern int playlist_is_embedded_shadow;
extern int buttons_at_the_bottom;
extern int override_skin_settings;
extern int enable_playlist_statusbar;
extern int enable_playlist_statusbar_shadow;

extern char playlist_font[MAX_FONTNAME_LEN];
extern char browser_font[MAX_FONTNAME_LEN];
extern char bigtimer_font[MAX_FONTNAME_LEN];
extern char smalltimer_font[MAX_FONTNAME_LEN];
extern PangoFontDescription *fd_playlist;
extern PangoFontDescription *fd_browser;
extern PangoFontDescription *fd_bigtimer;
extern PangoFontDescription *fd_smalltimer;

extern GtkWidget * bigtimer_label;
extern GtkWidget * smalltimer_label_1;
extern GtkWidget * smalltimer_label_2;

extern char activesong_color[MAX_COLORNAME_LEN];

int auto_save_playlist_shadow;
int show_rva_in_playlist_shadow;
int show_length_in_playlist_shadow;
int show_track_name_using_bold_shadow;
int rva_is_enabled_shadow;
int rva_env_shadow;
float rva_refvol_shadow;
float rva_steepness_shadow;
int rva_use_averaging_shadow;
int rva_use_linear_thresh_shadow;
float rva_avg_linear_thresh_shadow;
float rva_avg_stddev_thresh_shadow;

int restart_flag;
int override_past_state;

extern GtkWidget * music_tree;

extern GtkWidget * play_list;
extern GtkTreeViewColumn * track_column;
extern GtkTreeViewColumn * rva_column;
extern GtkTreeViewColumn * length_column;

extern char skin[MAXLEN];

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);
extern void disable_bold_font_in_playlist(void);

GtkWidget * options_window;
GtkWidget * optmenu_ladspa;
GtkWidget * optmenu_listening_env;
GtkWidget * optmenu_threshold;
GtkWidget * optmenu_replaygain;
GtkWidget * entry_title;
GtkWidget * entry_param;
GtkWidget * label_src;
GtkWidget * check_autoplsave;
GtkWidget * check_show_rva_in_playlist;
GtkWidget * check_show_length_in_playlist;
GtkWidget * check_show_track_name_using_bold;
GtkWidget * check_rva_is_enabled;
GtkWidget * check_rva_use_averaging;
GtkWidget * check_auto_use_meta_artist;
GtkWidget * check_auto_use_meta_record;
GtkWidget * check_auto_use_meta_track;
GtkWidget * check_auto_use_ext_meta_artist;
GtkWidget * check_auto_use_ext_meta_record;
GtkWidget * check_auto_use_ext_meta_track;
GtkWidget * check_enable_tooltips;
GtkWidget * check_hide_comment_pane;
GtkWidget * check_playlist_is_embedded;
GtkWidget * check_enable_playlist_statusbar;
GtkWidget * check_buttons_at_the_bottom;
GtkWidget * check_override_skin;

GtkObject * adj_refvol;
GtkObject * adj_steepness;
GtkObject * adj_linthresh;
GtkObject * adj_stdthresh;
GtkWidget * rva_drawing_area;
GdkPixmap * rva_pixmap = NULL;
GtkWidget * rva_viewport;
GtkWidget * spin_refvol;
GtkWidget * spin_steepness;
GtkWidget * spin_linthresh;
GtkWidget * spin_stdthresh;
GtkWidget * label_listening_env;
GtkWidget * label_refvol;
GtkWidget * label_steepness;
GtkWidget * label_threshold;
GtkWidget * label_linthresh;
GtkWidget * label_stdthresh;
GtkListStore * plistcol_store;

#define DEFAULT_FONT_NAME "Sans 11"

GtkWidget * entry_pl_font;
GtkWidget * entry_ms_font;
GtkWidget * entry_bt_font;
GtkWidget * entry_st_font;
GtkWidget * button_pl_font;
GtkWidget * button_ms_font;
GtkWidget * button_bt_font;
GtkWidget * button_st_font;

GdkColor color;
GtkWidget * color_picker;

void draw_rva_diagram(void);
void show_restart_info(void);


void open_font_desc(void)
{
        if(fd_playlist) pango_font_description_free(fd_playlist);
	fd_playlist = pango_font_description_from_string(playlist_font);
        if(fd_browser) pango_font_description_free(fd_browser);
	fd_browser = pango_font_description_from_string(browser_font);
        if(fd_bigtimer) pango_font_description_free(fd_bigtimer);
	fd_bigtimer = pango_font_description_from_string(bigtimer_font);
        if(fd_smalltimer) pango_font_description_free(fd_smalltimer);
	fd_smalltimer = pango_font_description_from_string(smalltimer_font);
}

static gint
ok(GtkWidget * widget, gpointer data) {

	int i;
	int n;
	int n_prev = 3;
        GdkColor color;

	strncpy(title_format, gtk_entry_get_text(GTK_ENTRY(entry_title)), MAXLEN - 1);
	strncpy(default_param, gtk_entry_get_text(GTK_ENTRY(entry_param)), MAXLEN - 1);
	auto_save_playlist = auto_save_playlist_shadow;
	rva_is_enabled = rva_is_enabled_shadow;
	rva_env = rva_env_shadow;
	rva_refvol = rva_refvol_shadow;
	rva_steepness = rva_steepness_shadow;
	rva_use_averaging = rva_use_averaging_shadow;
	rva_use_linear_thresh = rva_use_linear_thresh_shadow;
	rva_avg_linear_thresh = rva_avg_linear_thresh_shadow;
	rva_avg_stddev_thresh = rva_avg_stddev_thresh_shadow;


	show_rva_in_playlist = show_rva_in_playlist_shadow;
	if (show_rva_in_playlist) {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(rva_column), TRUE);
	} else {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(rva_column), FALSE);
	}

	show_length_in_playlist = show_length_in_playlist_shadow;
	if (show_length_in_playlist) {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(length_column), TRUE);
	} else {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(length_column), FALSE);
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_track_name_using_bold))) {
		show_track_name_using_bold = 1;
	} else {
		show_track_name_using_bold = 0;
                disable_bold_font_in_playlist();
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_artist))) {
		auto_use_meta_artist = 1;
	} else {
		auto_use_meta_artist = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_record))) {
		auto_use_meta_record = 1;
	} else {
		auto_use_meta_record = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_track))) {
		auto_use_meta_track = 1;
	} else {
		auto_use_meta_track = 0;
	}


	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_artist))) {
		auto_use_ext_meta_artist = 1;
	} else {
		auto_use_ext_meta_artist = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_record))) {
		auto_use_ext_meta_record = 1;
	} else {
		auto_use_ext_meta_record = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_track))) {
		auto_use_ext_meta_track = 1;
	} else {
		auto_use_ext_meta_track = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_tooltips))) {
		enable_tooltips = 1;
                gtk_tooltips_enable(aqualung_tooltips);
	} else {
	        enable_tooltips = 0;
                gtk_tooltips_disable(aqualung_tooltips);
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_buttons_at_the_bottom))) {
		buttons_at_the_bottom = 1;
	} else {
	        buttons_at_the_bottom = 0;
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_override_skin))) {
		override_skin_settings = 1;
	} else {
	        override_skin_settings = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_hide_comment_pane))) {
		hide_comment_pane_shadow = 1;
	} else {
	        hide_comment_pane_shadow = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_playlist_is_embedded))) {
		playlist_is_embedded_shadow = 1;
	} else {
	        playlist_is_embedded_shadow = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_playlist_statusbar))) {
		enable_playlist_statusbar_shadow = 1;
	} else {
	        enable_playlist_statusbar_shadow = 0;
	}

	replaygain_tag_to_use = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_replaygain));

        for (i = 0; i < 3; i++) {

		GtkTreeIter iter;
		GtkTreeViewColumn * cols[4];
		char * pnumstr;

		cols[0] = track_column;
		cols[1] = rva_column;
		cols[2] = length_column;
		cols[3] = NULL;

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(plistcol_store), &iter, NULL, i);
		
		gtk_tree_model_get(GTK_TREE_MODEL(plistcol_store), &iter, 1, &pnumstr, -1);
		n = atoi(pnumstr);
		g_free(pnumstr);

		gtk_tree_view_move_column_after(GTK_TREE_VIEW(play_list),
						cols[n], cols[n_prev]);

		plcol_idx[i] = n;
		n_prev = n;
	}	

	playlist_size_allocate(NULL, NULL);
	delayed_playlist_rearrange(100);

        /* apply changes */

        if(override_skin_settings) {

                /* apply fonts */

                open_font_desc();

                gtk_widget_modify_font (music_tree, fd_browser);
                gtk_widget_modify_font (play_list, fd_playlist);
                gtk_widget_modify_font (bigtimer_label, fd_bigtimer);
                gtk_widget_modify_font (smalltimer_label_1, fd_smalltimer);
                gtk_widget_modify_font (smalltimer_label_2, fd_smalltimer);

                /* apply colors */               

                if(gdk_color_parse(activesong_color, &color) == TRUE) {

                        play_list->style->fg[SELECTED].red = color.red;
                        play_list->style->fg[SELECTED].green = color.green;
                        play_list->style->fg[SELECTED].blue = color.blue;
                        set_playlist_color();
                }

        } else if(override_past_state) {

                /* reload skin */
                change_skin(skin);
                override_past_state = 0;
        }

        if (restart_flag) {
                show_restart_info();            
        }

	gtk_widget_destroy(options_window);
	return TRUE;
}


static gint
cancel(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(options_window);
	return TRUE;
}


void
autoplsave_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_autoplsave))) {
		auto_save_playlist_shadow = 1;
	} else {
		auto_save_playlist_shadow = 0;
	}
}


void
check_show_rva_in_playlist_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_rva_in_playlist))) {
		show_rva_in_playlist_shadow = 1;
	} else {
		show_rva_in_playlist_shadow = 0;
	}
}


void
check_show_length_in_playlist_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_length_in_playlist))) {
		show_length_in_playlist_shadow = 1;
	} else {
		show_length_in_playlist_shadow = 0;
	}
}

void
check_show_track_name_using_bold_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_track_name_using_bold))) {
		show_track_name_using_bold_shadow = 1;
	} else {
		show_track_name_using_bold_shadow = 0;
	}
}

void
changed_ladspa_prepost(GtkWidget * widget, gpointer * data) {

	int status = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_ladspa));
	pthread_mutex_lock(&output_thread_lock);
	ladspa_is_postfader = status;
	pthread_mutex_unlock(&output_thread_lock);
}


#ifdef HAVE_SRC
void
changed_src_type(GtkWidget * widget, gpointer * data) {

	src_type = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_src));
	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(src_type));
	set_src_type_label(src_type);
}
#endif /* HAVE_SRC */


void
check_rva_is_enabled_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rva_is_enabled))) {
		rva_is_enabled_shadow = 1;
		gtk_widget_set_sensitive(optmenu_listening_env, TRUE);
		gtk_widget_set_sensitive(label_listening_env, TRUE);
		gtk_widget_set_sensitive(spin_refvol, TRUE);
		gtk_widget_set_sensitive(label_refvol, TRUE);
		gtk_widget_set_sensitive(spin_steepness, TRUE);
		gtk_widget_set_sensitive(label_steepness, TRUE);

		gtk_widget_set_sensitive(check_rva_use_averaging, TRUE);

		if (rva_use_averaging_shadow) {
			gtk_widget_set_sensitive(optmenu_threshold, TRUE);
			gtk_widget_set_sensitive(label_threshold, TRUE);
			gtk_widget_set_sensitive(spin_linthresh, TRUE);
			gtk_widget_set_sensitive(label_linthresh, TRUE);
			gtk_widget_set_sensitive(spin_stdthresh, TRUE);
			gtk_widget_set_sensitive(label_stdthresh, TRUE);
		}
	} else {
		rva_is_enabled_shadow = 0;
		gtk_widget_set_sensitive(optmenu_listening_env, FALSE);
		gtk_widget_set_sensitive(label_listening_env, FALSE);
		gtk_widget_set_sensitive(spin_refvol, FALSE);
		gtk_widget_set_sensitive(label_refvol, FALSE);
		gtk_widget_set_sensitive(spin_steepness, FALSE);
		gtk_widget_set_sensitive(label_steepness, FALSE);

		gtk_widget_set_sensitive(check_rva_use_averaging, FALSE);
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	draw_rva_diagram();
}


void
check_rva_use_averaging_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rva_use_averaging))) {
		rva_use_averaging_shadow = 1;
		gtk_widget_set_sensitive(optmenu_threshold, TRUE);
		gtk_widget_set_sensitive(label_threshold, TRUE);
		gtk_widget_set_sensitive(spin_linthresh, TRUE);
		gtk_widget_set_sensitive(label_linthresh, TRUE);
		gtk_widget_set_sensitive(spin_stdthresh, TRUE);
		gtk_widget_set_sensitive(label_stdthresh, TRUE);
	} else {
		rva_use_averaging_shadow = 0;
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}
}


void
changed_listening_env(GtkWidget * widget, gpointer * data) {

	rva_env_shadow = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_listening_env));

	switch (rva_env_shadow) {
	case 0: /* Audiophile */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 1.0f);
		break;
	case 1: /* Living room */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 0.7f);
		break;
	case 2: /* Office */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 0.4f);
		break;
	case 3: /* Noisy workshop */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 0.1f);
		break;
	default:
		fprintf(stderr, "programmer error: options.c/changed_listening_env(): "
			"invalid rva_env_shadow value.\nPlease report this to the programmers!\n");
		break;
	}
}


void
draw_rva_diagram(void) {

	GdkGC * gc;
	GdkColor fg_color;
	int i;
	int width = rva_viewport->allocation.width - 4;
	int height = rva_viewport->allocation.height - 4;
	int dw = width / 24;
	int dh = height / 24;
	int xoffs = (width - 24*dw) / 2 - 1;
	int yoffs = (height - 24*dh) / 2 - 1;
	float volx, voly;
	int px1, py1, px2, py2;


	gdk_draw_rectangle(rva_pixmap,
			   rva_drawing_area->style->black_gc,
			   TRUE,
			   0, 0,
			   rva_drawing_area->allocation.width,
			   rva_drawing_area->allocation.height);
	
	gc = gdk_gc_new(rva_pixmap);
	if (rva_is_enabled_shadow) {
		fg_color.red = 10000;
		fg_color.green = 10000;
		fg_color.blue = 10000;
	} else {
		fg_color.red = 5000;
		fg_color.green = 5000;
		fg_color.blue = 5000;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg_color);

	for (i = 0; i <= 24; i++) {
		gdk_draw_line(rva_pixmap, gc,
			      xoffs + i * dw, yoffs,
			      xoffs + i * dw, yoffs + 24 * dh);
	}

	for (i = 0; i <= 24; i++) {
		gdk_draw_line(rva_pixmap, gc,
			      xoffs, yoffs + i * dh,
			      xoffs + 24 * dw, yoffs + i * dh);
	}

	if (rva_is_enabled_shadow) {
		fg_color.red = 0;
		fg_color.green = 0;
		fg_color.blue = 65535;
	} else {
		fg_color.red = 0;
		fg_color.green = 0;
		fg_color.blue = 30000;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg_color);
	gdk_draw_line(rva_pixmap, gc, xoffs, yoffs + 24 * dh, xoffs + 24 * dw, yoffs);

	if (rva_is_enabled_shadow) {
		fg_color.red = 65535;
		fg_color.green = 0;
		fg_color.blue = 0;
	} else {
		fg_color.red = 30000;
		fg_color.green = 0;
		fg_color.blue = 0;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg_color);


	volx = -24.0f;
	voly = volx + (volx - rva_refvol_shadow) * (rva_steepness_shadow - 1.0f);
	px1 = xoffs;
	py1 = yoffs - (voly * dh);

	volx = 0.0f;
	voly = volx + (volx - rva_refvol_shadow) * (rva_steepness_shadow - 1.0f);
	px2 = xoffs + 24*dw;
	py2 = yoffs - (voly * dh);

	gdk_draw_line(rva_pixmap, gc, px1, py1, px2, py2);

	gdk_draw_drawable(rva_drawing_area->window,
			rva_drawing_area->style->fg_gc[GTK_WIDGET_STATE(rva_drawing_area)],
			rva_pixmap,
			0, 0, 0, 0,
			width, height);

	g_object_unref(gc);
}


void
refvol_changed(GtkWidget * widget, gpointer * data) {

	rva_refvol_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
	draw_rva_diagram();
}


void
steepness_changed(GtkWidget * widget, gpointer * data) {

	rva_steepness_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
	draw_rva_diagram();
}


static gint
rva_configure_event(GtkWidget * widget, GdkEventConfigure * event) {

	if (rva_pixmap)
		g_object_unref(rva_pixmap);

	rva_pixmap = gdk_pixmap_new(widget->window,
				    widget->allocation.width,
				    widget->allocation.height,
				    -1);
	gdk_draw_rectangle(rva_pixmap,
			   widget->style->black_gc,
			   TRUE,
			   0, 0,
			   widget->allocation.width,
			   widget->allocation.height);
	draw_rva_diagram();
	return TRUE;
}


static gint
rva_expose_event(GtkWidget * widget, GdkEventExpose * event) {

	gdk_draw_drawable(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			rva_pixmap,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	return FALSE;
}


void
changed_threshold(GtkWidget * widget, gpointer * data) {

	rva_use_linear_thresh_shadow =
	        gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_threshold));
}


void
linthresh_changed(GtkWidget * widget, gpointer * data) {

	rva_avg_linear_thresh_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
}


void
stdthresh_changed(GtkWidget * widget, gpointer * data) {

	rva_avg_stddev_thresh_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) / 100.0f;
}

void show_restart_info(void)
{
GtkWidget *info_dialog;

        info_dialog = gtk_message_dialog_new (GTK_WINDOW (options_window),
       					GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					_("You will need to restart Aqualung for changes to take effect."));

	gtk_widget_show (info_dialog);
        gtk_dialog_run(GTK_DIALOG(info_dialog));
        gtk_widget_destroy(info_dialog);

}

void playlist_font_select(GtkWidget *widget)
{
gchar *s;
GtkWidget *font_selector;
gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), playlist_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(playlist_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_pl_font), s);
		g_free (s);

	}

	gtk_widget_destroy (font_selector);

}

void browser_font_select(GtkWidget *widget)
{
gchar *s;
GtkWidget *font_selector;
gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), browser_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(browser_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_ms_font), s);
		g_free (s);

	}

	gtk_widget_destroy (font_selector);

}

void bigtimer_font_select(GtkWidget *widget)
{
gchar *s;
GtkWidget *font_selector;
gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), bigtimer_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(bigtimer_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_bt_font), s);
		g_free (s);

	}

	gtk_widget_destroy (font_selector);

}

void smalltimer_font_select(GtkWidget *widget)
{
gchar *s;
GtkWidget *font_selector;
gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), smalltimer_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(smalltimer_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_st_font), s);
		g_free (s);

	}

	gtk_widget_destroy (font_selector);

}

void  restart_active (GtkToggleButton *togglebutton, gpointer user_data)
{
        restart_flag = 1;
}

void set_sensitive_part(void) {

        if(override_skin_settings) {

                gtk_widget_set_sensitive(entry_ms_font, TRUE);
                gtk_widget_set_sensitive(entry_pl_font, TRUE);
                gtk_widget_set_sensitive(entry_bt_font, TRUE);
                gtk_widget_set_sensitive(entry_st_font, TRUE);
                gtk_widget_set_sensitive(button_ms_font, TRUE);
                gtk_widget_set_sensitive(button_pl_font, TRUE);
                gtk_widget_set_sensitive(button_bt_font, TRUE);
                gtk_widget_set_sensitive(button_st_font, TRUE);
                gtk_widget_set_sensitive(color_picker, TRUE);

        } else {

                gtk_widget_set_sensitive(entry_ms_font, FALSE);
                gtk_widget_set_sensitive(entry_pl_font, FALSE);
                gtk_widget_set_sensitive(entry_bt_font, FALSE);
                gtk_widget_set_sensitive(entry_st_font, FALSE);
                gtk_widget_set_sensitive(button_ms_font, FALSE);
                gtk_widget_set_sensitive(button_pl_font, FALSE);
                gtk_widget_set_sensitive(button_bt_font, FALSE);
                gtk_widget_set_sensitive(button_st_font, FALSE);
                gtk_widget_set_sensitive(color_picker, FALSE);

        }
}

void  cb_toggle_override_skin (GtkToggleButton *togglebutton, gpointer user_data)
{
        override_skin_settings = override_skin_settings ? 0 : 1;
        set_sensitive_part();
}

void color_selected (GtkColorButton *widget, gpointer user_data)
{
GdkColor c;
gchar str[MAX_COLORNAME_LEN];

        gtk_color_button_get_color(widget, &c);
        sprintf(str, "#%02X%02X%02X", c.red * 256 / 65536, c.green * 256 / 65536, c.blue * 256 / 65536);

        strncpy(activesong_color, str, MAX_COLORNAME_LEN);
}

void
create_options_window(void) {

	GtkWidget * vbox;
	GtkWidget * notebook;

	GtkWidget * label_general;
	GtkWidget * vbox_general;
	GtkWidget * frame_title;
	GtkWidget * frame_param;
	GtkWidget * frame_misc;
	GtkWidget * vbox_title;
	GtkWidget * vbox_param;
	GtkWidget * vbox_misc;
	GtkWidget * vbox_appearance;
	GtkWidget * label_title;
	GtkWidget * label_param;
	GtkWidget * label_appearance;

	GtkWidget * label_pl;
	GtkWidget * vbox_pl;
	GtkWidget * frame_plistcol;
	GtkWidget * vbox_plistcol;
	GtkWidget * label_plistcol;
	GtkWidget * viewport;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	GtkWidget * plistcol_list;
	GtkWidget * label;

	GtkWidget * label_dsp;
	GtkWidget * vbox_dsp;
	GtkWidget * frame_ladspa;
	GtkWidget * frame_src;
	GtkWidget * frame_fonts;
	GtkWidget * frame_colors;
	GtkWidget * vbox_ladspa;
	GtkWidget * vbox_src;
	GtkWidget * vbox_fonts;
	GtkWidget * vbox_colors;

	GtkWidget * label_rva;
	GtkWidget * vbox_rva;
	GtkWidget * table_rva;
	GtkWidget * hbox_listening_env;
	GtkWidget * hbox_refvol;
	GtkWidget * hbox_steepness;
	GtkWidget * table_avg;
	GtkWidget * hbox_threshold;
	GtkWidget * hbox_linthresh;
	GtkWidget * hbox_stdthresh;

	GtkWidget * label_meta;
	GtkWidget * vbox_meta;

        GtkSizeGroup * label_size;

	GtkWidget * hbox;
	GtkWidget * hbox_s;
	GtkWidget * ok_btn;
	GtkWidget * cancel_btn;

	int status;
	int i;

        restart_flag = 0;

	options_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(options_window), GTK_WINDOW(main_window));
        gtk_window_set_title(GTK_WINDOW(options_window), _("Settings"));
	gtk_window_set_position(GTK_WINDOW(options_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(options_window), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(options_window), 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(options_window), vbox);

	notebook = gtk_notebook_new();
	gtk_container_add(GTK_CONTAINER(vbox), notebook);

        label_size = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	/* "General" notebook page */

	label_general = gtk_label_new(_("General"));
	vbox_general = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_general), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_general, label_general);

	frame_title = gtk_frame_new(_("Title format"));
	gtk_box_pack_start(GTK_BOX(vbox_general), frame_title, FALSE, TRUE, 5);

	vbox_title = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_title), 10);
	gtk_container_add(GTK_CONTAINER(frame_title), vbox_title);

	label_title = gtk_label_new(_("\nThe template string you enter here will be used to\n\
construct a single title line from an Artist, a Record\n\
and a Track name. These are denoted by %a, %r and %t,\n\
respectively. Everything else you enter here will be\n\
literally copied into the resulting string.\n"));
	gtk_box_pack_start(GTK_BOX(vbox_title), label_title, TRUE, TRUE, 0);

	entry_title = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_title), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_title), title_format);
	gtk_box_pack_start(GTK_BOX(vbox_title), entry_title, TRUE, TRUE, 0);


	frame_param = gtk_frame_new(_("Implicit command line"));
	gtk_box_pack_start(GTK_BOX(vbox_general), frame_param, FALSE, TRUE, 5);

	vbox_param = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_param), 10);
	gtk_container_add(GTK_CONTAINER(frame_param), vbox_param);

	label_param = gtk_label_new(_("\nThe string you enter here will be parsed as a command\n\
line before parsing the actual command line parameters.\n\
What you enter here will act as a default setting and may\n\
or may not be overrided from the 'real' command line.\n\
Example: enter '-o alsa -R' below to use ALSA output\n\
running realtime as a default.\n"));
	gtk_box_pack_start(GTK_BOX(vbox_param), label_param, TRUE, TRUE, 0);

	entry_param = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_param), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_param), default_param);
	gtk_box_pack_start(GTK_BOX(vbox_param), entry_param, TRUE, TRUE, 0);


	frame_misc = gtk_frame_new(_("Miscellaneous"));
	gtk_box_pack_start(GTK_BOX(vbox_general), frame_misc, FALSE, TRUE, 5);

	vbox_misc = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_misc), 10);
	gtk_container_add(GTK_CONTAINER(frame_misc), vbox_misc);

	check_enable_tooltips =
		gtk_check_button_new_with_label(_("Enable tooltips"));

        gtk_widget_set_name(check_enable_tooltips, "check_on_notebook");
	if (enable_tooltips) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_tooltips), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_enable_tooltips, FALSE, FALSE, 0);

	check_hide_comment_pane =
		gtk_check_button_new_with_label(_("Hide the Music Store comment pane"));
	gtk_widget_set_name(check_hide_comment_pane, "check_on_notebook");
	if (hide_comment_pane_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_hide_comment_pane), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_hide_comment_pane, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_hide_comment_pane), "toggled",
						G_CALLBACK (restart_active), NULL);

	check_buttons_at_the_bottom =
		gtk_check_button_new_with_label(_("Put control buttons at the bottom of playlist"));
	gtk_widget_set_name(check_buttons_at_the_bottom, "check_on_notebook");
	if (buttons_at_the_bottom) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_buttons_at_the_bottom), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_buttons_at_the_bottom, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_buttons_at_the_bottom), "toggled",
						G_CALLBACK (restart_active), NULL);

        /* "Playlist" notebook page */

	label_pl = gtk_label_new(_("Playlist"));
	vbox_pl = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_pl), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_pl, label_pl);
	
        check_playlist_is_embedded =
		gtk_check_button_new_with_label(_("Embed playlist into main window"));
	gtk_widget_set_name(check_playlist_is_embedded, "check_on_notebook");
	if (playlist_is_embedded_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_is_embedded), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_playlist_is_embedded, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_playlist_is_embedded), "toggled",
						G_CALLBACK (restart_active), NULL);
	check_autoplsave =
	    gtk_check_button_new_with_label(_("Save and restore the playlist on exit/startup"));
	gtk_widget_set_name(check_autoplsave, "check_on_notebook");
	if (auto_save_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_autoplsave), TRUE);
	}
	auto_save_playlist_shadow = auto_save_playlist;
	g_signal_connect(G_OBJECT(check_autoplsave), "toggled",
			 G_CALLBACK(autoplsave_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_autoplsave, FALSE, TRUE, 3);

	check_enable_playlist_statusbar =
		gtk_check_button_new_with_label(_("Enable statusbar in playlist"));
	gtk_widget_set_name(check_enable_playlist_statusbar, "check_on_notebook");
	if (enable_playlist_statusbar_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_playlist_statusbar), TRUE);
	}
	enable_playlist_statusbar_shadow = enable_playlist_statusbar;
	g_signal_connect(G_OBJECT(check_enable_playlist_statusbar), "toggled",
			 G_CALLBACK(restart_active), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_enable_playlist_statusbar, FALSE, TRUE, 3);

	check_show_rva_in_playlist =
		gtk_check_button_new_with_label(_("Show RVA values in playlist"));
	gtk_widget_set_name(check_show_rva_in_playlist, "check_on_notebook");
	if (show_rva_in_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_rva_in_playlist), TRUE);
	}
	show_rva_in_playlist_shadow = show_rva_in_playlist;
	g_signal_connect(G_OBJECT(check_show_rva_in_playlist), "toggled",
			 G_CALLBACK(check_show_rva_in_playlist_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_rva_in_playlist, FALSE, TRUE, 3);

	check_show_length_in_playlist =
		gtk_check_button_new_with_label(_("Show track lengths in playlist"));
	gtk_widget_set_name(check_show_length_in_playlist, "check_on_notebook");
	if (show_length_in_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_length_in_playlist), TRUE);
	}
	show_length_in_playlist_shadow = show_length_in_playlist;
	g_signal_connect(G_OBJECT(check_show_length_in_playlist), "toggled",
			 G_CALLBACK(check_show_length_in_playlist_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_length_in_playlist, FALSE, TRUE, 3);
	
	check_show_track_name_using_bold =
		gtk_check_button_new_with_label(_("Show track name using bold font"));
	gtk_widget_set_name(check_show_track_name_using_bold, "check_on_notebook");
	if (show_track_name_using_bold) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_track_name_using_bold), TRUE);
	}
	show_track_name_using_bold_shadow = show_track_name_using_bold;
	g_signal_connect(G_OBJECT(check_show_track_name_using_bold), "toggled",
			 G_CALLBACK(check_show_track_name_using_bold_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_track_name_using_bold, FALSE, TRUE, 3);


        frame_plistcol = gtk_frame_new(_("Playlist column order"));
        gtk_box_pack_start(GTK_BOX(vbox_pl), frame_plistcol, FALSE, TRUE, 5);

        vbox_plistcol = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_plistcol), 8);
        gtk_container_add(GTK_CONTAINER(frame_plistcol), vbox_plistcol);
	
	label_plistcol = gtk_label_new(_("Drag and drop entries in the list below \n\
to set the column order in the Playlist."));
        gtk_box_pack_start(GTK_BOX(vbox_plistcol), label_plistcol, FALSE, TRUE, 5);


	plistcol_store = gtk_list_store_new(2,
					    G_TYPE_STRING,   /* Column name */
					    G_TYPE_STRING);  /* Column index */

        plistcol_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(plistcol_store));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(plistcol_list), FALSE);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Column"),
							  renderer,
							  "text", 0,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(plistcol_list), column);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(plistcol_list), TRUE);

        viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox_plistcol), viewport, TRUE, TRUE, 5);

        gtk_container_add(GTK_CONTAINER(viewport), plistcol_list);

	for (i = 0; i < 3; i++) {
		switch (plcol_idx[i]) {
		case 0:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("Track titles"), 1, "0", -1);
			break;
		case 1:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("RVA values"), 1, "1", -1);
			break;
		case 2:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("Track lengths"), 1, "2", -1);
			break;
		}
	}


	/* "DSP" notebook page */

	label_dsp = gtk_label_new(_("DSP"));
	vbox_dsp = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_dsp), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_dsp, label_dsp);

	frame_ladspa = gtk_frame_new(_("LADSPA plugin processing"));
	gtk_box_pack_start(GTK_BOX(vbox_dsp), frame_ladspa, FALSE, TRUE, 5);

	vbox_ladspa = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_ladspa), 10);
	gtk_container_add(GTK_CONTAINER(frame_ladspa), vbox_ladspa);

        optmenu_ladspa = gtk_combo_box_new_text (); 
        gtk_box_pack_start(GTK_BOX(vbox_ladspa), optmenu_ladspa, TRUE, TRUE, 0);

	{
	        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_ladspa), _("Pre Fader (before Volume & Balance)"));
        	gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_ladspa), _("Post Fader (after Volume & Balance)"));

	}
	pthread_mutex_lock(&output_thread_lock);
	status = ladspa_is_postfader;
	pthread_mutex_unlock(&output_thread_lock);
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_ladspa), status);
        g_signal_connect(optmenu_ladspa, "changed", G_CALLBACK(changed_ladspa_prepost), NULL);


	frame_src = gtk_frame_new(_("Sample Rate Converter type"));
	gtk_box_pack_start(GTK_BOX(vbox_dsp), frame_src, FALSE, TRUE, 5);

	vbox_src = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_src), 10);
	gtk_container_add(GTK_CONTAINER(frame_src), vbox_src);

	label_src = gtk_label_new("");

#ifdef HAVE_SRC
	optmenu_src = gtk_combo_box_new_text ();
	gtk_box_pack_start(GTK_BOX(vbox_src), optmenu_src, TRUE, TRUE, 0);

	{
		int i = 0;

		while (src_get_name(i)) {
                        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_src), src_get_name(i));
			++i;
		}

	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_src), src_type);
	g_signal_connect(optmenu_src, "changed", G_CALLBACK(changed_src_type), NULL);

	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(src_type));
#else
	gtk_label_set_text(GTK_LABEL(label_src),
			   _("Aqualung is compiled without Sample Rate Converter support.\n\
See the About box and the documentation for details."));

#endif /* HAVE_SRC */

	gtk_box_pack_start(GTK_BOX(vbox_src), label_src, TRUE, TRUE, 0);


	/* "Playback RVA" notebook page */

	label_rva = gtk_label_new(_("Playback RVA"));
	vbox_rva = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_rva), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_rva, label_rva);

	check_rva_is_enabled = gtk_check_button_new_with_label(_("Enable playback RVA"));
	gtk_widget_set_name(check_rva_is_enabled, "check_on_notebook");
	if (rva_is_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rva_is_enabled), TRUE);
	}
	rva_is_enabled_shadow = rva_is_enabled;
	g_signal_connect(G_OBJECT(check_rva_is_enabled), "toggled",
			 G_CALLBACK(check_rva_is_enabled_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_rva), check_rva_is_enabled, FALSE, TRUE, 5);


	table_rva = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_rva), table_rva, TRUE, TRUE, 3);

	rva_viewport = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_size_request(rva_viewport, 244, 244);
        gtk_table_attach(GTK_TABLE(table_rva), rva_viewport, 0, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 2);
	
	rva_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(GTK_WIDGET(rva_drawing_area), 240, 240);
	gtk_container_add(GTK_CONTAINER(rva_viewport), rva_drawing_area);
	
	g_signal_connect(G_OBJECT(rva_drawing_area), "configure_event",
			 G_CALLBACK(rva_configure_event), NULL);
	g_signal_connect(G_OBJECT(rva_drawing_area), "expose_event",
			 G_CALLBACK(rva_expose_event), NULL);
	
        hbox_listening_env = gtk_hbox_new(FALSE, 0);
        label_listening_env = gtk_label_new(_("Listening environment:"));
        gtk_box_pack_start(GTK_BOX(hbox_listening_env), label_listening_env, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox_listening_env, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

	optmenu_listening_env = gtk_combo_box_new_text ();
        gtk_table_attach(GTK_TABLE(table_rva), optmenu_listening_env, 1, 2, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 2);

	{

                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Audiophile"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Living room"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Office"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Noisy workshop"));

	}

	rva_env_shadow = rva_env;
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_listening_env), rva_env);
	g_signal_connect(optmenu_listening_env, "changed", G_CALLBACK(changed_listening_env), NULL);

        hbox_refvol = gtk_hbox_new(FALSE, 0);
        label_refvol = gtk_label_new(_("Reference volume [dBFS] :"));
        gtk_box_pack_start(GTK_BOX(hbox_refvol), label_refvol, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox_refvol, 0, 1, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_refvol_shadow = rva_refvol;
        adj_refvol = gtk_adjustment_new(rva_refvol, -24.0f, 0.0f, 0.1f, 1.0f, 0.0f);
        spin_refvol = gtk_spin_button_new(GTK_ADJUSTMENT(adj_refvol), 0.1, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_refvol), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_refvol), FALSE);
	g_signal_connect(G_OBJECT(adj_refvol), "value_changed",
			 G_CALLBACK(refvol_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_refvol, 1, 2, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);

        hbox_steepness = gtk_hbox_new(FALSE, 0);
        label_steepness = gtk_label_new(_("Steepness [dB/dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox_steepness), label_steepness, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox_steepness, 0, 1, 3, 4,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_steepness_shadow = rva_steepness;
        adj_steepness = gtk_adjustment_new(rva_steepness, 0.0f, 1.0f, 0.01f, 0.1f, 0.0f);
        spin_steepness = gtk_spin_button_new(GTK_ADJUSTMENT(adj_steepness), 0.02, 2);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_steepness), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_steepness), FALSE);
	g_signal_connect(G_OBJECT(adj_steepness), "value_changed",
			 G_CALLBACK(steepness_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_steepness, 1, 2, 3, 4,
                         GTK_FILL, GTK_FILL, 5, 2);



	check_rva_use_averaging =
	    gtk_check_button_new_with_label(_("Apply averaged RVA to tracks of the same record"));
	gtk_widget_set_name(check_rva_use_averaging, "check_on_notebook");
	if (rva_use_averaging) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rva_use_averaging), TRUE);
	}
	rva_use_averaging_shadow = rva_use_averaging;
	g_signal_connect(G_OBJECT(check_rva_use_averaging), "toggled",
			 G_CALLBACK(check_rva_use_averaging_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_rva), check_rva_use_averaging, FALSE, TRUE, 5);


	table_avg = gtk_table_new(3, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_rva), table_avg, FALSE, TRUE, 3);

        hbox_threshold = gtk_hbox_new(FALSE, 0);
        label_threshold = gtk_label_new(_("Drop statistical aberrations based on"));
        gtk_box_pack_start(GTK_BOX(hbox_threshold), label_threshold, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_avg), hbox_threshold, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 2);

	optmenu_threshold = gtk_combo_box_new_text ();
        gtk_table_attach(GTK_TABLE(table_avg), optmenu_threshold, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 2);

	{

                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_threshold), _("% of standard deviation"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_threshold), _("Linear threshold [dB]"));

	}

	rva_use_linear_thresh_shadow = rva_use_linear_thresh;
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_threshold), rva_use_linear_thresh);
	g_signal_connect(optmenu_threshold, "changed", G_CALLBACK(changed_threshold), NULL);

        hbox_linthresh = gtk_hbox_new(FALSE, 0);
        label_linthresh = gtk_label_new(_("Linear threshold [dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox_linthresh), label_linthresh, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_avg), hbox_linthresh, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_avg_linear_thresh_shadow = rva_avg_linear_thresh;
        adj_linthresh = gtk_adjustment_new(rva_avg_linear_thresh, 0.0f, 60.0f, 0.1f, 1.0f, 0.0f);
        spin_linthresh = gtk_spin_button_new(GTK_ADJUSTMENT(adj_linthresh), 0.1, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_linthresh), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_linthresh), FALSE);
	g_signal_connect(G_OBJECT(adj_linthresh), "value_changed",
			 G_CALLBACK(linthresh_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_avg), spin_linthresh, 1, 2, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

        hbox_stdthresh = gtk_hbox_new(FALSE, 0);
        label_stdthresh = gtk_label_new(_("% of standard deviation :"));
        gtk_box_pack_start(GTK_BOX(hbox_stdthresh), label_stdthresh, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_avg), hbox_stdthresh, 0, 1, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_avg_stddev_thresh_shadow = rva_avg_stddev_thresh;
        adj_stdthresh = gtk_adjustment_new(rva_avg_stddev_thresh * 100.0f,
					   0.0f, 500.0f, 1.0f, 10.0f, 0.0f);
        spin_stdthresh = gtk_spin_button_new(GTK_ADJUSTMENT(adj_stdthresh), 0.5, 0);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_stdthresh), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_stdthresh), FALSE);
	g_signal_connect(G_OBJECT(adj_stdthresh), "value_changed",
			 G_CALLBACK(stdthresh_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_avg), spin_stdthresh, 1, 2, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);


	if (!rva_use_averaging_shadow) {
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	if (!rva_is_enabled_shadow) {
		gtk_widget_set_sensitive(optmenu_listening_env, FALSE);
		gtk_widget_set_sensitive(label_listening_env, FALSE);
		gtk_widget_set_sensitive(spin_refvol, FALSE);
		gtk_widget_set_sensitive(label_refvol, FALSE);
		gtk_widget_set_sensitive(spin_steepness, FALSE);
		gtk_widget_set_sensitive(label_steepness, FALSE);
		gtk_widget_set_sensitive(check_rva_use_averaging, FALSE);
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	/* "Metadata" notebook page */

	label_meta = gtk_label_new(_("Metadata"));
	vbox_meta = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_meta), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_meta, label_meta);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("When adding to playlist, use file metadata (if available) "
				"instead of\ninformation from the Music Store for:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_meta_artist = gtk_check_button_new_with_label(_("Artist name"));
	gtk_widget_set_name(check_auto_use_meta_artist, "check_on_notebook");
	if (auto_use_meta_artist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_artist), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_meta_artist, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_meta_record = gtk_check_button_new_with_label(_("Record name"));
	gtk_widget_set_name(check_auto_use_meta_record, "check_on_notebook");
	if (auto_use_meta_record) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_record), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_meta_record, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_meta_track = gtk_check_button_new_with_label(_("Track name"));
	gtk_widget_set_name(check_auto_use_meta_track, "check_on_notebook");
	if (auto_use_meta_track) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_track), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_meta_track, FALSE, TRUE, 20);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("When adding external files to playlist, use file metadata "
				"(if available) for:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_ext_meta_artist = gtk_check_button_new_with_label(_("Artist name"));
	gtk_widget_set_name(check_auto_use_ext_meta_artist, "check_on_notebook");
	if (auto_use_ext_meta_artist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_artist), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_ext_meta_artist, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_ext_meta_record = gtk_check_button_new_with_label(_("Record name"));
	gtk_widget_set_name(check_auto_use_ext_meta_record, "check_on_notebook");
	if (auto_use_ext_meta_record) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_record), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_ext_meta_record, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_ext_meta_track = gtk_check_button_new_with_label(_("Track name"));
	gtk_widget_set_name(check_auto_use_ext_meta_track, "check_on_notebook");
	if (auto_use_ext_meta_track) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_track), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_ext_meta_track, FALSE, TRUE, 20);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Replaygain tag to use in Ogg Vorbis and FLAC files: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	optmenu_replaygain = gtk_combo_box_new_text ();
        gtk_box_pack_start(GTK_BOX(hbox), optmenu_replaygain, FALSE, FALSE, 20);

	{

                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_replaygain), _("Replaygain_track_gain"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_replaygain), _("Replaygain_album_gain"));

	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_replaygain), replaygain_tag_to_use);


        /* Appearance notebook page */

        override_past_state = override_skin_settings;

	label_appearance = gtk_label_new(_("Appearance"));
	vbox_appearance = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_appearance), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_appearance, label_appearance);

        check_override_skin =
		gtk_check_button_new_with_label(_("Override skin settings"));

        gtk_widget_set_name(check_override_skin, "check_on_notebook");
	if (override_skin_settings) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_override_skin), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_appearance), check_override_skin, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_override_skin), "toggled",
						G_CALLBACK (cb_toggle_override_skin), NULL);

	frame_fonts = gtk_frame_new(_("Fonts"));
	gtk_box_pack_start(GTK_BOX(vbox_appearance), frame_fonts, FALSE, TRUE, 5);

	vbox_fonts = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_fonts), 10);
	gtk_container_add(GTK_CONTAINER(frame_fonts), vbox_fonts);

        /* playlist font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, FALSE, 3);

	label = gtk_label_new(_("Playlist font: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_pl_font = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(hbox), entry_pl_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_pl_font), FALSE);

        if(strlen(playlist_font)) {

                gtk_entry_set_text(GTK_ENTRY(entry_pl_font), playlist_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_pl_font), DEFAULT_FONT_NAME);
                strcpy(playlist_font, DEFAULT_FONT_NAME);
        }

        button_pl_font =  gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_pl_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_pl_font), "clicked",
						G_CALLBACK (playlist_font_select), NULL);

        /* music store font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Music Store font: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_ms_font = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(hbox), entry_ms_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_ms_font), FALSE);

        if(strlen(browser_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_ms_font), browser_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_ms_font), DEFAULT_FONT_NAME);
                strcpy(browser_font, DEFAULT_FONT_NAME);

        }

        button_ms_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_ms_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_ms_font), "clicked",
						G_CALLBACK (browser_font_select), NULL);


        /* big timer font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Big timer font: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_bt_font = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(hbox), entry_bt_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_bt_font), FALSE);

        if(strlen(bigtimer_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_bt_font), bigtimer_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_bt_font), DEFAULT_FONT_NAME);
                strcpy(bigtimer_font, DEFAULT_FONT_NAME);

        }

        button_bt_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_bt_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_bt_font), "clicked",
						G_CALLBACK (bigtimer_font_select), NULL);

        /* small timer font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Small timer font: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_st_font = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(hbox), entry_st_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_st_font), FALSE);

        if(strlen(smalltimer_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_st_font), smalltimer_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_st_font), DEFAULT_FONT_NAME);
                strcpy(smalltimer_font, DEFAULT_FONT_NAME);

        }

        button_st_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_st_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_st_font), "clicked",
						G_CALLBACK (smalltimer_font_select), NULL);


        /* colors */

	frame_colors = gtk_frame_new(_("Colors"));
	gtk_box_pack_start(GTK_BOX(vbox_appearance), frame_colors, FALSE, TRUE, 5);

	vbox_colors = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_colors), 10);
	gtk_container_add(GTK_CONTAINER(frame_colors), vbox_colors);

        /* active song */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_colors), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Active song in playlist: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	hbox_s = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), hbox_s, TRUE, TRUE, 3);

        if(gdk_color_parse(activesong_color, &color) == FALSE) {

                color.red = play_list->style->fg[SELECTED].red; 
                color.green = play_list->style->fg[SELECTED].green; 
                color.blue = play_list->style->fg[SELECTED].blue; 
                color.pixel = (gulong)((color.red & 0xff00)*256 + (color.green & 0xff00) + (color.blue & 0xff00)/256); 
        }

        color_picker = gtk_color_button_new_with_color (&color);
        gtk_widget_set_size_request(GTK_WIDGET(color_picker), 70, -1);
        gtk_box_pack_start(GTK_BOX(hbox), color_picker, FALSE, TRUE, 3);
	g_signal_connect (G_OBJECT (color_picker), "color-set",
						G_CALLBACK (color_selected), NULL);


        set_sensitive_part();

        /* end of notebook */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

        cancel_btn = gtk_button_new_from_stock (GTK_STOCK_CANCEL); 
	gtk_widget_set_size_request(cancel_btn, 70, 30);
	g_signal_connect(cancel_btn, "clicked", G_CALLBACK(cancel), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), cancel_btn, FALSE, FALSE, 6);

        ok_btn = gtk_button_new_from_stock (GTK_STOCK_OK); 
	gtk_widget_set_size_request(ok_btn, 70, 30);
	g_signal_connect(ok_btn, "clicked", G_CALLBACK(ok), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), ok_btn, FALSE, FALSE, 6);

	gtk_widget_show_all(options_window);
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

