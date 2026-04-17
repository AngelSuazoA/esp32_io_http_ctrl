#include "esp_http_server.h"
#include "web_page.h"
#include "control_logic.h"
#include "cJSON.h"
#include <stdlib.h>

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t params_post(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No mem");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        int received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        cur_len += received;
    }

    buf[total_len] = 0;

    /* ✅ NOW JSON IS COMPLETE */
    set_parameters_from_json(buf);

    free(buf);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t phase_post(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    buf[len] = 0;

    cJSON *r = cJSON_Parse(buf);
    char p = cJSON_GetStringValue(
        cJSON_GetObjectItem(r,"phase"))[0];
    select_phase(p);
    cJSON_Delete(r);

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t start_post(httpd_req_t *req)
{
    httpd_resp_send(req,
        start_test() ? "OK" : "FAIL",
        HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t stop_post(httpd_req_t *req)
{
    stop_test();
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t state_get(httpd_req_t *req)
{
    const system_state_t *s = get_system_state();
    cJSON *r = cJSON_CreateObject();

    cJSON_AddBoolToObject(r,"running",s->test_running);
    cJSON_AddStringToObject(r,"phase",(char[]){s->selected_phase,0});
    cJSON_AddStringToObject(r,"mode",
        s->mode==MODE_CALC?"CALC":"DIRECT");
    cJSON_AddStringToObject(r,"direct_mode",
        s->direct_mode==DIRECT_TON?"TON":"DUTY");

    cJSON_AddNumberToObject(r,"I",s->I);
    cJSON_AddNumberToObject(r,"Rs",s->Rs);
    cJSON_AddNumberToObject(r,"Frequency",s->Frequency);
    cJSON_AddNumberToObject(r,"Vpwm",s->Vpwm);
    cJSON_AddNumberToObject(r,"R3",s->R3);
    cJSON_AddNumberToObject(r,"R5",s->R5);

    cJSON_AddNumberToObject(r,"Ton_direct",s->Ton_direct);
    cJSON_AddNumberToObject(r,"Duty_direct",s->Duty_direct);

    cJSON_AddNumberToObject(r,"Gain",s->Gain);
    cJSON_AddNumberToObject(r,"Ts",s->Ts);
    cJSON_AddNumberToObject(r,"Ton",s->Ton);
    cJSON_AddNumberToObject(r,"Toff",s->Toff);
    cJSON_AddNumberToObject(r,"Duty_used",s->Duty_used);

    char *json = cJSON_PrintUnformatted(r);
    httpd_resp_set_type(req,"application/json");
    httpd_resp_send(req,json,HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(r);
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server,&config);

    static httpd_uri_t uris[] = {
        {"/",HTTP_GET,root_get,NULL},
        {"/params",HTTP_POST,params_post,NULL},
        {"/phase",HTTP_POST,phase_post,NULL},
        {"/test/start",HTTP_POST,start_post,NULL},
        {"/test/stop",HTTP_POST,stop_post,NULL},
        {"/state",HTTP_GET,state_get,NULL}
    };

    for(int i=0;i<6;i++)
        httpd_register_uri_handler(server,&uris[i]);
}