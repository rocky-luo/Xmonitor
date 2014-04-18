#include <stdio.h>
#include "hiredis.h"
int main ()
{
	redisContext *conn = redisConnect("127.0.0.1", 6379); //redis server默认端口
	if(conn->err){
		printf("connection error&&&");
	}
	while(1);
	redisFree(conn);
	return 0;
}
