/*
 *  Copyright (C) 2024 Matthias Krinninger
 *  E-mail: matrkin@protonmail.com
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any
 *  later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along with this program; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyversion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>
#include <stdio.h>
#include <stdbool.h>
#include "app/data-browser.h"
#include "app/wait.h"
#include "config.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "libdraw/gwyselection.h"
#include "libgwyddion/gwycontainer.h"
#include "libgwydgets/gwydataviewlayer.h"
#include "libgwydgets/gwypixmaplayer.h"
#include "libgwydgets/gwyvectorlayer.h"
#include "libgwymodule/gwymodule-file.h"
#include "libprocess/datafield.h"
#include "libprocess/gwyprocessenums.h"

#include <dirent.h>

#define MOD_NAME PACKAGE_NAME

#define RUN_MODE GWY_RUN_IMMEDIATE


struct DriftCorrectionData;
typedef struct DriftCorrectionData DriftCorrectionData;

struct SelectedImage;
typedef struct SelectedImage SelectedImage;

static gboolean   module_register   (void);
static void       level_all         (GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name);
static void       focus_main_window (GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name);
static void       level_plane       (GwyDataField* data_field);

static void       container_overview(GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name);
static gboolean   present_if_exists (const gchar *title);
static GtkWidget* create_iconview   (GwyContainer *data);
static gboolean   on_icon_dbl_click (GtkIconView* icon_view, GtkTreePath* path);

static void       folder_overview   (GwyContainer* data, GwyRunType run, G_GNUC_UNUSED const gchar *name);
static void       dirname           (const char* path, char* dir);
static void       basename          (const char* path, char* base);
static bool       endswith          (const char* str, const char* suffix);
static char*      concat_path       (const char *dir, const char *filename, char *fullpath);

static void       drift_correction  (GwyContainer* data, GwyRunType run, G_GNUC_UNUSED const gchar *name);
static void       setup_dc_data(GwyContainer* container, DriftCorrectionData* drift_correction_data);
static void       dc_data_append_selected_images(GtkIconView* icon_view, GtkTreePath* tree_path, DriftCorrectionData* dc_data);
static void       on_selection_finish(GwySelection* selection, DriftCorrectionData* drift_correction_data);
static void       on_prev_btn_click(GtkButton* select_btn, DriftCorrectionData* drift_correction_data);
static void       on_next_btn_click(GtkButton* select_btn, DriftCorrectionData* drift_correction_data);
static void       on_run_btn_click(GtkButton* run_btn, DriftCorrectionData* drift_correction_data);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "Level all images in the current container",
    "matkrin <matkrin@protonmail.com>",
    PACKAGE_VERSION,
    "Matthias Krinninger",
    "2024",
};

/* This is the ONLY exported symbol.  The argument is the module info.  NO semicolon after. */
GWY_MODULE_QUERY(module_info);

/*
 * Module registeration function.
 *
 * It is called at Gwyddion startup and registers one or more functions.
 */
static gboolean module_register(void) {
    gwy_process_func_register(
        "level_all",
        (GwyProcessFunc)&level_all,
        N_("/Zzz/Level All"),
        NULL,
        GWY_RUN_IMMEDIATE,
        GWY_MENU_FLAG_DATA,
        N_("Level all data in container.")
    );

    gwy_process_func_register(
        "container_overview",
        (GwyProcessFunc)&container_overview,
        N_("/Zzz/Container Overview"),
        NULL,
        GWY_RUN_INTERACTIVE,
        GWY_MENU_FLAG_DATA,
        N_("Show an overview of the current container")
    );

    gwy_process_func_register(
        "focus_main_window",
        (GwyProcessFunc)&focus_main_window,
        N_("/Zzz/Focus Main Window"),
        NULL,
        GWY_RUN_IMMEDIATE,
        GWY_MENU_FLAG_DATA,
        N_("Focus the main window")
    );

    gwy_process_func_register(
        "folder_overview",
        (GwyProcessFunc)&folder_overview,
        N_("/Zzz/Folder Overview"),
        NULL,
        GWY_RUN_INTERACTIVE,
        GWY_MENU_FLAG_DATA,
        N_("Show an overview of the current folder")
    );

    gwy_process_func_register(
        "drift_correction",
        (GwyProcessFunc)&drift_correction,
        N_("/Zzz/Drift correction"),
        NULL,
        GWY_RUN_INTERACTIVE,
        GWY_MENU_FLAG_DATA,
        N_("Drift correction")
    );


    return TRUE;
}


