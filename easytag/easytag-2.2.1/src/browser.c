/* browser.c - 2000/04/28 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


/* Some parts of this browser are taken from:
 * XMMS - Cross-platform multimedia player
 * Copyright (C) 1998-1999  Peter Alm, Mikael Alm, Olle Hallnas,
 * Thomas Nilsson and 4Front Technologies
 */

#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "gtk2_compat.h"
#include "easytag.h"
#include "browser.h"
#include "et_core.h"
#include "scan_dialog.h"
#include "bar.h"
#include "log.h"
#include "misc.h"
#include "setting.h"
#include "charset.h"
#include "dlm.h"

#include <assert.h>

#include "win32/win32dep.h"


/****************
 * Declarations *
 ****************/

static GtkWidget    *BrowserTree; /* Tree of directories. */
static GtkTreeStore *directoryTreeModel;
static GtkListStore *fileListModel;
static GtkWidget    *BrowserLabel;
static GtkWidget    *BrowserButton;
static GtkWidget    *BrowserNoteBook;
static GtkListStore *artistListModel;
static GtkListStore *albumListModel;
/* Path selected in the browser area (BrowserEntry or BrowserTree). */
static gchar        *BrowserCurrentPath = NULL;

static GtkListStore *RunProgramModel;

static GtkWidget *RunProgramTreeWindow = NULL;
static GtkWidget *RunProgramListWindow = NULL;

/* The Rename Directory window. */
GtkWidget *RenameDirectoryWindow = NULL;
static GtkWidget *RenameDirectoryCombo;
static GtkWidget *RenameDirectoryWithMask;
static GtkListStore *RenameDirectoryMaskModel = NULL;
GtkWidget *RenameDirectoryPreviewLabel = NULL;

/* The last ETFile selected in the BrowserList. */
static ET_File *LastBrowserListETFileSelected;

static const guint BOX_SPACING = 6;

static gchar *Rename_Directory_Masks [] =
{
    "%a - %b",
    "%a_-_%b",
    "%a - %b (%y) - %g",
    "%a_-_%b_(%y)_-_%g",
    "VA - %b (%y)",
    "VA_-_%b_(%y)",
    NULL
};

/*
 * EtPathState:
 * @ET_PATH_STATE_OPEN: the path is open or has been read
 * @ET_PATH_STATE_CLOSED: the path is closed or could not be read
 *
 * Whether to generate an icon with an indicaton that the directory is open
 * (being viewed) or closed (not yet viewed or read).
 */
typedef enum
{
    ET_PATH_STATE_OPEN,
    ET_PATH_STATE_CLOSED
} EtPathState;

/**************
 * Prototypes *
 **************/

static gboolean Browser_Tree_Key_Press (GtkWidget *tree, GdkEvent *event,
                                        gpointer data);
static void Browser_Tree_Set_Node_Visible (GtkWidget *directoryView,
                                           GtkTreePath *path);
static void Browser_List_Set_Row_Visible (GtkTreeModel *treeModel,
                                          GtkTreeIter *rowIter);
static void Browser_Tree_Initialize (void);
static gboolean Browser_Tree_Node_Selected (GtkTreeSelection *selection,
                                            gpointer user_data);
static void Browser_Tree_Rename_Directory (const gchar *last_path,
                                           const gchar *new_path);
static void Browser_Tree_Handle_Rename (GtkTreeIter *parentnode,
                                        const gchar *old_path,
                                        const gchar *new_path);

static gint Browser_List_Key_Press        (GtkWidget *list, GdkEvent *event, gpointer data);
static gboolean Browser_List_Button_Press (GtkTreeView *treeView,
                                           GdkEventButton *event);
static void Browser_List_Row_Selected (GtkTreeSelection * selection,
                                       gpointer data);
static void Browser_List_Set_Row_Appearance (GtkTreeIter *iter);
static gint Browser_List_Sort_Func (GtkTreeModel *model, GtkTreeIter *a,
                                    GtkTreeIter *b, gpointer data);
static void Browser_List_Select_File_By_Iter (GtkTreeIter *iter,
                                              gboolean select_it);

static void Browser_Entry_Activated (void);

static void Browser_Artist_List_Load_Files (ET_File *etfile_to_select);
static void Browser_Artist_List_Row_Selected (GtkTreeSelection *selection,
                                              gpointer data);
static void Browser_Artist_List_Set_Row_Appearance (GtkTreeIter *row);

static void Browser_Album_List_Load_Files (GList *albumlist,
                                           ET_File *etfile_to_select);
static void Browser_Album_List_Row_Selected (GtkTreeSelection *selection,
                                             gpointer data);
static void Browser_Album_List_Set_Row_Appearance (GtkTreeIter *row);

static void Browser_Update_Current_Path (const gchar *path);

#ifdef G_OS_WIN32
static gboolean Browser_Win32_Get_Drive_Root (gchar *drive,
                                              GtkTreeIter *rootNode,
                                              GtkTreePath **rootPath);
#endif /* G_OS_WIN32 */

static gboolean check_for_subdir (const gchar *path);

static GtkTreePath *Find_Child_Node(GtkTreeIter *parent, gchar *searchtext);

static GIcon *get_gicon_for_path (const gchar *path, EtPathState path_state);

static void expand_cb   (GtkWidget *tree, GtkTreeIter *iter, GtkTreePath *path, gpointer data);
static void collapse_cb (GtkWidget *tree, GtkTreeIter *iter, GtkTreePath *treePath, gpointer data);

/* Pop up menus */
static gboolean Browser_Popup_Menu_Handler (GtkWidget *widget,
                                            GdkEventButton *event,
                                            GtkMenu *menu);

/* For window to rename a directory */
static void Destroy_Rename_Directory_Window (void);
static void Rename_Directory (void);
static void Rename_Directory_With_Mask_Toggled (void);

/* For window to run a program with the directory */
static void Destroy_Run_Program_Tree_Window (void);
static void Run_Program_With_Directory (GtkWidget *combobox);

/* For window to run a program with the file */
static void Destroy_Run_Program_List_Window (void);
static void Run_Program_With_Selected_Files (GtkWidget *combobox);

static void empty_entry_disable_widget (GtkWidget *widget, GtkEntry *entry);

static void et_rename_directory_on_response (GtkDialog *dialog,
                                             gint response_id,
                                             gpointer user_data);
static void et_run_program_tree_on_response (GtkDialog *dialog,
                                             gint response_id,
                                             gpointer user_data);
static void et_run_program_list_on_response (GtkDialog *dialog,
                                             gint response_id,
                                             gpointer user_data);

static void et_browser_set_sorting_file_mode (GtkTreeViewColumn *column,
                                              gpointer data);


/*************
 * Functions *
 *************/
/*
 * Load home directory
 */
void Browser_Load_Home_Directory (void)
{
    Browser_Tree_Select_Dir (g_get_home_dir ());
}

/*
 * Load desktop directory
 */
void Browser_Load_Desktop_Directory (void)
{
    Browser_Tree_Select_Dir(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
}

/*
 * Load documents directory
 */
void Browser_Load_Documents_Directory (void)
{
    Browser_Tree_Select_Dir(g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS));
}

/*
 * Load downloads directory
 */
void Browser_Load_Downloads_Directory (void)
{
    Browser_Tree_Select_Dir(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD));
}

/*
 * Load music directory
 */
void Browser_Load_Music_Directory (void)
{
    Browser_Tree_Select_Dir(g_get_user_special_dir(G_USER_DIRECTORY_MUSIC));
}


/*
 * Load default directory
 */
void Browser_Load_Default_Directory (void)
{
    GFile **files;
    gchar *path_utf8;
    gchar *path;

    path_utf8 = g_strdup(DEFAULT_PATH_TO_MP3);
    if (!path_utf8 || strlen(path_utf8)<=0)
    {
        g_free(path_utf8);
        path_utf8 = g_strdup (g_get_home_dir ());
    }

    /* FIXME: only in UTF-8 if coming from the config file, when it should be
     * in GLib filename encoding in all cases. */
    /* 'DEFAULT_PATH_TO_MP3' is stored in UTF-8, we must convert it to the file
     * system encoding before... */
    path = filename_from_display(path_utf8);

    files = g_new (GFile *, 1);
    files[0] = g_file_new_for_path (path);
    g_application_open (g_application_get_default (), files, 1, "");

    g_object_unref (files[0]);
    g_free(path);
    g_free(path_utf8);
    g_free (files);
}


/*
 * Get the path from the selected node (row) in the browser
 * Warning: return NULL if no row selected int the tree.
 * Remember to free the value returned from this function!
 */
static gchar *
Browser_Tree_Get_Path_Of_Selected_Node (void)
{
    GtkTreeSelection *selection;
    GtkTreeIter selectedIter;
    gchar *path;

    g_return_val_if_fail (BrowserTree != NULL, NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserTree));
    if (selection
    && gtk_tree_selection_get_selected(selection, NULL, &selectedIter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), &selectedIter,
                           TREE_COLUMN_FULL_PATH, &path, -1);
        return path;
    }else
    {
        return NULL;
    }
}


/*
 * Set the 'path' within the variable BrowserCurrentPath.
 */
static void
Browser_Update_Current_Path (const gchar *path)
{
    g_return_if_fail (path != NULL);

    /* Be sure that we aren't passing 'BrowserCurrentPath' as parameter of the
     * function to avoid an invalid read. */
    if (path == BrowserCurrentPath) return;

    if (BrowserCurrentPath != NULL)
        g_free(BrowserCurrentPath);
    BrowserCurrentPath = g_strdup(path);

#ifdef G_OS_WIN32
    /* On win32 : "c:\path\to\dir" succeed with stat() for example, while "c:\path\to\dir\" fails */
    ET_Win32_Path_Remove_Trailing_Backslash(BrowserCurrentPath);
#endif /* G_OS_WIN32 */

    if (strcmp(G_DIR_SEPARATOR_S,BrowserCurrentPath) == 0)
        gtk_widget_set_sensitive(BrowserButton,FALSE);
    else
        gtk_widget_set_sensitive(BrowserButton,TRUE);
}


/*
 * Return the current path
 */
gchar *Browser_Get_Current_Path (void)
{
    return BrowserCurrentPath;
}

/*
 * Reload the current directory.
 */
void Browser_Reload_Directory (void)
{
    if (BrowserTree && BrowserCurrentPath != NULL)
    {
        // Unselect files, to automatically reload the file of the directory
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserTree));
        if (selection)
        {
            gtk_tree_selection_unselect_all(selection);
        }
        Browser_Tree_Select_Dir(BrowserCurrentPath);
    }
}

/*
 * Set the current path (selected node) in browser as default path (within config variable).
 */
void Set_Current_Path_As_Default (void)
{
    if (DEFAULT_PATH_TO_MP3 != NULL)
        g_free(DEFAULT_PATH_TO_MP3);
    DEFAULT_PATH_TO_MP3 = g_strdup(BrowserCurrentPath);
    Statusbar_Message(_("New default path for files selected"),TRUE);
}

/*
 * When you press the key 'enter' in the BrowserEntry to validate the text (browse the directory)
 */
static void
Browser_Entry_Activated (void)
{
    const gchar *path_utf8;
    gchar *path;

    path_utf8 = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo))));
    Add_String_To_Combo_List(GTK_LIST_STORE(BrowserEntryModel), (gchar *)path_utf8);

    path = filename_from_display(path_utf8);

    Browser_Tree_Select_Dir(path);
    g_free(path);
}

/*
 * Set a text into the BrowserEntry (and don't activate it)
 */
void Browser_Entry_Set_Text (gchar *text)
{
    if (!text || !BrowserEntryCombo)
        return;

    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo))),text);
}

/*
 * Button to go to parent directory
 */
void
et_browser_on_action_parent_directory (void)
{
    gchar *parent_dir, *path;

    parent_dir = Browser_Get_Current_Path();
    if (strlen(parent_dir)>1)
    {
        if ( parent_dir[strlen(parent_dir)-1]==G_DIR_SEPARATOR )
            parent_dir[strlen(parent_dir)-1] = '\0';
        path = g_path_get_dirname(parent_dir);

        Browser_Tree_Select_Dir(path);
        g_free(path);
    }
}

/*
 * Set a text into the BrowserLabel
 */
void Browser_Label_Set_Text (gchar *text)
{
    if (BrowserLabel && text)
        gtk_label_set_text(GTK_LABEL(BrowserLabel),text ? text : "");
}

/*
 * Key Press events into browser tree
 */
static gboolean
Browser_Tree_Key_Press (GtkWidget *tree, GdkEvent *event, gpointer data)
{
    GdkEventKey *kevent;
    GtkTreeIter SelectedNode;
    GtkTreeModel *treeModel;
    GtkTreeSelection *treeSelection;
    GtkTreePath *treePath;

    g_return_val_if_fail (tree != NULL, FALSE);

    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));

    if (event && event->type==GDK_KEY_PRESS)
    {
        if (!gtk_tree_selection_get_selected(treeSelection, &treeModel, &SelectedNode))
            return FALSE;

        kevent = (GdkEventKey *)event;
        treePath = gtk_tree_model_get_path(GTK_TREE_MODEL(treeModel), &SelectedNode);

        switch(kevent->keyval)
        {
            case GDK_KEY_KP_Enter:    /* Enter key in Num Pad */
            case GDK_KEY_Return:      /* 'Normal' Enter key */
            case GDK_KEY_t:           /* Expand/Collapse node */
            case GDK_KEY_T:           /* Expand/Collapse node */
                if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(tree), treePath))
                    gtk_tree_view_collapse_row(GTK_TREE_VIEW(tree), treePath);
                else
                    gtk_tree_view_expand_row(GTK_TREE_VIEW(tree), treePath, FALSE);

                gtk_tree_path_free(treePath);
                return TRUE;
                break;

            case GDK_KEY_e:           /* Expand node */
            case GDK_KEY_E:           /* Expand node */
                gtk_tree_view_expand_row(GTK_TREE_VIEW(tree), treePath, FALSE);
                gtk_tree_path_free(treePath);
                return TRUE;
                break;

            case GDK_KEY_c:           /* Collapse node */
            case GDK_KEY_C:           /* Collapse node */
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(tree), treePath);
                gtk_tree_path_free(treePath);
                return TRUE;
                break;
        }
        gtk_tree_path_free(treePath);
    }
    return FALSE;
}

/*
 * Key press into browser list
 *   - Delete = delete file
 * Also tries to capture text input and relate it to files
 */
gboolean Browser_List_Key_Press (GtkWidget *list, GdkEvent *event, gpointer data)
{
    GdkEventKey *kevent;
    GtkTreeSelection *fileSelection;

    g_return_val_if_fail (list != NULL, FALSE);

    fileSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

    kevent = (GdkEventKey *)event;
    if (event && event->type==GDK_KEY_PRESS)
    {
        if (gtk_tree_selection_count_selected_rows(fileSelection))
        {
            switch(kevent->keyval)
            {
                case GDK_KEY_Delete:
                    Action_Delete_Selected_Files();
                    return TRUE;
            }
        }
    }

    return FALSE;
}

/*
 * Action for double/triple click
 */
static gboolean
Browser_List_Button_Press (GtkTreeView *treeView, GdkEventButton *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    if (event->type==GDK_2BUTTON_PRESS && event->button==1)
    {
        /* Double left mouse click */
        // Select files of the same directory (useful when browsing sub-directories)
        GList *l;
        gchar *path_ref = NULL;
        gchar *patch_check = NULL;
        GtkTreePath *currentPath = NULL;

        if (!ETCore->ETFileDisplayed)
            return FALSE;

        // File taken as reference...
        path_ref = g_path_get_dirname( ((File_Name *)ETCore->ETFileDisplayed->FileNameCur->data)->value );

        // Search and select files of the same directory
        for (l = g_list_first (ETCore->ETFileDisplayedList); l != NULL;
             l = g_list_next (l))
        {
            // Path of the file to check if it is in the same directory
            patch_check = g_path_get_dirname (((File_Name *)((ET_File *)l->data)->FileNameCur->data)->value);

            if ( path_ref && patch_check && strcmp(path_ref,patch_check)==0 )
            {
                // Use of 'currentPath' to try to increase speed. Indeed, in many
                // cases, the next file to select, is the next in the list
                currentPath = Browser_List_Select_File_By_Etfile2 ((ET_File *)l->data,
                                                                   TRUE,
                                                                   currentPath);
            }
            g_free(patch_check);
        }
        g_free(path_ref);
        if (currentPath)
            gtk_tree_path_free(currentPath);
    }else if (event->type==GDK_3BUTTON_PRESS && event->button==1)
    {
        /* Triple left mouse click, select all files of the list. */
        et_on_action_select_all ();
    }
    return FALSE;
}

/*
 * Collapse (close) tree recursively up to the root node.
 */
void Browser_Tree_Collapse (void)
{
#ifndef G_OS_WIN32
    GtkTreePath *rootPath;
#endif /* !G_OS_WIN32 */

    g_return_if_fail (BrowserTree != NULL);

    gtk_tree_view_collapse_all(GTK_TREE_VIEW(BrowserTree));

#ifndef G_OS_WIN32
    /* But keep the main directory opened */
    rootPath = gtk_tree_path_new_first();
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(BrowserTree), rootPath);
    gtk_tree_path_free(rootPath);
#endif /* !G_OS_WIN32 */
}


/*
 * Set a row (or node) visible in the TreeView (by scrolling the tree)
 */
