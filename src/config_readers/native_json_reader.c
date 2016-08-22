/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 *
 */

#include "PI/pi_base.h"
#include "p4info/actions_int.h"
#include "p4info/tables_int.h"
#include "p4info/fields_int.h"
#include "p4info/act_profs_int.h"
#include "p4info/counters_int.h"
#include "p4info/meters_int.h"
#include "PI/int/pi_int.h"

#include <cJSON/cJSON.h>

#include <assert.h>

static pi_status_t read_actions(cJSON *root, pi_p4info_t *p4info) {
  assert(root);
  cJSON *actions = cJSON_GetObjectItem(root, "actions");
  if (!actions) return PI_STATUS_CONFIG_READER_ERROR;
  size_t num_actions = cJSON_GetArraySize(actions);
  pi_p4info_action_init(p4info, num_actions);

  cJSON *action;
  cJSON_ArrayForEach(action, actions) {
    const cJSON *item;
    item = cJSON_GetObjectItem(action, "name");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    const char *name = item->valuestring;
    item = cJSON_GetObjectItem(action, "id");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t pi_id = item->valueint;
    cJSON *params = cJSON_GetObjectItem(action, "params");
    if (!params) return PI_STATUS_CONFIG_READER_ERROR;
    size_t num_params = cJSON_GetArraySize(params);

    pi_p4info_action_add(p4info, pi_id, name, num_params);

    int param_id = 0;
    cJSON *param;
    cJSON_ArrayForEach(param, params) {
      item = cJSON_GetObjectItem(param, "name");
      if (!item) return PI_STATUS_CONFIG_READER_ERROR;
      const char *param_name = item->valuestring;

      item = cJSON_GetObjectItem(param, "bitwidth");
      if (!item) return PI_STATUS_CONFIG_READER_ERROR;
      int param_bitwidth = item->valueint;

      pi_p4info_action_add_param(p4info, pi_id,
                                 pi_make_action_param_id(pi_id, param_id++),
                                 param_name, param_bitwidth);
    }
  }

  return PI_STATUS_SUCCESS;
}

static pi_status_t read_fields(cJSON *root, pi_p4info_t *p4info) {
  assert(root);
  cJSON *fields = cJSON_GetObjectItem(root, "fields");
  if (!fields) return PI_STATUS_CONFIG_READER_ERROR;
  size_t num_fields = cJSON_GetArraySize(fields);
  pi_p4info_field_init(p4info, num_fields);

  cJSON *field;
  cJSON_ArrayForEach(field, fields) {
    const cJSON *item;
    item = cJSON_GetObjectItem(field, "name");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    const char *name = item->valuestring;
    item = cJSON_GetObjectItem(field, "id");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t pi_id = item->valueint;
    item = cJSON_GetObjectItem(field, "bitwidth");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    size_t bitwidth = item->valueint;

    pi_p4info_field_add(p4info, pi_id, name, bitwidth);
  }

  return PI_STATUS_SUCCESS;
}

static pi_status_t read_tables(cJSON *root, pi_p4info_t *p4info) {
  assert(root);
  cJSON *tables = cJSON_GetObjectItem(root, "tables");
  if (!tables) return PI_STATUS_CONFIG_READER_ERROR;
  size_t num_tables = cJSON_GetArraySize(tables);
  pi_p4info_table_init(p4info, num_tables);

  cJSON *table;
  cJSON_ArrayForEach(table, tables) {
    const cJSON *item;
    item = cJSON_GetObjectItem(table, "name");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    const char *name = item->valuestring;
    item = cJSON_GetObjectItem(table, "id");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t pi_id = item->valueint;
    cJSON *match_fields = cJSON_GetObjectItem(table, "match_fields");
    if (!match_fields) return PI_STATUS_CONFIG_READER_ERROR;
    size_t num_match_fields = cJSON_GetArraySize(match_fields);
    cJSON *actions = cJSON_GetObjectItem(table, "actions");
    if (!actions) return PI_STATUS_CONFIG_READER_ERROR;
    size_t num_actions = cJSON_GetArraySize(actions);

    pi_p4info_table_add(p4info, pi_id, name, num_match_fields, num_actions);

    cJSON *match_field;
    cJSON_ArrayForEach(match_field, match_fields) {
      item = cJSON_GetObjectItem(match_field, "id");
      if (!item) return PI_STATUS_CONFIG_READER_ERROR;
      pi_p4_id_t id = item->valueint;

      item = cJSON_GetObjectItem(match_field, "match_type");
      if (!item) return PI_STATUS_CONFIG_READER_ERROR;
      pi_p4info_match_type_t match_type = item->valueint;

      pi_p4info_table_add_match_field(
          p4info, pi_id, id, pi_p4info_field_name_from_id(p4info, id),
          match_type, pi_p4info_field_bitwidth(p4info, id));
    }

    cJSON *action;
    cJSON_ArrayForEach(action, actions) {
      pi_p4_id_t id = action->valueint;
      pi_p4info_table_add_action(p4info, pi_id, id);
    }

    item = cJSON_GetObjectItem(table, "const_default_action_id");
    if (item && item->valueint != PI_INVALID_ID) {
      pi_p4info_table_set_const_default_action(p4info, pi_id, item->valueint);
    }

    item = cJSON_GetObjectItem(table, "implementation");
    if (item && item->valueint != PI_INVALID_ID) {
      pi_p4info_table_set_implementation(p4info, pi_id, item->valueint);
    }

    item = cJSON_GetObjectItem(table, "direct_resources");
    if (item) {
      cJSON *direct_res;
      cJSON_ArrayForEach(direct_res, item) {
        pi_p4_id_t id = direct_res->valueint;
        pi_p4info_table_add_direct_resource(p4info, pi_id, id);
      }
    }
  }

  return PI_STATUS_SUCCESS;
}

