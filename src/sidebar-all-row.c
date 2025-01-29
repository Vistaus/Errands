#include "data.h"
#include "sidebar-rows.h"
#include "utils.h"

#include <glib/gi18n.h>

#include <stddef.h>

G_DEFINE_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_all_row_class_init(ErrandsSidebarAllRowClass *class) {}

static void errands_sidebar_all_row_init(ErrandsSidebarAllRow *self) {
  LOG("All Row: Create");

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(self), box);
  gtk_box_append(GTK_BOX(box), gtk_image_new_from_icon_name("errands-all-tasks-symbolic"));
  gtk_box_append(GTK_BOX(box), gtk_label_new(C_("All tasks", "All")));

  // Counter
  self->counter = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), self->counter);
  g_object_set(self->counter, "hexpand", true, "halign", GTK_ALIGN_END, NULL);
  gtk_widget_add_css_class(self->counter, "dim-label");
  gtk_widget_add_css_class(self->counter, "caption");
}

ErrandsSidebarAllRow *errands_sidebar_all_row_new() {
  return g_object_new(ERRANDS_TYPE_SIDEBAR_ALL_ROW, NULL);
}

void errands_sidebar_all_row_update_counter(ErrandsSidebarAllRow *row) {
  size_t len = 0;
  for (size_t i = 0; i < tdata->len; i++) {
    TaskData *td = tdata->pdata[i];
    if (!task_data_get_deleted(td) && !task_data_get_trash(td) && !task_data_get_completed(td))
      len++;
  }
  g_autofree char *num = g_strdup_printf("%zu", len);
  gtk_label_set_label(GTK_LABEL(row->counter), len > 0 ? num : "");
}