/*
 * The main function.
 *
 * This is what gwy_process_func_register() registers and what the program calls when the user invokes the function
 * from the menu or toolbox.
 */
static void level_all(GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name) {
    gint* data_ids = gwy_app_data_browser_get_data_ids(data);

    for (int i = 0; data_ids[i] != -1; i++) {
        GQuark key = gwy_app_get_data_key_for_id(data_ids[i]);
        GwyDataField* data_field = gwy_container_get_object(data, key);
        level_plane(data_field);
        gwy_data_field_data_changed(data_field);
    }

    g_free(data_ids);
}

static void level_plane(GwyDataField* data_field) {
    gdouble a, bx, by;

    gwy_data_field_fit_plane(data_field, &a, &bx, &by);
    gwy_data_field_plane_level(data_field, a, bx, by);
}

/* This function only exists to be able to create a keyboard shortcut for focusing the main menu */
static void focus_main_window(GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name) {
    GtkWindow* main_window = GTK_WINDOW(gwy_app_main_window_get());
    gtk_window_present(main_window);
}

typedef enum {
    IMG_ID_COL = 0,
    TITLE_COL = 1,
    THUMBNAIL_COL = 2,
    CONTAINER_ID_COL = 3,
    N_COLS = 4,
} StoreColumns;

static void container_overview(GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name) {
    const gchar* filename = gwy_file_get_filename_sys(data);

    if (present_if_exists(filename)) {
        return;
    }

    GtkWidget* main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), filename);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1350, 750);
    // gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);
    gwy_app_wait_start(GTK_WINDOW(main_window), "Creating Overview");

    GtkWidget* icon_view = create_iconview(data);
    g_signal_connect(icon_view, "item-activated", G_CALLBACK(on_icon_dbl_click), NULL);

    GtkWidget* scroll_area = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_area), icon_view);

    GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    gtk_box_pack_start(
        GTK_BOX(vbox),
        scroll_area,
        TRUE,
        TRUE,
        1
    );

    gwy_app_wait_finish();
    gwy_app_data_browser_set_keep_invisible(data, TRUE);
    gtk_widget_show_all(main_window);
}

static gboolean present_if_exists(const gchar *title) {
    GList *windows = gtk_window_list_toplevels();
    GList *iter;

    for (iter = windows; iter != NULL; iter = g_list_next(iter)) {
        if (GTK_IS_WINDOW(iter->data)) { // Check if it's a window
            GtkWidget *window = GTK_WIDGET(iter->data);
            const gchar *existing_title = gtk_window_get_title(GTK_WINDOW(window));

            // Compare the titles
            if (g_strcmp0(existing_title, title) == 0) {
                gtk_window_present(GTK_WINDOW(window));
                g_list_free(windows);
                return TRUE;
            }
        }
    }

    g_list_free(windows);
    return FALSE;
}

