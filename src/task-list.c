#include "task-list.h"
#include "components.h"
#include "data.h"
#include "settings.h"
#include "sidebar-all-row.h"
#include "sidebar-task-list-row.h"
#include "state.h"
#include "task/task.h"
#include "utils.h"
#include "utils/macros.h"

#include <glib/gi18n.h>
#include <string.h>

static void on_task_added(AdwEntryRow *entry, gpointer data);
static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data);
static void on_search_btn_toggle(GtkToggleButton *btn);
static void on_toggle_completed(bool active);
static void on_sort_by(const char *active_id);

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_class_init(ErrandsTaskListClass *class) {}

static void errands_task_list_init(ErrandsTaskList *self) {
  LOG("Creating task list");

  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "width-request", 360, NULL);
  adw_bin_set_child(ADW_BIN(self), tb);

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  self->title = adw_window_title_new("", "");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), self->title);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Sort and Filter menu
  GtkWidget *menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

  GtkWidget *filter_label = gtk_label_new(_("Filters"));
  g_object_set(filter_label, "halign", GTK_ALIGN_START, NULL);
  gtk_widget_add_css_class(filter_label, "heading");
  gtk_widget_add_css_class(filter_label, "dim-label");
  gtk_box_append(GTK_BOX(menu_box), filter_label);

  GtkWidget *show_completed = errands_menu_check_item(
      _("Show Completed"), errands_settings_get("show_completed", SETTING_TYPE_BOOL).b,
      on_toggle_completed);
  gtk_box_append(GTK_BOX(menu_box), show_completed);
  gtk_box_append(GTK_BOX(menu_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

  GtkWidget *sort_label = gtk_label_new(_("Sort By"));
  g_object_set(sort_label, "halign", GTK_ALIGN_START, NULL);
  gtk_widget_add_css_class(sort_label, "heading");
  gtk_widget_add_css_class(sort_label, "dim-label");
  gtk_box_append(GTK_BOX(menu_box), sort_label);

  GtkWidget *sort_by_default = errands_menu_radio_item(
      _("Default"), "default",
      !strcmp(errands_settings_get("sort_by", SETTING_TYPE_STRING).s, "default"), NULL, on_sort_by);
  gtk_box_append(GTK_BOX(menu_box), sort_by_default);

  GtkWidget *sort_by_created = errands_menu_radio_item(
      _("Created"), "created",
      !strcmp(errands_settings_get("sort_by", SETTING_TYPE_STRING).s, "creation"), sort_by_default,
      on_sort_by);
  gtk_box_append(GTK_BOX(menu_box), sort_by_created);

  GtkWidget *sort_by_due = errands_menu_radio_item(
      _("Due"), "due", !strcmp(errands_settings_get("sort_by", SETTING_TYPE_STRING).s, "due"),
      sort_by_default, on_sort_by);
  gtk_box_append(GTK_BOX(menu_box), sort_by_due);

  GtkWidget *sort_by_priority = errands_menu_radio_item(
      _("Priority"), "priority",
      !strcmp(errands_settings_get("sort_by", SETTING_TYPE_STRING).s, "priority"), sort_by_default,
      on_sort_by);
  gtk_box_append(GTK_BOX(menu_box), sort_by_priority);

  GtkWidget *menu_popover = gtk_popover_new();
  g_object_set(menu_popover, "child", menu_box, NULL);

  GtkWidget *menu_btn = gtk_menu_button_new();
  g_object_set(menu_btn, "icon-name", "errands-more-symbolic", "tooltip-text", _("Filter and Sort"),
               "popover", menu_popover, NULL);

  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), menu_btn);

  // Search Bar
  GtkWidget *sb = gtk_search_bar_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), sb);
  self->search_btn = gtk_toggle_button_new();
  g_object_set(self->search_btn, "icon-name", "errands-search-symbolic", "tooltip-text",
               _("Search (Ctrl+F)"), NULL);
  gtk_widget_add_css_class(self->search_btn, "flat");
  g_object_bind_property(self->search_btn, "active", sb, "search-mode-enabled",
                         G_BINDING_BIDIRECTIONAL);
  g_signal_connect(self->search_btn, "toggled", G_CALLBACK(on_search_btn_toggle), NULL);
  errands_add_shortcut(self->search_btn, "<Control>F", "activate");
  adw_header_bar_pack_end(ADW_HEADER_BAR(hb), self->search_btn);
  GtkWidget *se = gtk_search_entry_new();
  g_signal_connect(se, "search-changed", G_CALLBACK(on_task_list_search), NULL);
  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(sb), GTK_EDITABLE(se));
  gtk_search_bar_set_key_capture_widget(GTK_SEARCH_BAR(sb), tb);
  gtk_search_bar_set_child(GTK_SEARCH_BAR(sb), se);

  // Entry
  GtkWidget *task_entry = adw_entry_row_new();
  g_object_set(task_entry, "title", _("Add Task"), "activatable", false, "margin-start", 12,
               "margin-end", 12, NULL);
  gtk_widget_add_css_class(task_entry, "card");
  g_signal_connect(task_entry, "entry-activated", G_CALLBACK(on_task_added), NULL);
  GtkWidget *entry_clamp = adw_clamp_new();
  g_object_set(entry_clamp, "child", task_entry, "tightening-threshold", 300, "maximum-size", 1000,
               "margin-top", 6, "margin-bottom", 6, NULL);
  self->entry = gtk_revealer_new();
  g_object_set(self->entry, "child", entry_clamp, "transition-type",
               GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN, "margin-start", 12, "margin-end", 12, NULL);

  // Tasks Box
  self->task_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(self->task_list, "margin-bottom", 18, NULL);
  LOG("Task List: Loading tasks");
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *data = state.t_data->pdata[i];
    if (!strcmp(data->parent, "") && !data->deleted)
      gtk_box_append(GTK_BOX(self->task_list), GTK_WIDGET(errands_task_new(data)));
  }

  // Task list clamp
  GtkWidget *tbox_clamp = adw_clamp_new();
  g_object_set(tbox_clamp, "tightening-threshold", 300, "maximum-size", 1000, "margin-start", 12,
               "margin-end", 12, NULL);
  adw_clamp_set_child(ADW_CLAMP(tbox_clamp), self->task_list);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "child", tbox_clamp, "propagate-natural-height", true,
               "propagate-natural-width", true, NULL);

  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(vbox), self->entry);
  gtk_box_append(GTK_BOX(vbox), scrl);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);

  // Create task list page in the view stack
  adw_view_stack_add_named(ADW_VIEW_STACK(state.main_window->stack), GTK_WIDGET(self),
                           "errands_task_list_page");

  errands_task_list_sort_recursive(self->task_list);
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

