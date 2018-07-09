#define EFL_BETA_API_SUPPORT
#define EFL_EO_API_SUPPORT

#include <time.h>
#include <Elementary.h>
#include "e_mod_main.h"

#define _EET_ENTRY "config"

typedef struct
{
   char *name;
   char *path; /* Path to the repo data in the server */
   Eina_Bool is_local : 1;
} Repo_Server;

typedef struct
{
   char *name;
   Eina_List *servers; /* List of Repo_Server */
   Ecore_Exe *sync_exe;
   Eo *term_box; /* Terminal */
   Eo *term_text;
   Eo *sync_delete_bt;
   Eo *sync_no_delete_bt;
} Repository;

typedef struct
{
   Eina_List *repos; /* List of Repository */
} Config;

typedef struct
{
   Repository *r;
   Repo_Server *src;
   Repo_Server *dst;
} Transfert;

static Config *_config = NULL;

static Eet_Data_Descriptor *_config_edd = NULL;

typedef struct
{
   char *data;
   unsigned int max_len;
   unsigned int len;
} Download_Buffer;

typedef struct
{
   Evas_Object *o_icon;
   Eo *main, *main_box, *toolbar;
   Eo *table;
} Instance;

static Instance *_inst = NULL;

static char _hostname[100] = {0};
static void _box_update();

static void
_config_eet_load()
{
   Eet_Data_Descriptor *repo_edd, *server_edd;
   if (_config_edd) return;
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Repo_Server);
   server_edd = eet_data_descriptor_stream_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_BASIC(server_edd, Repo_Server, "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(server_edd, Repo_Server, "path", path, EET_T_STRING);

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Repository);
   repo_edd = eet_data_descriptor_stream_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_BASIC(repo_edd, Repository, "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_LIST(repo_edd, Repository, "servers", servers, server_edd);

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Config);
   _config_edd = eet_data_descriptor_stream_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_LIST(_config_edd, Config, "repos", repos, repo_edd);
}

static void
_config_save()
{
   char path[1024];
   sprintf(path, "%s/e_sync/config", efreet_config_home_get());
   _config_eet_load();
   Eet_File *file = eet_open(path, EET_FILE_MODE_WRITE);
   eet_data_write(file, _config_edd, _EET_ENTRY, _config, EINA_TRUE);
   eet_close(file);
}

static Eina_Bool
_mkdir(const char *dir)
{
   if (!ecore_file_exists(dir))
     {
        Eina_Bool success = ecore_file_mkdir(dir);
        if (!success)
          {
             printf("Cannot create a config folder \"%s\"\n", dir);
             return EINA_FALSE;
          }
     }
   return EINA_TRUE;
}

static void
_config_load()
{
   char path[1024];

   sprintf(path, "%s/e_sync", efreet_config_home_get());
   if (!_mkdir(path)) return;

   sprintf(path, "%s/e_sync/config", efreet_config_home_get());
   _config_eet_load();
   Eet_File *file = eet_open(path, EET_FILE_MODE_READ);
   if (!file)
     {
        _config = calloc(1, sizeof(Config));
     }
   else
     {
        _config = eet_data_read(file, _config_edd, _EET_ENTRY);
        eet_close(file);
     }

   Eina_List *itr;
   Repository *r;
   EINA_LIST_FOREACH(_config->repos, itr, r)
     {
        Eina_List *itr2;
        Repo_Server *rs;
        EINA_LIST_FOREACH(r->servers, itr2, rs)
          {
             if (!strcmp(rs->name, _hostname)) rs->is_local = EINA_TRUE;
          }
     }
   _config_save();
}

static Eo *
_label_create(Eo *parent, const char *text, Eo **wref)
{
   Eo *label = wref ? *wref : NULL;
   if (!label)
     {
        label = elm_label_add(parent);
        evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(label, 0.0, 0.0);
        evas_object_show(label);
        if (wref) efl_wref_add(label, wref);
     }
   elm_object_text_set(label, text);
   return label;
}

static Eo *
_button_create(Eo *parent, const char *text, Eo *icon, Eo **wref, Evas_Smart_Cb cb_func, void *cb_data)
{
   Eo *bt = wref ? *wref : NULL;
   if (!bt)
     {
        bt = elm_button_add(parent);
        evas_object_size_hint_align_set(bt, EVAS_HINT_FILL, 0.0);
        evas_object_size_hint_weight_set(bt, 0.0, 0.0);
        evas_object_show(bt);
        if (wref) efl_wref_add(bt, wref);
        if (cb_func) evas_object_smart_callback_add(bt, "clicked", cb_func, cb_data);
     }
   elm_object_text_set(bt, text);
   elm_object_part_content_set(bt, "icon", icon);
   return bt;
}

