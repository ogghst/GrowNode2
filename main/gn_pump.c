#include "gn_pump.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GN_PUMP_STATE_STOP 0
#define GN_PUMP_STATE_RUNNING 1

int state = GN_PUMP_STATE_STOP;

void _gn_pump_event_handler (void *handler_args, esp_event_base_t base,
		int32_t id, void *event_data) {


	switch (id) {
	case GN_NETWORK_CONNECTED_EVENT:
		//lv_label_set_text(network_status_label, "NET OK");
		state = GN_PUMP_STATE_RUNNING;
		break;
	case GN_NETWORK_DISCONNECTED_EVENT:
		//lv_label_set_text(network_status_label, "NET KO");
		state = GN_PUMP_STATE_STOP;
		break;
	case GN_SERVER_CONNECTED_EVENT:
		//lv_label_set_text(server_status_label, "SRV OK");
		state = GN_PUMP_STATE_RUNNING;
		break;
	case GN_SERVER_DISCONNECTED_EVENT:
		//lv_label_set_text(server_status_label, "SRV_KO");
		state = GN_PUMP_STATE_STOP;
		break;
	}

}

void _gn_pump_task(void *pvParam) {

	while (true) {

		vTaskDelay(5000 / portTICK_PERIOD_MS);

		if (state == GN_PUMP_STATE_RUNNING) {
			gn_log_message("pump tick");
		}

	}

}

void gn_pump_init(gn_leaf_config_handle_t config) {

	xTaskCreate(_gn_pump_task, "_gn_pump_task", 2048, NULL, 0, NULL);

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register_with(config->node_config->event_loop, GN_BASE_EVENT, GN_EVENT_ANY_ID, _gn_pump_event_handler, NULL, NULL));

}

void gn_pump_callback(gn_event_handle_t event, void *event_data) {

	gn_log_message("gn_pump_callback");

}

#ifdef __cplusplus
}
#endif //__cplusplus
