// Copyright 2021 Nicola Muratori (nicola.muratori@gmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "lvgl_helpers.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "gn_pump.h"

#define TAG "gn_leaf_pump"

void gn_pump_task(gn_leaf_config_handle_t leaf_config) {

	const size_t GN_PUMP_STATE_STOP = 0;
	const size_t GN_PUMP_STATE_RUNNING = 1;

	size_t gn_pump_state = GN_PUMP_STATE_RUNNING;
	gn_leaf_event_t evt;

	//parameter definition. if found in flash storage, they will be created with found values instead of default
	gn_leaf_param_handle_t gn_pump_status_param = gn_leaf_param_create(
			leaf_config, GN_PUMP_PARAM_STATUS, GN_VAL_TYPE_BOOLEAN,
			(gn_val_t ) { .b = false }, GN_LEAF_PARAM_ACCESS_WRITE,
			GN_LEAF_PARAM_STORAGE_PERSISTED);
	gn_leaf_param_add(leaf_config, gn_pump_status_param);

	gn_leaf_param_handle_t gn_pump_power_param = gn_leaf_param_create(
			leaf_config, GN_PUMP_PARAM_POWER, GN_VAL_TYPE_DOUBLE, (gn_val_t ) {
							.d = 0 }, GN_LEAF_PARAM_ACCESS_WRITE,
			GN_LEAF_PARAM_STORAGE_PERSISTED);
	gn_leaf_param_add(leaf_config, gn_pump_power_param);

	gn_leaf_param_handle_t gn_pump_gpio_param = gn_leaf_param_create(
			leaf_config, GN_PUMP_PARAM_GPIO, GN_VAL_TYPE_DOUBLE, (gn_val_t ) {
							.d = 32 }, GN_LEAF_PARAM_ACCESS_WRITE,
			GN_LEAF_PARAM_STORAGE_PERSISTED);
	gn_leaf_param_add(leaf_config, gn_pump_gpio_param);

	//setup pwm
	mcpwm_pin_config_t pin_config = { .mcpwm0a_out_num =
			gn_pump_gpio_param->param_val->v.d };
	mcpwm_set_pin(MCPWM_UNIT_0, &pin_config);
	mcpwm_config_t pwm_config;
	pwm_config.frequency = 3000; //TODO make configurable
	pwm_config.cmpr_a = 0.0;
	pwm_config.cmpr_b = 0.0;
	pwm_config.counter_mode = MCPWM_UP_COUNTER;
	pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
	mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config); //Configure PWM0A & PWM0B with above settings

	//setup screen, if defined in sdkconfig
