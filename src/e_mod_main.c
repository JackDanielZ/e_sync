#define EFL_BETA_API_SUPPORT
#define EFL_EO_API_SUPPORT

#include <time.h>
#ifndef STAND_ALONE
#include <e.h>
#else
#include <Elementary.h>
#endif
#include "e_mod_main.h"

#define _EET_ENTRY "config"

typedef struct
{
   char *name;
   char *path; /* Path to the repo data in the server */
} Repo_Server;

typedef struct
{
   char *name;
   Eina_List *servers; /* List of Repo_Server */
} Repository;

typedef struct
{
   Eina_List *repos; /* List of Repository */
} Config;

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
#ifndef STAND_ALONE
   E_Gadcon_Client *gcc;
   E_Gadcon_Popup *popup;
#endif
   Evas_Object *o_icon;
   Eo *main, *main_box, *toolbar;
   Eo *table;
} Instance;

#ifndef STAND_ALONE
static E_Module *_module = NULL;
#endif

static void _box_update(Instance *);

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
        evas_object_size_hint_align_set(bt, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(bt, 0.0, 0.0);
        evas_object_show(bt);
        if (wref) efl_wref_add(bt, wref);
        if (cb_func) evas_object_smart_callback_add(bt, "clicked", cb_func, cb_data);
     }
   elm_object_text_set(bt, text);
   elm_object_part_content_set(bt, "icon", icon);
   return bt;
}

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

#if 0
static void
_youtube_download(Instance *inst, Playlist_Item *pli)
{
   char cmd[1024];
   sprintf(cmd, "/tmp/yt-%s.opus", pli->id);
   pli->download_path = eina_stringshare_add(cmd);
   sprintf(cmd,
         "youtube-dl --audio-format opus --no-part -x \"http://youtube.com/watch?v=%s\" -o %s",
         pli->id, pli->download_path);
   pli->download_exe = ecore_exe_pipe_run(cmd, ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_ERROR, pli);
   efl_key_data_set(pli->download_exe, "Instance", inst);
   efl_key_data_set(pli->download_exe, "Type", "YoutubeDownload");
   efl_wref_add(pli->download_exe, &(pli->download_exe));
}

static void
_playlist_start_bt_clicked(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   char request[256];
   Playlist *pl = data;
   Instance *inst = efl_key_data_get(obj, "Instance");
   if (!strcmp(pl->platform->type, "youtube"))
     {
        sprintf(request, "http://www.youtube.com/watch?v=%s&list=%s", pl->first_id, pl->list_id);
     }
   Efl_Net_Dialer_Http *dialer = _dialer_create(EINA_TRUE, NULL, _playlist_html_downloaded);
   efl_key_data_set(dialer, "Instance", inst);
   efl_key_data_set(dialer, "Playlist", pl);
   efl_net_dialer_dial(dialer, request);
   inst->cur_playlist = pl;
}
#endif