#if 0
static Eo *
_icon_create(Eo *parent, const char *path, Eo **wref)
{
   Eo *ic = wref ? *wref : NULL;
   if (!ic)
     {
        ic = elm_icon_add(parent);
        elm_icon_standard_set(ic, path);
        evas_object_show(ic);
        if (wref) efl_wref_add(ic, wref);
     }
   return ic;
}

static Eo *
_check_create(Eo *parent, Eina_Bool enable, Eo **wref, Evas_Smart_Cb cb_func, void *cb_data)
{
   Eo *o = wref ? *wref : NULL;
   if (!o)
     {
        o = elm_check_add(parent);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(o, 0.0, 0.0);
        elm_check_selected_set(o, enable);
        evas_object_show(o);
        if (wref) efl_wref_add(o, wref);
        if (cb_func) evas_object_smart_callback_add(o, "changed", cb_func, cb_data);
     }
   return o;
}
#endif

static Eo *
_entry_create(Eo *parent, const char *text, Eo **wref)
{
   Eo *o = wref ? *wref : NULL;
   if (!o)
     {
        o = elm_entry_add(parent);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(o);
        elm_object_text_set(o, text);
        if (wref) efl_wref_add(o, wref);
     }
   return o;
}

static Eo *
_box_create(Eo *parent, Eina_Bool horiz, Eo **wref)
{
   Eo *o = elm_box_add(parent);
   elm_box_horizontal_set(o, horiz);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   efl_gfx_entity_visible_set(o, EINA_TRUE);
   if (wref) efl_wref_add(o, wref);
   return o;
}

static Eo *
_hoversel_create(Eo *parent, const char *text, Evas_Smart_Cb sel_cb, void *data)
{
   Eo *o = elm_hoversel_add(parent);
   elm_hoversel_hover_parent_set(o, parent);
   elm_object_text_set(o, text);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   efl_gfx_entity_visible_set(o, EINA_TRUE);
   if (sel_cb)
      evas_object_smart_callback_add(o, "selected", sel_cb, data);
   return o;
}

static Eina_Bool
_exe_error_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   //Ecore_Exe_Event_Data *event_data = (Ecore_Exe_Event_Data *)event;
   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_exe_output_cb(void *data EINA_UNUSED, int _type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Data *event_data = (Ecore_Exe_Event_Data *)event;
   const char *buf = event_data->data;
   unsigned int size = event_data->size;
   Ecore_Exe *exe = event_data->exe;
   const char *type = efl_key_data_get(exe, "Type");
   if (!strcmp(type, "RSyncDry") || !strcmp(type, "RSync"))
     {
        Transfert *t = ecore_exe_data_get(exe);
        Eina_Strbuf *sbuf = eina_strbuf_new();
        eina_strbuf_append_printf(sbuf, "%.*s", size, buf);
        eina_strbuf_replace_all(sbuf, "\n", "<br/>");
        eina_strbuf_replace_all(sbuf, "&", "");
        elm_entry_entry_append(t->r->term_text, eina_strbuf_string_get(sbuf));
        eina_strbuf_free(sbuf);
        elm_entry_cursor_end_set(t->r->term_text);
     }

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_exe_end_cb(void *data EINA_UNUSED, int _type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Del *event_info = (Ecore_Exe_Event_Del *)event;
   Ecore_Exe *exe = event_info->exe;
   const char *type = efl_key_data_get(exe, "Type");
   if (!type) return ECORE_CALLBACK_PASS_ON;
   if (!strcmp(type, "RSyncDry") || !strcmp(type, "RSync"))
     {
        Transfert *t = ecore_exe_data_get(exe);
        elm_entry_cursor_end_set(t->r->term_text);
     }
   if (event_info->exit_code) return ECORE_CALLBACK_DONE;
   return ECORE_CALLBACK_DONE;
}

static void
_repo_add_win_ok_cb(void *data, Evas_Object *bt, void *event_info EINA_UNUSED)
{
   Eo *win = data;
   Eo *name_ent = efl_key_data_get(bt, "name");

   const char *name = elm_entry_entry_get(name_ent);

   if (name)
     {
        Repository *r = calloc(1, sizeof(*r));
        r->name = strdup(name);
        _config->repos = eina_list_append(_config->repos, r);
        _config_save();
        _box_update();
     }

   efl_del(win);
}

static void
_repo_add_win_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eo *win = data;
   efl_del(win);
}