static void
Browser_Tree_Set_Node_Visible (GtkWidget *directoryView, GtkTreePath *path)
{
    g_return_if_fail (directoryView != NULL || path != NULL);

    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(directoryView), path, NULL, TRUE, 0.5, 0.0);
}


/*
 * Set a row visible in the file list (by scrolling the list)
 */
void Browser_List_Set_Row_Visible (GtkTreeModel *treeModel, GtkTreeIter *rowIter)
{
    /*
     * TODO: Make this only scroll to the row if it is not visible
     * (like in easytag GTK1)
     * See function gtk_tree_view_get_visible_rect() ??
     */
    GtkTreePath *rowPath;

    g_return_if_fail (treeModel != NULL || rowIter != NULL);

    rowPath = gtk_tree_model_get_path(treeModel, rowIter);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(BrowserList), rowPath, NULL, FALSE, 0, 0);
    gtk_tree_path_free(rowPath);
}

/*
 * Triggers when a new node in the browser tree is selected
 * Do file-save confirmation, and then prompt the new dir to be loaded
 */
static gboolean
Browser_Tree_Node_Selected (GtkTreeSelection *selection, gpointer user_data)
{
    gchar *pathName, *pathName_utf8;
    static int counter = 0;
    GtkTreeIter selectedIter;
    GtkTreePath *selectedPath;

    if (!gtk_tree_selection_get_selected(selection, NULL, &selectedIter))
        return TRUE;
    selectedPath = gtk_tree_model_get_path(GTK_TREE_MODEL(directoryTreeModel), &selectedIter);

    /* Open the node */
    if (OPEN_SELECTED_BROWSER_NODE)
    {
        gtk_tree_view_expand_row(GTK_TREE_VIEW(BrowserTree), selectedPath, FALSE);
    }
    gtk_tree_path_free(selectedPath);

    /* Don't start a new reading, if another one is running... */
    if (ReadingDirectory == TRUE)
        return TRUE;

    //Browser_Tree_Set_Node_Visible(BrowserTree, selectedPath);
    gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), &selectedIter,
                       TREE_COLUMN_FULL_PATH, &pathName, -1);
    if (!pathName)
        return FALSE;

    /* Save the current displayed data */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);
    Update_Command_Buttons_Sensivity(); // Not clean to put this here...

    /* Check if all files have been saved before changing the directory */
    if (CONFIRM_WHEN_UNSAVED_FILES && ET_Check_If_All_Files_Are_Saved() != TRUE)
    {
        GtkWidget *msgdialog;
        gint response;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           "%s",
                                           _("Some files have been modified but not saved"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",_("Do you want to save them before changing the directory?"));
        gtk_dialog_add_buttons(GTK_DIALOG(msgdialog),GTK_STOCK_DISCARD,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_SAVE,GTK_RESPONSE_YES,NULL);
        gtk_dialog_set_default_response (GTK_DIALOG (msgdialog),
                                         GTK_RESPONSE_YES);
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Confirm Directory Change"));

        response = gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        switch (response)
        {
            case GTK_RESPONSE_YES:
                if (Save_All_Files_With_Answer(FALSE)==-1)
                    return TRUE;
                break;
            case GTK_RESPONSE_NO:
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
                return TRUE;
                break;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    /* Memorize the current path */
    Browser_Update_Current_Path(pathName);

    /* Display the selected path into the BrowserEntry */
    pathName_utf8 = filename_to_display(pathName);
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo))), pathName_utf8);

    /* Start to read the directory */
    /* The first time, 'counter' is equal to zero. And if we don't want to load
     * directory on startup, we skip the 'reading', but newt we must read it each time */
    if (LOAD_ON_STARTUP || counter)
    {
        gboolean dir_loaded;
        GtkTreeIter parentIter;

        dir_loaded = Read_Directory(pathName);

        // If the directory can't be loaded, the directory musn't exist.
        // So we load the parent node and refresh the children
        if (dir_loaded == FALSE)
        {
            if (gtk_tree_selection_get_selected(selection, NULL, &selectedIter))
            {
                GFile *file = g_file_new_for_path (pathName);

                /* If the path could not be read, then it is possible that it
                 * has a subdirectory with readable permissions. In that case
                 * do not refresh the children. */
                if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(directoryTreeModel),&parentIter,&selectedIter) )
                {
                    selectedPath = gtk_tree_model_get_path(GTK_TREE_MODEL(directoryTreeModel), &parentIter);
                    gtk_tree_selection_select_iter (selection, &parentIter);
                    if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (directoryTreeModel),
                                                       &selectedIter) == FALSE
                                                       && !g_file_query_exists (file, NULL))
                    {
                        gtk_tree_view_collapse_row (GTK_TREE_VIEW (BrowserTree),
                                                    selectedPath);
                        if (OPEN_SELECTED_BROWSER_NODE)
                        {
                            gtk_tree_view_expand_row (GTK_TREE_VIEW (BrowserTree),
                                                      selectedPath, FALSE);
                        }
                        gtk_tree_path_free (selectedPath);
                    }
                }
                g_object_unref (file);
            }
        }

    }else
    {
        /* As we don't use the function 'Read_Directory' we must add this function here */
        Update_Command_Buttons_Sensivity();
    }
    counter++;

    g_free(pathName);
    g_free(pathName_utf8);
    return FALSE;
}


#ifdef G_OS_WIN32
static gboolean
Browser_Win32_Get_Drive_Root (gchar *drive, GtkTreeIter *rootNode, GtkTreePath **rootPath)
{
    gint root_index;
    gboolean found = FALSE;
    GtkTreeIter parentNode;
    gchar *nodeName;

    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(directoryTreeModel), &parentNode);

    // Find root of path, ie: the drive letter
    root_index = 0;

    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), &parentNode,
                           TREE_COLUMN_FULL_PATH, &nodeName, -1);
        if (strncasecmp(drive,nodeName, strlen(drive)) == 0)
        {
            g_free(nodeName);
            found = TRUE;
            break;
        }
        root_index++;
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(directoryTreeModel), &parentNode));

    if (!found) return FALSE;

    *rootNode = parentNode;
    *rootPath = gtk_tree_path_new_from_indices(root_index, -1);

    return TRUE;
}
#endif /* G_OS_WIN32 */


/*
 * Browser_Tree_Select_Dir: Select the directory corresponding to the 'path' in
 * the tree browser, but it doesn't read it!
 * Check if path is correct before selecting it. And returns TRUE on success,
 * else FALSE.
 *
 * - "current_path" is in file system encoding (not UTF-8)
 */
gboolean Browser_Tree_Select_Dir (const gchar *current_path)
{
    GtkTreePath *rootPath = NULL;
    GtkTreeIter parentNode, currentNode;
    gint index = 1; // Skip the first token as it is NULL due to leading /
    gchar **parts;
    gchar *nodeName;
    gchar *temp;

    g_return_val_if_fail (BrowserTree != NULL, FALSE);

    /* Load current_path */
    if(!current_path || !*current_path)
    {
        return TRUE;
    }

#ifdef G_OS_WIN32
    /* On win32 : stat("c:\path\to\dir") succeed, while stat("c:\path\to\dir\") fails */
    ET_Win32_Path_Remove_Trailing_Backslash(current_path);
#endif /* G_OS_WIN32 */


    /* Don't check here if the path is valid. It will be done later when
     * selecting a node in the tree */

    Browser_Update_Current_Path(current_path);

    parts = g_strsplit(current_path, G_DIR_SEPARATOR_S, 0);

    // Expand root node (fill parentNode and rootPath)
#ifdef G_OS_WIN32
    if (!Browser_Win32_Get_Drive_Root(parts[0], &parentNode, &rootPath))
        return FALSE;
#else /* !G_OS_WIN32 */
    if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (directoryTreeModel),
                                        &parentNode))
    {
        g_message ("%s", "directoryTreeModel is empty");
        return FALSE;
    }

    rootPath = gtk_tree_path_new_first();
#endif /* !G_OS_WIN32 */
    if (rootPath)
    {
        gtk_tree_view_expand_to_path(GTK_TREE_VIEW(BrowserTree), rootPath);
        gtk_tree_path_free(rootPath);
    }

    while (parts[index]) // it is NULL-terminated
    {
        if (strlen(parts[index]) == 0)
        {
            index++;
            continue;
        }

        if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(directoryTreeModel), &currentNode, &parentNode))
        {
            gchar *path, *parent_path;
            GFile *file;

            gtk_tree_model_get (GTK_TREE_MODEL (directoryTreeModel),
                                &parentNode, TREE_COLUMN_FULL_PATH,
                                &parent_path, -1);
            path = g_build_path (G_DIR_SEPARATOR_S, parent_path, parts[index],
                                 NULL);
            g_free (parent_path);

            file = g_file_new_for_path (path);

            /* As dir name was not found in any node, check whether it exists
             * or not. */
            if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL)
                == G_FILE_TYPE_DIRECTORY)
            {
                /* It exists and is readable permission of parent directory is executable */
                GIcon *icon;
                GtkTreeIter iter;

                /* Create a new node for this directory name. */
                icon = get_gicon_for_path (path, ET_PATH_STATE_CLOSED);

                gtk_tree_store_insert_with_values (GTK_TREE_STORE (directoryTreeModel),
                                                   &iter, &parentNode, 0,
                                                   TREE_COLUMN_DIR_NAME, parts[index],
                                                   TREE_COLUMN_FULL_PATH, path,
                                                   TREE_COLUMN_HAS_SUBDIR, check_for_subdir (current_path),
                                                   TREE_COLUMN_SCANNED, TRUE,
                                                   TREE_COLUMN_ICON, icon, -1);

                currentNode = iter;
                g_object_unref (icon);
            }
            else
            {
                g_object_unref (file);
                g_free (path);
                break;
            }

            g_object_unref (file);
            g_free (path);
        }
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), &currentNode,
                               TREE_COLUMN_FULL_PATH, &temp, -1);
            nodeName = g_path_get_basename(temp);
            g_free(temp);
#ifdef G_OS_WIN32
            if (strcasecmp(parts[index],nodeName) == 0)
#else /* !G_OS_WIN32 */
            if (strcmp(parts[index],nodeName) == 0)
#endif /* !G_OS_WIN32 */
            {
                g_free(nodeName);
                break;
            }
            g_free(nodeName);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(directoryTreeModel), &currentNode));

        parentNode = currentNode;
        rootPath = gtk_tree_model_get_path(GTK_TREE_MODEL(directoryTreeModel), &parentNode);
        if (rootPath)
        {
            gtk_tree_view_expand_to_path(GTK_TREE_VIEW(BrowserTree), rootPath);
            gtk_tree_path_free(rootPath);
        }
        index++;
    }

    rootPath = gtk_tree_model_get_path(GTK_TREE_MODEL(directoryTreeModel), &parentNode);
    if (rootPath)
    {
        gtk_tree_view_expand_to_path(GTK_TREE_VIEW(BrowserTree), rootPath);
        // Select the node to load the corresponding directory
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserTree)), rootPath);
        Browser_Tree_Set_Node_Visible(BrowserTree, rootPath);
        gtk_tree_path_free(rootPath);
    }

    g_strfreev(parts);
    return TRUE;
}

/*
 * Callback to select-row event
 * Displays the file info of the lowest selected file in the right-hand pane
 */
static void
Browser_List_Row_Selected (GtkTreeSelection *selection, gpointer data)
{
    GList *selectedRows;
    GtkTreePath *lastSelected;
    GtkTreeIter lastFile;
    ET_File *fileETFile;

    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    /*
     * After a file is deleted, this function is called :
     * So we must handle the situation if no rows are selected
     */
    if (!selectedRows)
    {
        return;
    }

    if (!LastBrowserListETFileSelected)
    {
        // Returns the last line selected (in ascending line order) to display the item
        lastSelected = (GtkTreePath *)g_list_last(selectedRows)->data;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &lastFile, lastSelected))
        {
            gtk_tree_model_get (GTK_TREE_MODEL(fileListModel), &lastFile,
                                LIST_FILE_POINTER, &fileETFile, -1);
            Action_Select_Nth_File_By_Etfile (fileETFile);
        }
        else
        {
            g_warning ("%s", "Error getting iter from last path in selection");
        }
    }else
    {
        // The real last selected line
        Action_Select_Nth_File_By_Etfile(LastBrowserListETFileSelected);
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Loads the specified etfilelist into the browser list
 * Also supports optionally selecting a specific etfile
 * but be careful, this does not call Browser_List_Row_Selected !
 */
void Browser_List_Load_File_List (GList *etfilelist, ET_File *etfile_to_select)
{
    GList *l;
    gboolean activate_bg_color = 0;
    GtkTreeIter rowIter;

    g_return_if_fail (BrowserList != NULL);

    gtk_list_store_clear(fileListModel);

    for (l = g_list_first (etfilelist); l != NULL; l = g_list_next (l))
    {
        guint fileKey = ((ET_File *)l->data)->ETFileKey;
        gchar *current_filename_utf8 = ((File_Name *)((ET_File *)l->data)->FileNameCur->data)->value_utf8;
        gchar *basename_utf8         = g_path_get_basename(current_filename_utf8);
        File_Tag *FileTag = ((File_Tag *)((ET_File *)l->data)->FileTag->data);
        gchar *track;
        gchar *disc;

        // Change background color when changing directory (the first row must not be changed)
        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fileListModel), NULL) > 0)
        {
            gchar *dir1_utf8;
            gchar *dir2_utf8;
            gchar *previous_filename_utf8 = ((File_Name *)((ET_File *)l->prev->data)->FileNameCur->data)->value_utf8;

            dir1_utf8 = g_path_get_dirname(previous_filename_utf8);
            dir2_utf8 = g_path_get_dirname(current_filename_utf8);

            if (g_utf8_collate(dir1_utf8, dir2_utf8) != 0)
                activate_bg_color = !activate_bg_color;

            g_free(dir1_utf8);
            g_free(dir2_utf8);
        }

        /* File list displays the current filename (name on disc) and tag
         * fields. */
        track = g_strconcat(FileTag->track ? FileTag->track : "",FileTag->track_total ? "/" : NULL,FileTag->track_total,NULL);
        disc  = g_strconcat (FileTag->disc_number ? FileTag->disc_number : "",
                             FileTag->disc_total ? "/"
                                                 : NULL, FileTag->disc_total,
                             NULL);

        gtk_list_store_insert_with_values (fileListModel, &rowIter, G_MAXINT,
                                           LIST_FILE_NAME, basename_utf8,
                                           LIST_FILE_POINTER, l->data,
                                           LIST_FILE_KEY, fileKey,
                                           LIST_FILE_OTHERDIR,
                                           activate_bg_color,
                                           LIST_FILE_TITLE, FileTag->title,
                                           LIST_FILE_ARTIST, FileTag->artist,
                                           LIST_FILE_ALBUM_ARTIST,
                                           FileTag->album_artist,
                                           LIST_FILE_ALBUM, FileTag->album,
                                           LIST_FILE_YEAR, FileTag->year,
                                           LIST_FILE_DISCNO, disc,
                                           LIST_FILE_TRACK, track,
                                           LIST_FILE_GENRE, FileTag->genre,
                                           LIST_FILE_COMMENT, FileTag->comment,
                                           LIST_FILE_COMPOSER,
                                           FileTag->composer,
                                           LIST_FILE_ORIG_ARTIST,
                                           FileTag->orig_artist,
                                           LIST_FILE_COPYRIGHT,
                                           FileTag->copyright,
                                           LIST_FILE_URL, FileTag->url,
                                           LIST_FILE_ENCODED_BY,
                                           FileTag->encoded_by, -1);
        g_free(basename_utf8);
        g_free(track);
        g_free (disc);

        if (etfile_to_select == l->data)
        {
            Browser_List_Select_File_By_Iter(&rowIter, TRUE);
            //ET_Display_File_Data_To_UI (l->data);
        }

        // Set appearance of the row
        Browser_List_Set_Row_Appearance(&rowIter);
    }
}


/*
 * Update state of files in the list after changes (without clearing the list model!)
 *  - Refresh 'filename' is file saved,
 *  - Change color is something changed on the file
 */
