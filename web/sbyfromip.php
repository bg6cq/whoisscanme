<?php

include "db.php";

include "top.html";

$fromip = $_REQUEST["fromip"];

$fromip = inet_ntop(inet_pton($fromip));

echo "来源IP: ".$fromip." ";
echo ipinfo($fromip);
echo "IP信息</a>";
?>

扫描来源IP排序
<table border=1 cellspacing=0>
<tr><th>日期</th><th>端口</th><th>次数</th></tr>
<?php
$q = "select date(tm), toport, count(*) c from scanlog where fromip = ? and tm > date_sub(now(), interval 7 day) group by date(tm), toport order by c desc";
$stmt=$mysqli->prepare($q);
$stmt->bind_param("s",$fromip);
$stmt->execute();
$stmt->bind_result($tm, $toport, $count);
while($stmt->fetch()){
	echo "<tr><td>".$tm."</td><td align=right><a href=sbyfromip_toport.php?fromip=".$fromip."&toport=".$toport.">".$toport."</a></td><td align=right>".$count."</td></tr>\n";
}
?>
</table>