static void
_repo_add_win_show_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eo *win, *bx1, *bx2, *name_ent, *o;

   win = efl_add(EFL_UI_WIN_CLASS, _inst->main_box,
         efl_text_set(efl_added, "Add repository"),
         efl_ui_win_type_set(efl_added, EFL_UI_WIN_DIALOG_BASIC),
         efl_ui_win_autodel_set(efl_added, EINA_TRUE));

   bx1 = _box_create(win, EINA_FALSE, NULL);
   efl_content_set(win, bx1);

   bx2 = _box_create(win, EINA_TRUE, NULL);
   elm_box_pack_end(bx1, bx2);
   elm_box_pack_end(bx2, _label_create(bx2, "Name: ", NULL));
   o = _entry_create(bx2, NULL, NULL);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_box_pack_end(bx2, o);
   name_ent = o;

   bx2 = _box_create(win, EINA_TRUE, NULL);
   elm_box_pack_end(bx1, bx2);
   o = _button_create(bx2, "Ok", NULL, NULL, _repo_add_win_ok_cb, win);
   elm_box_pack_end(bx2, o);
   efl_key_data_set(o, "name", name_ent);
   elm_box_pack_end(bx2, _button_create(bx2, "Cancel", NULL, NULL, _repo_add_win_cancel_cb, win));

   efl_gfx_entity_size_set(win, EINA_SIZE2D(300, 50));
}

static void
_server_add_win_ok_cb(void *data, Evas_Object *bt, void *event_info EINA_UNUSED)
{
   Eo *win = data;
   Eo *name_ent = efl_key_data_get(bt, "name");
   Eo *path_ent = efl_key_data_get(bt, "path");
   Repository *r = efl_key_data_get(bt, "Repository");

   const char *name = elm_entry_entry_get(name_ent);
   const char *path = elm_entry_entry_get(path_ent);

   if (name && path)
     {
        Repo_Server *rs = calloc(1, sizeof(*rs));
        rs->name = strdup(name);
        rs->path = strdup(path);
        r->servers = eina_list_append(r->servers, rs);
        _config_save();
        _box_update();
        efl_del(win);
     }
}

static void
_server_add_win_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eo *win = data;
   efl_del(win);
}

static void
_server_add_win_show_cb(void *data EINA_UNUSED, Evas_Object *menu, void *event_info EINA_UNUSED)
{
   Eo *win, *bx1, *bx2, *name_ent, *path_ent, *o;

   win = efl_add(EFL_UI_WIN_CLASS, elm_win_get(menu),
         efl_text_set(efl_added, "Add server"),
         efl_ui_win_type_set(efl_added, EFL_UI_WIN_DIALOG_BASIC),
         efl_ui_win_autodel_set(efl_added, EINA_TRUE));

   bx1 = _box_create(win, EINA_FALSE, NULL);
   efl_content_set(win, bx1);

   bx2 = _box_create(win, EINA_TRUE, NULL);
   elm_box_pack_end(bx1, bx2);
   elm_box_pack_end(bx2, _label_create(bx2, "Name: ", NULL));
   o = _entry_create(bx2, NULL, NULL);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_box_pack_end(bx2, o);
   name_ent = o;

   bx2 = _box_create(win, EINA_TRUE, NULL);
   elm_box_pack_end(bx1, bx2);
   elm_box_pack_end(bx2, _label_create(bx2, "Path: ", NULL));
   o = _entry_create(bx2, NULL, NULL);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_box_pack_end(bx2, o);
   path_ent = o;

   bx2 = _box_create(win, EINA_TRUE, NULL);
   elm_box_pack_end(bx1, bx2);
   o = _button_create(bx2, "Ok", NULL, NULL, _server_add_win_ok_cb, win);
   elm_box_pack_end(bx2, o);
   efl_key_data_set(o, "name", name_ent);
   efl_key_data_set(o, "path", path_ent);
   efl_key_data_set(o, "Repository", efl_key_data_get(menu, "Repository"));
   elm_box_pack_end(bx2, _button_create(bx2, "Cancel", NULL, NULL, _server_add_win_cancel_cb, win));

   efl_gfx_entity_size_set(win, EINA_SIZE2D(300, 50));
}