#ifdef CONFIG_GROWNODE_DISPLAY_ENABLED
	lv_obj_t *label_status = NULL;
	lv_obj_t *label_power = NULL;
	lv_obj_t *label_title = NULL;

	if (pdTRUE == gn_display_leaf_refresh_start()) {

		//parent container where adding elements
		lv_obj_t *_cnt = (lv_obj_t*) gn_display_setup_leaf_display(leaf_config);

		if (_cnt) {

			//style from the container
			lv_style_t *style = _cnt->styles->style;

			lv_obj_set_layout(_cnt, LV_LAYOUT_GRID);
			lv_coord_t col_dsc[] = { 90, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
			lv_coord_t row_dsc[] = { 20, 20, 20, LV_GRID_FR(1),
			LV_GRID_TEMPLATE_LAST };
			lv_obj_set_grid_dsc_array(_cnt, col_dsc, row_dsc);

			label_title = lv_label_create(_cnt);
			lv_label_set_text(label_title,
					gn_leaf_get_config_name(leaf_config));
			//lv_obj_add_style(label_title, style, 0);
			//lv_obj_align_to(label_title, _cnt, LV_ALIGN_TOP_MID, 0, LV_PCT(10));
			lv_obj_set_grid_cell(label_title, LV_GRID_ALIGN_CENTER, 0, 2,
					LV_GRID_ALIGN_STRETCH, 0, 1);

			label_status = lv_label_create(_cnt);
			//lv_obj_add_style(label_status, style, 0);
			lv_label_set_text(label_status,
					gn_pump_status_param->param_val->v.b ?
							"status: on" : "status: off");
			//lv_obj_align_to(label_status, label_title, LV_ALIGN_BOTTOM_LEFT,
			//		LV_PCT(10), LV_PCT(10));
			lv_obj_set_grid_cell(label_status, LV_GRID_ALIGN_STRETCH, 0, 1,
					LV_GRID_ALIGN_STRETCH, 1, 2);

			label_power = lv_label_create(_cnt);
			lv_obj_add_style(label_power, style, 0);

			char _p[21];
			snprintf(_p, 20, "power: %4.0f",
					gn_pump_power_param->param_val->v.d);
			lv_label_set_text(label_power, _p);
			//lv_obj_align_to(label_power, label_status, LV_ALIGN_TOP_LEFT, 0,
			//		LV_PCT(10));
			lv_obj_set_grid_cell(label_power, LV_GRID_ALIGN_STRETCH, 0, 1,
					LV_GRID_ALIGN_STRETCH, 2, 2);

		}

		gn_display_leaf_refresh_end();

	}

#endif

	//task cycle
	while (true) {

		//check for messages and cycle every 100ms
		if (xQueueReceive(gn_leaf_get_event_queue(leaf_config), &evt,
				pdMS_TO_TICKS(100)) == pdPASS) {

			ESP_LOGD(TAG, "received message: %d", evt.id);

			//event arrived for this node
			switch (evt.id) {

			//parameter change
			case GN_LEAF_PARAM_CHANGE_REQUEST_NETWORK_EVENT:
			case GN_LEAF_PARAM_CHANGE_REQUEST_EVENT:

				ESP_LOGD(TAG, "request to update param %s, data = '%s'",
						evt.param_name, evt.data);

				//parameter is status
				if (gn_common_leaf_event_mask_param(&evt, gn_pump_status_param)
						== 0) {

					const bool ret =
							strncmp((char*) evt.data, "0", evt.data_size) == 0 ?
									false : true;

					//execute change
					gn_leaf_param_set_bool(leaf_config, GN_PUMP_PARAM_STATUS,
							ret);

#ifdef CONFIG_GROWNODE_DISPLAY_ENABLED
					if (pdTRUE == gn_display_leaf_refresh_start()) {

						lv_label_set_text(label_status,
								gn_pump_status_param->param_val->v.b ?
										"status: on" : "status: off");

						gn_display_leaf_refresh_end();
					}
#endif

					//parameter is power
				} else if (gn_common_leaf_event_mask_param(&evt,
						gn_pump_power_param) == 0) {

					double pow = strtod(evt.data, NULL);
					if (pow < 0)
						pow = 0;
					if (pow > 1024)
						pow = 1024;

					//execute change
					gn_leaf_param_set_double(leaf_config, GN_PUMP_PARAM_POWER,
							pow);

#ifdef CONFIG_GROWNODE_DISPLAY_ENABLED
					if (pdTRUE == gn_display_leaf_refresh_start()) {
						char _p[21];
						snprintf(_p, 20, "power: %f", pow);
						lv_label_set_text(label_power, _p);

						gn_display_leaf_refresh_end();
					}
#endif

				}

				break;

				//what to do when network is connected
			case GN_NETWORK_CONNECTED_EVENT:
				//gn_pump_state = GN_PUMP_STATE_RUNNING;
				break;

				//what to do when network is disconnected
			case GN_NETWORK_DISCONNECTED_EVENT:
				gn_pump_state = GN_PUMP_STATE_STOP;
				break;

				//what to do when server is connected
			case GN_SERVER_CONNECTED_EVENT:
				gn_pump_state = GN_PUMP_STATE_RUNNING;
				break;

				//what to do when server is disconnected
			case GN_SERVER_DISCONNECTED_EVENT:
				gn_pump_state = GN_PUMP_STATE_STOP;
				break;

			default:
				break;

			}

		}

		//finally, we update sensor using the parameter values
		if (gn_pump_state != GN_PUMP_STATE_RUNNING) {
			mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A, 0);
		} else if (!gn_pump_status_param->param_val->v.b) {
			mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A, 0);
			//change = false;
		} else {
			mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A,
					gn_pump_power_param->param_val->v.d);
			//change = false;
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

}

#ifdef __cplusplus
}
#endif //__cplusplus