static GtkWidget* create_iconview(GwyContainer *data) {
    gint* data_ids = gwy_app_data_browser_get_data_ids(data);

    GtkListStore* list_store = gtk_list_store_new(N_COLS, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_INT);
    GtkTreeIter iter;

    for (int i = 0; data_ids[i] != -1; i++) {
        gint img_id = data_ids[i];
        GQuark key = gwy_app_get_data_key_for_id(img_id);
        GwyDataField* data_field = gwy_container_get_object(data, key);
        level_plane(data_field);
        // gwy_data_field_data_changed(data_field);
        GdkPixbuf* thumbnail = gwy_app_get_channel_thumbnail(data, img_id, 200, 200);
        char* data_field_title = gwy_app_get_data_field_title(data, img_id);

        int title_length = snprintf(NULL, 0, "%s", /*filename,*/ data_field_title);
        char* title = malloc(title_length + 1);
        snprintf(title, title_length + 1, "%s", /*filename,*/ data_field_title);

        int id_length = snprintf(NULL, 0, "%i", img_id);
        char* id_str = malloc(id_length + 1);
        snprintf(id_str, id_length + 1, "%d", img_id);

        gint container_id = gwy_app_data_browser_get_number(data);

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           IMG_ID_COL, img_id,
                           TITLE_COL, title,
                           THUMBNAIL_COL, thumbnail,
                           CONTAINER_ID_COL, container_id,
                           -1);
    }

    GtkWidget* icon_view = gtk_icon_view_new();
    gtk_icon_view_set_model(GTK_ICON_VIEW(icon_view), GTK_TREE_MODEL(list_store));
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(icon_view), 1);
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(icon_view), 2);

    return icon_view;
}

static gboolean on_icon_dbl_click(GtkIconView* icon_view, GtkTreePath* tree_path) {
    GtkTreeIter iter;
    GtkListStore *store;
    gint img_id;
    gint container_id;

    // Get the associated list store
    store = GTK_LIST_STORE(gtk_icon_view_get_model(icon_view));
    // Convert the path to an iter
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, tree_path);
    // Get the value of the first column (assuming it's an integer)
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, IMG_ID_COL, &img_id, -1);
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, CONTAINER_ID_COL, &container_id, -1);

    g_print("Activated item: %d\n", img_id);

    int vis_len = snprintf(NULL, 0, "/%i/data/visible", img_id);
    char* visible_ident = malloc(vis_len + 1);
    snprintf(visible_ident, vis_len + 1, "/%i/data/visible", img_id);

    GwyContainer* container_data = gwy_app_data_browser_get(container_id);
    gboolean is_visible = gwy_container_get_boolean_by_name(container_data, visible_ident);
    if (is_visible) {
        GtkWindow* img_window = gwy_app_find_window_for_channel(container_data, img_id);
        gtk_window_present(img_window);
    } else {
        gwy_container_set_boolean_by_name(container_data, visible_ident, TRUE);
    }

    return TRUE;
}

static void folder_overview(GwyContainer *data, GwyRunType run, G_GNUC_UNUSED const gchar *name) {
    const gchar* filename = gwy_file_get_filename_sys(data);
    char dir[256];
    dirname(filename, dir);
    printf("dirname: %s\n", dir);

    GtkWidget* main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Folder Overview");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1350, 750);
    // gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);

    gwy_app_wait_start(GTK_WINDOW(main_window), "Creating Overview");

    GtkWidget* scroll_area = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_area),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget* scroll_vbox = gtk_vbox_new(FALSE, 5);

    DIR *dfd;
    if ((dfd = opendir(dir)) == NULL) {
        fprintf(stderr, "Can't open %s\n", dir);
    }


    struct dirent *dp;
    while ((dp = readdir(dfd)) != NULL) {
        if (endswith(dp->d_name, ".mul")) {
            char full_path[PATH_MAX + 1];
            concat_path(dir, dp->d_name, full_path);
            printf("fullpath: %s\n", full_path);
            // GwyContainer* data_container = gwy_app_file_load(NULL, full_path, NULL);
            GwyContainer* data_container = gwy_file_load(full_path, GWY_RUN_IMMEDIATE, NULL);
            gwy_app_data_browser_add(data_container);
            gwy_app_data_browser_set_keep_invisible(data_container, TRUE);

            GtkWidget* icon_view = create_iconview(data_container);
            g_signal_connect(icon_view, "item-activated", G_CALLBACK(on_icon_dbl_click), NULL);
            // gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_area), icon_view);
            gtk_box_pack_start(GTK_BOX(scroll_vbox), icon_view, TRUE, TRUE, 0);
        }
    }

    // gtk_container_add(GTK_CONTAINER(scroll_area), scroll_vbox);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_area), scroll_vbox);

    GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    gtk_box_pack_start(
        GTK_BOX(vbox),
        scroll_area,
        TRUE,
        TRUE,
        1
    );

    gwy_app_wait_finish();
    gtk_widget_show_all(main_window);
}

