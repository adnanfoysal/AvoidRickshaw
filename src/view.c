#include <Elementary.h>
#include <app_preference.h>
#include "avoidrickshaw.h"
#include "view.h"
#include "view_defines.h"

#define BUF_MAX 16
#define WALK_STR "WALK"
#define RUN_STR "RUN"
#define STATIONARY_STR "STATIONARY"

static struct view_info {
	Evas_Object *win;
	Evas_Object *layout;
	Evas_Object *conform;
	view_button_clicked_callback_t button_start_clicked_cb;
	view_button_clicked_callback_t button_stop_clicked_cb;
	view_button_clicked_callback_t button_history_clicked_cb;
} s_info = {
	.win = NULL,
	.layout = NULL,
	.conform = NULL,
	.button_start_clicked_cb = NULL,
	.button_stop_clicked_cb = NULL,
	.button_history_clicked_cb = NULL,
};

static void _delete_win_request_cb(void *data, Evas_Object *obj, void *event_info);
static void _layout_back_cb(void *data, Evas_Object *obj, void *event_info);
static void _get_app_resource(const char *edj_file_in, char *edj_path_out, int edj_path_max);
static void _start_cb(void *data, Evas_Object *obj, void *event);
static void _stop_cb(void *data, Evas_Object *obj, void *event);
static void _show_history_cb(void *data, Evas_Object *obj, void *event);
static Evas_Object *_create_button(Evas_Object *parent, char *btn_text, Evas_Smart_Cb func);
static void _settings_cb(void *data, Evas_Object *obj, void *event);
static void _save_cb(void *data, Evas_Object *obj, void *event);

/**
 * @brief Creates essential objects: window, conformant and layout.
 * @param[in] user_data The data required for view module initialization.
 * @return This function returns 'EINA_TRUE' if the view is created successfully,
 * otherwise 'EINA_FALSE' is returned.
 */
Eina_Bool view_create(void *user_data)
{
	/* Create the window */
	s_info.win = view_create_win(PACKAGE);
	if (s_info.win == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a window.");
		return EINA_FALSE;
	}

	/* Create the conformant */
	s_info.conform = view_create_conformant(s_info.win);
	if (s_info.conform == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a conformant");
		return EINA_FALSE;
	}

	/* Base Layout */
	s_info.layout = view_create_layout(s_info.win);
	if (!s_info.layout) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create base layout");
		evas_object_del(s_info.win);
		return false;
	}
	elm_object_content_set(s_info.conform, s_info.layout);

	/* Show window after base GUI is set up */
	evas_object_show(s_info.win);

	return EINA_TRUE;
}

/**
 * @brief Creates a basic window named package.
 * @param[in] pkg_name Name of the window.
 * @return This function returns the window object if it was created successfully,
 * otherwise NULL is returned.
 */