static pi_status_t read_act_profs(cJSON *root, pi_p4info_t *p4info) {
  assert(root);
  cJSON *act_profs = cJSON_GetObjectItem(root, "act_profs");
  if (!act_profs) return PI_STATUS_CONFIG_READER_ERROR;
  size_t num_act_profs = cJSON_GetArraySize(act_profs);
  pi_p4info_act_prof_init(p4info, num_act_profs);

  cJSON *act_prof;
  cJSON_ArrayForEach(act_prof, act_profs) {
    const cJSON *item;
    item = cJSON_GetObjectItem(act_prof, "name");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    const char *name = item->valuestring;
    item = cJSON_GetObjectItem(act_prof, "id");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t pi_id = item->valueint;
    item = cJSON_GetObjectItem(act_prof, "with_selector");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    assert(item->type == cJSON_True || item->type == cJSON_False);
    bool with_selector = (item->type == cJSON_True);

    pi_p4info_act_prof_add(p4info, pi_id, name, with_selector);

    item = cJSON_GetObjectItem(act_prof, "tables");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    cJSON *table;
    cJSON_ArrayForEach(table, item) {
      pi_p4_id_t id = table->valueint;
      pi_p4info_act_prof_add_table(p4info, pi_id, id);
    }
  }

  return PI_STATUS_SUCCESS;
}

static pi_status_t read_counters(cJSON *root, pi_p4info_t *p4info) {
  assert(root);
  cJSON *counters = cJSON_GetObjectItem(root, "counters");
  if (!counters) return PI_STATUS_CONFIG_READER_ERROR;
  size_t num_counters = cJSON_GetArraySize(counters);
  pi_p4info_counter_init(p4info, num_counters);

  cJSON *counter;
  cJSON_ArrayForEach(counter, counters) {
    const cJSON *item;
    item = cJSON_GetObjectItem(counter, "name");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    const char *name = item->valuestring;
    item = cJSON_GetObjectItem(counter, "id");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t pi_id = item->valueint;
    item = cJSON_GetObjectItem(counter, "direct_table");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t direct_tid = item->valueint;
    item = cJSON_GetObjectItem(counter, "counter_unit");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4info_counter_unit_t counter_unit = item->valueint;
    item = cJSON_GetObjectItem(counter, "size");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    size_t size = item->valueint;

    pi_p4info_counter_add(p4info, pi_id, name, counter_unit, size);

    if (direct_tid != PI_INVALID_ID)
      pi_p4info_counter_make_direct(p4info, pi_id, direct_tid);
  }

  return PI_STATUS_SUCCESS;
}

static pi_status_t read_meters(cJSON *root, pi_p4info_t *p4info) {
  assert(root);
  cJSON *meters = cJSON_GetObjectItem(root, "meters");
  if (!meters) return PI_STATUS_CONFIG_READER_ERROR;
  size_t num_meters = cJSON_GetArraySize(meters);
  pi_p4info_meter_init(p4info, num_meters);

  cJSON *meter;
  cJSON_ArrayForEach(meter, meters) {
    const cJSON *item;
    item = cJSON_GetObjectItem(meter, "name");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    const char *name = item->valuestring;
    item = cJSON_GetObjectItem(meter, "id");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t pi_id = item->valueint;
    item = cJSON_GetObjectItem(meter, "direct_table");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4_id_t direct_tid = item->valueint;
    item = cJSON_GetObjectItem(meter, "meter_unit");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4info_meter_unit_t meter_unit = item->valueint;
    item = cJSON_GetObjectItem(meter, "meter_type");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    pi_p4info_meter_type_t meter_type = item->valueint;
    item = cJSON_GetObjectItem(meter, "size");
    if (!item) return PI_STATUS_CONFIG_READER_ERROR;
    size_t size = item->valueint;

    pi_p4info_meter_add(p4info, pi_id, name, meter_unit, meter_type, size);

    if (direct_tid != PI_INVALID_ID)
      pi_p4info_meter_make_direct(p4info, pi_id, direct_tid);
  }

  return PI_STATUS_SUCCESS;
}

pi_status_t pi_native_json_reader(const char *config, pi_p4info_t *p4info) {
  cJSON *root = cJSON_Parse(config);
  if (!root) return PI_STATUS_CONFIG_READER_ERROR;

  pi_status_t status;

  if ((status = read_actions(root, p4info)) != PI_STATUS_SUCCESS) {
    return status;
  }

  if ((status = read_fields(root, p4info)) != PI_STATUS_SUCCESS) {
    return status;
  }

  if ((status = read_tables(root, p4info)) != PI_STATUS_SUCCESS) {
    return status;
  }

  if ((status = read_act_profs(root, p4info)) != PI_STATUS_SUCCESS) {
    return status;
  }

  if ((status = read_counters(root, p4info)) != PI_STATUS_SUCCESS) {
    return status;
  }

  if ((status = read_meters(root, p4info)) != PI_STATUS_SUCCESS) {
    return status;
  }

  cJSON_Delete(root);

  return PI_STATUS_SUCCESS;
}
