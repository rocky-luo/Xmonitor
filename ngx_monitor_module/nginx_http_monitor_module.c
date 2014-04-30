#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include <string.h>
#include "adapters/libev.h"
#include "hiredis.h"
#define DEVICE	1
#define PARA 	2

typedef struct {
	ngx_int_t errflag;
	ngx_int_t ccount;
	ngx_http_request_t *r;
} ngx_http_monitor_redisasy_t;

static char *
ngx_http_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_monitor_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_monitor_send_result(ngx_http_request_t *r, ngx_str_t *response);
void getCallback(redisAsyncContext *c, void *r, void *privdata) {
	redisReply *reply = r;
	ngx_http_monitor_redisasy_t *env = privdata;
	env->ccount--;
    	if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		env->errflag++;
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, env->r->connection->log, 0, \
			      "++rocky_X++ reply == NULL or ERROR");
	}
	if (env->ccount == 0) {
    		redisAsyncDisconnect(c);
	}
	return;
}

void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
	/*TODO */
        return;
    }
	return;
}

void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
	/*TODO */
	/*	ngx_log_debug(NGX_LOG_DEBUG_HTTP, rq->connection->log, 0, \
			      "redis error:%s", c->errstr);*/
        return;
    }
	return;
}

ngx_int_t redis_store(char *key, char *value, int type,char *dev_id, \
		ngx_http_request_t *r)
{
	ngx_http_monitor_redisasy_t env = {0, 0, r};
    	redisAsyncContext *c = redisAsyncConnect("127.0.0.1", 6379);
    	if (c->err) {
        	/* TODO can't connect redis*/
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
			      "++rocky_X++ can't connect redis");
    		redisAsyncDisconnect(c);
        	return 1;
    	}
    	redisLibevAttach(EV_DEFAULT_ c);
    	redisAsyncSetConnectCallback(c,connectCallback);
    	redisAsyncSetDisconnectCallback(c,disconnectCallback);
	if (type == DEVICE) {
		env.ccount = 1;
    		redisAsyncCommand(c, getCallback, &env, \
				  "ZADD device:id %s %s", value, key);
	}
	else if (type == PARA) {
		env.ccount = 2;
		redisAsyncCommand(c, getCallback, &env, \
				  "SADD para:dev_id:%s %s", dev_id, key);
		redisAsyncCommand(c, getCallback, &env, "SET para:%s:%s %s", \
				  dev_id, key, value);
	}
    	ev_loop(EV_DEFAULT_ 0);
	if (env.errflag != 0)
		return env.errflag;
	return 0;	
	
}
char *parse_para(char *p, char *key, char *value)
{
	char *temp;
	int len;
	key[0] = '\0';
	value[0] = '\0';
	temp = strchr(p, '=');
	if (temp ==NULL)
		return NULL;
	len = temp - p;
	strncat(key, p, len);
	strcat(key, "\0");
	p = ++temp;
	temp = strchr(p, '&');
	len = temp - p;
	strncat(value, p, len);
	strcat(value, "\0");
	p = ++temp;
	return p;
	
}