static void dirname(const char *path, char *dir) {
    size_t len = strlen(path);

    // If the path is empty, return the current directory "."
    if (len == 0) {
        strcpy(dir, ".");
        return;
    }

    // Find the last occurrence of either slash '/' or backslash '\'
    const char *last_fwd_slash = strrchr(path, '/');
    const char *last_bck_slash = strrchr(path, '\\');
    const char *last_slash = last_fwd_slash > last_bck_slash ? last_fwd_slash : last_bck_slash;

    // Handle the cases:
    // 1. No slash found
    // 2. The path is just a single slash or backslash
    if (!last_slash || last_slash == path || last_slash == path + 1) {
        strcpy(dir, last_slash == path || last_slash == path + 1 ? "\\" : ".");
        return;
    }

    // Calculate the length of the directory part
    size_t dir_len = last_slash - path;

    // Copy the directory part to the output buffer
    strncpy(dir, path, dir_len);
    dir[dir_len] = '\0';
}

static void basename(const char* path, char* base) {
    size_t len = strlen(path);

    // If the path is empty, return the current directory "."
    if (len == 0) {
        strcpy(base, ".");
        return;
    }

    // Find the last occurrence of either slash '/' or backslash '\'
    const char* last_fwd_slash = strrchr(path, '/');
    const char* last_bck_slash = strrchr(path, '\\');
    const char* last_slash = last_fwd_slash > last_bck_slash ? last_fwd_slash : last_bck_slash;

    // If no slash is found, the entire path is the basename
    if (!last_slash) {
        strcpy(base, path);
        return;
    }

    // Move past the last slash to get the basename
    strcpy(base, last_slash + 1);
}

static bool endswith(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (str_len < suffix_len) {
        return false;
    }

    return strcmp(str + (str_len - suffix_len), suffix) == 0;
}

char* concat_path(const char* dir, const char* filename, char* fullpath) {
    strcpy(fullpath, dir);

    size_t len = strlen(fullpath);
    // Determine the appropriate separator based on the directory path
    char separator = (strchr(dir, '\\') != NULL) ? '\\' : '/';

    // Add the separator if it's not already at the end of the directory path
    if (fullpath[len - 1] != '/' && fullpath[len - 1] != '\\') {
        fullpath[len] = separator;
        fullpath[len + 1] = '\0';
    }

    strcat(fullpath, filename);

    return fullpath;
}

struct DriftCorrectionData {
    gint preview_container_id;
    gint preview_datafield_id;
    SelectedImage* images;
    int images_len;
    int images_cap;
    SelectedImage* selected_imgs;
    int selected_images_len;
    int selected_images_cap;
    GtkWidget* preview_img;
    int current_preview;
    SelectedImage* current_preview_img;
};

struct SelectedImage {
    gint container_id;
    gint img_id;
    gdouble x_selection;
    gdouble y_selection;
};