void Browser_List_Refresh_Whole_List (void)
{
    ET_File   *ETFile;
    File_Tag  *FileTag;
    File_Name *FileName;
    //GtkTreeIter iter;
    GtkTreePath *currentPath = NULL;
    GtkTreeIter iter;
    gint row;
    gchar *current_basename_utf8;
    gchar *track;
    gchar *disc;
    gboolean valid;
    GtkWidget *artist_radio;

    if (!ETCore->ETFileDisplayedList || !BrowserList
    ||  gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fileListModel), NULL) == 0)
    {
        return;
    }

    artist_radio = gtk_ui_manager_get_widget (UIManager,
                                              "/ToolBar/ArtistViewMode");

    // Browse the full list for changes
    //gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fileListModel), &iter);
    //    g_print("above worked %d rows\n", gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fileListModel), NULL));

    currentPath = gtk_tree_path_new_first();

    valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &iter, currentPath);
    while (valid)
    {
        // Refresh filename and other fields
        gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &iter,
                           LIST_FILE_POINTER, &ETFile, -1);

        FileName = (File_Name *)ETFile->FileNameCur->data;
        FileTag  = (File_Tag *)ETFile->FileTag->data;

        current_basename_utf8 = g_path_get_basename(FileName->value_utf8);
        track = g_strconcat(FileTag->track ? FileTag->track : "",FileTag->track_total ? "/" : NULL,FileTag->track_total,NULL);
        disc  = g_strconcat (FileTag->disc_number ? FileTag->disc_number : "",
                             FileTag->disc_total ? "/" : NULL,
                             FileTag->disc_total, NULL);

        gtk_list_store_set(fileListModel, &iter,
                           LIST_FILE_NAME,          current_basename_utf8,
                           LIST_FILE_TITLE,         FileTag->title,
                           LIST_FILE_ARTIST,        FileTag->artist,
                           LIST_FILE_ALBUM_ARTIST,  FileTag->album_artist,
						   LIST_FILE_ALBUM,         FileTag->album,
                           LIST_FILE_YEAR,          FileTag->year,
                           LIST_FILE_DISCNO, disc,
                           LIST_FILE_TRACK,         track,
                           LIST_FILE_GENRE,         FileTag->genre,
                           LIST_FILE_COMMENT,       FileTag->comment,
                           LIST_FILE_COMPOSER,      FileTag->composer,
                           LIST_FILE_ORIG_ARTIST,   FileTag->orig_artist,
                           LIST_FILE_COPYRIGHT,     FileTag->copyright,
                           LIST_FILE_URL,           FileTag->url,
                           LIST_FILE_ENCODED_BY,    FileTag->encoded_by,
                           -1);
        g_free(current_basename_utf8);
        g_free(track);
        g_free (disc);

        Browser_List_Set_Row_Appearance(&iter);

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(fileListModel), &iter);
    }
    gtk_tree_path_free(currentPath);

    // When displaying Artist + Album lists => refresh also rows color
    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (artist_radio)))
    {

        for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(artistListModel), NULL); row++)
        {
            if (row == 0)
                currentPath = gtk_tree_path_new_first();
            else
                gtk_tree_path_next(currentPath);

            gtk_tree_model_get_iter(GTK_TREE_MODEL(artistListModel), &iter, currentPath);
            Browser_Artist_List_Set_Row_Appearance(&iter);
        }
        gtk_tree_path_free(currentPath);


        for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(albumListModel), NULL); row++)
        {
            if (row == 0)
                currentPath = gtk_tree_path_new_first();
            else
                gtk_tree_path_next(currentPath);

            gtk_tree_model_get_iter(GTK_TREE_MODEL(albumListModel), &iter, currentPath);
            Browser_Album_List_Set_Row_Appearance(&iter);
        }
        gtk_tree_path_free(currentPath);
    }
}


/*
 * Update state of one file in the list after changes (without clearing the clist!)
 *  - Refresh filename is file saved,
 *  - Change color is something change on the file
 */
void Browser_List_Refresh_File_In_List (ET_File *ETFile)
{
    GList *selectedRow = NULL;
    GtkWidget *artist_radio;
    GtkTreeSelection *selection;
    GtkTreeIter selectedIter;
    GtkTreePath *currentPath = NULL;
    ET_File   *etfile;
    File_Tag  *FileTag;
    File_Name *FileName;
    gboolean row_found = FALSE;
    gchar *current_basename_utf8;
    gchar *track;
    gchar *disc;
    gboolean valid;
    gint row;
    gchar *artist, *album;

    if (!ETCore->ETFileDisplayedList || !BrowserList || !ETFile ||
        gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fileListModel), NULL) == 0)
    {
        return;
    }

    artist_radio = gtk_ui_manager_get_widget (UIManager,
                                              "/ToolBar/ArtistViewMode");

    // Search the row of the modified file to update it (when found: row_found=TRUE)
    // 1/3. Get position of ETFile in ETFileList
    if (row_found == FALSE)
    {
        valid = gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(fileListModel), &selectedIter, NULL, ETFile->IndexKey-1);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &selectedIter,
                           LIST_FILE_POINTER, &etfile, -1);
            if (ETFile->ETFileKey == etfile->ETFileKey)
            {
                row_found = TRUE;
            }
        }
    }

    // 2/3. Try with the selected file in list (works only if we select the same file)
    if (row_found == FALSE)
    {
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
        selectedRow = gtk_tree_selection_get_selected_rows(selection, NULL);
        if (selectedRow && selectedRow->data != NULL)
        {
            valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &selectedIter,
                                    (GtkTreePath*) selectedRow->data);
            if (valid)
            {
                gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &selectedIter,
                                   LIST_FILE_POINTER, &etfile, -1);
                if (ETFile->ETFileKey == etfile->ETFileKey)
                {
                    row_found = TRUE;
                }
            }
        }

        g_list_free_full (selectedRow, (GDestroyNotify)gtk_tree_path_free);
    }

    // 3/3. Fails, now we browse the full list to find it
    if (row_found == FALSE)
    {
        valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fileListModel), &selectedIter);
        while (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &selectedIter,
                               LIST_FILE_POINTER, &etfile, -1);
            if (ETFile->ETFileKey == etfile->ETFileKey)
            {
                row_found = TRUE;
                break;
            } else
            {
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(fileListModel), &selectedIter);
            }
        }
    }

    // Error somewhere...
    if (row_found == FALSE)
        return;

    // Displayed the filename and refresh other fields
    FileName = (File_Name *)etfile->FileNameCur->data;
    FileTag  = (File_Tag *)etfile->FileTag->data;

    current_basename_utf8 = g_path_get_basename(FileName->value_utf8);
    track = g_strconcat(FileTag->track ? FileTag->track : "",FileTag->track_total ? "/" : NULL,FileTag->track_total,NULL);
    disc  = g_strconcat (FileTag->disc_number ? FileTag->disc_number : "",
                         FileTag->disc_total ? "/" : NULL, FileTag->disc_total,
                         NULL);

    gtk_list_store_set(fileListModel, &selectedIter,
                       LIST_FILE_NAME,          current_basename_utf8,
                       LIST_FILE_TITLE,         FileTag->title,
                       LIST_FILE_ARTIST,        FileTag->artist,
                       LIST_FILE_ALBUM_ARTIST,  FileTag->album_artist,
					   LIST_FILE_ALBUM,         FileTag->album,
                       LIST_FILE_YEAR,          FileTag->year,
                       LIST_FILE_DISCNO, disc,
                       LIST_FILE_TRACK,         track,
                       LIST_FILE_GENRE,         FileTag->genre,
                       LIST_FILE_COMMENT,       FileTag->comment,
                       LIST_FILE_COMPOSER,      FileTag->composer,
                       LIST_FILE_ORIG_ARTIST,   FileTag->orig_artist,
                       LIST_FILE_COPYRIGHT,     FileTag->copyright,
                       LIST_FILE_URL,           FileTag->url,
                       LIST_FILE_ENCODED_BY,    FileTag->encoded_by,
                       -1);
    g_free(current_basename_utf8);
    g_free(track);
    g_free (disc);

    // Change appearance (line to red) if filename changed
    Browser_List_Set_Row_Appearance(&selectedIter);

    // When displaying Artist + Album lists => refresh also rows color
    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (artist_radio)))
    {
        gchar *current_artist = ((File_Tag *)ETFile->FileTag->data)->artist;
        gchar *current_album  = ((File_Tag *)ETFile->FileTag->data)->album;

        for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(artistListModel), NULL); row++)
        {
            if (row == 0)
                currentPath = gtk_tree_path_new_first();
            else
                gtk_tree_path_next(currentPath);

            valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(artistListModel), &selectedIter, currentPath);
            if (valid)
            {
                gtk_tree_model_get(GTK_TREE_MODEL(artistListModel), &selectedIter, ARTIST_NAME, &artist, -1);

                if ( (!current_artist && !artist)
                ||   (current_artist && artist && g_utf8_collate(current_artist,artist)==0) )
                {
                    // Set color of the row
                    Browser_Artist_List_Set_Row_Appearance(&selectedIter);
                    g_free(artist);
                    break;
                }
                g_free(artist);
            }
        }
        gtk_tree_path_free(currentPath); currentPath = NULL;

        //
        // FIX ME : see also if we must add a new line / or change list of the ETFile
        //
        for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(albumListModel), NULL); row++)
        {
            if (row == 0)
                currentPath = gtk_tree_path_new_first();
            else
                gtk_tree_path_next(currentPath);

            valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(albumListModel), &selectedIter, currentPath);
            if (valid)
            {
                gtk_tree_model_get(GTK_TREE_MODEL(albumListModel), &selectedIter, ALBUM_NAME, &album, -1);

                if ( (!current_album && !album)
                ||   (current_album && album && g_utf8_collate(current_album,album)==0) )
                {
                    // Set color of the row
                    Browser_Album_List_Set_Row_Appearance(&selectedIter);
                    g_free(album);
                    break;
                }
                g_free(album);
            }
        }
        gtk_tree_path_free(currentPath); currentPath = NULL;

        //
        // FIX ME : see also if we must add a new line / or change list of the ETFile
        //
    }
}


/*
 * Set the appearance of the row
 *  - change background according LIST_FILE_OTHERDIR
 *  - change foreground according file status (saved or not)
 */
static void
Browser_List_Set_Row_Appearance (GtkTreeIter *iter)
{
    ET_File *rowETFile = NULL;
    gboolean otherdir = FALSE;
    GdkColor *backgroundcolor;
    //gchar *temp = NULL;

    if (iter == NULL)
        return;

    // Get the ETFile reference
    gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), iter,
                       LIST_FILE_POINTER,   &rowETFile,
                       LIST_FILE_OTHERDIR,  &otherdir,
                       //LIST_FILE_NAME,      &temp,
                       -1);

    // Must change background color?
    if (otherdir)
        backgroundcolor = &LIGHT_BLUE;
    else
        backgroundcolor = NULL;

    // Set text to bold/red if 'filename' or 'tag' changed
    if ( ET_Check_If_File_Is_Saved(rowETFile) == FALSE )
    {
        if (CHANGED_FILES_DISPLAYED_TO_BOLD)
        {
            gtk_list_store_set(fileListModel, iter,
                               LIST_FONT_WEIGHT,    PANGO_WEIGHT_BOLD,
                               LIST_ROW_BACKGROUND, backgroundcolor,
                               LIST_ROW_FOREGROUND, NULL, -1);
        } else
        {
            gtk_list_store_set(fileListModel, iter,
                               LIST_FONT_WEIGHT,    PANGO_WEIGHT_NORMAL,
                               LIST_ROW_BACKGROUND, backgroundcolor,
                               LIST_ROW_FOREGROUND, &RED, -1);
        }
    } else
    {
        gtk_list_store_set(fileListModel, iter,
                           LIST_FONT_WEIGHT,    PANGO_WEIGHT_NORMAL,
                           LIST_ROW_BACKGROUND, backgroundcolor,
                           LIST_ROW_FOREGROUND, NULL ,-1);
    }

    // Update text fields
    // Don't do it here
    /*if (rowETFile)
    {
        File_Tag *FileTag = ((File_Tag *)((ET_File *)rowETFile)->FileTag->data);

        gtk_list_store_set(fileListModel, iter,
                           LIST_FILE_TITLE,         FileTag->title,
                           LIST_FILE_ARTIST,        FileTag->artist,
                           LIST_FILE_ALBUM_ARTIST,  FileTag->album_artist,
						   LIST_FILE_ALBUM,         FileTag->album,
                           LIST_FILE_YEAR,          FileTag->year,
                           LIST_FILE_TRACK,         FileTag->track,
                           LIST_FILE_GENRE,         FileTag->genre,
                           LIST_FILE_COMMENT,       FileTag->comment,
                           LIST_FILE_COMPOSER,      FileTag->composer,
                           LIST_FILE_ORIG_ARTIST,   FileTag->orig_artist,
                           LIST_FILE_COPYRIGHT,     FileTag->copyright,
                           LIST_FILE_URL,           FileTag->url,
                           LIST_FILE_ENCODED_BY,    FileTag->encoded_by,
                           -1);
    }*/

    // Frees allocated item from gtk_tree_model_get...
    //g_free(temp);
}


/*
 * Remove a file from the list, by ETFile
 */
void Browser_List_Remove_File (ET_File *searchETFile)
{
    gint row;
    GtkTreePath *currentPath = NULL;
    GtkTreeIter currentIter;
    ET_File *currentETFile;
    gboolean valid;

    if (searchETFile == NULL)
        return;

    // Go through the file list until it is found
    for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fileListModel), NULL); row++)
    {
        if (row == 0)
            currentPath = gtk_tree_path_new_first();
        else
            gtk_tree_path_next(currentPath);

        valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &currentIter, currentPath);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &currentIter,
                               LIST_FILE_POINTER, &currentETFile, -1);

            if (currentETFile == searchETFile)
            {
                // Reinit this value to avoid a crash after deleting files...
                if (LastBrowserListETFileSelected == searchETFile)
                    LastBrowserListETFileSelected = NULL;

                gtk_list_store_remove(fileListModel, &currentIter);
                break;
            }
        }
    }

    gtk_tree_path_free (currentPath);
}

/*
 * Get ETFile pointer of a file from a Tree Iter
 */
ET_File *Browser_List_Get_ETFile_From_Path (GtkTreePath *path)
{
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &iter, path))
        return NULL;

    return Browser_List_Get_ETFile_From_Iter(&iter);
}

/*
 * Get ETFile pointer of a file from a Tree Iter
 */
ET_File *Browser_List_Get_ETFile_From_Iter (GtkTreeIter *iter)
{
    ET_File *etfile;

    gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), iter, LIST_FILE_POINTER, &etfile, -1);
    return etfile;
}


/*
 * Select the specified file in the list, by its ETFile
 */
void Browser_List_Select_File_By_Etfile (ET_File *searchETFile, gboolean select_it)
{
    GtkTreePath *currentPath = NULL;

    currentPath = Browser_List_Select_File_By_Etfile2(searchETFile, select_it, NULL);
    if (currentPath)
        gtk_tree_path_free(currentPath);
}
/*
 * Select the specified file in the list, by its ETFile
 *  - startPath : if set : starting path to try increase speed
 *  - returns allocated "currentPath" to free
 */
GtkTreePath *Browser_List_Select_File_By_Etfile2 (ET_File *searchETFile, gboolean select_it, GtkTreePath *startPath)
{
    gint row;
    GtkTreePath *currentPath = NULL;
    GtkTreeIter currentIter;
    ET_File *currentETFile;
    gboolean valid;

     g_return_val_if_fail (searchETFile != NULL, NULL);

    // If the path is used, we try the next item (to increase speed), as it is correct in many cases...
    if (startPath)
    {
        // Try the next path
        gtk_tree_path_next(startPath);
        valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &currentIter, startPath);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &currentIter,
                               LIST_FILE_POINTER, &currentETFile, -1);
            // It is the good file?
            if (currentETFile == searchETFile)
            {
                Browser_List_Select_File_By_Iter(&currentIter, select_it);
                return startPath;
            }
        }
    }

    // Else, we try the whole list...
    // Go through the file list until it is found
    currentPath = gtk_tree_path_new_first();
    for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fileListModel), NULL); row++)
    {
        valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &currentIter, currentPath);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &currentIter,
                               LIST_FILE_POINTER, &currentETFile, -1);

            if (currentETFile == searchETFile)
            {
                Browser_List_Select_File_By_Iter(&currentIter, select_it);
                return currentPath;
                //break;
            }
        }
        gtk_tree_path_next(currentPath);
    }
    gtk_tree_path_free(currentPath);

    return NULL;
}


/*
 * Select the specified file in the list, by an iter
 */
static void
Browser_List_Select_File_By_Iter (GtkTreeIter *rowIter, gboolean select_it)
{
    g_return_if_fail (BrowserList != NULL);

    if (select_it)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
        if (selection)
        {
            g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
            gtk_tree_selection_select_iter(selection, rowIter);
            g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
        }
    }
    Browser_List_Set_Row_Visible(GTK_TREE_MODEL(fileListModel), rowIter);
}

/*
 * Select the specified file in the list, by a string representation of an iter
 * e.g. output of gtk_tree_model_get_string_from_iter()
 */
ET_File *Browser_List_Select_File_By_Iter_String (const gchar* stringIter, gboolean select_it)
{
    GtkTreeIter iter;
    ET_File *current_etfile = NULL;

    g_return_val_if_fail (fileListModel != NULL || BrowserList != NULL, NULL);

    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(fileListModel), &iter, stringIter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &iter,
                           LIST_FILE_POINTER, &current_etfile, -1);

        if (select_it)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));

            // FIX ME : Why signal was blocked if selected? Don't remember...
            if (selection)
            {
                //g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
                gtk_tree_selection_select_iter(selection, &iter);
                //g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
            }
        }
        Browser_List_Set_Row_Visible(GTK_TREE_MODEL(fileListModel), &iter);
    }

    return current_etfile;
}

/*
 * Select the specified file in the list, by fuzzy string matching based on
 * the Damerau-Levenshtein Metric (patch from Santtu Lakkala - 23/08/2004)
 */
ET_File *Browser_List_Select_File_By_DLM (const gchar* string, gboolean select_it)
{
    GtkTreeIter iter;
    GtkTreeIter iter2;
    GtkTreeSelection *selection;
    ET_File *current_etfile = NULL, *retval = NULL;
    gchar *current_filename = NULL, *current_title = NULL;
    int max = 0, this;

    g_return_val_if_fail (fileListModel != NULL || BrowserList != NULL, NULL);

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fileListModel), &iter))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &iter,
                               LIST_FILE_NAME,    &current_filename,
                               LIST_FILE_POINTER, &current_etfile, -1);
            current_title = ((File_Tag *)current_etfile->FileTag->data)->title;

            if ((this = dlm((current_title ? current_title : current_filename), string)) > max) // See "dlm.c"
            {
                max = this;
                iter2 = iter;
                retval = current_etfile;
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(fileListModel), &iter));

        if (select_it)
        {
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
            if (selection)
            {
                g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
                gtk_tree_selection_select_iter(selection, &iter2);
                g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
            }
        }
        Browser_List_Set_Row_Visible(GTK_TREE_MODEL(fileListModel), &iter2);
    }
    return retval;
}


