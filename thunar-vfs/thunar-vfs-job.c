/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

/* implement thunar-vfs-job's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __THUNAR_VFS_JOB_C__
#include <thunar-vfs/thunar-vfs-job.h>

#include <thunar-vfs/thunar-vfs-alias.h>

#include <exo/exo.h>



#define THUNAR_VFS_JOB_SOURCE(source)   ((ThunarVfsJobSource *) (source))
#define THUNAR_VFS_JOB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_VFS_TYPE_JOB, ThunarVfsJobPrivate))



/* Signal identifiers */
enum
{
  ERROR,
  FINISHED,
  LAST_SIGNAL,
};



typedef struct _ThunarVfsJobEmitDetails ThunarVfsJobEmitDetails;
typedef struct _ThunarVfsJobSource      ThunarVfsJobSource;



static void     thunar_vfs_job_class_init       (ThunarVfsJobClass *klass);
static void     thunar_vfs_job_init             (ThunarVfsJob      *job);
static void     thunar_vfs_job_execute          (gpointer           data,
                                                 gpointer           user_data);
static gboolean thunar_vfs_job_source_prepare   (GSource           *source,
                                                 gint              *timeout);
static gboolean thunar_vfs_job_source_check     (GSource           *source);
static gboolean thunar_vfs_job_source_dispatch  (GSource           *source,
                                                 GSourceFunc        callback,
                                                 gpointer           user_data);
static void     thunar_vfs_job_source_finalize  (GSource           *source);



struct _ThunarVfsJobPrivate
{
  volatile ThunarVfsJobEmitDetails *details;
  volatile gboolean                 running;
};

struct _ThunarVfsJobEmitDetails
{
  ThunarVfsJob     *job;
  guint             signal_id;
  GQuark            signal_detail;
  va_list           var_args;
  volatile gboolean pending;
};

struct _ThunarVfsJobSource
{
  GSource       source;
  ThunarVfsJob *job;
};



static GSourceFuncs thunar_vfs_job_source_funcs =
{
  thunar_vfs_job_source_prepare,
  thunar_vfs_job_source_check,
  thunar_vfs_job_source_dispatch,
  thunar_vfs_job_source_finalize,
  NULL,
};

static GObjectClass  *thunar_vfs_job_parent_class;
static guint          job_signals[LAST_SIGNAL];
static GThreadPool   *job_pool = NULL;
static GCond         *job_cond = NULL;
static GMutex        *job_mutex = NULL;
static volatile guint jobs_running = 0;



GType
thunar_vfs_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsJobClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_job_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsJob),
        0,
        (GInstanceInitFunc) thunar_vfs_job_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarVfsJob"), &info, G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



static void
thunar_vfs_job_class_init (ThunarVfsJobClass *klass)
{
  /* add our private data for this class */
  g_type_class_add_private (klass, sizeof (ThunarVfsJobPrivate));

  /* determine the parent class */
  thunar_vfs_job_parent_class = g_type_class_peek_parent (klass);

  /**
   * ThunarVfsJob::error:
   * @job   : a #ThunarVfsJob.
   * @error : a #GError describing the cause.
   *
   * Emitted whenever an error occurs while executing the
   * @job.
   **/
  job_signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsJob::finished:
   * @job : a #ThunarVfsJob.
   *
   * This signal will be automatically emitted once the
   * @job finishes its execution, no matter whether @job
   * completed successfully or was cancelled by the
   * user.
   **/
  job_signals[FINISHED] =
    g_signal_new (I_("finished"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_vfs_job_init (ThunarVfsJob *job)
{
  job->priv = THUNAR_VFS_JOB_GET_PRIVATE (job);
}



static void
thunar_vfs_job_execute (gpointer data,
                        gpointer user_data)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB (data);

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->priv->running);

  /* perform the real work */
  (*THUNAR_VFS_JOB_GET_CLASS (job)->execute) (job);

  /* mark the job as done */
  job->priv->running = FALSE;

  /* wake up the main event loop */
  g_main_context_wakeup (NULL);
}



static gboolean
thunar_vfs_job_source_prepare (GSource *source,
                               gint    *timeout)
{
  if (thunar_vfs_job_source_check (source))
    {
      *timeout = 0;
      return TRUE;
    }
  else
    {
      *timeout = -1;
      return FALSE;
    }
}



static gboolean
thunar_vfs_job_source_check (GSource *source)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB_SOURCE (source)->job;

  /* check if the job is done or has a pending emit */
  return (!job->priv->running || job->priv->details != NULL);
}



static gboolean
thunar_vfs_job_source_dispatch (GSource    *source,
                                GSourceFunc callback,
                                gpointer    user_data)
{
  volatile ThunarVfsJobEmitDetails *details;
  ThunarVfsJob                     *job = THUNAR_VFS_JOB_SOURCE (source)->job;

  /* check if the job is done */
  if (!job->priv->running)
    {
      /* emit the "finished" signal */
      GDK_THREADS_ENTER ();
      g_signal_emit (G_OBJECT (job), job_signals[FINISHED], 0);
      GDK_THREADS_LEAVE ();

      /* drop the source */
      return FALSE;
    }

  /* check if the job has a pending emit */
  if (G_LIKELY (job->priv->details != NULL))
    {
      /* acquire the job mutex */
      g_mutex_lock (job_mutex);

      /* determine and reset the emission details */
      details = job->priv->details;
      job->priv->details = NULL;

      /* emit the signal */
      GDK_THREADS_ENTER ();
      g_signal_emit_valist (job, details->signal_id, details->signal_detail, details->var_args);
      GDK_THREADS_LEAVE ();

      /* tell the other thread, that we're done */
      details->pending = FALSE;
      g_cond_broadcast (job_cond);
      g_mutex_unlock (job_mutex);
    }

  /* keep the source alive */
  return TRUE;
}