Evas_Object *view_create_win(const char *pkg_name)
{
	Evas_Object *win = NULL;

	/*
	 * Window
	 * Create and initialize elm_win.
	 * elm_win is mandatory to manipulate the window.
	 */
	win = elm_win_util_standard_add(pkg_name, pkg_name);
	elm_win_conformant_set(win, EINA_TRUE);
	elm_win_autodel_set(win, EINA_TRUE);
	elm_win_indicator_mode_set(win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(win, ELM_WIN_INDICATOR_OPAQUE);

	evas_object_smart_callback_add(win, "delete,request", _delete_win_request_cb, NULL);

	return win;
}

/**
 * @brief Creates a layout to target parent object with edje file.
 * @param[in] parent The object to which you want to add this layout.
 * @return This function returns the layout object if it was created successfully,
 * otherwise NULL is returned.
 */
Evas_Object *view_create_layout(Evas_Object *parent)
{
	Evas_Object *layout = NULL;
	Evas_Object *start_button = NULL;
	Evas_Object *stop_button = NULL;
	Evas_Object *history_button = NULL;

	char edj_path[PATH_MAX] = {0, };

	if (parent == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "parent is NULL.");
		return NULL;
	}

	_get_app_resource(EDJ_FILE, edj_path, (int)PATH_MAX);

	/* Create layout using EDC(an edje file) */
	layout = elm_layout_add(parent);
	elm_layout_file_set(layout, edj_path, GRP_MAIN);

	/* Layout size setting */
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	eext_object_event_callback_add(layout, EEXT_CALLBACK_BACK, _layout_back_cb, NULL);

	/* Initialize text parts */
	elm_object_part_text_set(layout, PART_GPS_STATUS, GPS_NOT_DETECTED);
	elm_object_part_text_set(layout, PART_STEPS_TEXT, STEPS_0);
	elm_object_part_text_set(layout, PART_DISTANCE_TEXT, NOT_AVAILABLE);
	elm_object_part_text_set(layout, PART_FARE_TEXT, NOT_AVAILABLE);
	elm_object_part_text_set(layout, PART_CALORIES_TEXT, NOT_AVAILABLE);

	/* Initialize buttons */
	start_button = _create_button(s_info.win, BTN_START_TEXT, _start_cb);
	stop_button = _create_button(s_info.win, BTN_STOP_TEXT, _stop_cb);
	history_button = _create_button(s_info.win, BTN_HISTORY_TEXT, _show_history_cb);

	elm_object_part_content_set(layout, PART_START_BTN, start_button);
	elm_object_part_content_set(layout, PART_STOP_BTN, stop_button);
	elm_object_part_content_set(layout, PART_SHOW_HISTORY_BTN, history_button);

	eext_object_event_callback_add(layout, EEXT_CALLBACK_MORE, _settings_cb, NULL);

	evas_object_show(layout);

	return layout;
}

/**
 * @brief Creates a conformant without indicator.
 * @param[in] win The object to which you want to set this conformant.
 * Conformant is mandatory for base GUI to have proper size.
 * @return This function returns the conformant object if it was created successfully,
 * otherwise NULL is returned.
 */
Evas_Object *view_create_conformant(Evas_Object *win)
{
	/*
	 * Conformant
	 * Create and initialize elm_conformant.
	 * elm_conformant is mandatory for base GUI to have proper size
	 * when indicator or virtual keypad is visible.
	 */
	Evas_Object *conform = NULL;

	if (win == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "window is NULL.");
		return NULL;
	}

	conform = elm_conformant_add(win);
	evas_object_size_hint_weight_set(conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(win, conform);

	evas_object_show(conform);

	return conform;
}

/**
 * @brief Displays the GPS state message.
 * @param[in] gps_ok The GPS status flag.
 */
void view_set_gps_ok_text(bool gps_ok)
{
	if (gps_ok) {
		elm_object_part_text_set(s_info.layout, PART_GPS_STATUS, GPS_OK_TEXT);
		dlog_print(DLOG_DEBUG, LOG_TAG, "ok text");
	}
	else {
		elm_object_part_text_set(s_info.layout, PART_GPS_STATUS, GPS_NOT_DETECTED);
		dlog_print(DLOG_DEBUG, LOG_TAG, "not detected text");
	}
}

/**
 * @brief Displays the number of passed steps in current pedometer session.
 * @param[in] count The number of passed steps.
 */
void view_set_steps_count(int count)
{
	char steps[BUF_MAX] = {0, };
	snprintf(steps, BUF_MAX, "%d", count);
	elm_object_part_text_set(s_info.layout, PART_STEPS_TEXT, steps);
}

/**
 * @brief Displays the total distance passed in current session.
 * @param[in] distance The total passed distance.
 */
void view_set_total_distance(double distance)
{
	char dist[BUF_MAX] = {0, };
	snprintf(dist, BUF_MAX, "%g m", distance);
	elm_object_part_text_set(s_info.layout, PART_DISTANCE_TEXT, dist);
}

/**
 * @brief Displays the total rickshaw fare saved in current session.
 * @param[in] fare The total fare amount.
 */
void view_set_fare(int fare)
{
	char fare_string[BUF_MAX] = {0, };
	snprintf(fare_string, BUF_MAX, "Tk. %d", fare);
	elm_object_part_text_set(s_info.layout, PART_FARE_TEXT, fare_string);
}

void view_set_calories(double calories)
{
	char calories_string[BUF_MAX] = {0, };

	snprintf(calories_string, BUF_MAX, "%.2lf Cal", calories);

	elm_object_part_text_set(s_info.layout, PART_CALORIES_TEXT, calories_string);
}

