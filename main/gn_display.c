#ifdef __cplusplus
extern "C" {
#endif


/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"

#include "gn_display.h"
#include "gn_commons.h"
#include "gn_event_source.h"

#define LV_TICK_PERIOD_MS 1

static const char *TAG = "grownode";

gn_config_handle_t _config;

//event groups
const int GN_EVT_GROUP_GUI_COMPLETED_EVENT = BIT0;
EventGroupHandle_t _gn_gui_event_group;
SemaphoreHandle_t _gn_xGuiSemaphore;

//TODO revise log structure with structs
lv_obj_t *statusLabel[10];
char rawMessages[10][30];
int rawIdx = 0;

lv_obj_t *connection_label;

void _gn_display_lv_tick_task(void *arg) {
	(void) arg;
	lv_tick_inc(LV_TICK_PERIOD_MS);
}

void _gn_display_btn_ota_event_handler(lv_obj_t *obj, lv_event_t event) {
	if (event == LV_EVENT_CLICKED) {
		ESP_LOGI(TAG, "_gn_display_btn_ota_event_handler - clicked");

		ESP_ERROR_CHECK(
				esp_event_post_to(_config->event_loop, GN_BASE_EVENT, GN_NET_OTA_START, NULL,
						0, portMAX_DELAY));

	} else if (event == LV_EVENT_VALUE_CHANGED) {
		ESP_LOGI(TAG, "_gn_display_btn_ota_event_handler - toggled");

	}
}

void _gn_display_btn_rst_event_handler(lv_obj_t *obj, lv_event_t event) {
	if (event == LV_EVENT_CLICKED) {

		ESP_LOGI(TAG, "_gn_display_btn_rst_event_handler - clicked");

		ESP_ERROR_CHECK(
				esp_event_post_to(_config->event_loop, GN_BASE_EVENT, GN_NET_RESET_START, NULL,
						0, portMAX_DELAY));

	} else if (event == LV_EVENT_VALUE_CHANGED) {
		ESP_LOGI(TAG, "_gn_display_btn_rst_event_handler - toggled");

	}
}

void _gn_display_log_system_handler(void *handler_args, esp_event_base_t base,
		int32_t id, void *event_data) {

	char *message = (char*) event_data;

	//lv_label_set_text(statusLabel, message.c_str());
	//const char *t = text.c_str();

	if (rawIdx > 9) {

		//scroll messages
		for (int row = 1; row < 10; row++) {
			//ESP_LOGI(TAG, "scrolling %i from %s to %s", row, rawMessages[row], rawMessages[row - 1]);
			strncpy(rawMessages[row - 1], rawMessages[row], 30);
		}
		strncpy(rawMessages[9], message, 30);
	} else {
		//ESP_LOGI(TAG, "setting %i", rawIdx);
		strncpy(rawMessages[rawIdx], message, 30);
		rawIdx++;
	}

	if (pdTRUE == xSemaphoreTake(_gn_xGuiSemaphore, portMAX_DELAY)) {

		//ESP_LOGI(TAG, "printing %s", message);
		//print
		for (int row = 0; row < 10; row++) {
			//ESP_LOGI(TAG, "label %i to %s", 9 - row, rawMessages[row]);
			lv_label_set_text(statusLabel[row], rawMessages[row]);
		}
		xSemaphoreGive(_gn_xGuiSemaphore);
	}

}

void _gn_display_net_connected_handler(void *handler_args,
		esp_event_base_t base, int32_t id, void *event_data) {

	//TODO pass mqtt server info to the event
	char *message = (char*) event_data;

	if (pdTRUE == xSemaphoreTake(_gn_xGuiSemaphore, portMAX_DELAY)) {

		lv_label_set_text(connection_label, "OK");

		xSemaphoreGive(_gn_xGuiSemaphore);
	}

}

void _gn_display_create_gui() {

	lv_obj_t *scr = lv_disp_get_scr_act(NULL);

	//style
	static lv_style_t style;
	lv_style_init(&style);
	//lv_style_set_bg_opa(&style, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_style_set_bg_color(&style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
	lv_style_set_text_color(&style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	lv_style_set_bg_color(&style, LV_STATE_PRESSED, LV_COLOR_RED);
	lv_style_set_text_color(&style, LV_STATE_PRESSED, LV_COLOR_WHITE);
	//lv_style_set_border_color(&style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
	lv_style_set_border_width(&style, LV_STATE_DEFAULT, 0);
	lv_style_set_margin_all(&style, LV_STATE_DEFAULT, 0);
	lv_style_set_pad_all(&style, LV_STATE_DEFAULT, 2);
	//lv_style_set_radius(&style, LV_STATE_DEFAULT, 0);

	static lv_style_t style_log;
	lv_style_init(&style_log);
	//lv_style_set_bg_opa(&style_log, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_style_set_bg_color(&style_log, LV_STATE_DEFAULT, LV_COLOR_BLACK);
	lv_style_set_text_color(&style_log, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	//lv_style_set_border_color(&style_log, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	//lv_style_set_radius(&style_log, LV_STATE_DEFAULT, 0);

	//main container
	lv_obj_t *cont = lv_cont_create(scr, NULL);
	lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style);
	lv_obj_set_style_local_radius(cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_align(cont, scr, LV_ALIGN_IN_TOP_MID, 0, 0);
	lv_cont_set_fit(cont, LV_FIT_MAX);
	lv_cont_set_layout(cont, LV_LAYOUT_COLUMN_MID);

	//title container
	lv_obj_t *title_cont = lv_cont_create(cont, NULL);
	lv_obj_add_style(title_cont, LV_CONT_PART_MAIN, &style);
	lv_obj_align(title_cont, cont, LV_ALIGN_IN_TOP_MID, 0, 0);
	//lv_cont_set_fit(title_cont, LV_FIT_TIGHT);
	lv_cont_set_fit2(title_cont, LV_FIT_MAX, LV_FIT_TIGHT);
	//lv_obj_set_width(title_cont, 240);
	//lv_obj_set_height(title_cont, 20);
	lv_cont_set_layout(title_cont, LV_LAYOUT_COLUMN_MID);

	//log container
	lv_obj_t *log_cont = lv_cont_create(cont, NULL);
	lv_obj_add_style(log_cont, LV_CONT_PART_MAIN, &style);
	lv_obj_align(log_cont, title_cont, LV_ALIGN_IN_TOP_MID, 0, 0);
	lv_cont_set_fit2(log_cont, LV_FIT_MAX, LV_FIT_TIGHT);
	//lv_obj_set_width(log_cont, 240);
	//lv_obj_set_height(log_cont, 260);
	lv_cont_set_layout(log_cont, LV_LAYOUT_COLUMN_LEFT);

	//bottom container
	lv_obj_t *bottom_cont = lv_cont_create(cont, NULL);
	lv_obj_add_style(bottom_cont, LV_CONT_PART_MAIN, &style);
	lv_obj_align(bottom_cont, cont, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
	lv_cont_set_fit2(bottom_cont, LV_FIT_MAX, LV_FIT_TIGHT);
	//lv_obj_set_width(button_cont, 240);
	//lv_obj_set_height(button_cont, 40);
	lv_cont_set_layout(bottom_cont, LV_LAYOUT_ROW_MID);

	//label title
	lv_obj_t *label_title = lv_label_create(title_cont, NULL);
	lv_obj_add_style(label_title, LV_LABEL_PART_MAIN, &style_log);
	lv_label_set_text(label_title, "GrowNode Board");
	//lv_obj_align(label_title, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

	//log labels

	for (int row = 0; row < 10; row++) {

		//log labels
		statusLabel[row] = lv_label_create(log_cont, NULL);
		lv_obj_add_style(statusLabel[row], LV_LABEL_PART_MAIN, &style_log);
		lv_label_set_text(statusLabel[row], "");
		lv_obj_set_width_fit(statusLabel[row], LV_FIT_MAX);

		if (row == 0) {
			lv_obj_align(statusLabel[row], NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

		} else {
			lv_obj_align(statusLabel[row], statusLabel[row - 1],
					LV_ALIGN_IN_TOP_LEFT, 0, 0);
		}

	}

	//bottom container

	//TODO put into a specific configuration panel
	//buttons
	lv_obj_t *label_ota;

	lv_obj_t *btn_ota = lv_btn_create(bottom_cont, NULL);
	lv_obj_add_style(btn_ota, LV_CONT_PART_MAIN, &style);
	lv_obj_set_event_cb(btn_ota, _gn_display_btn_ota_event_handler);
	lv_obj_align(btn_ota, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_btn_set_checkable(btn_ota, false);
	//lv_btn_toggle(btn_ota);
	lv_btn_set_fit2(btn_ota, LV_FIT_TIGHT, LV_FIT_TIGHT);

	label_ota = lv_label_create(btn_ota, NULL);
	lv_label_set_text(label_ota, "OTA");

	lv_obj_t *label_rst;

	lv_obj_t *btn_rst = lv_btn_create(bottom_cont, NULL);
	lv_obj_add_style(btn_rst, LV_CONT_PART_MAIN, &style);
	lv_obj_set_event_cb(btn_rst, _gn_display_btn_rst_event_handler);
	lv_obj_align(btn_rst, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_btn_set_checkable(btn_rst, false);
	//lv_btn_toggle(btn_ota);
	lv_btn_set_fit2(btn_rst, LV_FIT_TIGHT, LV_FIT_TIGHT);

	label_rst = lv_label_create(btn_rst, NULL);
	lv_label_set_text(label_rst, "RST");

	//TODO change to a connection icon
	//connection status
	connection_label = lv_label_create(bottom_cont, NULL);
	lv_obj_add_style(connection_label, LV_LABEL_PART_MAIN, &style_log);
	lv_obj_align(connection_label, bottom_cont, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
	lv_obj_set_width_fit(connection_label, LV_FIT_MAX);
	lv_label_set_text(connection_label, "OFFLINE");

}

void _gn_display_gui_task(void *pvParameter) {

	(void) pvParameter;
	_gn_xGuiSemaphore = xSemaphoreCreateMutex();

	lv_init();

	/* Initialize SPI or I2C bus used by the drivers */
	lvgl_driver_init();

	lv_color_t *buf1 = (lv_color_t*) heap_caps_malloc(
			DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
	assert(buf1 != NULL);

	/* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	lv_color_t *buf2 = (lv_color_t*) heap_caps_malloc(
			DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
	assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

	static lv_disp_buf_t disp_buf;

	uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820         \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A    \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D     \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

	/* Initialize the working buffer depending on the selected display.
	 * NOTE: buf2 == NULL when using monochrome displays. */
	lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.flush_cb = disp_driver_flush;

	/* When using a monochrome display we need to register the callbacks:
	 * - rounder_cb
	 * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

	disp_drv.buffer = &disp_buf;
	lv_disp_drv_register(&disp_drv);

	/* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.read_cb = touch_driver_read;
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	lv_indev_drv_register(&indev_drv);

#endif

	/* Create and start a periodic timer interrupt to call lv_tick_inc */
	const esp_timer_create_args_t periodic_timer_args = { .callback =
			&_gn_display_lv_tick_task, .name = "_gn_display_lv_tick_task" };
	esp_timer_handle_t periodic_timer;
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	ESP_ERROR_CHECK(
			esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

	_gn_display_create_gui();

	xEventGroupSetBits(_gn_gui_event_group, GN_EVT_GROUP_GUI_COMPLETED_EVENT);

	while (1) {
		/* Delay 1 tick (assumes FreeRTOS tick is 10ms */
		vTaskDelay(pdMS_TO_TICKS(10));

		/* Try to take the semaphore, call lvgl related function on success */
		if (pdTRUE == xSemaphoreTake(_gn_xGuiSemaphore, portMAX_DELAY)) {
			lv_task_handler();
			xSemaphoreGive(_gn_xGuiSemaphore);
		}
	}

	/* A task should NEVER return */
	free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	free(buf2);
#endif
	vTaskDelete(NULL);
}

esp_err_t gn_init_display(gn_config_handle_t conf) {

	//TODO initialization guards
	//TODO add define to kconfig to ignore display if not present

	_gn_gui_event_group = xEventGroupCreate();

	ESP_LOGI(TAG, "gn_init_display");

	xTaskCreatePinnedToCore(_gn_display_gui_task, "_gn_display_gui_task",
			4096 * 2, NULL, 10, NULL, 1);

	ESP_LOGI(TAG, "_gn_display_gui_task created");

	xEventGroupWaitBits(_gn_gui_event_group, GN_EVT_GROUP_GUI_COMPLETED_EVENT,
	pdTRUE, pdTRUE, portMAX_DELAY);

	//add event handlers
	ESP_ERROR_CHECK(
			esp_event_handler_instance_register_with(conf->event_loop, GN_BASE_EVENT, GN_DISPLAY_LOG_SYSTEM, _gn_display_log_system_handler, NULL, NULL));

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register_with(conf->event_loop, GN_BASE_EVENT, GN_NET_CONNECTED, _gn_display_net_connected_handler, NULL, NULL));

	ESP_LOGI(TAG, "gn_init_display done");

	//TODO dangerous, better update through events
	_config = conf;

	return ESP_OK;

}

#ifdef __cplusplus
}
#endif //__cplusplus