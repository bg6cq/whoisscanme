<?php

include "db.php";

include "top.html";

$toport = intval($_REQUEST["toport"]);

echo "目的端口: ".$toport;
?>

扫描来源IP排序
<table border=1 cellspacing=0>
<tr><th>日期</th><th>来源IP</th><th>端口</th><th>次数</th><th>IP信息</th></tr>
<?php
$q = "select date(tm), fromip, toport, count(*) c from scanlog where toport = ? and tm > date_sub(now(), interval 7 day) group by date(tm), fromip order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->bind_param("i",$toport);
$stmt->execute();
$stmt->bind_result($tm, $fromip, $toport, $count);
while($stmt->fetch()){
	echo "<tr><td>".$tm."</td><td><a href=sbyfromip.php?fromip=".$fromip.">".$fromip."</a></td><td align=right><a href=sbyfromip_toport.php?fromip=".$fromip."&toport=".$toport.">".$toport."</a></td><td align=right>".$count."</td><td>";
	echo ipinfo($fromip);
        echo getipdesc($fromip);
	echo "</tr>\n";
}
?>
</table>