/**
 * @brief Attaches callback functions.
 * @param[in] start_button_clicked_cb The callback function attached
 * to the on click event of the 'Start' button.
 * @param[in] stop_button_clicked_cb The callback function attached
 * to the on click event of the 'Stop' button.
 */
void view_set_button_callbacks(view_button_clicked_callback_t start_button_clicked_cb,
		view_button_clicked_callback_t stop_button_clicked_cb, view_button_clicked_callback_t history_button_clicked_cb)
{
	s_info.button_start_clicked_cb = start_button_clicked_cb;
	s_info.button_stop_clicked_cb = stop_button_clicked_cb;
	s_info.button_history_clicked_cb = history_button_clicked_cb;
}

/**
 * @brief Destroys window and frees its resources.
 */
void view_destroy(void)
{
	if (s_info.win == NULL)
		return;

	evas_object_del(s_info.win);
}

/**
 * @brief Internal callback function invoked when the main window needs to be destroyed.
 * @param[in] data The user data passed to the evas_object_smart_callback_add() function.
 * @param[in] obj The object invoking this callback function.
 * @param[in] event_info The structure containing the information on this event.
 */
static void _delete_win_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	ui_app_exit();
}

/**
 * @brief Internal callback function invoked on HW Back button press.
 * @param[in] data The user data passed to the eext_object_event_callback_add() function.
 * @param[in] obj The object invoking this callback function.
 * @param[in] event_info The structure containing the information on this event.
 */
static void _layout_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	/* Let window go to hide state. */
	Evas_Object *win = data;

	if (data == NULL)
		elm_win_lower(s_info.win);
	else
		evas_object_del(win);
}

/**
 * @brief Internal function which creates fully qualified path to the provided resource file.
 * @param[in] edj_file_in The file name.
 * @param[out] edj_path_out The fully qualified path to the edj_file_in file.
 */
static void _get_app_resource(const char *edj_file_in, char *edj_path_out, int edj_path_max)
{
	char *res_path = app_get_resource_path();
	if (res_path) {
		snprintf(edj_path_out, edj_path_max, "%s%s", res_path, edj_file_in);
		free(res_path);
	}
}

/**
 * @brief Internal callback function invoked on 'Start' button click.
 * This callback function invokes the button_start_clicked_cb callback function
 * attached with the view_set_button_callbacks() function.
 * @param[in] data Not used.
 * @param[in] obj The object invoking this callback function.
 * @param[in] event_info The structure containing the information on this event.
 */
static void _start_cb(void *data, Evas_Object *obj, void *event)
{
	dlog_print(DLOG_DEBUG, LOG_TAG, "Start button clicked");
	if (s_info.button_start_clicked_cb)
		s_info.button_start_clicked_cb();
}

/**
 * @brief Internal callback function invoked on 'Stop' button click.
 * This callback function invokes the button_stop_clicked_cb callback function
 * attached with the view_set_button_callbacks() function.
 * @param[in] data Not used.
 * @param[in] obj The object invoking this callback function.
 * @param[in] event_info The structure containing the information on this event.
 */
static void _stop_cb(void *data, Evas_Object *obj, void *event)
{
	dlog_print(DLOG_DEBUG, LOG_TAG, "Stop button clicked");
	if (s_info.button_stop_clicked_cb)
		s_info.button_stop_clicked_cb();
}

/**
 * @brief Callback function invoked on 'Show History' button click.
 */
static void _show_history_cb(void *data, Evas_Object *obj, void *event)
{
	dlog_print(DLOG_DEBUG, LOG_TAG, "History button clicked");

	if (s_info.button_history_clicked_cb)
		s_info.button_history_clicked_cb();
}

/**
 * @brief Internal function which creates a button object.
 * @param[in] parent The parent object for the button.
 * @param[in] btn_text The text to be displayed over the button.
 * @param[in] func The callback function to be invoked on button click.
 * @return This function returns the button object if it was created successfully,
 * otherwise NULL is returned.
 */
