<?php

include "db.php";

include "top.html";

$fromip = $_REQUEST["fromip"];
$toport = intval($_REQUEST["toport"]);

$fromip = inet_ntop(inet_pton($fromip));

echo "来源IP: ".$fromip." ";
echo ipinfo($fromip);
echo "IP信息</a>";
?>

<table border=1 cellspacing=0>
<tr><th>时间</th><th>目的IP地址</th><th>端口</th><th>次数</th></tr>
<?php
$q = "select date(tm), toip, toport, count(*) c from scanlog where fromip = ? and toport = ? and tm > date_sub(now(), interval 7 day) group by date(tm), toip order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->bind_param("si",$fromip,$toport);
$stmt->execute();
$stmt->bind_result($tm, $toip, $toport, $count);
while($stmt->fetch()){
	echo "<tr><td>".$tm."</td><td><a href=sbyfromip_toport_toip.php?fromip=".$fromip."&toport=".$toport."&toip=".$toip.">".$toip."</td><td align=right>".$toport."</a></td><td align=right>".$count."</td></tr>\n";
}
?>
</table>

