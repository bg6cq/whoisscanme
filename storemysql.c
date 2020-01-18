
#define DBUSER "root"
#define DBPASSWD ""
#define DB     "scanlog"
#define DBHOST "localhost"
#define DBPORT 3306
#define DBSOCKPATH "/var/lib/mysql/mysql.sock"

#include <mysql/mysql.h>
MYSQL *mysql;

MYSQL *connectdb(void)
{
	MYSQL *mysql;

	if ((mysql = mysql_init(NULL)) == NULL) {
		err_msg("mysql_init error\n");
		exit(0);
	}
	if (mysql_real_connect(mysql, DBHOST, DBUSER, DBPASSWD, DB, DBPORT, DBSOCKPATH, 0) == NULL) {
		err_msg("mysql_init error\n");
		exit(0);
	}
	return mysql;
}

void store_mysql(char *fromip, int fromport, char *toip, int toport)
{
	char sqlbuf[MAXLEN];
	snprintf(sqlbuf, MAXLEN, "insert into scanlog (tm, fromip, fromport, toip, toport) values(now(), '%s', %d, '%s', %d)", fromip, fromport, toip, toport);
	Debug("SQL: %s", sqlbuf);
	if (mysql_real_query(mysql, sqlbuf, strlen(sqlbuf))) {
		err_msg("Failed to store to mysql, Error: %s\n", mysql_error(mysql));
		exit(0);
	}
}
