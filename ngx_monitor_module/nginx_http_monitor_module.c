#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include "adapters/libev.h"
#include "hiredis.h"
#define DEVICE	1
#define PARA 	2

static char *
ngx_http_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_monitor_handler(ngx_http_request_t *r);
int redis_store(char *key, char *value, int type,char *dev_id)
{
    	redisAsyncContext *c = redisAsyncConnect("127.0.0.1", 6379);
    	if (c->err) {
        	/* TODO can't connect redis*/
	
        	return 1;
    	}
    	redisLibevAttach(EV_DEFAULT_ c);
    	redisAsyncSetConnectCallback(c,connectCallback);
    	redisAsyncSetDisconnectCallback(c,disconnectCallback);
	if (type == DEVICE) {
    		redisAsyncCommand(c, NULL, NULL, "ZADD device:id %s %s", \
				  value, key);
	}
	else if (type == PARA) {
		redisAsyncCommand(c, NULL, NULL, "SADD para:dev_id:%s %s", \
				  dev_id, key);
		redisAsyncCommand(c, NULL, NULL, "SET para:%s:%s %s", \
				  dev_id, key, valuse);
	}
	redisAsyncDisconnect(c);
    	ev_loop(EV_DEFAULT_ 0);
	return;	
	
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
	stncat(key, p, len);
	stcat(key, "\0");
	p = ++temp;
	temp = strchr(p, '&');
	len = temp - p;
	strncat(value, p, len);
	strcat(value, "\0");
	p = ++temp;
	return p;
	
}
void getCallback(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = r;
    if (reply == NULL) return;
    printf("argv[%s]: %s\n", (char*)privdata, reply->str);

    /* Disconnect after receiving the reply to GET */
    redisAsyncDisconnect(c);
}

void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
	/*TODO */
        return;
    }
	return
}

void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
	/*TODO */
        return;
    }
	return;
}

static void ngx_http_monitor_body_handler(ngx_http_request_t *r)
{
	off_t content_length;
	content_length = r->headers_in->content_length_n;
	char temp[content_length];
	char *parse_head;
	char key[10];
	char value[10];
	char dev_name[10];
	char dev_id[10];
	ssize_t n;
	n = ngx_read_file(r->request_body->temp_file->file, temp, content_length, 0);
	if (n !=  content_length) {
		/*TODO */
	}
	/*TODO parse para*/
	parse_head = parse_para(dev_name, dev_id, value);
	redis_store(dev_name, dev_id, DEVICE, NULL);
	while (parse_head != NULL) {
		parse_head = parse_para(parse_head, key, value);
		redis_store(key, value, PARA, dev_id);
	}
	ngx_http_finalize_request(r, 0)
		
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

    //±ØÐëÊÇGET»òÕßHEAD·œ·š£¬·ñÔò·µ»Ø405 Not Allowed

    if (!(r->method & (NGX_HTTP_POST | NGX_HTTP_HEAD)))

    {

        return NGX_HTTP_NOT_ALLOWED;

    }



    //¶ªÆúÇëÇóÖÐµÄ°üÌå

    ngx_int_t rc = ngx_http_read_client_request_body(r,ngx_http_monitor_body_handler);

    if (rc != NGX_OK)

    {

        return rc;

    }



    //ÉèÖÃ·µ»ØµÄContent-Type¡£×¢Òâ£¬ngx_str_tÓÐÒ»žöºÜ·œ±ãµÄ³õÊŒ»¯ºê

//ngx_string£¬Ëü¿ÉÒÔ°Ñngx_str_tµÄdataºÍlen³ÉÔ±¶ŒÉèÖÃºÃ

    ngx_str_t type = ngx_string("text/plain");

    //·µ»ØµÄ°üÌåÄÚÈÝ

    ngx_str_t response = ngx_string("Hello World!");

    //ÉèÖÃ·µ»Ø×ŽÌ¬Âë

    r->headers_out.status = NGX_HTTP_OK;

    //ÏìÓŠ°üÊÇÓÐ°üÌåÄÚÈÝµÄ£¬ËùÒÔÐèÒªÉèÖÃContent-Length³€¶È

    r->headers_out.content_length_n = response.len;

    //ÉèÖÃContent-Type

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



    //·¢ËÍhttpÍ·²¿

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)

    {

        return rc;

    }



    //¹¹Ôìngx_buf_tœá¹¹×Œ±ž·¢ËÍ°üÌå

    ngx_buf_t                 *b;

    b = ngx_create_temp_buf(r->pool, response.len);

    if (b == NULL)

    {

        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    }

    //œ«Hello World¿œ±Žµœngx_buf_tÖžÏòµÄÄÚŽæÖÐ

    ngx_memcpy(b->pos, response.data, response.len);

    //×¢Òâ£¬Ò»¶šÒªÉèÖÃºÃlastÖžÕë

    b->last = b->pos + response.len;

    //ÉùÃ÷ÕâÊÇ×îºóÒ»¿é»º³åÇø

    b->last_buf = 1;



    //¹¹Ôì·¢ËÍÊ±µÄngx_chain_tœá¹¹Ìå

    ngx_chain_t		out;

    //ž³Öµngx_buf_t

    out.buf = b;

    //ÉèÖÃnextÎªNULL

    out.next = NULL;



    //×îºóÒ»²œ·¢ËÍ°üÌå£¬http¿òŒÜ»áµ÷ÓÃngx_http_finalize_request·œ·š

//œáÊøÇëÇó

    return ngx_http_output_filter(r, &out);

}