void errands_task_list_add(TaskData *td) {
  gtk_box_append(GTK_BOX(state.task_list->task_list), GTK_WIDGET(errands_task_new(td)));
}

void errands_task_list_update_title() {
  // If no data - show for all tasks
  if (!state.task_list->data) {
    adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), _("All Tasks"));
    // Set completed stats
    int completed = 0;
    int total = 0;
    for (int i = 0; i < state.t_data->len; i++) {
      TaskData *td = state.t_data->pdata[i];
      if (!td->deleted && !td->trash) {
        total++;
        if (td->completed)
          completed++;
      }
    }
    g_autofree char *stats = g_strdup_printf("%s %d / %d", _("Completed:"), completed, total);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
    return;
  }

  // Set name of the list
  adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), state.task_list->data->name);

  // Set completed stats
  int completed = 0;
  int total = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(td->list_uid, state.task_list->data->uid) && !td->deleted && !td->trash) {
      total++;
      if (td->completed)
        completed++;
    }
  }
  char *stats = g_strdup_printf("%s %d / %d", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
  g_free(stats);
}

void errands_task_list_filter_by_completion(GtkWidget *task_list, bool show_completed) {
  g_autoptr(GPtrArray) tasks = get_children(task_list);
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    gtk_revealer_set_reveal_child(GTK_REVEALER(task->revealer),
                                  !task->data->deleted && !task->data->trash &&
                                      (!task->data->completed || show_completed));
    errands_task_list_filter_by_completion(task->sub_tasks, show_completed);
  }
}

void errands_task_list_filter_by_uid(const char *uid) {
  g_autoptr(GPtrArray) tasks = get_children(state.task_list->task_list);
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    if (!strcmp(uid, "") && !task->data->deleted && !task->data->trash) {
      gtk_widget_set_visible(GTK_WIDGET(task), true);
      continue;
    }
    if (!strcmp(uid, task->data->list_uid) && !task->data->deleted && !task->data->trash)
      gtk_widget_set_visible(GTK_WIDGET(task), true);
    else
      gtk_widget_set_visible(GTK_WIDGET(task),
                             !strcmp(task->data->list_uid, uid) && !task->data->deleted);
  }
}