/*
 * Clear all entries on the file list
 */
void Browser_List_Clear()
{
    gtk_list_store_clear(fileListModel);
    gtk_list_store_clear(artistListModel);
    gtk_list_store_clear(albumListModel);

}

/*
 * Refresh the list sorting (call me after SORTING_FILE_MODE has changed)
 */
void Browser_List_Refresh_Sort (void)
{
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(fileListModel), 0, Browser_List_Sort_Func, NULL, NULL);
}

/*
 * Intelligently sort the file list based on the current sorting method
 * see also 'ET_Sort_File_List'
 */
static gint
Browser_List_Sort_Func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
                        gpointer data)
{
    ET_File *ETFile1;
    ET_File *ETFile2;
    gint result = 0;

    gtk_tree_model_get(model, a, LIST_FILE_POINTER, &ETFile1, -1);
    gtk_tree_model_get(model, b, LIST_FILE_POINTER, &ETFile2, -1);

    switch (SORTING_FILE_MODE)
    {
        case SORTING_UNKNOWN:
        case SORTING_BY_ASCENDING_FILENAME:
            result = ET_Comp_Func_Sort_File_By_Ascending_Filename(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_FILENAME:
            result = ET_Comp_Func_Sort_File_By_Descending_Filename(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_TITLE:
            result = ET_Comp_Func_Sort_File_By_Ascending_Title(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_TITLE:
            result = ET_Comp_Func_Sort_File_By_Descending_Title(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_ARTIST:
            result = ET_Comp_Func_Sort_File_By_Ascending_Artist(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_ARTIST:
            result = ET_Comp_Func_Sort_File_By_Descending_Artist(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_ALBUM_ARTIST:
            result = ET_Comp_Func_Sort_File_By_Ascending_Album_Artist(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_ALBUM_ARTIST:
            result = ET_Comp_Func_Sort_File_By_Descending_Album_Artist(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_ALBUM:
            result = ET_Comp_Func_Sort_File_By_Ascending_Album(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_ALBUM:
            result = ET_Comp_Func_Sort_File_By_Descending_Album(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_YEAR:
            result = ET_Comp_Func_Sort_File_By_Ascending_Year(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_YEAR:
            result = ET_Comp_Func_Sort_File_By_Descending_Year(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_DISC_NUMBER:
            result = et_comp_func_sort_file_by_ascending_disc_number (ETFile1,
                                                                      ETFile2);
            break;
        case SORTING_BY_DESCENDING_DISC_NUMBER:
            result = et_comp_func_sort_file_by_descending_disc_number (ETFile1,
                                                                       ETFile2);
            break;
        case SORTING_BY_ASCENDING_TRACK_NUMBER:
            result = ET_Comp_Func_Sort_File_By_Ascending_Track_Number (ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_TRACK_NUMBER:
            result = ET_Comp_Func_Sort_File_By_Descending_Track_Number (ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_GENRE:
            result = ET_Comp_Func_Sort_File_By_Ascending_Genre(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_GENRE:
            result = ET_Comp_Func_Sort_File_By_Descending_Genre(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_COMMENT:
            result = ET_Comp_Func_Sort_File_By_Ascending_Comment(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_COMMENT:
            result = ET_Comp_Func_Sort_File_By_Descending_Comment(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_COMPOSER:
            result = ET_Comp_Func_Sort_File_By_Ascending_Composer(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_COMPOSER:
            result = ET_Comp_Func_Sort_File_By_Descending_Composer(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_ORIG_ARTIST:
            result = ET_Comp_Func_Sort_File_By_Ascending_Orig_Artist(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_ORIG_ARTIST:
            result = ET_Comp_Func_Sort_File_By_Descending_Orig_Artist(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_COPYRIGHT:
            result = ET_Comp_Func_Sort_File_By_Ascending_Copyright(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_COPYRIGHT:
            result = ET_Comp_Func_Sort_File_By_Descending_Copyright(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_URL:
            result = ET_Comp_Func_Sort_File_By_Ascending_Url(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_URL:
            result = ET_Comp_Func_Sort_File_By_Descending_Url(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_ENCODED_BY:
            result = ET_Comp_Func_Sort_File_By_Ascending_Encoded_By(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_ENCODED_BY:
            result = ET_Comp_Func_Sort_File_By_Descending_Encoded_By(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_CREATION_DATE:
            result = ET_Comp_Func_Sort_File_By_Ascending_Creation_Date (ETFile1,                                                                        ETFile2);
            break;
        case SORTING_BY_DESCENDING_CREATION_DATE:
            result = ET_Comp_Func_Sort_File_By_Descending_Creation_Date (ETFile1,
                                                                         ETFile2);
            break;
        case SORTING_BY_ASCENDING_FILE_TYPE:
            result = ET_Comp_Func_Sort_File_By_Ascending_File_Type(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_FILE_TYPE:
            result = ET_Comp_Func_Sort_File_By_Descending_File_Type(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_FILE_SIZE:
            result = ET_Comp_Func_Sort_File_By_Ascending_File_Size(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_FILE_SIZE:
            result = ET_Comp_Func_Sort_File_By_Descending_File_Size(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_FILE_DURATION:
            result = ET_Comp_Func_Sort_File_By_Ascending_File_Duration(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_FILE_DURATION:
            result = ET_Comp_Func_Sort_File_By_Descending_File_Duration(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_FILE_BITRATE:
            result = ET_Comp_Func_Sort_File_By_Ascending_File_Bitrate(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_FILE_BITRATE:
            result = ET_Comp_Func_Sort_File_By_Descending_File_Bitrate(ETFile1, ETFile2);
            break;
        case SORTING_BY_ASCENDING_FILE_SAMPLERATE:
            result = ET_Comp_Func_Sort_File_By_Ascending_File_Samplerate(ETFile1, ETFile2);
            break;
        case SORTING_BY_DESCENDING_FILE_SAMPLERATE:
            result = ET_Comp_Func_Sort_File_By_Descending_File_Samplerate(ETFile1, ETFile2);
            break;
    }

    return result;
}

/*
 * Select all files on the file list
 */
void Browser_List_Select_All_Files (void)
{
    GtkTreeSelection *selection;

    g_return_if_fail (BrowserList != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    if (selection)
    {
        // Must block the select signal to avoid the selecting, one by one, of all files in the main files list
        g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
        gtk_tree_selection_select_all(selection);
        g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
    }
}

/*
 * Unselect all files on the file list
 */
void Browser_List_Unselect_All_Files (void)
{
    GtkTreeSelection *selection;

    g_return_if_fail (BrowserList != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    if (selection)
    {
        gtk_tree_selection_unselect_all(selection);
    }
}

/*
 * Invert the selection of the file list
 */
void Browser_List_Invert_File_Selection (void)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    gboolean valid;

    g_return_if_fail (fileListModel != NULL || BrowserList != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList));
    if (selection)
    {
        /* Must block the select signal to avoid selecting all files (one by one) in the main files list */
        g_signal_handlers_block_by_func(G_OBJECT(selection), G_CALLBACK(Browser_List_Row_Selected), NULL);
        valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fileListModel), &iter);
        while (valid)
        {
            if (gtk_tree_selection_iter_is_selected(selection, &iter))
            {
                gtk_tree_selection_unselect_iter(selection, &iter);
            } else
            {
                gtk_tree_selection_select_iter(selection, &iter);
            }
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(fileListModel), &iter);
        }
        g_signal_handlers_unblock_by_func(G_OBJECT(selection), G_CALLBACK(Browser_List_Row_Selected), NULL);
    }
}


void Browser_Artist_List_Load_Files (ET_File *etfile_to_select)
{
    GList *AlbumList;
    GList *etfilelist;
    ET_File *etfile;
    GList *l;
    GList *m;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    gchar *artistname, *artist_to_select = NULL;

    g_return_if_fail (BrowserArtistList != NULL);

    if (etfile_to_select)
        artist_to_select = ((File_Tag *)etfile_to_select->FileTag->data)->artist;

    gtk_list_store_clear(artistListModel);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserArtistList));

    for (l = ETCore->ETArtistAlbumFileList; l != NULL; l = g_list_next (l))
    {
        gint   nbr_files = 0;

        // Insert a line for each artist
        AlbumList = (GList *)l->data;
        etfilelist = (GList *)AlbumList->data;
        etfile     = (ET_File *)etfilelist->data;
        artistname = ((File_Tag *)etfile->FileTag->data)->artist;

        // Third column text : number of files
        for (m = g_list_first (AlbumList); m != NULL; m = g_list_next (m))
        {
            nbr_files += g_list_length (g_list_first ((GList *)m->data));
        }

        /* Add the new row. */
        gtk_list_store_insert_with_values (artistListModel, &iter, G_MAXINT,
                                           ARTIST_PIXBUF, "easytag-artist",
                                           ARTIST_NAME, artistname,
                                           ARTIST_NUM_ALBUMS,
                                           g_list_length (g_list_first (AlbumList)),
                                           ARTIST_NUM_FILES, nbr_files,
                                           ARTIST_ALBUM_LIST_POINTER,
                                           AlbumList, -1);

        // Todo: Use something better than string comparison
        if ( (!artistname && !artist_to_select)
        ||   (artistname  &&  artist_to_select && strcmp(artistname,artist_to_select) == 0) )
        {
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(artistListModel), &iter);

            g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_Artist_List_Row_Selected),NULL);
            gtk_tree_selection_select_iter(selection, &iter);
            g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_Artist_List_Row_Selected),NULL);

            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(BrowserArtistList), path, NULL, FALSE, 0, 0);
            gtk_tree_path_free(path);

            Browser_Album_List_Load_Files(AlbumList, etfile_to_select);

            // Now that we've found the artist, no need to continue searching
            artist_to_select = NULL;
        }

        // Set color of the row
        Browser_Artist_List_Set_Row_Appearance(&iter);
    }

    // Select the first line if we weren't asked to select anything
    if (!etfile_to_select && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(artistListModel), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(artistListModel), &iter,
                           ARTIST_ALBUM_LIST_POINTER, &AlbumList,
                           -1);
        ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);
        Browser_Album_List_Load_Files(AlbumList,NULL);
    }
}


/*
 * Callback to select-row event
 */
static void
Browser_Artist_List_Row_Selected (GtkTreeSelection* selection, gpointer data)
{
    GList *AlbumList;
    GtkTreeIter iter;

    // Display the relevant albums
    if(!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return; // We might be called with no row selected

    // Save the current displayed data
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    gtk_tree_model_get(GTK_TREE_MODEL(artistListModel), &iter,
                       ARTIST_ALBUM_LIST_POINTER, &AlbumList, -1);
    Browser_Album_List_Load_Files(AlbumList, NULL);
}

/*
 * Set the color of the row of BrowserArtistList
 */
static void
Browser_Artist_List_Set_Row_Appearance (GtkTreeIter *iter)
{
    GList *l;
    GList *m;
    gboolean not_all_saved = FALSE;


    // Change the style (red/bold) of the row if one of the files was changed
    for (gtk_tree_model_get (GTK_TREE_MODEL (artistListModel), iter,
                             ARTIST_ALBUM_LIST_POINTER, &l, -1);
         l != NULL; l = g_list_next (l))
    {
        for (m = (GList *)l->data; m != NULL; m = g_list_next (m))
        {
            if (ET_Check_If_File_Is_Saved ((ET_File *)m->data) == FALSE)
            {
                if (CHANGED_FILES_DISPLAYED_TO_BOLD)
                {
                    // Set the font-style to "bold"
                    gtk_list_store_set(artistListModel, iter,
                                       ARTIST_FONT_WEIGHT, PANGO_WEIGHT_BOLD, -1);
                } else
                {
                    // Set the background-color to "red"
                    gtk_list_store_set(artistListModel, iter,
                                       ARTIST_FONT_WEIGHT,    PANGO_WEIGHT_NORMAL,
                                       ARTIST_ROW_FOREGROUND, &RED, -1);
                }
                not_all_saved = TRUE;
                break;
            }
        }
    }

    // Reset style if all files saved
    if (not_all_saved == FALSE)
    {
        gtk_list_store_set(artistListModel, iter,
                           ARTIST_FONT_WEIGHT,    PANGO_WEIGHT_NORMAL,
                           ARTIST_ROW_FOREGROUND, NULL, -1);
    }
}



/*
 * Load the list of Albums for each Artist
 */
static void
Browser_Album_List_Load_Files (GList *albumlist, ET_File *etfile_to_select)
{
    GList *l;
    GList *etfilelist = NULL;
    ET_File *etfile;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    gchar *albumname, *album_to_select = NULL;

    g_return_if_fail (BrowserAlbumList != NULL);

    if (etfile_to_select)
        album_to_select = ((File_Tag *)etfile_to_select->FileTag->data)->album;

    gtk_list_store_clear(albumListModel);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserAlbumList));

    // Create a first row to select all albums of the artist
    // FIX ME : the attached list must be freed!
    for (l = albumlist; l != NULL; l = g_list_next (l))
    {
        GList *etfilelist_tmp;

        etfilelist_tmp = (GList *)l->data;
        // We must make a copy to not "alter" the initial list by appending another list
        etfilelist_tmp = g_list_copy(etfilelist_tmp);
        etfilelist = g_list_concat(etfilelist, etfilelist_tmp);
    }

    gtk_list_store_insert_with_values (albumListModel, &iter, G_MAXINT,
                                       ALBUM_NAME, _("<All albums>"),
                                       ALBUM_NUM_FILES,
                                       g_list_length (g_list_first (etfilelist)),
                                       ALBUM_ETFILE_LIST_POINTER, etfilelist,
                                       -1);

    // Create a line for each album of the artist
    for (l = albumlist; l != NULL; l = g_list_next (l))
    {
        GIcon *icon;

        // Insert a line for each album
        etfilelist = (GList *)l->data;
        etfile     = (ET_File *)etfilelist->data;
        albumname  = ((File_Tag *)etfile->FileTag->data)->album;

        /* TODO: Make the icon use the symbolic variant. */
        icon = g_themed_icon_new_with_default_fallbacks ("media-optical-cd-audio");

        /* Add the new row. */
        gtk_list_store_insert_with_values (albumListModel, &iter, G_MAXINT,
                                           ALBUM_GICON, icon,
                                           ALBUM_NAME, albumname,
                                           ALBUM_NUM_FILES,
                                           g_list_length (g_list_first (etfilelist)),
                                           ALBUM_ETFILE_LIST_POINTER,
                                           etfilelist, -1);

        g_object_unref (icon);

        if ( (!albumname && !album_to_select)
        ||   (albumname &&  album_to_select && strcmp(albumname,album_to_select) == 0) )
        {
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(albumListModel), &iter);

            g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_Album_List_Row_Selected),NULL);
            gtk_tree_selection_select_iter(selection, &iter);
            g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_Album_List_Row_Selected),NULL);

            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(BrowserAlbumList), path, NULL, FALSE, 0, 0);
            gtk_tree_path_free(path);

            ET_Set_Displayed_File_List(etfilelist);
            Browser_List_Load_File_List(etfilelist,etfile_to_select);

            // Now that we've found the album, no need to continue searching
            album_to_select = NULL;
        }

        // Set color of the row
        Browser_Album_List_Set_Row_Appearance(&iter);
    }

    // Select the first line if we werent asked to select anything
    if (!etfile_to_select && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(albumListModel), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(albumListModel), &iter,
                           ALBUM_ETFILE_LIST_POINTER, &etfilelist,
                           -1);
        ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

        // Set the attached list as "Displayed List"
        ET_Set_Displayed_File_List(etfilelist);
        Browser_List_Load_File_List(etfilelist, NULL);

        // Displays the first item
        Action_Select_Nth_File_By_Etfile((ET_File *)etfilelist->data);
    }
}

/*
 * Callback to select-row event
 */
static void
Browser_Album_List_Row_Selected (GtkTreeSelection *selection, gpointer data)
{
    GList *etfilelist;
    GtkTreeIter iter;


    // We might be called with no rows selected
    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(albumListModel), &iter,
                       ALBUM_ETFILE_LIST_POINTER, &etfilelist, -1);

    // Save the current displayed data
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    // Set the attached list as "Displayed List"
    ET_Set_Displayed_File_List(etfilelist);

    Browser_List_Load_File_List(etfilelist, NULL);

    // Displays the first item
    Action_Select_Nth_File_By_Etfile((ET_File *)etfilelist->data);
}


/*
 * Set the color of the row of BrowserAlbumList
 */
static void
Browser_Album_List_Set_Row_Appearance (GtkTreeIter *iter)
{
    GList *l;
    gboolean not_all_saved = FALSE;


    // Change the style (red/bold) of the row if one of the files was changed
    for (gtk_tree_model_get (GTK_TREE_MODEL (albumListModel), iter,
                             ALBUM_ETFILE_LIST_POINTER, &l, -1);
         l != NULL; l = g_list_next (l))
    {
        if (ET_Check_If_File_Is_Saved ((ET_File *)l->data) == FALSE)
        {
            if (CHANGED_FILES_DISPLAYED_TO_BOLD)
            {
                // Set the font-style to "bold"
                gtk_list_store_set(albumListModel, iter,
                                   ALBUM_FONT_WEIGHT, PANGO_WEIGHT_BOLD, -1);
            } else
            {
                // Set the background-color to "red"
                gtk_list_store_set(albumListModel, iter,
                                   ALBUM_FONT_WEIGHT,    PANGO_WEIGHT_NORMAL,
                                   ALBUM_ROW_FOREGROUND, &RED, -1);
            }
            not_all_saved = TRUE;
            break;
        }
    }

    // Reset style if all files saved
    if (not_all_saved == FALSE)
    {
        gtk_list_store_set(albumListModel, iter,
                           ALBUM_FONT_WEIGHT,    PANGO_WEIGHT_NORMAL,
                           ALBUM_ROW_FOREGROUND, NULL, -1);
    }
}

void Browser_Display_Tree_Or_Artist_Album_List (void)
{
    ET_File *etfile = ETCore->ETFileDisplayed; // ETFile to display again after changing browser view
    GtkWidget *artist_radio;

    // Save the current displayed data
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    // Toggle button to switch view
    artist_radio = gtk_ui_manager_get_widget (UIManager,
                                              "/ToolBar/ArtistViewMode");

    // Button pressed in the toolbar
    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (artist_radio)))
    {
        /*
         * Artist + Album view
         */

        // Display Artist + Album lists
        gtk_notebook_set_current_page(GTK_NOTEBOOK(BrowserNoteBook),1);
        ET_Create_Artist_Album_File_List();
        Browser_Artist_List_Load_Files(etfile);

    }else
    {

        /*
         * Browser (classic) view
         */
        // Set the whole list as "Displayed list"
        ET_Set_Displayed_File_List(ETCore->ETFileList);

        // Display Tree Browser
        gtk_notebook_set_current_page(GTK_NOTEBOOK(BrowserNoteBook),0);
        Browser_List_Load_File_List(ETCore->ETFileDisplayedList, etfile);

        // Displays the first file if nothing specified
        if (!etfile)
        {
            GList *etfilelist = ET_Displayed_File_List_First();
            if (etfilelist)
                etfile = (ET_File *)etfilelist->data;
            Action_Select_Nth_File_By_Etfile(etfile);
        }
    }

    //ET_Display_File_Data_To_UI(etfile); // Causes a crash
}

/*
 * Disable (FALSE) / Enable (TRUE) all user widgets in the browser area (Tree + List + Entry)
 */
void Browser_Area_Set_Sensitive (gboolean activate)
{
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserEntryCombo),activate);
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserTree),      activate);
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserList),      activate);
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserArtistList),activate);
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserAlbumList), activate);
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserButton),    activate);
    gtk_widget_set_sensitive(GTK_WIDGET(BrowserLabel),     activate);
}