static Evas_Object *_create_button(Evas_Object *parent, char *btn_text, Evas_Smart_Cb func)
{
	Evas_Object *btn = elm_button_add(parent);
	if (!btn) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create button");
		return NULL;
	}

	elm_object_text_set(btn, btn_text);
	evas_object_smart_callback_add(btn, "clicked", func, NULL);
	evas_object_show(btn);

	return btn;
}

static void _settings_cb(void *data, Evas_Object *obj, void *event)
{
	dlog_print(DLOG_DEBUG, LOG_TAG, "Settings button pressed.");

	if (!view_settings_create(NULL))
		dlog_print(DLOG_DEBUG, LOG_TAG, "Failed to create settings view.");
}


Eina_Bool view_settings_create(void *user_data)
{
	/* Create the window */
	Evas_Object *win = view_create_win(PACKAGE);
	if (win == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a settings window.");
		return EINA_FALSE;
	}

	/* Create the conformant */
	Evas_Object *conform = view_create_conformant(win);

	if (conform == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a conformant for settings");
		return EINA_FALSE;
	}

	/* Base Layout */
	Evas_Object *layout = view_create_settings_layout(win);
	if (!layout) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create settings layout");
		evas_object_del(win);
		return false;
	}
	elm_object_content_set(conform, layout);

	/* Show window after base GUI is set up */
	evas_object_show(win);

	return EINA_TRUE;
}

Evas_Object *view_create_settings_layout(Evas_Object *parent)
{
	Evas_Object *layout = NULL;
	Evas_Object *save_btn = NULL;

	char edj_path[PATH_MAX] = {0, };

	if (parent == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "parent is NULL.");
		return NULL;
	}

	_get_app_resource(EDJ_FILE, edj_path, (int)PATH_MAX);

	/* Create layout using EDC(an edje file) */
	layout = elm_layout_add(parent);
	elm_layout_file_set(layout, edj_path, GRP_SETTINGS);

	/* Layout size setting */
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	eext_object_event_callback_add(layout, EEXT_CALLBACK_BACK, _layout_back_cb, parent);

	Evas_Object *weightEntry = elm_entry_add(layout);
	elm_entry_input_panel_layout_set(weightEntry, ELM_INPUT_PANEL_LAYOUT_NUMBER);
	elm_object_part_content_set(layout, PART_WEIGHT_ENTRY, weightEntry);

	const char key_name[] = "weight\0";
	bool existing;
	double weight;
	char weight_str[BUF_MAX];


	preference_is_existing(key_name, &existing);

	if (existing) {
		preference_get_double(key_name, &weight);
		snprintf(weight_str, BUF_MAX, "%.0lf", weight);
	}
	else {
		weight = 70.0;
		snprintf(weight_str, BUF_MAX, "%.0lf", weight);
	}
	elm_object_text_set(weightEntry, weight_str);

	elm_entry_editable_set(weightEntry, EINA_TRUE);
	elm_entry_single_line_set(weightEntry, EINA_TRUE);
	elm_entry_text_style_user_push(weightEntry, "DEFAULT='font=Tizen:style=regular font_size=100 color=#0af align=center valign=center underline=single underline_color=#000'");
	evas_object_show(weightEntry);

	/* Initialize button */
	save_btn = elm_button_add(parent);

	if (!save_btn) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create button");
	}

	elm_object_text_set(save_btn, BTN_SAVE_TEXT);
	evas_object_smart_callback_add(save_btn, "clicked", _save_cb, weightEntry);

	elm_object_part_content_set(layout, PART_SAVE_BTN, save_btn);

	evas_object_show(layout);

	return layout;
}

static void _save_cb(void *data, Evas_Object *obj, void *event)
{
	Evas_Object *weight_entry = data;

	dlog_print(DLOG_DEBUG, LOG_TAG, "Save Button pressed!");

	const char key_name[] = "weight\0";
	char *ptr;
	double weight = 0.0;
	const char *weight_str = elm_entry_entry_get(weight_entry);

	dlog_print(DLOG_DEBUG, LOG_TAG, "Weight: %s", weight_str);

	weight = strtod(weight_str, &ptr);

	preference_set_double(key_name, weight);

	dlog_print(DLOG_DEBUG, LOG_TAG, "Weight of type double: %lf", weight);
}