static Eina_Bool
_exe_error_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   //Ecore_Exe_Event_Data *event_data = (Ecore_Exe_Event_Data *)event;
   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_exe_output_cb(void *data EINA_UNUSED, int _type EINA_UNUSED, void *event)
{
#if 0
   Ecore_Exe_Event_Data *event_data = (Ecore_Exe_Event_Data *)event;
   const char *buf = event_data->data;
   unsigned int size = event_data->size;
   Ecore_Exe *exe = event_data->exe;
   Instance *inst = efl_key_data_get(exe, "Instance");
   const char *type = efl_key_data_get(exe, "Type");
   if (!strcmp(type, "YoutubeDownload"))
     {
        Playlist_Item *pli = ecore_exe_data_get(exe);
        while (!pli->is_playable && (buf = strstr(buf, "[download] ")))
          {
             buf += 11;
             float percent = strtod(buf, NULL);
             if (percent)
               {
                  pli->is_playable = EINA_TRUE;
                  if (pli == inst->item_to_play) _media_play_set(inst, pli, EINA_TRUE);
               }
          }
     }
   else if (!strcmp(type, "YoutubeGetFullPlaylist"))
     {
        Download_Buffer *dbuf = efl_key_data_get(exe, "Download_Buffer");
        if (size > (dbuf->max_len - dbuf->len))
          {
             dbuf->max_len = dbuf->len + size;
             dbuf->data = realloc(dbuf->data, dbuf->max_len + 1);
             memcpy(dbuf->data+dbuf->len, buf, size);
             dbuf->len += size;
             dbuf->data[dbuf->max_len] = '\0';
          }
     }
#endif

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_exe_end_cb(void *data EINA_UNUSED, int _type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Del *event_info = (Ecore_Exe_Event_Del *)event;
   Ecore_Exe *exe = event_info->exe;
   Instance *inst = efl_key_data_get(exe, "Instance");
   const char *type = efl_key_data_get(exe, "Type");
   if (!type) return ECORE_CALLBACK_PASS_ON;
   if (event_info->exit_code) return ECORE_CALLBACK_DONE;
#if 0
   if (!strcmp(type, "YoutubeDownload"))
     {
        Playlist_Item *pli = ecore_exe_data_get(exe);
        pli->is_playable = EINA_TRUE;
        if (pli == inst->item_to_play) _media_play_set(inst, pli, EINA_TRUE);
     }
   else if (!strcmp(type, "YoutubeGetFullPlaylist"))
     {
        Playlist *pl = ecore_exe_data_get(exe);
        Download_Buffer *dbuf = efl_key_data_get(exe, "Download_Buffer");
        char *id_str = strstr(dbuf->data, "\"id\": \"");
        Eina_List *old_items = pl->items;
        pl->items = NULL;
        while (id_str)
          {
             Eina_List *itr;
             Playlist_Item *pli, *found_old = NULL;
             id_str += 7;
             char *end_str = strchr(id_str, '\"');
             Eina_Stringshare *id = eina_stringshare_add_length(id_str, end_str-id_str);
             EINA_LIST_FOREACH(old_items, itr, pli)
               {
                  if (!found_old && pli->id == id) found_old = pli;
               }
             if (found_old)
               {
                  pli = found_old;
                  elm_object_item_del(pli->gl_item);
               }
             else
               {
                  char request[256];
                  pli = calloc(1, sizeof(*pli));
                  pli->id = id;
                  pli->inst = inst;
                  sprintf(request,
                        "https://www.youtube.com/oembed?url=https://www.youtube.com/watch?v=%s&format=json",
                        id);
                  Efl_Net_Dialer_Http *dialer = _dialer_create(EINA_TRUE, NULL, _playlist_item_json_downloaded);
                  efl_key_data_set(dialer, "Instance", inst);
                  efl_key_data_set(dialer, "Playlist_Item", pli);
                  efl_net_dialer_dial(dialer, request);
               }
             pl->items = eina_list_append(pl->items, pli);
             id_str = strstr(id_str, "\"id\": \"");
          }
        _box_update(inst);
     }
#endif
   return ECORE_CALLBACK_DONE;
}

#ifdef STAND_ALONE
static void
_repo_add_win_ok_cb(void *data, Evas_Object *bt, void *event_info EINA_UNUSED)
{
   Eo *win = data;
   Eo *name_ent = efl_key_data_get(bt, "name");
   Instance *inst = efl_key_data_get(bt, "Instance");

   const char *name = elm_entry_entry_get(name_ent);

   if (name)
     {
        Repository *r = calloc(1, sizeof(*r));
        r->name = strdup(name);
        _config->repos = eina_list_append(_config->repos, r);
        _config_save();
        _box_update(inst);
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
_repo_add_win_show_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Eo *win, *bx1, *bx2, *name_ent, *o;

   win = efl_add(EFL_UI_WIN_CLASS, inst->main_box,
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
   efl_key_data_set(o, "Instance", inst);
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
   Instance *inst = efl_key_data_get(bt, "Instance");

   const char *name = elm_entry_entry_get(name_ent);
   const char *path = elm_entry_entry_get(path_ent);

   if (name && path)
     {
        Repo_Server *rs = calloc(1, sizeof(*rs));
        rs->name = strdup(name);
        rs->path = strdup(path);
        r->servers = eina_list_append(r->servers, rs);
        _config_save();
        _box_update(inst);
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
_server_add_win_show_cb(void *data, Evas_Object *menu, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
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
   efl_key_data_set(o, "Instance", inst);
   efl_key_data_set(o, "Repository", efl_key_data_get(menu, "Repository"));
   elm_box_pack_end(bx2, _button_create(bx2, "Cancel", NULL, NULL, _server_add_win_cancel_cb, win));

   efl_gfx_entity_size_set(win, EINA_SIZE2D(300, 50));
}
#endif

static void
_repo_menu_show(void *data, Evas_Object *bt, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Repository *r = efl_key_data_get(bt, "Repository");
   Eina_Position2D pos = efl_gfx_entity_position_get(bt);
   Eo *menu = elm_menu_add(elm_win_get(bt));
   efl_key_data_set(menu, "Repository", r);
   elm_menu_move(menu, pos.x, pos.y);
#ifdef STAND_ALONE
   elm_menu_item_add(menu, NULL, NULL, "Add a server", _server_add_win_show_cb, inst);
#endif
   efl_gfx_entity_visible_set(menu, EINA_TRUE);
}

static void
_box_update(Instance *inst)
{
   Eina_List *itr;
   Eo *o;

   if (!inst->main_box) return;

   if (!inst->toolbar)
     {
        Eo *tb = elm_toolbar_add(inst->main_box);
        elm_toolbar_item_append(tb, "refresh", NULL, NULL, NULL);
#ifdef STAND_ALONE
        elm_toolbar_item_append(tb, NULL, "Add repository", _repo_add_win_show_cb, inst);
#endif
        evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, 0.0);
        evas_object_show(tb);
        elm_box_pack_end(inst->main_box, tb);
        efl_wref_add(tb, &inst->toolbar);
     }

   if (!inst->table)
     {
        Repository *r;
        int row = 0, i = 5, max_col = 1;
        Eo *tb = elm_table_add(inst->main_box);
        elm_table_padding_set(tb, 10, 50);
        evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(tb);
        elm_box_pack_end(inst->main_box, tb);
        EINA_LIST_FOREACH(_config->repos, itr, r)
          {
             int c = eina_list_count(r->servers);
             if (max_col < c) max_col = c;
          }
        elm_table_pack(tb, _label_create(tb, "Repositories", NULL), 0, 0, 1, 1);
        elm_table_pack(tb, _label_create(tb, "Servers", NULL), 1, 0, max_col, 1);
        row++;
        while (i--)
          {
        EINA_LIST_FOREACH(_config->repos, itr, r)
          {
             int col = 0;
             Eo *bt;
             Eina_List *itr2;
             Repo_Server *rs;
             o = _box_create(tb, EINA_TRUE, NULL);
             elm_box_pack_end(o, _label_create(o, r->name, NULL));
             bt = _button_create(o, "...", NULL, NULL, _repo_menu_show, inst);
             efl_key_data_set(bt, "Repository", r);
             elm_box_pack_end(o, bt);
             elm_table_pack(tb, o, col++, row, 1, 1);
             EINA_LIST_FOREACH(r->servers, itr2, rs)
               {
                  elm_table_pack(tb, _label_create(tb, rs->name, NULL), col++, row, 1, 1);
               }
             row++;
          }
          }
        efl_wref_add(tb, &inst->table);
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

#ifndef STAND_ALONE
static void
_popup_del(Instance *inst)
{
   E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_popup_del_cb(void *obj)
{
   _popup_del(e_object_data_get(obj));
}

static void
_popup_comp_del_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   Instance *inst = data;

   E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_button_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst;
   Evas_Event_Mouse_Down *ev;

   inst = data;
   ev = event_info;
   if (ev->button == 1)
     {
        if (!inst->popup)
          {
             Evas_Object *o;
             inst->popup = e_gadcon_popup_new(inst->gcc, 0);

             inst->main = e_comp->elm;
             o = elm_box_add(e_comp->elm);
             evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
             evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_show(o);
             efl_wref_add(o, &inst->main_box);

             _box_update(inst);

             e_gadcon_popup_content_set(inst->popup, inst->main_box);
             e_comp_object_util_autoclose(inst->popup->comp_object,
                   _popup_comp_del_cb, NULL, inst);
             e_gadcon_popup_show(inst->popup);
             e_object_data_set(E_OBJECT(inst->popup), inst);
             E_OBJECT_DEL_SET(inst->popup, _popup_del_cb);
          }
     }
}

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Instance *inst;
   E_Gadcon_Client *gcc;
   char buf[4096];

//   printf("TRANS: In - %s\n", __FUNCTION__);

   inst = _instance_create();

   snprintf(buf, sizeof(buf), "%s/icon.edj", e_module_dir_get(_module));

   inst->o_icon = edje_object_add(gc->evas);
   if (!e_theme_edje_object_set(inst->o_icon,
				"base/theme/modules/sync",
                                "modules/sync/main"))
      edje_object_file_set(inst->o_icon, buf, "modules/sync/main");
   evas_object_show(inst->o_icon);

   gcc = e_gadcon_client_new(gc, name, id, style, inst->o_icon);
   gcc->data = inst;
   inst->gcc = gcc;

   evas_object_event_callback_add(inst->o_icon, EVAS_CALLBACK_MOUSE_DOWN,
				  _button_cb_mouse_down, inst);

   ecore_event_handler_add(ECORE_EXE_EVENT_ERROR, _exe_error_cb, inst);
   ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _exe_output_cb, inst);
   ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _exe_end_cb, inst);

   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   _instance_delete(gcc->data);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   e_gadcon_client_aspect_set(gcc, 32, 16);
   e_gadcon_client_min_size_set(gcc, 32, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return "Sync";
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[4096];

   if (!_module) return NULL;

   snprintf(buf, sizeof(buf), "%s/e-module.edj", e_module_dir_get(_module));

   o = edje_object_add(evas);
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   char buf[32];
   static int id = 0;
   sprintf(buf, "%s.%d", client_class->name, ++id);
   return eina_stringshare_add(buf);
}

EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Sync"
};

static const E_Gadcon_Client_Class _gc_class =
{
   GADCON_CLIENT_CLASS_VERSION, "sync",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL, NULL
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

EAPI void *
e_modapi_init(E_Module *m)
{
//   printf("TRANS: In - %s\n", __FUNCTION__);
   ecore_init();
   ecore_con_init();
   ecore_con_url_init();
   efreet_init();

   _module = m;
   _config_load();
   e_gadcon_provider_register(&_gc_class);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
//   printf("TRANS: In - %s\n", __FUNCTION__);
   e_gadcon_provider_unregister(&_gc_class);

   _module = NULL;
   efreet_shutdown();
   ecore_con_url_shutdown();
   ecore_con_shutdown();
   ecore_shutdown();
   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   //e_config_domain_save("module.sync", conf_edd, cpu_conf);
   return 1;
}
#else
int main(int argc, char **argv)
{
   Instance *inst;

   elm_init(argc, argv);

   _config_load();
   inst = _instance_create();

   elm_policy_set(ELM_POLICY_EXIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   Eo *win = elm_win_add(NULL, "Sync", ELM_WIN_BASIC);
   elm_win_maximized_set(win, EINA_TRUE);
   elm_win_autodel_set(win, EINA_TRUE);
   inst->main = win;

   Eo *bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   Eo *o = elm_box_add(win);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(o);
   elm_win_resize_object_add(win, o);
   efl_wref_add(o, &inst->main_box);

   evas_object_resize(win, 480, 480);
   evas_object_show(win);

   ecore_event_handler_add(ECORE_EXE_EVENT_ERROR, _exe_error_cb, inst);
   ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _exe_output_cb, inst);
   ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _exe_end_cb, inst);

   _box_update(inst);

   elm_run();

   _instance_delete(inst);
   elm_shutdown();
   return 0;
}
#endif