static void drift_correction(GwyContainer* data, GwyRunType run, G_GNUC_UNUSED const gchar *name) {
    printf("Drift correction\n");
    DriftCorrectionData* drift_correction_data = malloc(sizeof(DriftCorrectionData));
    drift_correction_data->images_len = 0;
    drift_correction_data->images_cap = 32;
    drift_correction_data->images = malloc(drift_correction_data->images_cap * sizeof(SelectedImage));

    drift_correction_data->selected_images_len = 0;
    drift_correction_data->selected_images_cap = 32;
    drift_correction_data->selected_imgs = malloc(drift_correction_data->images_cap * sizeof(SelectedImage));

    drift_correction_data->current_preview = 0;

    // // Collect all images from all currently opened files
    gwy_app_data_browser_foreach(*(GwyAppDataForeachFunc)setup_dc_data, drift_correction_data);
    for (int i = 0; i < drift_correction_data->images_len; i++) {
        printf("ALL IMAGES: container id : %d, img_id : %d\n", drift_correction_data->images[i].container_id, drift_correction_data->images[i].img_id);
    }

    // GtkWidget* prompt_dialog = gwy_dialog_new("Select Images");
    GtkWidget* prompt_dialog = gtk_dialog_new_with_buttons("Select Images", NULL, 0,
                                         // _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_window_set_default_size(GTK_WINDOW(prompt_dialog), 1350, 750);

    // gwy_dialog_add_buttons(GWY_DIALOG(prompt_dialog), GWY_RESPONSE_RESET, GTK_RESPONSE_CANCEL, GTK_RESPONSE_OK, 0);
    gtk_dialog_set_default_response(GTK_DIALOG(prompt_dialog), GTK_RESPONSE_OK);

    GtkListStore* list_store = gtk_list_store_new(N_COLS, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_INT);
    GtkTreeIter iter;

    for (int i = 0; i < drift_correction_data->images_len; i++) {
        gint img_id = drift_correction_data->images[i].img_id;
        gint container_id = drift_correction_data->images[i].container_id;
        GwyContainer* container = gwy_app_data_browser_get(container_id);
        GQuark key = gwy_app_get_data_key_for_id(img_id);
        GwyDataField* data_field = gwy_container_get_object(container, key);
        level_plane(data_field);
        // gwy_data_field_data_changed(data_field);
        GdkPixbuf* thumbnail = gwy_app_get_channel_thumbnail(container, img_id, 200, 200);
        char* data_field_title = gwy_app_get_data_field_title(container, img_id);

        int title_length = snprintf(NULL, 0, "%s", /*filename,*/ data_field_title);
        char* title = malloc(title_length + 1);
        snprintf(title, title_length + 1, "%s", /*filename,*/ data_field_title);

        int id_length = snprintf(NULL, 0, "%i", img_id);
        char* id_str = malloc(id_length + 1);
        snprintf(id_str, id_length + 1, "%d", img_id);

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           IMG_ID_COL, img_id,
                           TITLE_COL, title,
                           THUMBNAIL_COL, thumbnail,
                           CONTAINER_ID_COL, container_id,
                           -1);
    }

    GtkWidget* icon_view = gtk_icon_view_new();
    gtk_icon_view_set_model(GTK_ICON_VIEW(icon_view), GTK_TREE_MODEL(list_store));
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(icon_view), TITLE_COL);
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(icon_view), THUMBNAIL_COL);
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(icon_view), GTK_SELECTION_MULTIPLE);

    GtkWidget* scroll_area = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_area), icon_view);
    // gwy_dialog_add_content(GWY_DIALOG(prompt_dialog), scroll_area, TRUE, TRUE, 0);
    //
    // gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(prompt_dialog))), scroll_area, TRUE, TRUE, 2);
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(prompt_dialog));
    gtk_container_add(GTK_CONTAINER(content_area), scroll_area);


    // GwyDialogOutcome prompt_outcome = gwy_dialog_run(GWY_DIALOG(prompt_dialog));
    gtk_widget_show_all(prompt_dialog);
    gint prompt_response = gtk_dialog_run(GTK_DIALOG(prompt_dialog));

    // if (prompt_outcome == GWY_DIALOG_PROCEED) {
    switch (prompt_response){
        case GTK_RESPONSE_OK:
            printf("OK");
            gtk_icon_view_selected_foreach(GTK_ICON_VIEW(icon_view), (GtkIconViewForeachFunc)dc_data_append_selected_images, drift_correction_data);
            for (int i = 0; i < drift_correction_data->selected_images_len; i++) {
                printf("SELECTED IMAGES: img_id: %d, container_id: %d\n", drift_correction_data->selected_imgs[i].img_id, drift_correction_data->selected_imgs[i].container_id);
            }
            gtk_widget_destroy(prompt_dialog);
            break;

        case GTK_RESPONSE_CANCEL:
            printf("CANCEL");
            gtk_widget_destroy(prompt_dialog);
            g_free(drift_correction_data);
            return;
    }

    GwyContainer* dc_container = gwy_container_new();
    gwy_app_data_browser_add(dc_container);
    gwy_app_data_browser_set_keep_invisible(dc_container, TRUE);
    gint dc_container_id = gwy_app_data_browser_get_number(dc_container);

    GwyContainer* first_img_container = gwy_app_data_browser_get(drift_correction_data->selected_imgs[0].container_id);
    GQuark first_img_key = gwy_app_get_data_key_for_id(drift_correction_data->selected_imgs[0].img_id);
    GwyDataField* first_df = gwy_container_get_object(first_img_container, first_img_key);
    GwyDataField* preview_datafield = gwy_data_field_duplicate(first_df);
    drift_correction_data->preview_datafield_id = gwy_app_data_browser_add_data_field(preview_datafield, dc_container, TRUE);

    for (int i = 0; i < drift_correction_data->selected_images_len; i++) {
        GwyContainer* container = gwy_app_data_browser_get(drift_correction_data->selected_imgs[i].container_id);
        GQuark key = gwy_app_get_data_key_for_id(drift_correction_data->selected_imgs[i].img_id);

        GwyDataField* df = gwy_container_get_object(container, key);
        GwyDataField* new_df = gwy_data_field_duplicate(df);
        gint new_img_id = gwy_app_data_browser_add_data_field(new_df, dc_container, TRUE);
        drift_correction_data->selected_imgs[i].img_id = new_img_id;
        drift_correction_data->selected_imgs[i].container_id = dc_container_id;
    }


    // Main Window
    GtkWidget* stack_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Start with first image
    drift_correction_data->preview_img = gwy_create_preview(dc_container, drift_correction_data->preview_datafield_id, 512, FALSE);
    // Box where preview image and previous and next buttons live in
    GtkWidget* preview_vbox = gtk_vbox_new(FALSE, 4);

    GwyPixmapLayer* view_layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(drift_correction_data->preview_img));
    if (view_layer == NULL) {
        printf("VIEW LAYER IS NULL");
    }
    GwyVectorLayer* vec_layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(drift_correction_data->preview_img));
    if (vec_layer == NULL) {
        printf("isnull\n");
        vec_layer = g_object_new(g_type_from_name("GwyLayerPoint"), NULL);
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(drift_correction_data->preview_img), vec_layer);
    }

    gwy_vector_layer_set_selection_key(vec_layer, "/0/select/dc/point");
    GwySelection* selection = gwy_vector_layer_ensure_selection(vec_layer);
    gwy_selection_set_max_objects(selection, 1);
    if (selection == NULL) {
        printf("selection is null\n");
    }

    for (int i = 0; i < drift_correction_data->selected_images_len; i++) {
        printf("IN NEW CONTAINER: imd_id: %d, container_id: %d\n", drift_correction_data->selected_imgs[i].img_id, drift_correction_data->selected_imgs[i].container_id);
    }

    g_signal_connect(selection, "finished", G_CALLBACK(on_selection_finish), drift_correction_data);

    // Box where previous and next buttons live in
    GtkWidget* preview_controls_hbox = gtk_hbox_new(FALSE, 4);

    GtkWidget* prev_btn =  gtk_button_new_with_label ("<");
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_prev_btn_click), drift_correction_data);
    GtkWidget* next_btn =  gtk_button_new_with_label (">");
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_next_btn_click), drift_correction_data);
    GtkWidget* run_btn =  gtk_button_new_with_label ("Ok");
    g_signal_connect(run_btn, "clicked", G_CALLBACK(on_run_btn_click), drift_correction_data);

    gtk_box_pack_start(GTK_BOX(preview_controls_hbox), prev_btn, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(preview_controls_hbox), run_btn, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(preview_controls_hbox), next_btn, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(preview_vbox), drift_correction_data->preview_img, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(preview_vbox), preview_controls_hbox, FALSE, FALSE, 4);

    gtk_container_add(GTK_CONTAINER(stack_window), preview_vbox);

    gtk_widget_show_all(stack_window);
}

