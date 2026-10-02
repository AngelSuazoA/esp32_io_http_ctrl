#include "esp_http_server.h"
#include "web_page.h"
#include "control_logic.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
/* ===== helpers ===== */
static char *recv_body(httpd_req_t *req)
{
   int len = req->content_len;
   char *buf = malloc(len + 1);
   if (!buf) return NULL;
   int cur = 0;
   while (cur < len) {
       int r = httpd_req_recv(req, buf + cur, len - cur);
       if (r <= 0) { free(buf); return NULL; }
       cur += r;
   }
   buf[len] = 0;
   return buf;
}
static void add_params_to_json(cJSON *obj, const pwm_params_t *p)
{
   cJSON_AddStringToObject(obj, "mode",
       p->mode == MODE_CALC ? "CALC" : "DIRECT");
   cJSON_AddStringToObject(obj, "direct_mode",
       p->direct_mode == DIRECT_TON ? "TON" : "DUTY");
   cJSON_AddNumberToObject(obj, "I",          p->I);
   cJSON_AddNumberToObject(obj, "Rs",         p->Rs);
   cJSON_AddNumberToObject(obj, "Frequency",  p->Frequency);
   cJSON_AddNumberToObject(obj, "Vpwm",       p->Vpwm);
   cJSON_AddNumberToObject(obj, "R3",         p->R3);
   cJSON_AddNumberToObject(obj, "R5",         p->R5);
   cJSON_AddNumberToObject(obj, "Ton_direct", p->Ton_direct);
   cJSON_AddNumberToObject(obj, "Duty_direct",p->Duty_direct);
   cJSON_AddNumberToObject(obj, "Gain",       p->Gain);
   cJSON_AddNumberToObject(obj, "Ts",         p->Ts);
   cJSON_AddNumberToObject(obj, "Ton",        p->Ton);
   cJSON_AddNumberToObject(obj, "Toff",       p->Toff);
   cJSON_AddNumberToObject(obj, "Duty_used",  p->Duty_used);
}
/* ===== ROOT ===== */
static esp_err_t root_get(httpd_req_t *req)
{
   httpd_resp_set_type(req, "text/html");
   httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
/* ===== SINGLE PARAMS ===== */
static esp_err_t single_params_post(httpd_req_t *req)
{
   char *buf = recv_body(req);
   if (!buf) {
       httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No mem");
       return ESP_FAIL;
   }
   set_single_params_from_json(buf);
   free(buf);
   httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
/* ===== FRAME PARAMS ===== */
static esp_err_t frame_params_post(httpd_req_t *req)
{
   char *buf = recv_body(req);
   if (!buf) {
       httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No mem");
       return ESP_FAIL;
   }
   set_frame_params_from_json(buf);
   free(buf);
   httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
/* ===== PHASE ===== */
static esp_err_t phase_post(httpd_req_t *req)
{
   char buf[64];
   int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
   if (len <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data"); return ESP_FAIL; }
   buf[len] = 0;
   cJSON *r = cJSON_Parse(buf);
   if (!r) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }
   const cJSON *ph = cJSON_GetObjectItem(r, "phase");
   if (cJSON_IsString(ph) && ph->valuestring[0])
       select_phase(ph->valuestring[0]);
   cJSON_Delete(r);
   httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
/* ===== TEST START/STOP ===== */
static esp_err_t single_start_post(httpd_req_t *req)
{
   httpd_resp_send(req, start_single_test() ? "OK" : "FAIL",
                   HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
static esp_err_t r_frame_start_post(httpd_req_t *req)
{
   httpd_resp_send(req, start_r_frame_test() ? "OK" : "FAIL",
                   HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
static esp_err_t v_frame_start_post(httpd_req_t *req)
{
   httpd_resp_send(req, start_v_frame_test() ? "OK" : "FAIL",
                   HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
static esp_err_t stop_post(httpd_req_t *req)
{
   stop_test();
   httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
   return ESP_OK;
}
/* ===== STATE ===== */
static esp_err_t state_get(httpd_req_t *req)
{
   const system_state_t *s = get_system_state();
   cJSON *root = cJSON_CreateObject();
   cJSON_AddBoolToObject(root, "running", s->test_running);
   cJSON_AddStringToObject(root, "phase",
       (char[]){s->selected_phase, 0});
   cJSON_AddStringToObject(root, "active_test",
       s->active_test == TEST_SINGLE  ? "SINGLE"  :
       s->active_test == TEST_R_FRAME ? "R_FRAME" : "V_FRAME");
   cJSON_AddNumberToObject(root, "seq_step", s->seq_step);
   /* Single params */
   cJSON *single_obj = cJSON_CreateObject();
   add_params_to_json(single_obj, &s->single);
   cJSON_AddItemToObject(root, "single", single_obj);
   /* Inrush / Steady params */
   cJSON *inrush_obj = cJSON_CreateObject();
   add_params_to_json(inrush_obj, &s->inrush);
   cJSON_AddItemToObject(root, "inrush", inrush_obj);
   cJSON *steady_obj = cJSON_CreateObject();
   add_params_to_json(steady_obj, &s->steady);
   cJSON_AddItemToObject(root, "steady", steady_obj);
   char *json_str = cJSON_PrintUnformatted(root);
   httpd_resp_set_type(req, "application/json");
   httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
   free(json_str);
   cJSON_Delete(root);
   return ESP_OK;
}
/* ===== REGISTER ===== */
void start_webserver(void)
{
   httpd_handle_t server;
   httpd_config_t config = HTTPD_DEFAULT_CONFIG();
   config.max_uri_handlers = 16;
   httpd_start(&server, &config);
   static const httpd_uri_t uris[] = {
       { "/",                  HTTP_GET,  root_get,            NULL },
       { "/params/single",     HTTP_POST, single_params_post,  NULL },
       { "/params/frame",      HTTP_POST, frame_params_post,   NULL },
       { "/phase",             HTTP_POST, phase_post,          NULL },
       { "/test/single/start", HTTP_POST, single_start_post,   NULL },
       { "/test/rframe/start", HTTP_POST, r_frame_start_post,  NULL },
       { "/test/vframe/start", HTTP_POST, v_frame_start_post,  NULL },
       { "/test/stop",         HTTP_POST, stop_post,           NULL },
       { "/state",             HTTP_GET,  state_get,           NULL },
   };
   for (int i = 0; i < (int)(sizeof(uris) / sizeof(uris[0])); i++)
       httpd_register_uri_handler(server, &uris[i]);
}