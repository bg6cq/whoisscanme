<?php

include "db.php";

include "top.html";

$fromip = $_REQUEST["fromip"];
$toip = $_REQUEST["toip"];
$toport = intval($_REQUEST["toport"]);

$fromip = inet_ntop(inet_pton($fromip));
$toip = inet_ntop(inet_pton($toip));

echo "来源IP: ".$fromip." ";
echo ipinfo($fromip);
echo "IP信息</a>";
?>

<table border=1 cellspacing=0>
<tr><th>时间</th><th>源IP地址</th><th>源端口</th><th>目的IP地址</th><th>目的端口</th></tr>
<?php
$q = "select tm, fromip, fromport, toip, toport from scanlog where fromip = ? and toport = ? and toip= ? order by tm desc limit 1000";
$stmt=$mysqli->prepare($q);
$stmt->bind_param("sis",$fromip,$toport,$toip);
$stmt->execute();
$stmt->bind_result($tm, $fromip, $fromport, $toip, $toport);
while($stmt->fetch()){
	echo "<tr><td>".$tm."</td><td>".$fromip."<td align=right>".$fromport."<td>".$toip."</td><td align=right>".$toport."</td></tr>\n";
}
?>
</table>