static void
_sync_with_deletion_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   char cmd[1024];
   Transfert *t = data;
   Repository *r = t->r;
   efl_del(r->sync_no_delete_bt);
   efl_del(r->sync_delete_bt);
   elm_entry_entry_set(r->term_text, "");
   if (t->src->is_local || t->dst->is_local)
     {
        if (t->src->is_local)
           sprintf(cmd, "rsync -avzhP --delete-after %s/ %s:%s",
                 t->src->path, t->dst->name, t->dst->path);
        else
           sprintf(cmd, "rsync -avzhP --delete-after %s:%s/ %s",
                 t->src->name, t->src->path, t->dst->path);
     }
   else
     {
        sprintf(cmd, "ssh %s 'rsync -avzhP --delete-after %s/ %s:%s'",
              t->src->name, t->src->path,
              t->dst->name, t->dst->path);
     }
   printf("%s\n", cmd);
   r->sync_exe = ecore_exe_pipe_run(cmd, ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_ERROR, t);
   efl_key_data_set(r->sync_exe, "Type", "RSync");
   efl_wref_add(r->sync_exe, &(r->sync_exe));
}

static void
_sync_without_deletion_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   char cmd[1024];
   Transfert *t = data;
   Repository *r = t->r;
   efl_del(r->sync_no_delete_bt);
   efl_del(r->sync_delete_bt);
   elm_entry_entry_set(r->term_text, "");
   if (t->src->is_local || t->dst->is_local)
     {
        if (t->src->is_local)
           sprintf(cmd, "rsync -avzhP %s/ %s:%s",
                 t->src->path, t->dst->name, t->dst->path);
        else
           sprintf(cmd, "rsync -avzhP %s:%s/ %s",
                 t->src->name, t->src->path, t->dst->path);
     }
   else
     {
        sprintf(cmd, "ssh %s 'rsync -avzhP %s/ %s:%s'",
              t->src->name, t->src->path,
              t->dst->name, t->dst->path);
     }
   printf("%s\n", cmd);
   r->sync_exe = ecore_exe_pipe_run(cmd, ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_ERROR, t);
   efl_key_data_set(r->sync_exe, "Type", "RSync");
   efl_wref_add(r->sync_exe, &(r->sync_exe));
}

static void
_sync_cancel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Transfert *t = data;
   Repository *r = t->r;
   ecore_exe_kill(r->sync_exe);
   elm_table_unpack(_inst->table, r->term_box);
   efl_del(r->term_box);
}

static void
_push_to_server(void *data EINA_UNUSED, Evas_Object *hs EINA_UNUSED, void *event_info)
{
   char cmd[1024];
   Eo *hsi = event_info, *bx;
   Transfert *t = efl_key_data_get(hsi, "transfert");
   Repository *r = t->r;
   if (r->sync_exe) return;
   if (t->src->is_local || t->dst->is_local)
     {
        if (t->src->is_local)
           sprintf(cmd, "rsync -avzhP --dry-run %s/ %s:%s",
                 t->src->path, t->dst->name, t->dst->path);
        else
           sprintf(cmd, "rsync -avzhP --dry-run %s:%s/ %s",
                 t->src->name, t->src->path, t->dst->path);
     }
   else
     {
        sprintf(cmd, "ssh %s 'rsync -avzhP --dry-run %s/ %s:%s'",
              t->src->name, t->src->path,
              t->dst->name, t->dst->path);
     }
   printf("%s\n", cmd);
   r->sync_exe = ecore_exe_pipe_run(cmd, ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_ERROR, t);
   efl_key_data_set(r->sync_exe, "Type", "RSyncDry");
   efl_wref_add(r->sync_exe, &(r->sync_exe));
   if (r->term_box) efl_del(r->term_box);
   r->term_box = _box_create(_inst->main_box, EINA_FALSE, &r->term_box);
   r->term_text = _entry_create(r->term_box, NULL, &r->term_text);
   elm_entry_scrollable_set(r->term_text, EINA_TRUE);
   elm_entry_editable_set(r->term_text, EINA_FALSE);
   elm_box_pack_end(r->term_box, r->term_text);
   bx = _box_create(r->term_box, EINA_TRUE, NULL);
   elm_box_pack_end(r->term_box, bx);
   elm_box_pack_end(bx, _button_create(bx, "Sync (with deletion)", NULL,
            &r->sync_delete_bt, _sync_with_deletion_cb, t));
   elm_box_pack_end(bx, _button_create(bx, "Sync (without deletion)", NULL,
            &r->sync_no_delete_bt, _sync_without_deletion_cb, t));
   elm_box_pack_end(bx, _button_create(bx, "Cancel", NULL, NULL, _sync_cancel, t));
   _box_update();
}