void errands_task_list_filter_by_text(const char *text) {
  LOG("Task List: Filter by text '%s'", text);

  g_autoptr(GPtrArray) tasks = get_children(state.task_list->task_list);
  bool search_all_tasks = !state.task_list->data || !strcmp(state.task_list->data->uid, "");
  // Search all tasks
  if (search_all_tasks) {
    for (int i = 0; i < tasks->len; i++) {
      ErrandsTask *task = tasks->pdata[i];
      bool contains = string_contains(task->data->text, text) ||
                      string_contains(task->data->notes, text) ||
                      string_array_contains(task->data->tags, text);
      gtk_widget_set_visible(GTK_WIDGET(task), !task->data->deleted && contains);
    }
    return;
  }
  // Search for task list uid
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    bool contains = string_contains(task->data->text, text) ||
                    string_contains(task->data->notes, text) ||
                    string_array_contains(task->data->tags, text);
    gtk_widget_set_visible(GTK_WIDGET(task),
                           !task->data->deleted &&
                               !strcmp(task->data->list_uid, state.task_list->data->uid) &&
                               contains);
  }
}

static bool errands_task_list_sorted_by_completion(GtkWidget *task_list) {
  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_last_child(task_list);
  ErrandsTask *prev_task = NULL;
  while (task) {
    prev_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    if (prev_task)
      if (task->data->completed - prev_task->data->completed <= 0)
        return false;
    task = prev_task;
  }
  return true;
}

void errands_task_list_sort_by_completion(GtkWidget *task_list) {
  // Check if the task list is already sorted by completion
  if (errands_task_list_sorted_by_completion(task_list))
    return;

  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_last_child(task_list);

  // Return if there are no tasks
  if (!task)
    return;

  // Skip if last task completed
  ErrandsTask *last_cmp_task = task;
  if (task->data->completed)
    task = last_cmp_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));

  // Sort the rest of the tasks
  while (task) {
    if (task->data->completed) {
      gtk_box_reorder_child_after(GTK_BOX(task_list), GTK_WIDGET(task), GTK_WIDGET(last_cmp_task));
      last_cmp_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    }
    task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
  }
}

// - SORT BY DUE DATE - //

static gint __due_date_sort_func(gconstpointer a, gconstpointer b) {
  ErrandsTask *task_a = *((ErrandsTask **)a);
  ErrandsTask *task_b = *((ErrandsTask **)b);
  g_autoptr(GDateTime) dt_a = parse_date(task_a->data->due_date);
  g_autoptr(GDateTime) dt_b = parse_date(task_b->data->due_date);
  if (!dt_a && dt_b)
    return -1;
  else if (dt_a && !dt_b)
    return 1;
  else if (!dt_a && !dt_b)
    return 0;
  return g_date_time_compare(dt_a, dt_b);
}

void errands_task_list_sort_by_due_date(GtkWidget *task_list) {
  g_autoptr(GPtrArray) children = get_children(task_list);
  if (children->len <= 1)
    return;
  g_ptr_array_sort(children, __due_date_sort_func);
  for (int i = 0; i < children->len; i++) {
    GtkWidget *task = children->pdata[i];
    GtkWidget *first_task = gtk_widget_get_first_child(task_list);
    if (task != first_task)
      gtk_widget_insert_before(task, task_list, first_task);
  }
}

// - SORT BY PRIORITY - //

// Sort tasks by priority
static gint __priority_sort_func(gconstpointer a, gconstpointer b) {
  return ((ErrandsTask *)a)->data->priority > ((ErrandsTask *)b)->data->priority;
}

void errands_task_list_sort_by_priority(GtkWidget *task_list) {
  g_autoptr(GPtrArray) children = get_children(task_list);
  g_ptr_array_sort_values(children, __priority_sort_func);
  for (int i = 0; i < children->len; i++) {
    GtkWidget *task = children->pdata[i];
    GtkWidget *first_task = gtk_widget_get_first_child(task_list);
    if (task != first_task)
      gtk_widget_insert_before(task, task_list, first_task);
  }
}