/*
 * Browser_Popup_Menu_Handler : displays the corresponding menu
 */
static gboolean
Browser_Popup_Menu_Handler (GtkWidget *widget, GdkEventButton *event,
                            GtkMenu *menu)
{
    if (event && (event->type == GDK_BUTTON_PRESS) && (event->button == 3))
    {
        if (GTK_IS_TREE_VIEW (widget))
        {
            GtkTreeView *browser_tree;

            browser_tree = GTK_TREE_VIEW (widget);

            if (event->window == gtk_tree_view_get_bin_window (browser_tree))
            {
                GtkTreePath *tree_path;

                if (gtk_tree_view_get_path_at_pos (browser_tree, event->x,
                                                   event->y, &tree_path, NULL,
                                                   NULL,NULL))
                {
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (browser_tree),
                                                    tree_path);
                    gtk_tree_path_free (tree_path);
                }
            }
        }

        gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button,
	                event->time);

        return TRUE;
    }

    return FALSE;
}

/*
 * Destroy the whole tree up to the root node
 */
static void
Browser_Tree_Initialize (void)
{
#ifdef G_OS_WIN32
    DWORD drives;
    UINT drive_type;
    gchar drive[] = "A:/";
    gchar drive_backslashed[] = "A:\\";
    gchar drive_slashless[] = "A:";
    gchar drive_label[256];
#endif
    GtkTreeIter parent_iter;
    GtkTreeIter dummy_iter;
    GIcon *drive_icon;

    g_return_if_fail (directoryTreeModel != NULL);

    gtk_tree_store_clear(directoryTreeModel);

#ifdef G_OS_WIN32
    /* Code strangely familiar with gtkfilesystemwin32.c */

    drives = GetLogicalDrives();
    if (!drives)
    {
        g_warning ("GetLogicalDrives failed");
        drive_icon = g_themed_icon_new ("folder");
    }

    while (drives && drive[0] <= 'Z')
    {
        if (drives & 1)
        {
            char *drive_dir_name;

            drive_type = GetDriveType(drive_backslashed);

            // DRIVE_REMOVABLE   2
            // DRIVE_FIXED       3
            // DRIVE_REMOTE      4
            // DRIVE_CDROM       5
            // DRIVE_RAMDISK     6
            // DRIVE_UNKNOWN     0
            // DRIVE_NO_ROOT_DIR 1
            switch(drive_type)
            {
                case DRIVE_FIXED:
                    drive_icon = g_themed_icon_new ("drive-harddisk");
                    break;
                case DRIVE_REMOVABLE:
                    drive_icon = g_themed_icon_new ("drive-removable-media");
                    break;
                case DRIVE_CDROM:
                    drive_icon = g_themed_icon_new ("drive-optical");
                    break;
                case DRIVE_REMOTE:
                    drive_icon = g_themed_icon_new ("folder-remote");
                    break;
                case DRIVE_RAMDISK:
                    /* FIXME: There is no standard RAM icon, so create one. */
                    drive_icon = g_themed_icon_new ("drive-removable-media");
                    break;
                default:
                    drive_icon = g_themed_icon_new ("folder");
            }

            drive_label[0] = 0;

            GetVolumeInformation(drive_backslashed, drive_label, 256, NULL, NULL, NULL, NULL, 0);

            /* Drive letter first so alphabetical drive list order works */
            drive_dir_name = g_strconcat("(", drive_slashless, ") ", drive_label, NULL);

            gtk_tree_store_insert_with_values (directoryTreeModel,
                                               &parent_iter, NULL, G_MAXINT,
                                               TREE_COLUMN_DIR_NAME,
                                               drive_dir_name,
                                               TREE_COLUMN_FULL_PATH,
                                               drive_backslashed,
                                               TREE_COLUMN_HAS_SUBDIR, TRUE,
                                               TREE_COLUMN_SCANNED, FALSE,
                                               TREE_COLUMN_ICON, drive_icon,
                                               -1);
            /* Insert dummy node. */
            gtk_tree_store_append (directoryTreeModel, &dummy_iter,
                                   &parent_iter);

            g_free(drive_dir_name);
        }
        drives >>= 1;
        drive[0]++;
        drive_backslashed[0]++;
        drive_slashless[0]++;
    }

#else /* !G_OS_WIN32 */
    drive_icon = get_gicon_for_path (G_DIR_SEPARATOR_S, ET_PATH_STATE_CLOSED);
    gtk_tree_store_insert_with_values (directoryTreeModel, &parent_iter, NULL,
                                       G_MAXINT, TREE_COLUMN_DIR_NAME,
                                       G_DIR_SEPARATOR_S,
                                       TREE_COLUMN_FULL_PATH,
                                       G_DIR_SEPARATOR_S,
                                       TREE_COLUMN_HAS_SUBDIR, TRUE,
                                       TREE_COLUMN_SCANNED, FALSE,
                                       TREE_COLUMN_ICON, drive_icon, -1);
    /* Insert dummy node. */
    gtk_tree_store_append (directoryTreeModel, &dummy_iter, &parent_iter);
#endif /* !G_OS_WIN32 */

    g_object_unref (drive_icon);
}

/*
 * Browser_Tree_Rebuild: Refresh the tree browser by destroying it and rebuilding it.
 * Opens tree nodes corresponding to 'path_to_load' if this parameter isn't NULL.
 * If NULL, selects the current path.
 */
void Browser_Tree_Rebuild (gchar *path_to_load)
{
    gchar *current_path = NULL;
    GtkTreeSelection *selection;

    /* May be called from GtkUIManager callback */
    if (GTK_IS_ACTION(path_to_load))
        path_to_load = NULL;

    if (path_to_load != NULL)
    {
        Browser_Tree_Initialize();
        Browser_Tree_Select_Dir(path_to_load);
        return;
    }

    /* Memorize the current path to load it again at the end */
    current_path = Browser_Tree_Get_Path_Of_Selected_Node();
    if (current_path==NULL && BrowserEntryCombo)
    {
        /* If no node selected, get path from BrowserEntry or default path */
        if (BrowserCurrentPath != NULL)
            current_path = g_strdup(BrowserCurrentPath);
        else if (g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo)))), -1) > 0)
            current_path = filename_from_display(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo)))));
        else
            current_path = g_strdup(DEFAULT_PATH_TO_MP3);
    }

    Browser_Tree_Initialize();
    /* Select again the memorized path without loading files */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserTree));
    if (selection)
    {
        g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_Tree_Node_Selected),NULL);
        Browser_Tree_Select_Dir(current_path);
        g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_Tree_Node_Selected),NULL);
    }
    g_free(current_path);

    Update_Command_Buttons_Sensivity();
}

/*
 * Renames a directory
 * last_path:
 * new_path:
 * Parameters are non-utf8!
 */
static void
Browser_Tree_Rename_Directory (const gchar *last_path, const gchar *new_path)
{

    gchar **textsplit;
    gint i;
    GtkTreeIter  iter;
    GtkTreePath *childpath;
    GtkTreePath *parentpath;
    gchar *new_basename;
    gchar *new_basename_utf8;
    gchar *path;

    if (!last_path || !new_path)
        return;

    /*
     * Find the existing tree entry
     */
    textsplit = g_strsplit(last_path, G_DIR_SEPARATOR_S, 0);

#ifdef G_OS_WIN32
    if (!Browser_Win32_Get_Drive_Root(textsplit[0], &iter, &parentpath))
        return;
#else /* !G_OS_WIN32 */
    parentpath = gtk_tree_path_new_first();
#endif /* !G_OS_WIN32 */

    for (i = 1; textsplit[i] != NULL; i++)
    {
        gboolean valid = gtk_tree_model_get_iter (GTK_TREE_MODEL (directoryTreeModel),
                                                  &iter, parentpath);
        if (valid)
        {
            childpath = Find_Child_Node (&iter, textsplit[i]);
        }
        else
        {
            childpath = NULL;
        }

        if (childpath == NULL)
        {
            // ERROR! Could not find it!
            gchar *text_utf8 = filename_to_display(textsplit[i]);
            Log_Print(LOG_ERROR,_("Error: Searching for %s, could not find node %s in tree."), last_path, text_utf8);
            g_strfreev(textsplit);
            g_free(text_utf8);
            return;
        }
        gtk_tree_path_free(parentpath);
        parentpath = childpath;
    }

    gtk_tree_model_get_iter(GTK_TREE_MODEL(directoryTreeModel), &iter, parentpath);
    gtk_tree_path_free(parentpath);

    /* Rename the on-screen node */
    new_basename = g_path_get_basename(new_path);
    new_basename_utf8 = filename_to_display(new_basename);
    gtk_tree_store_set(directoryTreeModel, &iter,
                       TREE_COLUMN_DIR_NAME,  new_basename_utf8,
                       TREE_COLUMN_FULL_PATH, new_path,
                       -1);

    /* Update fullpath of child nodes */
    Browser_Tree_Handle_Rename(&iter, last_path, new_path);

    /* Update the variable of the current path */
    path = Browser_Tree_Get_Path_Of_Selected_Node();
    Browser_Update_Current_Path(path);
    g_free(path);

    g_strfreev(textsplit);
    g_free(new_basename);
    g_free(new_basename_utf8);
}

/*
 * Recursive function to update paths of all child nodes
 */
static void
Browser_Tree_Handle_Rename (GtkTreeIter *parentnode, const gchar *old_path,
                            const gchar *new_path)
{
    GtkTreeIter iter;
    gchar *path;
    gchar *path_shift;
    gchar *path_new;

    // If there are no children then nothing needs to be done!
    if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(directoryTreeModel), &iter, parentnode))
        return;

    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), &iter,
                           TREE_COLUMN_FULL_PATH, &path, -1);
        if(path == NULL)
            continue;

        path_shift = g_utf8_offset_to_pointer(path, g_utf8_strlen(old_path, -1));
        path_new = g_strconcat(new_path, path_shift, NULL);

        gtk_tree_store_set(directoryTreeModel, &iter,
                           TREE_COLUMN_FULL_PATH, path_new, -1);

        g_free(path_new);
        g_free(path);

        // Recurse if necessary
        if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(directoryTreeModel), &iter))
            Browser_Tree_Handle_Rename(&iter, old_path, new_path);

    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(directoryTreeModel), &iter));

}

/*
 * Find the child node of "parentnode" that has text of "childtext
 * Returns NULL on failure
 */
static
GtkTreePath *Find_Child_Node (GtkTreeIter *parentnode, gchar *childtext)
{
    gint row;
    GtkTreeIter iter;
    gchar *text;
    gchar *temp;

    for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(directoryTreeModel), parentnode); row++)
    {
        if (row == 0)
        {
            if (gtk_tree_model_iter_children(GTK_TREE_MODEL(directoryTreeModel), &iter, parentnode) == FALSE) return NULL;
        } else
        {
            if (gtk_tree_model_iter_next(GTK_TREE_MODEL(directoryTreeModel), &iter) == FALSE)
                return NULL;
        }
        gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), &iter,
                           TREE_COLUMN_FULL_PATH, &temp, -1);
        text = g_path_get_basename(temp);
        g_free(temp);
        if(strcmp(childtext,text) == 0)
        {
            g_free(text);
            return gtk_tree_model_get_path(GTK_TREE_MODEL(directoryTreeModel), &iter);
        }
        g_free(text);

    }

    return NULL;
}

/*
 * check_for_subdir:
 * @path: (type filename): the path to test
 *
 * Check if @path has any subdirectories.
 *
 * Returns: %TRUE if subdirectories exist, %FALSE otherwise
 */
static gboolean
check_for_subdir (const gchar *path)
{
    GFile *dir;
    GFileEnumerator *enumerator;

    dir = g_file_new_for_path (path);
    enumerator = g_file_enumerate_children (dir,
                                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                            G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL, NULL);
    g_object_unref (dir);

    if (enumerator)
    {
        GFileInfo *childinfo;

        while ((childinfo = g_file_enumerator_next_file (enumerator,
                                                         NULL, NULL)))
        {
            if ((g_file_info_get_file_type (childinfo) ==
                 G_FILE_TYPE_DIRECTORY) &&
                (BROWSE_HIDDEN_DIR || !g_file_info_get_is_hidden (childinfo)))
            {
                g_object_unref (childinfo);
                g_file_enumerator_close (enumerator, NULL, NULL);
                g_object_unref (enumerator);
                return TRUE;
            }

            g_object_unref (childinfo);
        }

        g_file_enumerator_close (enumerator, NULL, NULL);
        g_object_unref (enumerator);
    }

    return FALSE;
}

/*
 * get_gicon_for_path:
 * @path: (type filename): path to create icon for
 * @path_state: whether the icon should be shown open or closed
 *
 * Check the permissions for the supplied @path (authorized?, readonly?,
 * unreadable?) and return an appropriate icon.
 *
 * Returns: an icon corresponding to the @path
 */
static GIcon *
get_gicon_for_path (const gchar *path, EtPathState path_state)
{
    GIcon *folder_icon;
    GIcon *emblem_icon;
    GIcon *emblemed_icon;
    GEmblem *emblem;
    GFile *file;
    GFileInfo *info;
    GError *error = NULL;

    switch (path_state)
    {
        case ET_PATH_STATE_OPEN:
            folder_icon = g_themed_icon_new ("folder-open");
            break;
        case ET_PATH_STATE_CLOSED:
            folder_icon = g_themed_icon_new ("folder");
            break;
        default:
            g_assert_not_reached ();
    }

    file = g_file_new_for_path (path);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_READ ","
                              G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                              G_FILE_QUERY_INFO_NONE, NULL, &error);

    if (info == NULL)
    {
        g_warning ("Error while querying path information: %s",
                   error->message);
        g_clear_error (&error);
        info = g_file_info_new ();
        g_file_info_set_attribute_boolean (info,
                                           G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
                                           FALSE);
    }

    if (!g_file_info_get_attribute_boolean (info,
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        emblem_icon = g_themed_icon_new ("emblem-unreadable");
        emblem = g_emblem_new_with_origin (emblem_icon,
                                           G_EMBLEM_ORIGIN_LIVEMETADATA);
        emblemed_icon = g_emblemed_icon_new (folder_icon, emblem);
        g_object_unref (folder_icon);
        g_object_unref (emblem_icon);
        g_object_unref (emblem);

        folder_icon = emblemed_icon;
    }
    else if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    {
        emblem_icon = g_themed_icon_new ("emblem-readonly");
        emblem = g_emblem_new_with_origin (emblem_icon,
                                           G_EMBLEM_ORIGIN_LIVEMETADATA);
        emblemed_icon = g_emblemed_icon_new (folder_icon, emblem);
        g_object_unref (folder_icon);
        g_object_unref (emblem_icon);
        g_object_unref (emblem);

        folder_icon = emblemed_icon;
    }

    g_object_unref (file);
    g_object_unref (info);

    return folder_icon;
}


