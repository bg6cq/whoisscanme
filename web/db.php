<?php

$db_host = "localhost";
$db_user = "root";
$db_passwd = "";
$db_dbname = "scanlog";


$mysqli = new mysqli($db_host, $db_user, $db_passwd, $db_dbname);
if(mysqli_connect_error()){
	echo mysqli_connect_error();
}

if (isset($_COOKIE[session_name()]))
	session_start();

function getipdesc($ip) {
       	$url = "http://ip.ustc.edu.cn/ip/".$ip;
	$u = file_get_contents($url);
	return $u;
}

function ipinfo($ip) {
	echo "<a href=http://ip.tool.chinaz.com/".$ip." target=_blank>";
}
?>