// - SORT BY CREATION DATE - //

static gint __creation_date_sort_func(gconstpointer a, gconstpointer b) {
  ErrandsTask *task_a = *((ErrandsTask **)a);
  ErrandsTask *task_b = *((ErrandsTask **)b);
  g_autoptr(GDateTime) dt_a = parse_date(task_a->data->created_at);
  g_autoptr(GDateTime) dt_b = parse_date(task_b->data->created_at);
  if (!dt_a && dt_b)
    return -1;
  else if (dt_a && !dt_b)
    return 1;
  else if (!dt_a && !dt_b)
    return 0;
  return g_date_time_compare(dt_a, dt_b);
}

void errands_task_list_sort_by_creation_date(GtkWidget *task_list) {
  g_autoptr(GPtrArray) children = get_children(task_list);
  if (children->len <= 1)
    return;
  g_ptr_array_sort(children, __creation_date_sort_func);
  for (int i = 0; i < children->len; i++) {
    GtkWidget *task = children->pdata[i];
    GtkWidget *first_task = gtk_widget_get_first_child(task_list);
    if (task != first_task)
      gtk_widget_insert_before(task, task_list, first_task);
  }
}

// - ALL SORT - //

void errands_task_list_sort(GtkWidget *task_list) {
  const char *sort_by = errands_settings_get("sort_by", SETTING_TYPE_STRING).s;
  if (!strcmp(sort_by, "manual"))
    return;
  else if (!strcmp(sort_by, "due"))
    errands_task_list_sort_by_due_date(task_list);
  else if (!strcmp(sort_by, "created"))
    errands_task_list_sort_by_creation_date(task_list);
  else if (!strcmp(sort_by, "priority"))
    errands_task_list_sort_by_priority(task_list);
  errands_task_list_sort_by_completion(task_list);
}

void errands_task_list_sort_recursive(GtkWidget *task_list) {
  LOG("Task List: Sort recursive");
  errands_task_list_sort(task_list);
  g_autoptr(GPtrArray) children = get_children(task_list);
  for (int i = 0; i < children->len; i++) {
    ErrandsTask *task = children->pdata[i];
    errands_task_list_sort(task->sub_tasks);
  }
}

void errands_task_list_reload() {
  LOG("Task List: Reload");
  gtk_box_remove_all(state.task_list->task_list);
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *data = state.t_data->pdata[i];
    if (!strcmp(data->parent, "") && !data->deleted)
      gtk_box_append(GTK_BOX(state.task_list->task_list), GTK_WIDGET(errands_task_new(data)));
  }
  errands_task_list_sort_recursive(state.task_list->task_list);
}

// --- SIGNAL HANDLERS --- //

static void on_task_added(AdwEntryRow *entry, gpointer data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  // Skip empty text
  if (!strcmp(text, ""))
    return;

  if (!strcmp(state.task_list->data->uid, ""))
    return;

  TaskData *td = errands_data_add_task((char *)text, state.task_list->data->uid, "");
  ErrandsTask *t = errands_task_new(td);
  gtk_box_prepend(GTK_BOX(state.task_list->task_list), GTK_WIDGET(t));
  errands_task_list_sort(state.task_list->task_list);

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(state.task_list->data->uid));
  errands_sidebar_all_row_update_counter(state.sidebar->all_row);

  errands_task_list_update_title();

  LOG("Add task '%s' to task list '%s'", td->uid, td->list_uid);
}

static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  errands_task_list_filter_by_text(text);
}

static void on_search_btn_toggle(GtkToggleButton *btn) {
  bool active = gtk_toggle_button_get_active(btn);
  LOG("Task List: Toggle search %s", active ? "on" : "off");
  if (state.task_list->data)
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), !active);
  else
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
}

static void on_toggle_completed(bool active) {
  LOG("Task List: Set show completed to '%s'", active ? "on" : "off");
  errands_settings_set("show_completed", SETTING_TYPE_BOOL, &active);
  errands_task_list_filter_by_completion(state.task_list->task_list, active);
}

static void on_sort_by(const char *active_id) {
  LOG("Task List: Set sort by '%s'", active_id);
  errands_settings_set("sort_by", SETTING_TYPE_STRING, (void *)active_id);
  if (strcmp(active_id, "default"))
    errands_task_list_sort_recursive(state.task_list->task_list);
}
