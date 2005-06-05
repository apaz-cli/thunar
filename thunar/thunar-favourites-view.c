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

#include <thunar/thunar-favourites-model.h>
#include <thunar/thunar-favourites-view.h>



enum
{
  FAVOURITE_ACTIVATED,
  LAST_SIGNAL,
};



static void     thunar_favourites_view_class_init     (ThunarFavouritesViewClass *klass);
static void     thunar_favourites_view_init           (ThunarFavouritesView      *view);
static void     thunar_favourites_view_row_activated  (GtkTreeView               *tree_view,
                                                       GtkTreePath               *path,
                                                       GtkTreeViewColumn         *column);
#if GTK_CHECK_VERSION(2,6,0)
static gboolean thunar_favourites_view_separator_func (GtkTreeModel              *model,
                                                       GtkTreeIter               *iter,
                                                       gpointer                   user_data);
#endif



struct _ThunarFavouritesViewClass
{
  GtkTreeViewClass __parent__;

  /* signals */
  void (*favourite_activated) (ThunarFavouritesView *view,
                               ThunarFile           *file);
};

struct _ThunarFavouritesView
{
  GtkTreeView __parent__;
};



static GObjectClass *parent_class;
static guint         view_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarFavouritesView, thunar_favourites_view, GTK_TYPE_TREE_VIEW);



static void
thunar_favourites_view_class_init (ThunarFavouritesViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;

  parent_class = g_type_class_peek_parent (klass);

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_favourites_view_row_activated;

  /**
   * ThunarFavouritesView:favourite-activated:
   *
   * Invoked whenever a favourite is activated by the user.
   **/
  view_signals[FAVOURITE_ACTIVATED] =
    g_signal_new ("favourite-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFavouritesViewClass, favourite_activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_TYPE_FILE);
}



static void
thunar_favourites_view_init (ThunarFavouritesView *view)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         NULL);
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", THUNAR_FAVOURITES_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_FAVOURITES_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

#if GTK_CHECK_VERSION(2,6,0)
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), thunar_favourites_view_separator_func, NULL, NULL);
#endif
}



static void
thunar_favourites_view_row_activated (GtkTreeView       *tree_view,
                                      GtkTreePath       *path,
                                      GtkTreeViewColumn *column)
{
  ThunarFavouritesView *view = THUNAR_FAVOURITES_VIEW (tree_view);
  GtkTreeModel         *model;
  GtkTreeIter           iter;
  ThunarFile           *file;

  g_return_if_fail (THUNAR_IS_FAVOURITES_VIEW (view));
  g_return_if_fail (path != NULL);

  /* determine the iter for the path */
  model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_get_iter (model, &iter, path);

  /* determine the file for the favourite and invoke the signal */
  file = thunar_favourites_model_file_for_iter (THUNAR_FAVOURITES_MODEL (model), &iter);
  g_signal_emit (G_OBJECT (view), view_signals[FAVOURITE_ACTIVATED], 0, file);

  /* call the row-activated method in the parent class */
  if (GTK_TREE_VIEW_CLASS (parent_class)->row_activated != NULL)
    GTK_TREE_VIEW_CLASS (parent_class)->row_activated (tree_view, path, column);
}



#if GTK_CHECK_VERSION(2,6,0)
static gboolean
thunar_favourites_view_separator_func (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       gpointer      user_data)
{
  gboolean separator;
  gtk_tree_model_get (model, iter, THUNAR_FAVOURITES_MODEL_COLUMN_SEPARATOR, &separator, -1);
  return separator;
}
#endif



/**
 * thunar_favourites_view_new:
 *
 * Allocates a new #ThunarFavouritesView instance and associates
 * it with the default #ThunarFavouritesModel instance.
 *
 * Return value: the newly allocated #ThunarFavouritesView instance.
 **/
GtkWidget*
thunar_favourites_view_new (void)
{
  ThunarFavouritesModel *model;
  GtkWidget             *view;

  model = thunar_favourites_model_get_default ();
  view = g_object_new (THUNAR_TYPE_FAVOURITES_VIEW,
                       "model", model, NULL);
  g_object_unref (G_OBJECT (model));

  return view;
}



/**
 * thunar_favourites_view_select_by_file:
 * @view : a #ThunarFavouritesView instance.
 * @file : a #ThunarFile instance.
 *
 * Looks up the first favourite that refers to @file in @view and selects it.
 * If @file is not present in the underlying #ThunarFavouritesModel, no
 * favourite will be selected afterwards.
 **/
void
thunar_favourites_view_select_by_file (ThunarFavouritesView *view,
                                       ThunarFile           *file)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  g_return_if_fail (THUNAR_IS_FAVOURITES_VIEW (view));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* clear the selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_unselect_all (selection);

  /* try to lookup a tree iter for the given file */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (thunar_favourites_model_iter_for_file (THUNAR_FAVOURITES_MODEL (model), file, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}