/*
 * Sets the selection function. If set, this function is called before any node
 * is selected or unselected, giving some control over which nodes are selected.
 * The select function should return TRUE if the state of the node may be toggled,
 * and FALSE if the state of the node should be left unchanged.
 */
static gboolean
Browser_List_Select_Func (GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data)
{
    /* This line will be selected at the end of the event.
     * We store the last ETFile selected, as gtk_tree_selection_get_selected_rows
     * returns the selection, in the ascending line order, instead of the real
     * order of line selection (so we can't displayed the last selected file)
     * FIXME : should generate a list to get the previous selected file if unselected the last selected file */
    if (!path_currently_selected)
    {
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(fileListModel), &iter, path))
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &iter,
                               LIST_FILE_POINTER, &LastBrowserListETFileSelected, -1);
    }else
    {
        LastBrowserListETFileSelected = NULL;
    }
    //g_print(">>>%s -> %d -> %x\n",gtk_tree_path_to_string(path),path_currently_selected,LastBrowserListETFileSelected);

    return TRUE;
}


/*
 * Open up a node on the browser tree
 * Scanning and showing all subdirectories
 */
static void expand_cb (GtkWidget *tree, GtkTreeIter *iter, GtkTreePath *gtreePath, gpointer data)
{
    GFile *dir;
    GFileEnumerator *enumerator;
    gchar *fullpath_file;
    gchar *parentPath;
    gboolean treeScanned;
    gboolean has_subdir = FALSE;
    GtkTreeIter currentIter;
    GtkTreeIter subNodeIter;
    GIcon *icon;

    g_return_if_fail (directoryTreeModel != NULL);

    gtk_tree_model_get(GTK_TREE_MODEL(directoryTreeModel), iter,
                       TREE_COLUMN_FULL_PATH, &parentPath,
                       TREE_COLUMN_SCANNED,   &treeScanned, -1);

    if (treeScanned)
        return;

    dir = g_file_new_for_path (parentPath);
    enumerator = g_file_enumerate_children (dir,
                                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                            G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                            G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL, NULL);

    if (enumerator)
    {
        GFileInfo *childinfo;

        while ((childinfo = g_file_enumerator_next_file (enumerator,
                                                         NULL, NULL))
               != NULL)
        {
            const gchar *name;
            GFile *child;
            gboolean isdir = FALSE;

            name = g_file_info_get_name (childinfo);
            child = g_file_get_child (dir, name);
            fullpath_file = g_file_get_path (child);
            isdir = g_file_info_get_file_type (childinfo) == G_FILE_TYPE_DIRECTORY;

            if (isdir &&
                (BROWSE_HIDDEN_DIR || !g_file_info_get_is_hidden (childinfo)))
            {
                const gchar *dirname_utf8;
                dirname_utf8 = g_file_info_get_display_name (childinfo);

                has_subdir = check_for_subdir (fullpath_file);

                /* Select pixmap according permissions for the directory. */
                icon = get_gicon_for_path (fullpath_file,
                                           ET_PATH_STATE_CLOSED);

                gtk_tree_store_insert_with_values (directoryTreeModel,
                                                   &currentIter, iter,
                                                   G_MAXINT,
                                                   TREE_COLUMN_DIR_NAME,
                                                   dirname_utf8,
                                                   TREE_COLUMN_FULL_PATH,
                                                   fullpath_file,
                                                   TREE_COLUMN_HAS_SUBDIR,
                                                   !has_subdir,
                                                   TREE_COLUMN_SCANNED, FALSE,
                                                   TREE_COLUMN_ICON, icon, -1);

                if (has_subdir)
                {
                    /* Insert a dummy node. */
                    gtk_tree_store_append(directoryTreeModel, &subNodeIter, &currentIter);
                }

                g_object_unref (icon);
            }

            g_free (fullpath_file);
            g_object_unref (childinfo);
            g_object_unref (child);
        }

        g_file_enumerator_close (enumerator, NULL, NULL);
        g_object_unref (enumerator);

        /* Remove dummy node. */
        gtk_tree_model_iter_children (GTK_TREE_MODEL (directoryTreeModel),
                                      &subNodeIter, iter);
        gtk_tree_store_remove (directoryTreeModel, &subNodeIter);
    }

    g_object_unref (dir);
    icon = get_gicon_for_path (parentPath, ET_PATH_STATE_OPEN);

#ifdef G_OS_WIN32
    // set open folder pixmap except on drive (depth == 0)
    if (gtk_tree_path_get_depth(gtreePath) > 1)
    {
        // update the icon of the node to opened folder :-)
        gtk_tree_store_set(directoryTreeModel, iter,
                           TREE_COLUMN_SCANNED, TRUE,
                           TREE_COLUMN_ICON, icon,
                           -1);
    }
#else /* !G_OS_WIN32 */
    // update the icon of the node to opened folder :-)
    gtk_tree_store_set(directoryTreeModel, iter,
                       TREE_COLUMN_SCANNED, TRUE,
                       TREE_COLUMN_ICON, icon,
                       -1);
#endif /* !G_OS_WIN32 */

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(directoryTreeModel),
                                         TREE_COLUMN_DIR_NAME, GTK_SORT_ASCENDING);

    g_object_unref (icon);
    g_free(parentPath);
}

static void collapse_cb (GtkWidget *tree, GtkTreeIter *iter, GtkTreePath *treePath, gpointer data)
{
    GtkTreeIter subNodeIter;
    gchar *path;
    GIcon *icon;
    GFile *file;
    GFileInfo *fileinfo;
    GError *error = NULL;

    g_return_if_fail (directoryTreeModel != NULL);

    gtk_tree_model_get (GTK_TREE_MODEL (directoryTreeModel), iter,
                        TREE_COLUMN_FULL_PATH, &path, -1);

    /* If the directory is not readable, do not delete its children. */
    file = g_file_new_for_path (path);
    g_free (path);
    fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
                                  G_FILE_QUERY_INFO_NONE, NULL, &error);
    g_object_unref (file);

    if (fileinfo)
    {
        if (!g_file_info_get_attribute_boolean (fileinfo,
                                                G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
        {
            g_object_unref (fileinfo);
            return;
        }

        g_object_unref (fileinfo);
    }

    gtk_tree_model_iter_children(GTK_TREE_MODEL(directoryTreeModel),
                                 &subNodeIter, iter);
    while (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(directoryTreeModel), iter))
    {
        gtk_tree_model_iter_children(GTK_TREE_MODEL(directoryTreeModel), &subNodeIter, iter);
        gtk_tree_store_remove(directoryTreeModel, &subNodeIter);
    }

    gtk_tree_model_get (GTK_TREE_MODEL (directoryTreeModel), iter,
                        TREE_COLUMN_FULL_PATH, &path, -1);
    icon = get_gicon_for_path (path, ET_PATH_STATE_OPEN);
    g_free (path);
#ifdef G_OS_WIN32
    // set closed folder pixmap except on drive (depth == 0)
    if(gtk_tree_path_get_depth(treePath) > 1)
    {
        // update the icon of the node to closed folder :-)
        gtk_tree_store_set(directoryTreeModel, iter,
                           TREE_COLUMN_SCANNED, FALSE,
                           TREE_COLUMN_ICON, icon, -1);
    }
#else /* !G_OS_WIN32 */
    // update the icon of the node to closed folder :-)
    gtk_tree_store_set(directoryTreeModel, iter,
                       TREE_COLUMN_SCANNED, FALSE,
                       TREE_COLUMN_ICON, icon, -1);
#endif /* !G_OS_WIN32 */

    /* Insert dummy node only if directory exists. */
    if (error)
    {
        /* Remove the parent (missing) directory from the tree. */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
            gtk_tree_store_remove (directoryTreeModel, iter);
        }

        g_error_free (error);
    }
    else
    {
        gtk_tree_store_append (directoryTreeModel, &subNodeIter, iter);
    }

    g_object_unref (icon);
}

/*
 * Create item of the browser (Entry + Tree + List).
 */
GtkWidget *Create_Browser_Items (GtkWidget *parent)
{
	GtkWidget *VerticalBox;
    GtkWidget *HBox;
    GtkWidget *ScrollWindowDirectoryTree;
    GtkWidget *ScrollWindowFileList;
    GtkWidget *ScrollWindowArtistList;
    GtkWidget *ScrollWindowAlbumList;
    GtkWidget *Label;
    gsize i;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *PopupMenu;
    const gchar *BrowserTree_Titles[] = { N_("Tree") };
    const gchar *BrowserList_Titles[] = { N_("Filename"), N_("Title"),
                                          N_("Artist"), N_("Album Artist"),
                                          N_("Album"), N_("Year"), N_("Disc"),
                                          N_("Track"), N_("Genre"),
                                          N_("Comment"), N_("Composer"),
                                          N_("Original Artist"),
                                          N_("Copyright"), N_("URL"),
                                          N_("Encoded By") };
    const gchar *ArtistList_Titles[] = { N_("Artist"), N_("# Albums"),
                                         N_("# Files") };
    const gchar *AlbumList_Titles[] = { N_("Album"), N_("# Files") };

    VerticalBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_set_border_width(GTK_CONTAINER(VerticalBox),2);


    // HBox for BrowserEntry + BrowserLabel
    HBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_pack_start(GTK_BOX(VerticalBox),HBox,FALSE,TRUE,0);

    /*
     * The entry box for displaying path
     */
    g_assert (BrowserEntryModel == NULL);
    BrowserEntryModel = gtk_list_store_new (MISC_COMBO_COUNT, G_TYPE_STRING);

    BrowserEntryCombo = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(BrowserEntryModel));
    g_object_unref (BrowserEntryModel);
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(BrowserEntryCombo), MISC_COMBO_TEXT);
    /* History list */
    Load_Path_Entry_List(BrowserEntryModel, MISC_COMBO_TEXT);
    //gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(BrowserEntryCombo),2); // Two columns to display paths

    g_signal_connect(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo)))),"activate",G_CALLBACK(Browser_Entry_Activated),NULL);
    gtk_box_pack_start(GTK_BOX(HBox),BrowserEntryCombo,TRUE,TRUE,1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo)))),_("Enter a directory to browse."));

    /*
     * The button to select a directory to browse
     */
    BrowserButton = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(HBox),BrowserButton,FALSE,FALSE,1);
    g_signal_connect_swapped(G_OBJECT(BrowserButton),"clicked",
                             G_CALLBACK(File_Selection_Window_For_Directory),G_OBJECT(gtk_bin_get_child(GTK_BIN(BrowserEntryCombo))));
    gtk_widget_set_tooltip_text(BrowserButton,_("Select a directory to browse."));


    /*
     * The label for displaying number of files in path (without subdirs)
     */
    /* Translators: No files, as in "0 files". */
    BrowserLabel = gtk_label_new(_("No files"));
    gtk_box_pack_start(GTK_BOX(HBox),BrowserLabel,FALSE,FALSE,2);

    /* Browser NoteBook :
     *  - one tab for the BrowserTree
     *  - one tab for the BrowserArtistList and the BrowserAlbumList
     */
    BrowserNoteBook = gtk_notebook_new();
    //gtk_notebook_popup_enable(GTK_NOTEBOOK(BrowserNoteBook));
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(BrowserNoteBook),FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(BrowserNoteBook),FALSE);


    /*
     * The ScrollWindow and the Directory-Tree
     */
    ScrollWindowDirectoryTree = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollWindowDirectoryTree),
                                   GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
    directoryTreeModel = gtk_tree_store_new(TREE_COLUMN_COUNT,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_BOOLEAN,
                                            G_TYPE_BOOLEAN,
                                            G_TYPE_ICON);

    Label = gtk_label_new(_("Tree"));
    gtk_notebook_append_page(GTK_NOTEBOOK(BrowserNoteBook),ScrollWindowDirectoryTree,Label);

    /* The tree view */
    BrowserTree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(directoryTreeModel));
    g_object_unref (directoryTreeModel);
    gtk_container_add(GTK_CONTAINER(ScrollWindowDirectoryTree),BrowserTree);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(BrowserTree), TRUE);

    // Column for the pixbuf + text
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _(BrowserTree_Titles[0]));

    // Cell of the column for the pixbuf
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                       "gicon", TREE_COLUMN_ICON,
                                        NULL);
    // Cell of the column for the text
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                       "text", TREE_COLUMN_DIR_NAME,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(BrowserTree), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    Browser_Tree_Initialize();


    /* Signals */
    g_signal_connect(G_OBJECT(BrowserTree), "row-expanded",  G_CALLBACK(expand_cb),NULL);
    g_signal_connect(G_OBJECT(BrowserTree), "row-collapsed", G_CALLBACK(collapse_cb),NULL);
    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserTree))),
            "changed", G_CALLBACK(Browser_Tree_Node_Selected), NULL);

    g_signal_connect(G_OBJECT(BrowserTree),"key_press_event", G_CALLBACK(Browser_Tree_Key_Press),NULL);

    /* Create Popup Menu on browser tree view */
    PopupMenu = gtk_ui_manager_get_widget(UIManager, "/DirPopup");
    gtk_menu_attach_to_widget (GTK_MENU (PopupMenu), BrowserTree, NULL);
    g_signal_connect (G_OBJECT (BrowserTree), "button-press-event",
                      G_CALLBACK (Browser_Popup_Menu_Handler), PopupMenu);


    /*
     * The ScrollWindows with the Artist and Album Lists
     */

    ArtistAlbumVPaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    Label = gtk_label_new(_("Artist & Album"));
    gtk_notebook_append_page(GTK_NOTEBOOK(BrowserNoteBook),ArtistAlbumVPaned,Label);

    ScrollWindowArtistList = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollWindowArtistList),
                                   GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

    artistListModel = gtk_list_store_new(ARTIST_COLUMN_COUNT,
                                         G_TYPE_STRING, // Stock-id
                                         G_TYPE_STRING,
                                         G_TYPE_UINT,
                                         G_TYPE_UINT,
                                         G_TYPE_POINTER,
                                         PANGO_TYPE_STYLE,
                                         G_TYPE_INT,
                                         GDK_TYPE_COLOR);

    BrowserArtistList = gtk_tree_view_new_with_model(GTK_TREE_MODEL(artistListModel));
    g_object_unref (artistListModel);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(BrowserArtistList), TRUE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserArtistList)),GTK_SELECTION_SINGLE);


    // Artist column
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _(ArtistList_Titles[0]));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                       "stock-id",        ARTIST_PIXBUF,
                                        NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "text",           ARTIST_NAME,
                                        "weight",         ARTIST_FONT_WEIGHT,
                                        "style",          ARTIST_FONT_STYLE,
                                        "foreground-gdk", ARTIST_ROW_FOREGROUND,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(BrowserArtistList), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    // # Albums of Artist column
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _(ArtistList_Titles[1]));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "text",           ARTIST_NUM_ALBUMS,
                                        "weight",         ARTIST_FONT_WEIGHT,
                                        "style",          ARTIST_FONT_STYLE,
                                        "foreground-gdk", ARTIST_ROW_FOREGROUND,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(BrowserArtistList), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    // # Files of Artist column
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _(ArtistList_Titles[2]));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "text",           ARTIST_NUM_FILES,
                                        "weight",         ARTIST_FONT_WEIGHT,
                                        "style",          ARTIST_FONT_STYLE,
                                        "foreground-gdk", ARTIST_ROW_FOREGROUND,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(BrowserArtistList), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserArtistList))),"changed",G_CALLBACK(Browser_Artist_List_Row_Selected),NULL);

    gtk_container_add(GTK_CONTAINER(ScrollWindowArtistList),BrowserArtistList);

    // Create Popup Menu on browser artist list
    PopupMenu = gtk_ui_manager_get_widget(UIManager, "/DirArtistPopup");
    gtk_menu_attach_to_widget (GTK_MENU (PopupMenu), BrowserArtistList, NULL);
    g_signal_connect (G_OBJECT (BrowserArtistList), "button-press-event",
                      G_CALLBACK (Browser_Popup_Menu_Handler), PopupMenu);
    // Not available yet!
    //ui_widget_set_sensitive(MENU_FILE, AM_ARTIST_OPEN_FILE_WITH, FALSE);

    ScrollWindowAlbumList = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollWindowAlbumList),
                                   GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

    albumListModel = gtk_list_store_new (ALBUM_COLUMN_COUNT,
                                         G_TYPE_ICON,
                                         G_TYPE_STRING,
                                         G_TYPE_UINT,
                                         G_TYPE_POINTER,
                                         PANGO_TYPE_STYLE,
                                         G_TYPE_INT,
                                         GDK_TYPE_COLOR);

    BrowserAlbumList = gtk_tree_view_new_with_model(GTK_TREE_MODEL(albumListModel));
    g_object_unref (albumListModel);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(BrowserAlbumList), TRUE);
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(BrowserAlbumList), FALSE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserAlbumList)), GTK_SELECTION_SINGLE);

    // Album column
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _(AlbumList_Titles[0]));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "gicon",
                                         ALBUM_GICON, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "text",           ALBUM_NAME,
                                        "weight",         ALBUM_FONT_WEIGHT,
                                        "style",          ALBUM_FONT_STYLE,
                                        "foreground-gdk", ALBUM_ROW_FOREGROUND,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(BrowserAlbumList), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    // # files column
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _(AlbumList_Titles[1]));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "text",           ALBUM_NUM_FILES,
                                        "weight",         ALBUM_FONT_WEIGHT,
                                        "style",          ALBUM_FONT_STYLE,
                                        "foreground-gdk", ALBUM_ROW_FOREGROUND,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(BrowserAlbumList), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserAlbumList))),"changed",G_CALLBACK(Browser_Album_List_Row_Selected),NULL);
    gtk_container_add(GTK_CONTAINER(ScrollWindowAlbumList),BrowserAlbumList);

    // Create Popup Menu on browser album list
    PopupMenu = gtk_ui_manager_get_widget(UIManager, "/DirAlbumPopup");
    gtk_menu_attach_to_widget (GTK_MENU (PopupMenu), BrowserAlbumList, NULL);
    g_signal_connect (G_OBJECT (BrowserAlbumList), "button-press-event",
                      G_CALLBACK (Browser_Popup_Menu_Handler), PopupMenu);
    // Not available yet!
    //ui_widget_set_sensitive(MENU_FILE, AM_ALBUM_OPEN_FILE_WITH, FALSE);


    gtk_paned_pack1(GTK_PANED(ArtistAlbumVPaned),ScrollWindowArtistList,TRUE,TRUE); // Top side
    gtk_paned_pack2(GTK_PANED(ArtistAlbumVPaned),ScrollWindowAlbumList,TRUE,TRUE);   // Bottom side
    gtk_paned_set_position(GTK_PANED(ArtistAlbumVPaned),PANE_HANDLE_POSITION3);


    /*
     * The ScrollWindow and the List
     */
    ScrollWindowFileList = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollWindowFileList),
                                   GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

    /* The file list */
    fileListModel = gtk_list_store_new (LIST_COLUMN_COUNT,
                                        G_TYPE_STRING, /* Filename. */
                                        G_TYPE_STRING, /* Title tag. */
                                        G_TYPE_STRING, /* Artist tag. */
                                        G_TYPE_STRING, /* Album artist tag. */
                                        G_TYPE_STRING, /* Album tag. */
                                        G_TYPE_STRING, /* Year tag. */
                                        G_TYPE_STRING, /* Disc/CD number tag. */
                                        G_TYPE_STRING, /* Track tag. */
                                        G_TYPE_STRING, /* Genre tag. */
                                        G_TYPE_STRING, /* Comment tag. */
                                        G_TYPE_STRING, /* Composer tag. */
                                        G_TYPE_STRING, /* Orig. artist tag. */
                                        G_TYPE_STRING, /* Copyright tag. */
                                        G_TYPE_STRING, /* URL tag. */
                                        G_TYPE_STRING, /* Encoded by tag. */
                                        G_TYPE_POINTER, /* File pointer. */
                                        G_TYPE_INT, /* File key. */
                                        G_TYPE_BOOLEAN, /* File OtherDir. */
                                        G_TYPE_INT, /* Font weight. */
                                        GDK_TYPE_COLOR, /* Row background. */
                                        GDK_TYPE_COLOR); /* Row foreground. */

    BrowserList = gtk_tree_view_new_with_model(GTK_TREE_MODEL(fileListModel));
    g_object_unref (fileListModel);
    gtk_container_add(GTK_CONTAINER(ScrollWindowFileList), BrowserList);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(BrowserList), TRUE);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(BrowserList), FALSE);


    /* Add columns to tree view. See ET_FILE_LIST_COLUMN. */
    for (i = 0; i <= LIST_FILE_ENCODED_BY; i++)
    {
        guint ascending_sort = 2 * i;
        column = gtk_tree_view_column_new ();
        renderer = gtk_cell_renderer_text_new ();

        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_set_title (column, _(BrowserList_Titles[i]));
        gtk_tree_view_column_set_attributes(column, renderer, "text", i,
                                            "weight", LIST_FONT_WEIGHT,
                                            "background-gdk",
                                            LIST_ROW_BACKGROUND,
                                            "foreground-gdk",
                                            LIST_ROW_FOREGROUND, NULL);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (GTK_TREE_VIEW (BrowserList), column);
        gtk_tree_view_column_set_clickable (column, TRUE);
        g_signal_connect (column, "clicked",
                          G_CALLBACK (et_browser_set_sorting_file_mode),
                          GINT_TO_POINTER (ascending_sort));
    }

    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(BrowserList), FALSE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList)),GTK_SELECTION_MULTIPLE);
    // When selecting a line
    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList)), Browser_List_Select_Func, NULL, NULL);
    // To sort list
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (fileListModel), 0,
                                      Browser_List_Sort_Func, NULL, NULL);
    Browser_List_Refresh_Sort();
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fileListModel), 0, GTK_SORT_ASCENDING);

    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList))),
            "changed", G_CALLBACK(Browser_List_Row_Selected), NULL);
    g_signal_connect(G_OBJECT(BrowserList),"key_press_event", G_CALLBACK(Browser_List_Key_Press),NULL);
    g_signal_connect(G_OBJECT(BrowserList),"button_press_event", G_CALLBACK(Browser_List_Button_Press),NULL);


    /*
     * Create Popup Menu on file list
     */
    PopupMenu = gtk_ui_manager_get_widget(UIManager, "/FilePopup");
    gtk_menu_attach_to_widget (GTK_MENU (PopupMenu), BrowserList, NULL);
    g_signal_connect(G_OBJECT(BrowserList),"button-press-event",
                     G_CALLBACK (Browser_Popup_Menu_Handler), PopupMenu);

    /*
     * The list store for run program combos
     */
    RunProgramModel = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);

    /*
     * The pane for the tree and list
     */
    BrowserHPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(VerticalBox),BrowserHPaned,TRUE,TRUE,0);
    gtk_paned_pack1(GTK_PANED(BrowserHPaned),BrowserNoteBook,TRUE,TRUE);   // Left side
    gtk_paned_pack2(GTK_PANED(BrowserHPaned),ScrollWindowFileList,TRUE,TRUE); // Right side
    gtk_paned_set_position(GTK_PANED(BrowserHPaned),PANE_HANDLE_POSITION2);

    gtk_widget_show_all(VerticalBox);

    /* Set home variable as current path */
    Browser_Update_Current_Path (g_get_home_dir ());

    return VerticalBox;
}

