#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>
#include <ctype.h>
#include <string.h>
#include "utt_wubi.h"
#include "utt_dashboard.h"

#define TEXT_NUM_CONF "/apps/utt/wubi/jianma/text_num"
#define CLASS_INDEX_CONF "/apps/utt/wubi/jianma/class_index"

static gchar *class_name[] = {
  "一级简码",
};

static struct priv {
  struct utt_wubi *utt;
  struct utt_ui ui;
  struct utt_dashboard *dash;
  gchar *gen_chars;
  gchar *key_press;
  gboolean match;
} _priv;			/* _ means instance */
static struct priv *priv = &_priv;

static gint class_num (void);

/* utils */

static gchar
wubi_jianma_get_current_char ()
{
  return *(priv->gen_chars +
	   utt_class_record_get_current (priv->utt->record) % TEXT_MOD);
}

static void
wubi_jianma_genchars ()
{
  if (priv->gen_chars) {
    g_free (priv->gen_chars);
  }
  priv->gen_chars = wubi_class_gen_jianma_chars (&priv->utt->wubi, DISPLAY_CHAR_NUM);
}

static gboolean
wubi_jianma_genchars_with_check ()
{
  if (utt_class_record_get_current (priv->utt->record) % TEXT_MOD == 0) {
    wubi_jianma_genchars ();
    return TRUE;
  }
  return FALSE;
}

static gint
get_text_num ()
{
  GConfClient *config;
  GConfValue *value;
  gint default_num = MIN_KEYTEXT;

  config = gconf_client_get_default ();
  value = gconf_client_get (config, TEXT_NUM_CONF, NULL);
  if (value && value->type == GCONF_VALUE_INT) {
    default_num = gconf_value_get_int (value);
  }
  g_object_unref (config);
  return default_num;
}

static gboolean
set_text_num (gint num)
{
  GConfClient *config;

  if (num >= MIN_KEYTEXT && num <= MAX_KEYTEXT && num % TEXT_MOD == 0) {
    config = gconf_client_get_default ();
    gconf_client_set_int (config, TEXT_NUM_CONF, num, NULL);
    g_object_unref (config);
    return TRUE;
  }
  return FALSE;
}

static gint
get_class_index ()
{
  GConfClient *config;
  GConfValue *value;
  gint default_index = 0;

  config = gconf_client_get_default ();
  value = gconf_client_get (config, CLASS_INDEX_CONF, NULL);
  if (value && value->type == GCONF_VALUE_INT) {
    default_index = gconf_value_get_int (value);
  }
  g_object_unref (config);
  return default_index;
}

static gboolean
set_class_index (gint index)
{
  GConfClient *config;

  if (index >= 0 && index < class_num ()) {
    config = gconf_client_get_default ();
    gconf_client_set_int (config, CLASS_INDEX_CONF, index, NULL);
    g_object_unref (config);
    return TRUE;
  }
  return FALSE;
}

static gboolean
on_kb_draw_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  cairo_t *cr;
  struct button_pos *pos;

  /* @0 is current page is not the chosen class, draw keyboard only */
  if (!utt_current_page_is_chosen_class (priv->utt)) {
    cr = gdk_cairo_create (event->window);
    cairo_set_source_surface (cr, priv->ui.kb_image, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);
    return TRUE;
  }

  cr = gdk_cairo_create (event->window);
  cairo_set_source_surface (cr, priv->ui.kb_image, 0, 0);
  cairo_paint (cr);
  if (priv->key_press) {
    pos = g_hash_table_lookup (priv->utt->kb_layout.name_ht, priv->key_press);
    if (pos) {
      if (!priv->match) {
	cairo_set_source_rgba (cr, 1, 0, 0, 0.6);
      }
      else {
	cairo_set_source_rgba (cr, 0, 1, 0, 0.3);
      }
      cairo_rectangle (cr, pos->x, pos->y,
		       priv->utt->kb_layout.button_width,
		       priv->utt->kb_layout.button_height);
      cairo_fill (cr);
    }
  }
  cairo_destroy (cr);
  return TRUE;
}