static void dc_data_append_image(DriftCorrectionData* dc_data, SelectedImage image) {
    dc_data->images[dc_data->images_len] = image;
    dc_data->images_len++;

    if (dc_data->images_len == dc_data->images_cap) {
        dc_data->images = realloc(dc_data->images, dc_data->images_cap * 2 * sizeof(SelectedImage));
        dc_data->images_cap *= 2;
    }
}

static void dc_data_append_selected_images(GtkIconView* icon_view, GtkTreePath* tree_path, DriftCorrectionData* dc_data) {
    GtkTreeIter iter;
    GtkListStore *store;
    gint img_id;
    gint container_id;

    // Get the associated list store
    store = GTK_LIST_STORE(gtk_icon_view_get_model(icon_view));
    // Convert the path to an iter
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, tree_path);
    // Get the value of the first column (assuming it's an integer)
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, IMG_ID_COL, &img_id, -1);
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, CONTAINER_ID_COL, &container_id, -1);

    SelectedImage selected_image;
    selected_image.container_id = container_id;
    selected_image.img_id = img_id;

    dc_data->selected_imgs[dc_data->selected_images_len] = selected_image;
    dc_data->selected_images_len++;

    if (dc_data->selected_images_len == dc_data->selected_images_cap) {
        dc_data->selected_imgs = realloc(dc_data->selected_imgs, dc_data->selected_images_cap * 2 * sizeof(SelectedImage));
        dc_data->selected_images_cap *= 2;
    }
}

