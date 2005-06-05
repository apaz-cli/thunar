/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-favourites-pane.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-window.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void thunar_window_class_init    (ThunarWindowClass  *klass);
static void thunar_window_init          (ThunarWindow       *window);
static void thunar_window_dispose       (GObject            *object);
static void thunar_window_get_property  (GObject            *object,
                                         guint               prop_id,
                                         GValue             *value,
                                         GParamSpec         *pspec);
static void thunar_window_set_property  (GObject            *object,
                                         guint               prop_id,
                                         const GValue       *value,
                                         GParamSpec         *pspec);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  GtkWidget   *side_pane;
  GtkWidget   *view;
  GtkWidget   *statusbar;

  ThunarFile  *current_directory;
};



static GObjectClass *parent_class;



G_DEFINE_TYPE (ThunarWindow, thunar_window, GTK_TYPE_WINDOW);



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  GObjectClass *gobject_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_window_dispose;
  gobject_class->get_property = thunar_window_get_property;
  gobject_class->set_property = thunar_window_set_property;

  /**
   * ThunarWindow:current-directory:
   *
   * The directory currently displayed within this #ThunarWindow
   * or %NULL.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_DIRECTORY,
                                   g_param_spec_object ("current-directory",
                                                        _("Current directory"),
                                                        _("The directory currently displayed within this window"),
                                                        THUNAR_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}



static void
thunar_window_init (ThunarWindow *window)
{
  ThunarListModel *model;
  GtkWidget       *vbox;
  GtkWidget       *paned;
  GtkWidget       *swin;

  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), _("Thunar"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);
  
  paned = g_object_new (GTK_TYPE_HPANED, "border-width", 6, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);
  gtk_widget_show (paned);

  window->side_pane = thunar_favourites_pane_new ();
  exo_mutual_binding_new (G_OBJECT (window), "current-directory",
                          G_OBJECT (window->side_pane), "current-directory");
  gtk_paned_pack1 (GTK_PANED (paned), window->side_pane, FALSE, FALSE);
  gtk_widget_show (window->side_pane);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_IN);
  gtk_paned_pack2 (GTK_PANED (paned), swin, TRUE, FALSE);
  gtk_widget_show (swin);

  window->view = thunar_icon_view_new ();
  g_signal_connect_swapped (G_OBJECT (window->view), "change-directory",
                            G_CALLBACK (thunar_window_set_current_directory), window);
  gtk_container_add (GTK_CONTAINER (swin), window->view);
  gtk_widget_show (window->view);

  model = thunar_list_model_new ();
  thunar_view_set_list_model (THUNAR_VIEW (window->view), model);
  g_object_unref (G_OBJECT (model));

  window->statusbar = thunar_statusbar_new ();
  exo_binding_new (G_OBJECT (window->view), "statusbar-text",
                   G_OBJECT (window->statusbar), "text");
  gtk_box_pack_start (GTK_BOX (vbox), window->statusbar, FALSE, FALSE, 0);
  gtk_widget_show (window->statusbar);
}



static void
thunar_window_dispose (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);
  thunar_window_set_current_directory (window, NULL);
  G_OBJECT_CLASS (parent_class)->dispose (object);
}



static void
thunar_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_window_get_current_directory (window));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_window_set_property (GObject            *object,
                            guint               prop_id,
                            const GValue       *value,
                            GParamSpec         *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_window_set_current_directory (window, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_window_new:
 *
 * Allocates a new #ThunarWindow instance, which isn't
 * associated with any directory.
 *
 * Return value: the newly allocated #ThunarWindow instance.
 **/
GtkWidget*
thunar_window_new (void)
{
  return g_object_new (THUNAR_TYPE_WINDOW, NULL);
}



/**
 * thunar_window_get_current_directory:
 * @window : a #ThunarWindow instance.
 * 
 * Queries the #ThunarFile instance, which represents the directory
 * currently displayed within @window. %NULL is returned if @window
 * is not currently associated with any directory.
 *
 * Return value: the directory currently displayed within @window or %NULL.
 **/
ThunarFile*
thunar_window_get_current_directory (ThunarWindow *window)
{
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
  return window->current_directory;
}



/**
 * thunar_window_set_current_directory:
 * @window            : a #ThunarWindow instance.
 * @current_directory : the new directory or %NULL.
 **/
void
thunar_window_set_current_directory (ThunarWindow *window,
                                     ThunarFile   *current_directory)
{
  ThunarListModel *model;
  ThunarFolder    *folder;
  GtkWidget       *dialog;
  GdkPixbuf       *icon;
  GError          *error = NULL;

  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  // TODO: We need a better error-recovery here!

  /* check if we already display the requests directory */
  if (G_UNLIKELY (window->current_directory == current_directory))
    return;

  /* disconnect from the previously active directory */
  if (G_LIKELY (window->current_directory != NULL))
    g_object_unref (G_OBJECT (window->current_directory));

  window->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    g_object_ref (G_OBJECT (window->current_directory));

  /* set window title/icon to the selected directory */
  if (G_LIKELY (current_directory != NULL))
    {
      icon = thunar_file_load_icon (current_directory, 48);
      gtk_window_set_icon (GTK_WINDOW (window), icon);
      gtk_window_set_title (GTK_WINDOW (window),
                            thunar_file_get_display_name (current_directory));
      g_object_unref (G_OBJECT (icon));
    }

  /* setup the folder for the view */
  model = thunar_view_get_list_model (THUNAR_VIEW (window->view));
  if (G_LIKELY (current_directory != NULL))
    {
      /* try to open the directory */
      folder = thunar_folder_get_for_file (current_directory, &error);
      if (G_LIKELY (folder != NULL))
        {
          thunar_list_model_set_folder (model, folder);
          g_object_unref (G_OBJECT (folder));
        }
      else
        {
          /* error condition, reset the folder */
          thunar_list_model_set_folder (model, NULL);

          /* make sure the window is shown */
          gtk_widget_show_now (GTK_WIDGET (window));

          /* display an error dialog */
          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failed to open directory `%s': %s",
                                           thunar_file_get_display_name (current_directory),
                                           error->message);
          gtk_dialog_run (GTK_DIALOG (dialog));
          gtk_widget_destroy (dialog);

          /* free the error details */
          g_error_free (error);
        }
    }
  else
    {
      /* just reset the folder, so nothing is displayed */
      thunar_list_model_set_folder (model, NULL);
    }

  /* tell everybody that we have a new "current-directory" */
  g_object_notify (G_OBJECT (window), "current-directory");
}