/*
 * et_browser_set_sorting_file_mode:
 * @column: the tree view column to sort
 * @data: the (required) #ET_Sorting_Type, converted to a pointer with
 * #GINT_TO_POINTER
 *
 * Set the #SORTING_FILE_MODE and display appropriate sort indicator when
 * column is clicked.
 */
static void
et_browser_set_sorting_file_mode (GtkTreeViewColumn *column, gpointer data)
{
    gint column_id = SORTING_FILE_MODE / 2;

    if (gtk_tree_view_column_get_sort_indicator (column) == FALSE
        && SORTING_FILE_MODE < SORTING_BY_ASCENDING_CREATION_DATE)
    {
        if (get_sort_order_for_column_id (SORTING_FILE_MODE / 2) == GTK_SORT_DESCENDING)
        {
            gtk_tree_view_column_set_sort_order (get_column_for_column_id (column_id),
                                                 GTK_SORT_ASCENDING);
        }
        gtk_tree_view_column_set_sort_indicator (get_column_for_column_id (column_id),
                                                 FALSE);
    }
    else if (gtk_tree_view_column_get_sort_order (column) == GTK_SORT_ASCENDING)
    {
        gtk_tree_view_column_set_sort_order (column, GTK_SORT_DESCENDING);
    }
    else
    {
        gtk_tree_view_column_set_sort_order (column, GTK_SORT_ASCENDING);
    }

    if (SORTING_FILE_MODE > SORTING_BY_DESCENDING_ENCODED_BY)
    {
        gtk_tree_view_column_set_sort_indicator (column, TRUE);
        gtk_tree_view_column_set_sort_order (column, GTK_SORT_ASCENDING);
    }

    if (gtk_tree_view_column_get_sort_order (column) == GTK_SORT_ASCENDING)
    {
        SORTING_FILE_MODE = GPOINTER_TO_INT (data);
    }
    else
    {
        SORTING_FILE_MODE = GPOINTER_TO_INT (data) + 1;
    }

    gtk_tree_view_column_set_sort_indicator (column, TRUE);

    Browser_List_Refresh_Sort ();
}


/*
 * The window to Rename a directory into the browser.
 */
void Browser_Open_Rename_Directory_Window (void)
{
    GtkWidget *VBox;
    GtkWidget *HBox;
    GtkWidget *Label;
    GtkWidget *Button;
    gchar *directory_parent = NULL;
    gchar *directory_name = NULL;
    gchar *directory_name_utf8 = NULL;
    gchar *address = NULL;
    gchar *string;

    if (RenameDirectoryWindow != NULL)
    {
        gtk_window_present(GTK_WINDOW(RenameDirectoryWindow));
        return;
    }

    /* We get the full path but we musn't display the parent directories */
    directory_parent = g_strdup(BrowserCurrentPath);
    if (!directory_parent || strlen(directory_parent) == 0)
    {
        g_free(directory_parent);
        return;
    }

    // Remove the last '/' in the path if it exists
    if (strlen(directory_parent)>1 && directory_parent[strlen(directory_parent)-1]==G_DIR_SEPARATOR)
        directory_parent[strlen(directory_parent)-1]=0;
    // Get name of the directory to rename (without path)
    address = strrchr(directory_parent,G_DIR_SEPARATOR);
    if (!address) return;
    directory_name = g_strdup(address+1);
    *(address+1) = 0;

    if (!directory_name || strlen(directory_name)==0)
    {
        g_free(directory_name);
        g_free(directory_parent);
        return;
    }

    directory_name_utf8 = filename_to_display(directory_name);

    RenameDirectoryWindow = gtk_dialog_new_with_buttons (_("Rename Directory"),
                                                         GTK_WINDOW (MainWindow),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_STOCK_CANCEL,
                                                         GTK_RESPONSE_CANCEL,
                                                         GTK_STOCK_APPLY,
                                                         GTK_RESPONSE_APPLY,
                                                         NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (RenameDirectoryWindow),
                                     GTK_RESPONSE_APPLY);

    /* We attach useful data to the combobox */
    g_object_set_data(G_OBJECT(RenameDirectoryWindow), "Parent_Directory", directory_parent);
    g_object_set_data(G_OBJECT(RenameDirectoryWindow), "Current_Directory", directory_name);
    g_signal_connect (RenameDirectoryWindow, "response",
                      G_CALLBACK (et_rename_directory_on_response), NULL);

    VBox = gtk_dialog_get_content_area (GTK_DIALOG (RenameDirectoryWindow));
    gtk_container_set_border_width (GTK_CONTAINER (RenameDirectoryWindow),
                                    BOX_SPACING);

    string = g_strdup_printf(_("Rename the directory '%s' to:"),directory_name_utf8);
    Label = gtk_label_new (string);
    g_free(string);
    gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,TRUE,0);
    gtk_label_set_line_wrap(GTK_LABEL(Label),TRUE);

    /* The combobox to rename the directory */
    RenameDirectoryCombo = gtk_combo_box_text_new_with_entry();
    gtk_box_pack_start(GTK_BOX(VBox),RenameDirectoryCombo,FALSE,FALSE,0);
    /* Set the directory into the combobox */
    gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(RenameDirectoryCombo), directory_name_utf8);
    gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(RenameDirectoryCombo), "");
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryCombo))),directory_name_utf8);
    Attach_Popup_Menu_To_Tag_Entries(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryCombo))));

    /* Rename directory : check box + combo box + Status icon */
    HBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BOX_SPACING);
    gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);

    RenameDirectoryWithMask = gtk_check_button_new_with_label(_("Use mask:"));
    gtk_box_pack_start(GTK_BOX(HBox),RenameDirectoryWithMask,FALSE,FALSE,0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RenameDirectoryWithMask),RENAME_DIRECTORY_WITH_MASK);
    gtk_widget_set_tooltip_text(RenameDirectoryWithMask,_("If activated, it will use masks to rename directory."));
    g_signal_connect(G_OBJECT(RenameDirectoryWithMask),"toggled",G_CALLBACK(Rename_Directory_With_Mask_Toggled),NULL);

    // Set up list model which is used by the combobox
    /* Rename directory from mask */
    if (!RenameDirectoryMaskModel)
        RenameDirectoryMaskModel = gtk_list_store_new(MASK_EDITOR_COUNT, G_TYPE_STRING);
    else
        gtk_list_store_clear(RenameDirectoryMaskModel);

    // The combo box to select the mask to apply
    RenameDirectoryMaskCombo = gtk_combo_box_new_with_entry();
    gtk_combo_box_set_model(GTK_COMBO_BOX(RenameDirectoryMaskCombo), GTK_TREE_MODEL(RenameDirectoryMaskModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(RenameDirectoryMaskCombo), MASK_EDITOR_TEXT);
    gtk_widget_set_size_request(RenameDirectoryMaskCombo, 80, -1);

    gtk_box_pack_start(GTK_BOX(HBox),RenameDirectoryMaskCombo,TRUE,TRUE,0);
    gtk_widget_set_tooltip_text(GTK_WIDGET(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryMaskCombo)))),
        _("Select or type in a mask using codes (see Legend in Scanner Window) to rename "
        "the directory from tag fields."));
    // Signal to generate preview (preview of the new directory)
    g_signal_connect_swapped(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryMaskCombo)))),"changed",
        G_CALLBACK(Scan_Rename_Directory_Generate_Preview),NULL);

    // Load masks into the combobox from a file
    Load_Rename_Directory_Masks_List(RenameDirectoryMaskModel, MASK_EDITOR_TEXT, Rename_Directory_Masks);
    if (RENAME_DIRECTORY_DEFAULT_MASK)
    {
        Add_String_To_Combo_List(RenameDirectoryMaskModel, RENAME_DIRECTORY_DEFAULT_MASK);
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryMaskCombo))), RENAME_DIRECTORY_DEFAULT_MASK);
    }else
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(RenameDirectoryMaskCombo), 0);
    }

    // Mask status icon
    // Signal connection to check if mask is correct into the mask entry
    g_signal_connect (gtk_bin_get_child (GTK_BIN (RenameDirectoryMaskCombo)),
                      "changed", G_CALLBACK (entry_check_rename_file_mask),
                      NULL);

    // Preview label
    RenameDirectoryPreviewLabel = gtk_label_new (_("Rename directory preview"));
    gtk_label_set_line_wrap(GTK_LABEL(RenameDirectoryPreviewLabel),TRUE);
    ////gtk_widget_show(FillTagPreviewLabel);
    gtk_box_pack_start(GTK_BOX(VBox),RenameDirectoryPreviewLabel,TRUE,TRUE,0);

    /* Button to save: to rename directory */
    Button = gtk_dialog_get_widget_for_response (GTK_DIALOG (RenameDirectoryWindow),
                                                 GTK_RESPONSE_APPLY);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (RenameDirectoryCombo)),
                              "changed",
                              G_CALLBACK (empty_entry_disable_widget),
                              G_OBJECT (Button));

    gtk_widget_show_all(RenameDirectoryWindow);

    // To initialize the 'Use mask' check button state
    g_signal_emit_by_name(G_OBJECT(RenameDirectoryWithMask),"toggled");

    // To initialize PreviewLabel + MaskStatusIconBox
    g_signal_emit_by_name(G_OBJECT(gtk_bin_get_child(GTK_BIN(RenameDirectoryMaskCombo))),"changed");

    g_free(directory_name_utf8);
}

static void
Destroy_Rename_Directory_Window (void)
{
    if (RenameDirectoryWindow)
    {
        g_free(g_object_get_data(G_OBJECT(RenameDirectoryWindow),"Parent_Directory"));
        g_free(g_object_get_data(G_OBJECT(RenameDirectoryWindow),"Current_Directory"));

        if (RENAME_DIRECTORY_DEFAULT_MASK) g_free(RENAME_DIRECTORY_DEFAULT_MASK);
        RENAME_DIRECTORY_DEFAULT_MASK = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryMaskCombo)))));
        Add_String_To_Combo_List(RenameDirectoryMaskModel, RENAME_DIRECTORY_DEFAULT_MASK);
        Save_Rename_Directory_Masks_List(RenameDirectoryMaskModel, MASK_EDITOR_TEXT);

        RENAME_DIRECTORY_WITH_MASK = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RenameDirectoryWithMask));

        gtk_list_store_clear(RenameDirectoryMaskModel);

        gtk_widget_destroy(RenameDirectoryWindow);
        RenameDirectoryPreviewLabel = NULL;
        RenameDirectoryWindow = NULL;
    }
}

