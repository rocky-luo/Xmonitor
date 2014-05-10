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
typedef struct {
	ngx_str_t key;
	ngx_str_t value;
	ngx_queue_t l;
} ngx_http_monitor_elt_t;

static char *
ngx_http_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_monitor_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_monitor_send_result(ngx_http_request_t *r, ngx_str_t *response);
void getCallback(redisAsyncContext *c, void *r, void *privdata) {
	redisReply *reply = r;
	ngx_http_monitor_redisasy_t *env = privdata;
	//env->ccount--;
    	if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		env->errflag++;
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, env->r->connection->log, 0, \
			      "[Xmonitor] fail:reply == NULL or reply->type == REDIS_REPLY_ERROR");
    		redisAsyncDisconnect(c);
	}
/*
    	if (reply->type == REDIS_REPLY_ERROR) {
		env->errflag++;
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, env->r->connection->log, 0, \
			      "[Xmonitor] fail:reply ==REDIS_REPLY_ERROR");
    		redisAsyncDisconnect(c);
	}
*/
	if (env->ccount == 0) {
		//ngx_log_debug(NGX_LOG_DEBUG_HTTP, env->r->connection->log,0,"[Xmonitor]here i can free redis");
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, env->r->connection->log,0,"[Xmonitor]when free redis env->errflag=%d",env->errflag);
    		redisAsyncDisconnect(c);
	}
	return;
}

void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
	/*TODO */
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, ((ngx_http_request_t *)c->data)->connection->log, 0, \
				"[Xmonitor] fail:connectcb can't connect redis:%s", c->errstr);
//    		redisAsyncDisconnect(c);
        return;
    }
	return;
}

void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
	/*TODO */
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, ((ngx_http_request_t *)c->data)->connection->log, 0, \
			      "[Xmonitor] fail:redis disconnect with error:%s", c->errstr);
        return;
    }
	return;
}

ngx_int_t redis_store(ngx_http_request_t *r, ngx_queue_t *h)
{
//	signal(SIGPIPE, SIG_IGN);
	ngx_http_monitor_redisasy_t env = {0, 0, r};
    	redisAsyncContext *c = redisAsyncConnect("127.0.0.1", 6379);
    	if (c->err) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
			      "[Xmoniter] fail:can't connect redis");
    		redisAsyncDisconnect(c);
        	return 1;
    	}
	c->data = r;
    	redisLibevAttach(EV_DEFAULT_ c);
    	redisAsyncSetConnectCallback(c,connectCallback);
    	redisAsyncSetDisconnectCallback(c,disconnectCallback);
	ngx_queue_t *travel = ngx_queue_head(h);
	while ( travel != ngx_queue_sentinel(h)) {
		env.ccount++;
		travel = ngx_queue_next(travel);
	}
	env.ccount = (env.ccount-1)*2+1;
	travel = ngx_queue_head(h);
	ngx_http_monitor_elt_t *dev = ngx_queue_data(ngx_queue_head(h), \
					ngx_http_monitor_elt_t, l);
	ngx_http_monitor_elt_t *t;
    	redisAsyncCommand(c, NULL, NULL, \
			  "ZADD device:id %s %s", dev->value.data, \
			  dev->key.data);
	env.ccount--;
/*
    	redisAsyncCommand(c, getCallback, &env, \
			  "ZADD device:id %s %s", dev->value.data, \
			  dev->key.data);
*/
	travel = ngx_queue_next(travel);
	while (travel != ngx_queue_sentinel(h)) {
		t = ngx_queue_data(travel, ngx_http_monitor_elt_t, l);
		env.ccount--;
		redisAsyncCommand(c, NULL, NULL, \
				  "SADD para:dev_id:%s %s", \
				  dev->value.data, t->key.data);
		env.ccount--;
		if(env.ccount == 0) {
			redisAsyncCommand(c, getCallback, &env, \
				  "SET para:%s:%s %s",dev->value.data, \
				  t->key.data, t->value.data);
		}
		else {
			redisAsyncCommand(c, NULL, NULL, \
				  "SET para:%s:%s %s",dev->value.data, \
				  t->key.data, t->value.data);
 		}
/*
		redisAsyncCommand(c, getCallback, &env, \
				  "SADD para:dev_id:%s %s", \
				  dev->value.data, t->key.data);
		redisAsyncCommand(c, getCallback, &env, \
				  "SET para:%s:%s %s",dev->value.data, \
				  t->key.data, t->value.data);
*/
		travel = ngx_queue_next(travel);
	}
    	ev_loop(EV_DEFAULT_ 0);
	ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
			      "[Xmoniter] fail:end of ev_loop,errflag:%d",env.errflag);

	if (env.errflag != 0)
		return env.errflag;
	return 0;	
	
/*
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
*/	
}