static void setup_dc_data(GwyContainer* container, DriftCorrectionData* drift_correction_data) {
    gint container_id = gwy_app_data_browser_get_number(container);
    gint* data_ids = gwy_app_data_browser_get_data_ids(container);

    for (int i = 0; data_ids[i] != -1; i++) {
        SelectedImage selected_image;
        selected_image.container_id = container_id;
        selected_image.img_id = data_ids[i];
        dc_data_append_image(drift_correction_data, selected_image);
    }
}

static void on_selection_finish(GwySelection* selection, DriftCorrectionData* dc_data) {

    gdouble selection_coords[2];
    gint n = gwy_selection_get_data(selection, selection_coords);
    printf("n: %d\n", n);
    guint num_selections = gwy_selection_get_object_size(selection);


    // gint img_id = drift_correction_data->images[0].img_id;
    // printf("img_id: %d\n", img_id);
    // GQuark key = gwy_app_get_data_key_for_id(img_id);
    // GwyContainer* container = gwy_app_data_browser_get(drift_correction_data->images[0].container_id);
    // GwyDataField* data_field = gwy_container_get_object(container, key);


    for (guint i = 0; i < num_selections; i++) {
        printf("selection coord [%d]: %e\n", i, selection_coords[i]);
    }

    SelectedImage* img = dc_data->current_preview_img;
    img->x_selection = selection_coords[0];
    img->y_selection = selection_coords[1];
}