static gboolean
on_ch_draw_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  cairo_t *cr;
  PangoLayout *layout;
  PangoFontDescription *desc;
  gint index = GPOINTER_TO_INT (data);
  gchar *jianma;
  gint width, height;
  gdouble x, y;

  /* @0 if the current page isn't related to the chosen class,
     return, stop propagate the event further*/
  if (!utt_current_page_is_chosen_class (priv->utt)) {
    return TRUE;
  }

  cr = gdk_cairo_create (event->window);
  cairo_set_source_surface (cr, priv->ui.button_image, 0, 0);
  cairo_paint (cr);

  jianma = wubi_class_get_jianma_by_char (priv->gen_chars[index]);

  layout = pango_cairo_create_layout (cr);
  desc = pango_font_description_from_string ("Sans");
  pango_font_description_set_absolute_size (desc, 26 * PANGO_SCALE);
  pango_layout_set_font_description (layout, desc);
  pango_layout_set_text (layout, jianma, -1);
  pango_layout_get_size (layout, &width, &height);
  x = (priv->ui.button_width - ((gdouble)width / PANGO_SCALE)) / 2;
  y = (priv->ui.button_height - ((gdouble)height / PANGO_SCALE)) / 2;
  cairo_move_to (cr, x, y);
  cairo_set_source_rgb (cr, 0, 0, 0);
  pango_cairo_show_layout (cr, layout);

  /* if class end, don't paint color */
  if (utt_class_record_has_begin (priv->utt->record) &&
      utt_class_record_get_current (priv->utt->record) % TEXT_MOD == index) {
    cairo_set_source_rgba (cr, 0, 0, 1, 0.3);
    cairo_rectangle (cr, 0, 0,
		     priv->ui.button_width,
		     priv->ui.button_height);
    cairo_fill (cr);
  }
  g_object_unref (layout);
  pango_font_description_free (desc);
  cairo_destroy (cr);
  return TRUE;
}

static gboolean
on_key_press (GtkWidget *widget, GdkEventKey *event, struct utt_wubi *utt)
{
  struct ui *ui = &priv->utt->ui;
  gunichar unicode;
  gboolean ret;

  /* @0 if class not begin,
     return, stop propagate the event further.
     if the current page isn't related to the chosen class,
     return, stop propagate the event further*/
  if (!utt_class_record_has_begin (priv->utt->record) ||
      !utt_current_page_is_chosen_class (utt)) {
    return TRUE;
  }


  /* @1 handle unprintable keys */
  if (event->keyval == GDK_F2) {			/* help button */
    if (priv->gen_chars) {
      utt_info (utt, "按 %c 键",
		toupper (wubi_jianma_get_current_char ()));
    }
  }

  /* @2 if it's not printable key, return,
     else, record keyname, type++ */
  unicode = gdk_keyval_to_unicode (event->keyval);
  if (!g_unichar_isprint (unicode)) {
    return FALSE;
  }
  priv->key_press = gdk_keyval_name (event->keyval);
  utt_class_record_type_inc (priv->utt->record);

  g_assert (priv->gen_chars);
  /* @3 check if the key have pressed is one character length */
  if (strlen (priv->key_press) == 1) {
    if (tolower(*priv->key_press) == wubi_jianma_get_current_char ()) {
      /* @4 if the key has press equals to the generate key,
	 class advance, current++, and set the match flag TRUE,
	 else, set the match flag FALSE */
      utt_class_record_correct_inc (priv->utt->record);
      priv->match = TRUE;

      /* @5 check if we should end the class,
	 if we should, update ui, run the continue dialog then return */
      if (utt_class_record_end_with_check (priv->utt->record)) {
	gtk_widget_queue_draw (ui->main_window);
	ret = utt_continue_dialog_run (priv->utt);
	if (ret == GTK_RESPONSE_YES) {
	  /* genchars, class begin, update ui */
	  wubi_jianma_genchars ();
	  utt_class_record_begin (priv->utt->record);
	  gtk_widget_queue_draw (ui->main_window);
	  return FALSE;
	}
	else if (ret == GTK_RESPONSE_NO) {
	  /* set subclass_id to NONE, set pause button insensitive */
	  priv->utt->subclass_id = SUBCLASS_TYPE_NONE;
	  gtk_widget_set_sensitive (GTK_WIDGET (ui->pause_button), FALSE);
	}
	return FALSE;
      }
      /* @6 check if we shoud regenerate characters */
      wubi_jianma_genchars_with_check ();
    }
    else {
      priv->match = FALSE;
    }
    /* @ update ui */
    gtk_widget_queue_draw (ui->main_window);
  }
  return FALSE;
}

static void
on_spin_change (GtkSpinButton *button, gpointer user_data)
{
  set_text_num (gtk_spin_button_get_value (button));
}

static GtkWidget *
create_config_page (GtkWidget *dialog)
{
  GtkWidget *vbox, *hbox, *label, *spin;

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  label = gtk_label_new ("简码数目(下次训练时生效):");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  spin = gtk_spin_button_new_with_range (MIN_KEYTEXT, MAX_KEYTEXT, TEXT_MOD);
  g_signal_connect (spin, "value-changed", G_CALLBACK (on_spin_change), NULL);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), get_text_num ());
  gtk_box_pack_end (GTK_BOX (hbox), spin, FALSE, TRUE, 0);
  return vbox;
}