static void
Rename_Directory (void)
{
    DIR   *dir;
    gchar *directory_parent;
    gchar *directory_last_name;
    gchar *directory_new_name;
    gchar *directory_new_name_file;
    gchar *last_path;
    gchar *last_path_utf8;
    gchar *new_path;
    gchar *new_path_utf8;
    gchar *tmp_path;
    gchar *tmp_path_utf8;
    gint   fd_tmp;


    g_return_if_fail (RenameDirectoryWindow != NULL);

    directory_parent    = g_object_get_data(G_OBJECT(RenameDirectoryWindow),"Parent_Directory");
    directory_last_name = g_object_get_data(G_OBJECT(RenameDirectoryWindow),"Current_Directory");

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RenameDirectoryWithMask)))
    {
        // Renamed from mask
        gchar *mask = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryMaskCombo)))));
        directory_new_name = Scan_Generate_New_Directory_Name_From_Mask(ETCore->ETFileDisplayed,mask,FALSE);
        g_free(mask);

    }else
    {
        // Renamed 'manually'
        directory_new_name  = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RenameDirectoryCombo)))));
    }

    /* Check if a name for the directory have been supplied */
    if (!directory_new_name || g_utf8_strlen(directory_new_name, -1) < 1)
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "%s",
                                           _("You must type a directory name"));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Directory Name Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        g_free(directory_new_name);
        return;
    }

    /* Check that we can write the new directory name */
    directory_new_name_file = filename_from_display(directory_new_name);
    if (!directory_new_name_file)
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL  | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Could not convert '%s' into filename encoding."),
                                           directory_new_name);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),_("Please use another name"));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Directory Name Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        g_free(directory_new_name_file);
    }

    g_free (directory_new_name);

    /* If the directory name haven't been changed, we do nothing! */
    if (directory_last_name && directory_new_name_file
    && strcmp(directory_last_name,directory_new_name_file)==0)
    {
        Destroy_Rename_Directory_Window();
        g_free(directory_new_name_file);
        return;
    }

    /* Build the current and new absolute paths */
    last_path = g_strconcat(directory_parent, directory_last_name, NULL);
    last_path_utf8 = filename_to_display(last_path);
    new_path = g_strconcat(directory_parent, directory_new_name_file, NULL);
    new_path_utf8 = filename_to_display(new_path);

    /* Check if the new directory name doesn't already exists, and detect if
     * it's only a case change (needed for vfat) */
    if ( (dir=opendir(new_path))!=NULL )
    {
        GtkWidget *msgdialog;
        //gint response;

        closedir(dir);
        if (strcasecmp(last_path,new_path) != 0)
        {
    // TODO
    //        // The same directory already exists. So we ask if we want to move the files
    //        msg = g_strdup_printf(_("The directory already exists!\n(%s)\nDo you want "
    //            "to move the files?"),new_path_utf8);
    //        msgbox = msg_box_new(_("Confirm"),
    //                             GTK_WINDOW(MainWindow),
    //                             NULL,
    //                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    //                             msg,
    //                             GTK_STOCK_DIALOG_QUESTION,
    //                             GTK_STOCK_NO,  GTK_RESPONSE_NO,
	//                             GTK_STOCK_YES, GTK_RESPONSE_YES,
    //                             NULL);
    //        g_free(msg);
    //        response = gtk_dialog_run(GTK_DIALOG(msgbox));
    //        gtk_widget_destroy(msgbox);
    //
    //        switch (response)
    //        {
    //            case GTK_STOCK_YES:
    //                // Here we must rename all files with the new location, and remove the directory
    //
    //                Rename_File ()
    //
    //                break;
    //            case BUTTON_NO:
    //                break;
    //        }

            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s",
                                               "Cannot rename file");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),_("The directory name '%s' already exists"),new_path_utf8);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));

            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);

            g_free(directory_new_name_file);
            g_free(last_path);
            g_free(last_path_utf8);
            g_free(new_path);
            g_free(new_path_utf8);

            return;
        }
    }

    /* Temporary path (useful when changing only string case) */
    tmp_path = g_strdup_printf("%s.XXXXXX",last_path);
    tmp_path_utf8 = filename_to_display(tmp_path);

    if ( (fd_tmp = mkstemp(tmp_path)) >= 0 )
    {
        close(fd_tmp);
        unlink(tmp_path);
    }

    /* Rename the directory from 'last name' to 'tmp name' */
    if ( rename(last_path,tmp_path)!=0 )
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Cannot rename directory '%s' to '%s'",
                                           last_path_utf8,
                                           tmp_path_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename Directory Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        g_free(directory_new_name_file);
        g_free(last_path);
        g_free(last_path_utf8);
        g_free(new_path);
        g_free(new_path_utf8);
        g_free(tmp_path);
        g_free(tmp_path_utf8);

        return;
    }

    /* Rename the directory from 'tmp name' to 'new name' (final name) */
    if ( rename(tmp_path,new_path)!=0 )
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Cannot rename directory '%s' to '%s",
                                           tmp_path_utf8,
                                           new_path_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename Directory Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        g_free(directory_new_name_file);
        g_free(last_path);
        g_free(last_path_utf8);
        g_free(new_path);
        g_free(new_path_utf8);
        g_free(tmp_path);
        g_free(tmp_path_utf8);

        return;
    }

    ET_Update_Directory_Name_Into_File_List(last_path,new_path);
    Browser_Tree_Rename_Directory(last_path,new_path);

    // To update file path in the browser entry
    if (ETCore->ETFileDisplayedList)
    {
        ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);
    }else
    {
        gchar *tmp = filename_to_display(Browser_Get_Current_Path());
        Browser_Entry_Set_Text(tmp);
        g_free(tmp);
    }

    Destroy_Rename_Directory_Window();
    g_free(last_path);
    g_free(last_path_utf8);
    g_free(new_path);
    g_free(new_path_utf8);
    g_free(tmp_path);
    g_free(tmp_path_utf8);
    g_free(directory_new_name_file);
    Statusbar_Message(_("Directory renamed"),TRUE);
}

static void
Rename_Directory_With_Mask_Toggled (void)
{
    gtk_widget_set_sensitive(GTK_WIDGET(RenameDirectoryCombo),            !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RenameDirectoryWithMask)));
    gtk_widget_set_sensitive(GTK_WIDGET(RenameDirectoryMaskCombo),         gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RenameDirectoryWithMask)));
    gtk_widget_set_sensitive(GTK_WIDGET(RenameDirectoryPreviewLabel),      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RenameDirectoryWithMask)));
}


/*
 * Window where is typed the name of the program to run, which
 * receives the current directory as parameter.
 */
void Browser_Open_Run_Program_Tree_Window (void)
{
    GtkWidget *VBox;
    GtkWidget *HBox;
    GtkWidget *Label;
    GtkWidget *RunProgramComboBox;
    GtkWidget *Button;
    gchar *current_directory = NULL;

    if (RunProgramTreeWindow != NULL)
    {
        gtk_window_present(GTK_WINDOW(RunProgramTreeWindow));
        return;
    }

    // Current directory
    current_directory = g_strdup(BrowserCurrentPath);
    if (!current_directory || strlen(current_directory)==0)
        return;

    RunProgramTreeWindow = gtk_dialog_new_with_buttons (_("Browse Directory With"),
                                                        GTK_WINDOW (MainWindow),
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        GTK_STOCK_CANCEL,
                                                        GTK_RESPONSE_CANCEL,
                                                        GTK_STOCK_EXECUTE,
                                                        GTK_RESPONSE_OK, NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (RunProgramTreeWindow),
                                     GTK_RESPONSE_OK);
    g_signal_connect (RunProgramTreeWindow, "response",
                      G_CALLBACK (et_run_program_tree_on_response), NULL);
    VBox = gtk_dialog_get_content_area (GTK_DIALOG (RunProgramTreeWindow));
    gtk_container_set_border_width (GTK_CONTAINER (RunProgramTreeWindow),
                                    BOX_SPACING);

    Label = gtk_label_new(_("Program to run:"));
    gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,FALSE,0);
    gtk_label_set_line_wrap(GTK_LABEL(Label),TRUE);

    HBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BOX_SPACING);
    gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,2);

    /* The combobox to enter the program to run */
    RunProgramComboBox = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(RunProgramModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(RunProgramComboBox), MISC_COMBO_TEXT);
    gtk_box_pack_start(GTK_BOX(HBox),RunProgramComboBox,TRUE,TRUE,0);
    gtk_widget_set_size_request(GTK_WIDGET(RunProgramComboBox),250,-1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RunProgramComboBox)))),_("Enter the program to run. "
        "It will receive the current directory as parameter."));

    /* History list */
    gtk_list_store_clear(RunProgramModel);
    Load_Run_Program_With_Directory_List(RunProgramModel, MISC_COMBO_TEXT);
    g_signal_connect_swapped(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RunProgramComboBox)))),"activate",
        G_CALLBACK(Run_Program_With_Directory),G_OBJECT(RunProgramComboBox));

    /* The button to Browse */
    Button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(HBox),Button,FALSE,FALSE,0);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked",
                             G_CALLBACK(File_Selection_Window_For_File),G_OBJECT(gtk_bin_get_child(GTK_BIN(RunProgramComboBox))));

    /* We attach useful data to the combobox (into Run_Program_With_Directory) */
    g_object_set_data(G_OBJECT(RunProgramComboBox), "Current_Directory", current_directory);

    /* Button to execute */
    Button = gtk_dialog_get_widget_for_response (GTK_DIALOG (RunProgramTreeWindow),
                                                 GTK_RESPONSE_OK);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked", G_CALLBACK(Run_Program_With_Directory),G_OBJECT(RunProgramComboBox));
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (RunProgramComboBox)),
                              "changed",
                              G_CALLBACK (empty_entry_disable_widget),
                              G_OBJECT (Button));
    g_signal_emit_by_name(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RunProgramComboBox)))),"changed",NULL);

    gtk_widget_show_all(RunProgramTreeWindow);
}

static void
Destroy_Run_Program_Tree_Window (void)
{
    if (RunProgramTreeWindow)
    {
        gtk_widget_destroy(RunProgramTreeWindow);
        RunProgramTreeWindow = NULL;
    }
}

void Run_Program_With_Directory (GtkWidget *combobox)
{
    gchar *program_name;
    gchar *current_directory;
    GList *args_list = NULL;
    gboolean program_ran;

    g_return_if_fail (GTK_IS_COMBO_BOX (combobox));

    program_name      = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combobox)))));
    current_directory = g_object_get_data(G_OBJECT(combobox), "Current_Directory");
#ifdef G_OS_WIN32
    /* On win32 : 'winamp.exe "c:\path\to\dir"' succeed, while 'winamp.exe "c:\path\to\dir\"' fails */
    ET_Win32_Path_Remove_Trailing_Backslash(current_directory);
#endif /* G_OS_WIN32 */

    // List of parameters (here only one! : the current directory)
    args_list = g_list_append(args_list,current_directory);

    program_ran = et_run_program (program_name, args_list);
    g_list_free(args_list);

    if (program_ran)
    {
        // Append newest choice to the drop down list
        Add_String_To_Combo_List(RunProgramModel, program_name);

        // Save list attached to the combobox
        Save_Run_Program_With_Directory_List(RunProgramModel, MISC_COMBO_TEXT);

        Destroy_Run_Program_Tree_Window();
    }
    g_free(program_name);
}

/*
 * Window where is typed the name of the program to run, which
 * receives the current file as parameter.
 */
void Browser_Open_Run_Program_List_Window (void)
{
    GtkWidget *VBox;
    GtkWidget *HBox;
    GtkWidget *Label;
    GtkWidget *RunProgramComboBox;
    GtkWidget *Button;

    if (RunProgramListWindow != NULL)
    {
        gtk_window_present(GTK_WINDOW(RunProgramListWindow));
        return;
    }

    RunProgramListWindow = gtk_dialog_new_with_buttons (_("Open Files With"),
                                                        GTK_WINDOW (MainWindow),
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        GTK_STOCK_CANCEL,
                                                        GTK_RESPONSE_CANCEL,
                                                        GTK_STOCK_EXECUTE,
                                                        GTK_RESPONSE_OK,
                                                        NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (RunProgramListWindow),
                                     GTK_RESPONSE_OK);
    g_signal_connect ((RunProgramListWindow), "response",
                      G_CALLBACK (et_run_program_list_on_response), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (RunProgramListWindow),
                                    BOX_SPACING);

    VBox = gtk_dialog_get_content_area (GTK_DIALOG (RunProgramListWindow));
    gtk_container_set_border_width (GTK_CONTAINER(VBox), BOX_SPACING);

    Label = gtk_label_new(_("Program to run:"));
    gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,TRUE,0);
    gtk_label_set_line_wrap(GTK_LABEL(Label),TRUE);

    HBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BOX_SPACING);
    gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

    /* The combobox to enter the program to run */
    RunProgramComboBox = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(RunProgramModel));
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(RunProgramComboBox),MISC_COMBO_TEXT);
    gtk_box_pack_start(GTK_BOX(HBox),RunProgramComboBox,TRUE,TRUE,0);
    gtk_widget_set_size_request(GTK_WIDGET(RunProgramComboBox),250,-1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RunProgramComboBox)))),_("Enter the program to run. "
        "It will receive the current file as parameter."));

    /* History list */
    gtk_list_store_clear(RunProgramModel);
    Load_Run_Program_With_File_List(RunProgramModel, MISC_COMBO_TEXT);
    g_signal_connect_swapped(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RunProgramComboBox)))),"activate",
        G_CALLBACK(Run_Program_With_Selected_Files),G_OBJECT(RunProgramComboBox));

    /* The button to Browse */
    Button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(HBox),Button,FALSE,FALSE,0);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked",
                             G_CALLBACK(File_Selection_Window_For_File),G_OBJECT(gtk_bin_get_child(GTK_BIN(RunProgramComboBox))));

    /* Button to execute */
    Button = gtk_dialog_get_widget_for_response (GTK_DIALOG (RunProgramListWindow),
                                                 GTK_RESPONSE_OK);
    g_signal_connect_swapped(G_OBJECT(Button),"clicked", G_CALLBACK(Run_Program_With_Selected_Files),G_OBJECT(RunProgramComboBox));
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (RunProgramComboBox)),
                              "changed",
                              G_CALLBACK (empty_entry_disable_widget),
                              G_OBJECT (Button));
    g_signal_emit_by_name(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(RunProgramComboBox)))),"changed",NULL);

    gtk_widget_show_all(RunProgramListWindow);
}

static void
Destroy_Run_Program_List_Window (void)
{
    if (RunProgramListWindow)
    {
        gtk_widget_destroy(RunProgramListWindow);
        RunProgramListWindow = NULL;
    }
}

static void
Run_Program_With_Selected_Files (GtkWidget *combobox)
{
    gchar   *program_name;
    ET_File *ETFile;
    GList   *selected_paths;
    GList *l;
    GList   *args_list = NULL;
    GtkTreeIter iter;
    gboolean program_ran;

    if (!GTK_IS_COMBO_BOX(combobox) || !ETCore->ETFileDisplayedList)
        return;

    // Programe name to run
    program_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combobox)))));

    // List of files to pass as parameters
    selected_paths = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(BrowserList)), NULL);

    for (l = selected_paths; l != NULL; l = g_list_next (l))
    {
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (fileListModel), &iter,
                                     (GtkTreePath *)l->data))
        {
            gtk_tree_model_get(GTK_TREE_MODEL(fileListModel), &iter,
                               LIST_FILE_POINTER, &ETFile,
                               -1);

            args_list = g_list_prepend (args_list,
                                        ((File_Name *)ETFile->FileNameCur->data)->value);
            //args_list = g_list_append(args_list,((File_Name *)ETFile->FileNameCur->data)->value_utf8);
        }
    }

    args_list = g_list_reverse (args_list);
    program_ran = et_run_program (program_name, args_list);

    g_list_free_full (selected_paths, (GDestroyNotify)gtk_tree_path_free);
    g_list_free(args_list);

    if (program_ran)
    {
        // Append newest choice to the drop down list
        //gtk_list_store_prepend(GTK_LIST_STORE(RunProgramModel), &iter);
        //gtk_list_store_set(RunProgramModel, &iter, MISC_COMBO_TEXT, program_name, -1);
        Add_String_To_Combo_List(GTK_LIST_STORE(RunProgramModel), program_name);

        // Save list attached to the combobox
        Save_Run_Program_With_File_List(RunProgramModel, MISC_COMBO_TEXT);

        Destroy_Run_Program_List_Window();
    }
    g_free(program_name);
}

/*
 * empty_entry_disable_widget:
 * @widget: a widget to set sensitive if @entry contains text
 * @entry: the entry for which to test the text
 *
 * Make @widget insensitive if @entry contains no text, or sensitive otherwise.
 */
static void
empty_entry_disable_widget (GtkWidget *widget, GtkEntry *entry)
{
    const gchar *text;

    g_return_if_fail (widget != NULL && entry != NULL);

    text = gtk_entry_get_text (GTK_ENTRY (entry));

    gtk_widget_set_sensitive (widget, text && *text);
}

/*
 * et_rename_directory_on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the rename directory dialog.
 */
static void
et_rename_directory_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_APPLY:
            Rename_Directory ();
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Rename_Directory_Window ();
            break;
        default:
            g_assert_not_reached ();
    }
}

/*
 * et_run_program_tree_on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the run program on directory tree dialog.
 */
static void
et_run_program_tree_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            /* FIXME: Ignored for now. */
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Run_Program_Tree_Window ();
            break;
        default:
            g_assert_not_reached ();
    }
}
/*
 * et_run_program_list_on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the run program on selected file dialog.
 */
static void
et_run_program_list_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            /* FIXME: Ignored for now. */
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Run_Program_List_Window ();
            break;
        default:
            g_assert_not_reached ();
    }
}

/*
 * get_sort_order_for_column_id:
 * @column_id: the column ID for which to set the sort order
 *
 * Gets the sort order for the given column ID from the browser list treeview
 * column.
 *
 * Returns: the sort order for @column_id
 */
GtkSortType
get_sort_order_for_column_id (gint column_id)
{
    return gtk_tree_view_column_get_sort_order (get_column_for_column_id (column_id));
}

/*
 * get_column_for_column_id:
 * @column_id: the column ID of the #GtkTreeViewColumn to fetch
 *
 * Gets the browser list treeview column for the given column ID.
 *
 * Returns: (transfer none): the tree view column corresponding to @column_id
 */
GtkTreeViewColumn *
get_column_for_column_id (gint column_id)
{
    return gtk_tree_view_get_column (GTK_TREE_VIEW (BrowserList), column_id);
}
