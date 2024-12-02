#pragma once

#include "data.h"

#include <gtk/gtk.h>

#define ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW (errands_sidebar_task_list_row_get_type())

G_DECLARE_FINAL_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, ERRANDS,
                     SIDEBAR_TASK_LIST_ROW, GtkListBoxRow)

struct _ErrandsSidebarTaskListRow {
  GtkListBoxRow parent_instance;
  GtkColorDialog *color_dialog;
  GtkWidget *color_btn;
  GtkWidget *label;
  GtkWidget *counter;
  // GtkEventController *hover_ctrl;
  TaskListData *data;
};

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(TaskListData *data);
void errands_sidebar_task_list_row_update_counter(ErrandsSidebarTaskListRow *row);
void errands_sidebar_task_list_row_update_title(ErrandsSidebarTaskListRow *row);
ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid);
void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row,
                                               gpointer user_data);