static void ngx_http_monitor_body_handler(ngx_http_request_t *r)
{
	off_t content_length;
	content_length = r->headers_in.content_length_n;
	char temp[content_length+1];
	char *parse_head;
	char key[10];
	char value[10];
	char dev_name[10];
	char dev_id[10];
	ssize_t n;
	ngx_int_t rc;
	n = ngx_read_file(&r->request_body->temp_file->file, temp, \
			  content_length, 0);
	if (n !=  content_length) {
		/*TODO */
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
		      "++rocky_X++body length is %O,but read %z\n", content_length, n);
	}
	temp[content_length] = '\0';
	parse_head = parse_para(temp, dev_name, dev_id);
	if (redis_store(dev_name, dev_id, DEVICE, NULL, r) != 0) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
			"++rocky_X++redis_store error in %s=%s", dev_name, \
			dev_id);
		ngx_str_t result = ngx_string("error in redis_store");
		rc = ngx_http_monitor_send_result(r, &result);
		if (rc == NGX_ERROR || rc > NGX_OK)
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, \
			"++rocky_X++ngx_http_monitor_send_result error:%d", rc);
		ngx_http_finalize_request(r, NGX_ERROR);
		return;
	}
	while (parse_head != NULL) {
		parse_head = parse_para(parse_head, key, value);
		if (redis_store(key, value, PARA, dev_id, r) != 0) {
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, \
				0, "++rocky_X++redis_store error in %s=%s", \
				dev_name, dev_id);
			ngx_str_t result = ngx_string("error in redis_store");
			rc = ngx_http_monitor_send_result(r, &result);
			if (rc == NGX_ERROR || rc > NGX_OK)
				ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
					      r->connection->log, \
				"++rocky_X++ngx_http_monitor_send_result error:%d", rc);
			ngx_http_finalize_request(r, NGX_ERROR);
			return;
		}
	}
	ngx_str_t result = ngx_string("ALL OK");
	rc = ngx_http_monitor_send_result(r, &result);
	if (rc == NGX_ERROR || rc > NGX_OK)
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, \
			"++rocky_X++ngx_http_monitor_send_result error:%d", rc);
	ngx_http_finalize_request(r, NGX_HTTP_OK);
	return;
		
}
static ngx_command_t  ngx_http_monitor_commands[] =
{
    {
        ngx_string("monitor"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_monitor,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};



static ngx_http_module_t  ngx_http_monitor_module_ctx =
{
    NULL,                              /* preconfiguration */
    NULL,                  		/* postconfiguration */
    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */
    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */
    NULL,       			/* create location configuration */
    NULL         			/* merge location configuration */
};

ngx_module_t  ngx_http_monitor_module =
{
    NGX_MODULE_V1,
    &ngx_http_monitor_module_ctx,           /* module context */
    ngx_http_monitor_commands,              /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *
ngx_http_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)

{

    ngx_http_core_loc_conf_t  *clcf;



    //Ê×ÏÈÕÒµœmytestÅäÖÃÏîËùÊôµÄÅäÖÃ¿é£¬clcfÃ²ËÆÊÇlocation¿éÄÚµÄÊýŸÝ

//œá¹¹£¬ÆäÊµ²»È»£¬Ëü¿ÉÒÔÊÇmain¡¢srv»òÕßlocŒ¶±ðÅäÖÃÏî£¬Ò²ŸÍÊÇËµÔÚÃ¿žö

//http{}ºÍserver{}ÄÚÒ²¶ŒÓÐÒ»žöngx_http_core_loc_conf_tœá¹¹Ìå

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);



    //http¿òŒÜÔÚŽŠÀíÓÃ»§ÇëÇóœøÐÐµœNGX_HTTP_CONTENT_PHASEœ×¶ÎÊ±£¬Èç¹û

//ÇëÇóµÄÖ÷»úÓòÃû¡¢URIÓëmytestÅäÖÃÏîËùÔÚµÄÅäÖÃ¿éÏàÆ¥Åä£¬ŸÍœ«µ÷ÓÃÎÒÃÇ

//ÊµÏÖµÄngx_http_mytest_handler·œ·šŽŠÀíÕâžöÇëÇó

    clcf->handler = ngx_http_monitor_handler;



    return NGX_CONF_OK;

}

static ngx_int_t ngx_http_monitor_handler(ngx_http_request_t *r)

{
	r->request_body_in_file_only = 1;
    	if (!(r->method & (NGX_HTTP_POST | NGX_HTTP_HEAD)))
    	{
        	return NGX_HTTP_NOT_ALLOWED;
    	}
    	ngx_int_t rc = ngx_http_read_client_request_body(r,ngx_http_monitor_body_handler);
    	if (rc >= NGX_HTTP_SPECIAL_RESPONSE)
    	{
        	return rc;
    	}
	return NGX_DONE;
}


ngx_int_t 
ngx_http_monitor_send_result(ngx_http_request_t *r, ngx_str_t *response)
{
	ngx_int_t rc;
    	ngx_str_t type = ngx_string("text/plain");
    	//ngx_str_t response = ngx_string("Hello World!");
    	r->headers_out.status = NGX_HTTP_OK;
    	r->headers_out.content_length_n = response->len;
    	r->headers_out.content_type = type;
    	ngx_table_elt_t *h = ngx_list_push(&r->headers_out.headers);
	if (h == NULL) {
		return NGX_ERROR;
	}
	h->hash = 1;
	h->key.len = sizeof("TestHead")-1;
	h->key.data = (u_char *) "TestHead";
	h->value.len = sizeof("TestValue")-1;
	h->value.data = (u_char *) "TestValue";
    	rc = ngx_http_send_header(r);
    	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)
    	{
        	return rc;
    	}
    	ngx_buf_t                 *b;
    	b = ngx_create_temp_buf(r->pool, response->len);
    	if (b == NULL)
    	{
        	return NGX_HTTP_INTERNAL_SERVER_ERROR;
    	}
    	ngx_memcpy(b->pos, response->data, response->len);
    	b->last = b->pos + response->len;
    	b->last_buf = 1;
    	ngx_chain_t		out;
    	out.buf = b;
    	out.next = NULL;
    	return ngx_http_output_filter(r, &out);
	
}
