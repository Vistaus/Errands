#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_TOOLBAR (errands_task_toolbar_get_type())

G_DECLARE_FINAL_TYPE(ErrandsTaskToolbar, errands_task_toolbar, ERRANDS, TASK_TOOLBAR, AdwBin)

struct _ErrandsTaskToolbar {
  AdwBin parent_instance;
  struct _ErrandsTask *task; // Avoid circular import
  GtkWidget *date_btn;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
};

ErrandsTaskToolbar *errands_task_toolbar_new(struct _ErrandsTask *task);
void errands_task_toolbar_update_date_btn(ErrandsTaskToolbar *tb);