ngx_queue_t *parse_para(ngx_http_request_t *r, ngx_str_t *p)
{
	u_char *temp;
	size_t nleft = p->len;
	u_char *pdata = p->data;
	ngx_http_monitor_elt_t *u;
	ngx_queue_t *lhead = ngx_pcalloc(r->pool, sizeof(ngx_queue_t));
	if (lhead == NULL) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
			      "[Xmonitor] fail: can't pcalloc ngx_queue_t");
		return NULL;
	}
	ngx_queue_init(lhead);
	while (nleft != 0) {
		temp = ngx_strchr(pdata, '=');
		if (temp == NULL) {
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
				      r->connection->log, 0, \
				      "[Xmonitor] fail:input format error,can't find '='");
			return NULL;
		}
		u = ngx_pcalloc(r->pool, sizeof(ngx_http_monitor_elt_t));
		if (u == NULL) {
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
				      r->connection->log, 0, \
				      "[Xmonitor] fail:can't pcalloc ngx_http_monitor_elt_t");
			return NULL;
		}
		u->key.data = pdata;
		u->key.len = (size_t)(temp - pdata);
		nleft = nleft - u->key.len - 1;
		pdata = temp + 1;
		ngx_memzero(temp,1);
		temp = ngx_strchr(pdata, '&');
		if (temp == NULL) {
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
				      r->connection->log, 0, \
				      "[Xmonitor] fail:input format error,can't find '&'");
			return NULL;
		}
		u->value.data = pdata;
		u->value.len = (size_t)(temp - pdata);
		nleft = nleft - u->value.len -1;
		pdata = temp + 1;
		ngx_memzero(temp,1);
		ngx_queue_insert_tail(lhead, &(u->l));
	}
	return lhead;
	
}


static void ngx_http_monitor_body_handler(ngx_http_request_t *r)
{
	off_t content_length;
	content_length = r->headers_in.content_length_n;
	//char temp[content_length+1];
	char *parse_head;
	ssize_t n;
	ngx_int_t rc;
	u_char *bodydata = ngx_pcalloc(r->pool, content_length);
	if (bodydata == NULL) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
			      r->connection->log, 0, \
			      "[Xmonitor] fail:can't pcalloc for request bodydata");
		ngx_str_t result = ngx_string("can't pcalloc for request bodydata");
		rc = ngx_http_monitor_send_result(r, &result);
		if (rc == NGX_ERROR || rc > NGX_OK)
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
				      r->connection->log, 0, \
				      "[Xmonitor] fail:ngx_http_monitor_send_result error:%d", rc);
		ngx_http_finalize_request(r, NGX_ERROR);
		return;
	}
	n = ngx_read_file(&r->request_body->temp_file->file, bodydata, \
			  content_length, 0);
	if (n !=  content_length) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
		      "[Xmonitor] fail:body length is %O,but read %z\n", content_length, n);
	}
	ngx_str_t *body = ngx_pcalloc(r->pool, sizeof(ngx_str_t));
	if (body == NULL) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
			      r->connection->log, 0, \
			      "[Xmonitor] fail:can't pcalloc for request body");
		ngx_str_t result = ngx_string("can't pcalloc for request body");
		rc = ngx_http_monitor_send_result(r, &result);
		if (rc == NGX_ERROR || rc > NGX_OK)
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, \
			"[Xmonitor] fail:ngx_http_monitor_send_result error:%d", rc);
		ngx_http_finalize_request(r, NGX_ERROR);
		return;
	}
	body->data = bodydata;
	body->len = content_length;
	ngx_queue_t *blist = parse_para(r, body);
	if (blist == NULL) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
			      r->connection->log, 0, \
			      "[Xmonitor] fail:can't parse request body");
		ngx_str_t result = ngx_string("can't parse request body");
		rc = ngx_http_monitor_send_result(r, &result);
		if (rc == NGX_ERROR || rc > NGX_OK)
			ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, \
			"[Xmonitor] fail:ngx_http_monitor_send_result error:%d", rc);
		ngx_http_finalize_request(r, NGX_ERROR);
	}
	rc = redis_store(r, blist);
	if (rc != 0) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, \
			    "[Xmonitor] fail:redis_store fail with %d", rc);
			ngx_str_t result = ngx_string("error in redis_store");
			rc = ngx_http_monitor_send_result(r, &result);
			if (rc == NGX_ERROR || rc > NGX_OK)
				ngx_log_debug(NGX_LOG_DEBUG_HTTP, \
					      r->connection->log, 0, \
				"[Xmonitor] fail:ngx_http_monitor_send_result error:%d", rc);
			ngx_http_finalize_request(r, NGX_ERROR);
			return;
		}
	ngx_str_t result = ngx_string("ALL OK");
	rc = ngx_http_monitor_send_result(r, &result);
	if (rc == NGX_ERROR || rc > NGX_OK)
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, \
			"[Xmonitor] fail:ngx_http_monitor_send_result error:%d", rc);
	ngx_http_finalize_request(r, NGX_HTTP_OK);
	return;
	



/*	
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
*/		
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
