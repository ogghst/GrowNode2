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

#include "unity.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "grownode.h"
#include "grownode_intl.h"
#include "gn_mqtt_protocol.h"
#include "gn_relay.h"

static const char *TAG = "test_relay";

//functions hidden to be tested
void _gn_mqtt_event_handler(void *handler_args, esp_event_base_t base,
		int32_t event_id, void *event_data);

void _gn_mqtt_build_leaf_parameter_command_topic(
		const gn_leaf_config_handle_t _leaf_config, const char *param_name,
		char *buf);

extern gn_config_handle_t config;
extern gn_node_config_handle_t node_config;
gn_leaf_config_handle_t relay_config;

TEST_CASE("gn_init_add_relay", "[relay]") {
	config = gn_init();
	TEST_ASSERT(config != NULL);
	node_config = gn_node_create(config, "node");
	TEST_ASSERT_EQUAL_STRING("node", gn_get_node_config_name(node_config));
	TEST_ASSERT_EQUAL(gn_node_get_size(node_config), 0);
	relay_config = gn_leaf_create(node_config, "relay", gn_relay_task, 4096);
	TEST_ASSERT_EQUAL(gn_node_get_size(node_config), 1);
	esp_err_t ret = gn_node_start(node_config);
	TEST_ASSERT_EQUAL(ret, ESP_OK);

}

TEST_CASE("gn_leaf_create relay", "[relay]") {

	size_t oldsize = gn_node_get_size(node_config);
	relay_config = gn_leaf_create(node_config, "relay", gn_relay_task, 4096);
	TEST_ASSERT_EQUAL(gn_node_get_size(node_config), oldsize+1);
	TEST_ASSERT(relay_config != NULL);

}

TEST_CASE("gn_receive_status_0", "[relay]") {

	esp_mqtt_event_handle_t event = (esp_mqtt_event_t*) malloc(
			sizeof(esp_mqtt_event_t));
	event->client = ((gn_config_handle_intl_t) config)->mqtt_client;
	esp_mqtt_event_id_t event_id = MQTT_EVENT_DATA;
	char *topic = malloc(100 * sizeof(char));

	_gn_mqtt_build_leaf_parameter_command_topic(relay_config, GN_RELAY_PARAM_STATUS, topic);

	char data[] = "0";
	event->topic = (char*) malloc(strlen(topic) * sizeof(char));
	event->data = (char*) malloc(strlen(data) * sizeof(char));
	event->topic_len = strlen(topic);
	event->data_len = strlen(data);

	strncpy(event->topic, topic, event->topic_len);
	strncpy(event->data, data, event->data_len);

	esp_event_base_t base = "base";
	void *handler_args = 0;

	_gn_mqtt_event_handler(handler_args, base, event_id, event);
	TEST_ASSERT(true);

	free(event);
	free(topic);
	free(event->topic);
	free(event->data);

}

TEST_CASE("gn_receive_status_1", "[relay]") {

	esp_mqtt_event_handle_t event = (esp_mqtt_event_t*) malloc(
			sizeof(esp_mqtt_event_t));
	event->client = ((gn_config_handle_intl_t) config)->mqtt_client;
	esp_mqtt_event_id_t event_id = MQTT_EVENT_DATA;
	char *topic = malloc(100 * sizeof(char));

	_gn_mqtt_build_leaf_parameter_command_topic(relay_config, GN_RELAY_PARAM_STATUS, topic);

	char data[] = "1";
	event->topic = (char*) malloc(strlen(topic) * sizeof(char));
	event->data = (char*) malloc(strlen(data) * sizeof(char));
	event->topic_len = strlen(topic);
	event->data_len = strlen(data);

	strncpy(event->topic, topic, event->topic_len);
	strncpy(event->data, data, event->data_len);

	esp_event_base_t base = "base";
	void *handler_args = 0;

	_gn_mqtt_event_handler(handler_args, base, event_id, event);
	TEST_ASSERT(true);

	free(event);
	free(topic);
	free(event->topic);
	free(event->data);

}

TEST_CASE("gn_relay_mqtt_stress_test", "[relay]") {

	config = gn_init();
	TEST_ASSERT(config != NULL);
	node_config = gn_node_create(config, "node");
	TEST_ASSERT_EQUAL_STRING("node", gn_get_node_config_name(node_config));
	TEST_ASSERT_EQUAL(gn_node_get_size(node_config), 0);
	relay_config = gn_leaf_create(node_config, "relay", gn_relay_task, 4096);
	TEST_ASSERT_EQUAL(gn_node_get_size(node_config), 1);
	esp_err_t ret = gn_node_start(node_config);
	TEST_ASSERT_EQUAL(ret, ESP_OK);

	esp_mqtt_event_handle_t event = (esp_mqtt_event_t*) malloc(
			sizeof(esp_mqtt_event_t));
	event->client = ((gn_config_handle_intl_t) config)->mqtt_client;
	esp_mqtt_event_id_t event_id = MQTT_EVENT_DATA;
	event->event_id = event_id;
	char *topic = malloc(100 * sizeof(char));
	esp_event_base_t base = "base";
	void *handler_args = 0;
	char data[10];

	event->topic = (char*) malloc(100 * sizeof(char));
	event->data = (char*) malloc(10 * sizeof(char));

	for (size_t j = 0; j < 100; j++) {

		_gn_mqtt_build_leaf_parameter_command_topic(relay_config,
				GN_RELAY_PARAM_STATUS, topic);

		if (j % 20 == 0) {
			strcpy(data, "1");
		} else {
			strcpy(data, "0");
		}

		event->topic_len = strlen(topic);
		event->data_len = strlen(data);

		strncpy(event->topic, topic, event->topic_len);
		strncpy(event->data, data, event->data_len);

		ESP_LOGD(TAG, "sending command - topic %s, data %s", topic, data);

		_gn_mqtt_event_handler(handler_args, base, event_id, event);

		vTaskDelay(pdMS_TO_TICKS(100));

	}

	free(event->topic);
	free(event->data);
	free(event);
	free(topic);

	TEST_ASSERT(true);
}