static void
_box_update()
{
   Eina_List *itr;

   if (!_inst->main_box) return;

   efl_del(_inst->toolbar);
   efl_del(_inst->table);

   if (!_inst->toolbar)
     {
        Eo *tb = elm_toolbar_add(_inst->main_box);
        elm_toolbar_item_append(tb, "refresh", NULL, NULL, NULL);
        elm_toolbar_item_append(tb, NULL, "Add repository", _repo_add_win_show_cb, _inst);
        evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, 0.0);
        evas_object_show(tb);
        elm_box_pack_end(_inst->main_box, tb);
        efl_wref_add(tb, &_inst->toolbar);
     }

   if (!_inst->table)
     {
        Repository *r;
        int row = 0, max_col = 1;
        Eo *tb = elm_table_add(_inst->main_box);
        elm_table_homogeneous_set(tb, EINA_TRUE);
        evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(tb);
        elm_box_pack_end(_inst->main_box, tb);
        EINA_LIST_FOREACH(_config->repos, itr, r)
          {
             int c = eina_list_count(r->servers);
             if (max_col < c) max_col = c;
          }
        elm_table_pack(tb, _label_create(tb, "Repositories", NULL), 0, 0, 1, 1);
        elm_table_pack(tb, _label_create(tb, "Servers", NULL), 1, 0, max_col, 1);
        row++;
        EINA_LIST_FOREACH(_config->repos, itr, r)
          {
             int col = 0;
             Eina_List *itr2;
             Repo_Server *rs;
             Eo *hs = _hoversel_create(_inst->main_box, r->name, NULL, NULL), *hsi;
             hsi = elm_hoversel_item_add(hs, "Add a server",
                   "list-add", ELM_ICON_STANDARD, _server_add_win_show_cb, NULL);
             efl_key_data_set(hsi, "Repository", r);
             elm_table_pack(tb, hs, col++, row, 1, 1);
             EINA_LIST_FOREACH(r->servers, itr2, rs)
               {
                  Eina_List *itr3;
                  Repo_Server *rs2;
                  hs = _hoversel_create(_inst->main_box, rs->name, NULL, NULL);
                  efl_key_data_set(hs, "source_server", rs);
                  efl_key_data_set(hs, "Repository", r);
                  elm_table_pack(tb, hs, col, row, 1, 1);
                  EINA_LIST_FOREACH(r->servers, itr3, rs2)
                    {
                       char str[256];
                       Transfert *t = calloc(1, sizeof(*t));
                       t->r = r;
                       t->src = rs;
                       t->dst = rs2;
                       if (rs == rs2) continue;
                       sprintf(str, "Push to %s", rs2->name);
                       hsi = elm_hoversel_item_add(hs, str,
                             "go-up", ELM_ICON_STANDARD, _push_to_server, NULL);
                       efl_key_data_set(hsi, "transfert", t);
                    }
                  col++;
               }
             if (r->term_box)
               {
                  elm_table_pack(tb, r->term_box, 0, row+1, max_col+1, 6);
                  row+=6;
               }
             row++;
          }
        efl_wref_add(tb, &_inst->table);
     }
}

static Instance *
_instance_create()
{
   Instance *inst = calloc(1, sizeof(Instance));
   return inst;
}

static void
_instance_delete(Instance *inst)
{
   if (inst->o_icon) evas_object_del(inst->o_icon);
   if (inst->main_box) evas_object_del(inst->main_box);

   free(inst);
}

int main(int argc, char **argv)
{
   gethostname(_hostname, sizeof(_hostname) - 1);
   elm_init(argc, argv);

   _config_load();
   _inst = _instance_create();

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   Eo *win = elm_win_add(NULL, "Sync", ELM_WIN_BASIC);
//   elm_win_maximized_set(win, EINA_TRUE);
   elm_win_autodel_set(win, EINA_TRUE);
   _inst->main = win;

   Eo *bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   Eo *o = elm_box_add(win);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(o);
   elm_win_resize_object_add(win, o);
   efl_wref_add(o, &_inst->main_box);

   evas_object_resize(win, 480, 480);
   evas_object_show(win);

   ecore_event_handler_add(ECORE_EXE_EVENT_ERROR, _exe_error_cb, NULL);
   ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _exe_output_cb, NULL);
   ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _exe_end_cb, NULL);

   _box_update();

   elm_run();

   _instance_delete(_inst);
   elm_shutdown();
   return 0;
}