static void
thunar_vfs_job_source_finalize (GSource *source)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB_SOURCE (source)->job;

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));

  /* decrement the number of running jobs */
  jobs_running -= 1;

  /* release the job reference */
  GDK_THREADS_ENTER ();
  g_object_unref (G_OBJECT (job));
  GDK_THREADS_LEAVE ();
}



/**
 * thunar_vfs_job_launch:
 * @job : a #ThunarVfsJob.
 *
 * This functions schedules @job to be run as soon
 * as possible, in a separate thread.
 *
 * Return value: a pointer to @job.
 **/
ThunarVfsJob*
thunar_vfs_job_launch (ThunarVfsJob *job)
{
  GSource *source;

  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (!job->priv->running, NULL);
  g_return_val_if_fail (job_pool != NULL, NULL);

  /* allocate and initialize the watch source */
  source = g_source_new (&thunar_vfs_job_source_funcs, sizeof (ThunarVfsJobSource));
  g_source_set_priority (source, G_PRIORITY_HIGH);

  /* connect the source to the job */
  THUNAR_VFS_JOB_SOURCE (source)->job = g_object_ref (G_OBJECT (job));

  /* increment the number of running jobs */
  jobs_running += 1;

  /* mark the job as running */
  job->priv->running = TRUE;

  /* schedule a thread to handle the job */
  g_thread_pool_push (job_pool, job, NULL);

  /* attach the source to the main context */
  g_source_attach (source, NULL);

  /* the main context now keeps the reference */
  g_source_unref (source);

  return job;
}



/**
 * thunar_vfs_job_cancel:
 * @job : a #ThunarVfsJob.
 *
 * Attempts to cancel the operation currently
 * performed by @job. Even after the cancellation
 * of @job, it may still emit signals, so you
 * must take care of disconnecting all handlers
 * appropriately if you cannot handle signals
 * after cancellation.
 **/
void
thunar_vfs_job_cancel (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  job->cancelled = TRUE;
}



/**
 * thunar_vfs_job_emit_valist:
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on @job.
 * @signal_detail : the detail.
 * @var_args      : a list of paramters to be passed to the signal,
 *                  folled by a location for the return value. If
 *                  the return type of the signals is %G_TYPE_NONE,
 *                  the return value location can be omitted.
 *
 * Emits the signal identified by @signal_id on @job in
 * the context of the main thread. This method doesn't
 * return until the signal was emitted.
 **/
void
thunar_vfs_job_emit_valist (ThunarVfsJob *job,
                            guint         signal_id,
                            GQuark        signal_detail,
                            va_list       var_args)
{
  ThunarVfsJobEmitDetails details;

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->priv->details == NULL);
  g_return_if_fail (job->priv->running);

  /* prepare the emission details */
  details.job = job;
  details.signal_id = signal_id;
  details.signal_detail = signal_detail;
  details.pending = TRUE;

  /* copy the variable argument list (portable) */
  G_VA_COPY (details.var_args, var_args);

  /* emit the signal using our source */
  g_mutex_lock (job_mutex);
  job->priv->details = &details;
  g_main_context_wakeup (NULL);
  while (G_UNLIKELY (details.pending))
    g_cond_wait (job_cond, job_mutex);
  g_mutex_unlock (job_mutex);
}



/**
 * thunar_vfs_job_emit:
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on qjob.
 * @signal_detail : the signal detail.
 * @...           : a list of parameters to be passed to the signal.
 *
 * Convenience wrapper for thunar_vfs_job_emit_valist().
 **/
void
thunar_vfs_job_emit (ThunarVfsJob *job,
                     guint         signal_id,
                     GQuark        signal_detail,
                     ...)
{
  va_list var_args;

  va_start (var_args, signal_detail);
  thunar_vfs_job_emit_valist (job, signal_id, signal_detail, var_args);
  va_end (var_args);
}



/**
 * thunar_vfs_job_error:
 * @job   : a #ThunarVfsJob.
 * @error : a #GError describing the error cause.
 *
 * Emits the ::error signal on @job with the given @error. Whether
 * or not the @job continues after emitting the error depends on
 * the particular implementation of @job, but most jobs will
 * terminate instantly after emitting an error.
 **/
void
thunar_vfs_job_error (ThunarVfsJob *job,
                      GError       *error)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (error != NULL && error->message != NULL);

  thunar_vfs_job_emit (job, job_signals[ERROR], 0, error);
}




/**
 * _thunar_vfs_job_init:
 *
 * Initializes the jobs module of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_job_init (void)
{
  g_return_if_fail (job_pool == NULL);

  /* allocate the synchronization entities */
  job_cond = g_cond_new ();
  job_mutex = g_mutex_new ();

  /* allocate the shared thread pool */
  job_pool = g_thread_pool_new (thunar_vfs_job_execute, NULL, 8, FALSE, NULL);
}



/**
 * _thunar_vfs_job_shutdown:
 *
 * Shuts down the jobs module of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_job_shutdown (void)
{
  g_return_if_fail (job_pool != NULL);

  /* wait for all jobs to terminate */
  while (G_UNLIKELY (jobs_running > 0))
    g_main_context_iteration (NULL, TRUE);

  /* release the thread pool */
  g_thread_pool_free (job_pool, FALSE, TRUE);
  job_pool = NULL;

  /* release the synchronization entities */
  g_mutex_free (job_mutex);
  g_cond_free (job_cond);
}



#define __THUNAR_VFS_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