static void on_prev_btn_click(GtkButton* select_btn, DriftCorrectionData* dc_data) {
    if (dc_data->current_preview > 0) {
        dc_data->current_preview -= 1;
    }
    gint img_id = dc_data->selected_imgs[dc_data->current_preview].img_id;
    gint container_id = dc_data->selected_imgs[dc_data->current_preview].container_id;
    printf("IN PREV: current_preview: %d, container id: %d,img id: %d\n", dc_data->current_preview, container_id, img_id);

    GwyContainer* container = gwy_app_data_browser_get(container_id);
    GtkWidget* preview_img = gwy_create_preview(container, img_id, 512, FALSE);
    GwyVectorLayer* vec_layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(preview_img));
    if (vec_layer == NULL) {
        vec_layer = g_object_new(g_type_from_name("GwyLayerPoint"), NULL);
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(preview_img), vec_layer);
    }

    if (gwy_vector_layer_get_selection_key(vec_layer) == NULL) {
        char selection_key[30];
        sprintf(selection_key, "/%d/select/dc/point", img_id);
        gwy_vector_layer_set_selection_key(vec_layer, selection_key);
    }
    GwySelection* selection = gwy_vector_layer_ensure_selection(vec_layer);
    gwy_selection_set_max_objects(selection, 1);

    GQuark key = gwy_app_get_data_key_for_id(img_id);
    GwyDataField* data_field = gwy_container_get_object(container, key);

    GQuark preview_datafield_key = gwy_app_get_data_key_for_id(dc_data->preview_datafield_id);
    GwyDataField* preview_datafield = gwy_container_get_object(container, preview_datafield_key);

    gwy_data_field_assign(preview_datafield, data_field);
    gwy_data_field_data_changed(preview_datafield);

    g_signal_connect(selection, "finished", G_CALLBACK(on_selection_finish), dc_data);

    dc_data->current_preview_img = &dc_data->selected_imgs[dc_data->current_preview];
}

static void on_next_btn_click(GtkButton* select_btn, DriftCorrectionData* dc_data) {
    if (dc_data->current_preview < dc_data->selected_images_len - 1) {
        dc_data->current_preview += 1;
    }
    gint img_id = dc_data->selected_imgs[dc_data->current_preview].img_id;
    gint container_id = dc_data->selected_imgs[dc_data->current_preview].container_id;
    printf("IN NEXT: current_preview: %d, container id: %d,img id: %d\n", dc_data->current_preview, container_id, img_id);


    GwyContainer* container = gwy_app_data_browser_get(container_id);
    if (container == NULL) {
        printf("CONTAINER IS NULL\n");
    }

    GtkWidget* preview_img = gwy_create_preview(container, img_id, 512, FALSE);
    if (preview_img == NULL) {
        printf("preview_img is null\n");
    }
    GwyVectorLayer* vec_layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(preview_img));
    if (vec_layer == NULL) {
        vec_layer = g_object_new(g_type_from_name("GwyLayerPoint"), NULL);
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(preview_img), vec_layer);
    }

    if (gwy_vector_layer_get_selection_key(vec_layer) == NULL) {
        char selection_key[30];
        sprintf(selection_key, "/%d/select/dc/point", img_id);
        gwy_vector_layer_set_selection_key(vec_layer, selection_key);
    }
    GwySelection* selection = gwy_vector_layer_ensure_selection(vec_layer);
    gwy_selection_set_max_objects(selection, 1);


    GQuark key = gwy_app_get_data_key_for_id(img_id);
    GwyDataField* data_field = gwy_container_get_object(container, key);

    GQuark preview_datafield_key = gwy_app_get_data_key_for_id(dc_data->preview_datafield_id);
    GwyDataField* preview_datafield = gwy_container_get_object(container, preview_datafield_key);

    gwy_data_field_assign(preview_datafield, data_field);
    gwy_data_field_data_changed(preview_datafield);

    g_signal_connect(selection, "finished", G_CALLBACK(on_selection_finish), dc_data);

    dc_data->current_preview_img = &dc_data->selected_imgs[dc_data->current_preview];
}

static void on_run_btn_click(GtkButton* run_btn, DriftCorrectionData* dc_data) {
    printf("RUN\n");

    for (int i = 0; i < dc_data->selected_images_len; i++) {
        GwyContainer* container = gwy_app_data_browser_get(dc_data->selected_imgs[i].container_id);
        GQuark key = gwy_app_get_data_key_for_id(dc_data->selected_imgs[i].img_id);
        GwyDataField* data_field = gwy_container_get_object(container, key);
        gdouble selection_x = dc_data->selected_imgs[i].x_selection;
        gdouble selection_y = dc_data->selected_imgs[i].y_selection;
        printf("x %f, y %f\n", gwy_data_field_rtoj(data_field, selection_x), gwy_data_field_rtoi(data_field, selection_y));
    }
}