static GtkWidget *
create_main_page ()
{
  GtkWidget *vbox;
  struct utt_wubi *utt = priv->utt;
  GtkWidget *frame, *hbox, *hbox2, *align;
  gchar *path;
  gint i;

  vbox = gtk_vbox_new (FALSE, 0);
  priv->utt = utt;

  frame = gtk_frame_new ("显示区");
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

  /* padding */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
  path = g_build_filename (PKGDATADIR, "button.png", NULL);
  if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
    g_error (G_STRLOC ": %s doesn't exists.", path);
  }
  priv->ui.button_image = cairo_image_surface_create_from_png (path); /* FIXME: need to free */
  g_free (path);
  priv->ui.button_width = cairo_image_surface_get_width (priv->ui.button_image);
  priv->ui.button_height = cairo_image_surface_get_height (priv->ui.button_image);
  for (i = 0; i < DISPLAY_CHAR_NUM; i++) {
    priv->ui.ch_draw[i] = gtk_drawing_area_new ();
    gtk_widget_set_size_request (priv->ui.ch_draw[i],
				 priv->ui.button_width,
				 priv->ui.button_height);
    g_signal_connect (priv->ui.ch_draw[i], "expose-event", G_CALLBACK (on_ch_draw_expose), GINT_TO_POINTER (i));
    hbox2 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
    align = gtk_alignment_new (0.5, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER (align), priv->ui.ch_draw[i]);
    gtk_box_pack_start (GTK_BOX (hbox2), align, TRUE, TRUE, 0);
  }
  /* padding */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);

  priv->ui.kb_draw = gtk_drawing_area_new ();
  path = g_build_filename (PKGDATADIR, "keyboard_yiji.png", NULL);
  if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
    g_error (G_STRLOC ": %s doesn't exists.", path);
  }
  priv->ui.kb_image = cairo_image_surface_create_from_png (path); /* FIXME: need to free */
  g_free (path);
  gtk_widget_add_events (priv->ui.kb_draw, GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events (priv->ui.kb_draw, GDK_KEY_PRESS_MASK);
  /* set can focus and grab focus, so we can receive key press event */
  gtk_widget_set_can_focus (priv->ui.kb_draw, TRUE);
  gtk_widget_set_size_request (priv->ui.kb_draw,
			       cairo_image_surface_get_width (priv->ui.kb_image),
			       cairo_image_surface_get_height (priv->ui.kb_image));
  align = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (align), priv->ui.kb_draw);
  gtk_container_set_border_width (GTK_CONTAINER (align), 4);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  g_signal_connect (priv->ui.kb_draw, "expose-event", G_CALLBACK (on_kb_draw_expose), NULL);
  g_signal_connect (priv->ui.kb_draw, "key-press-event", G_CALLBACK (on_key_press), utt);

  priv->dash = utt_dashboard_new (priv->utt);
  gtk_box_pack_start (GTK_BOX (vbox), priv->dash->align, FALSE, FALSE, 0);
  return vbox;
}

static void
class_clean ()
{
  utt_class_record_end (priv->utt->record);
  if (priv->gen_chars) {
    g_free (priv->gen_chars);
    priv->gen_chars = NULL;
  }
  priv->key_press = NULL;
  priv->match = FALSE;
  if (gtk_main_level () != 0) {
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->dash->progress), "0%");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->dash->progress), 0);
  }
}

static void
class_begin ()
{
  utt_class_record_set_total (priv->utt->record, get_text_num ());
  utt_class_record_set_mode (priv->utt->record, CLASS_ADVANCE_NEED_CORRECT);
  wubi_jianma_genchars ();

  /* @2 class begin, grab keyboard focus, and update all ui */
  utt_class_record_begin (priv->utt->record);
  gtk_widget_grab_focus (priv->ui.kb_draw);
  gtk_widget_queue_draw (priv->utt->ui.main_window);
}

static void
init (gpointer user_data)
{
  priv->utt = user_data;
}

static void
destroy ()
{
}

static gchar *
nth_class_name (gint n)
{
  return class_name[n];
}

static gint
class_num (void)
{
  return G_N_ELEMENTS (class_name);
}

struct utt_plugin wubi_jianma_plugin = {
  .plugin_name = "wubi::jianma",
  .locale_name = "简码",
  .class_num = class_num,
  .nth_class_name = nth_class_name,
  .get_class_index = get_class_index,
  .set_class_index = set_class_index,
  .init = init,
  .destroy = destroy,
  .class_begin = class_begin,
  .class_clean = class_clean,
  .create_main_page = create_main_page,
  .create_config_page = create_config_page,
};
