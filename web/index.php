<?php

include "db.php";

include "top.html";

?>
<table>
<tr>
<td align=center>
扫描来源IP排序
<table border=1 cellspacing=0>
<tr><th>IP地址</th><th>次数</th><th>IP信息</th></tr>
<?php
$q = "select fromip, count(*) c from scanlog where tm > date_sub(now(), interval 1 day) group by fromip order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->execute();
$stmt->bind_result($fromip, $count);
while($stmt->fetch()){
	echo "<tr><td><a href=sbyfromip.php?fromip=".$fromip.">".$fromip."</a></td><td align=right>".$count."</td><td>";
	echo ipinfo($fromip);
	echo getipdesc($fromip);
	echo "</a></td></tr>\n";
}
?>
</table>
</td>

<td align=center>
扫描来源IP、目的端口数排序
<table border=1 cellspacing=0>
<tr><th>IP地址</th><th>目的端口数</th></th><th>IP信息</th></tr>
<?php
$q = "select fromip, count(distinct toport) c from scanlog where tm > date_sub(now(), interval 1 day) group by fromip order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->execute();
$stmt->bind_result($fromip, $count);
while($stmt->fetch()){
        echo "<tr><td><a href=sbyfromip.php?fromip=".$fromip.">".$fromip."</a></td><td align=right>".$count."</td><td>";
	echo ipinfo($fromip);
	echo getipdesc($fromip);
	echo "</a></td></tr>\n";
}
?>
</table>
</td>

<td align=center>
扫描来源IP、目的端口、次数排序
<table border=1 cellspacing=0>
<tr><th>IP地址</th><th>目的端口</th><th>次数</th><th>IP信息</th></tr>
<?php
$q = "select fromip, toport, count(*) c from scanlog where tm > date_sub(now(), interval 1 day) group by fromip, toport order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->execute();
$stmt->bind_result($fromip, $toport, $count);
while($stmt->fetch()){
        echo "<tr><td><a href=sbyfromip.php?fromip=".$fromip.">".$fromip."</a></td><td align=right><a href=sbyfromip_toport.php?fromip=".$fromip."&toport=".$toport.">".$toport."</a></td><td align=right>".$count."</td><td>";
	echo ipinfo($fromip);
	echo getipdesc($fromip);
	echo "</a></td></tr>\n";
}
?>
</table>
</td>


<td align=center>
扫描目的端口排序
<table border=1 cellspacing=0>
<tr><th>目的端口</th><th>次数</th></tr>
<?php
$q = "select toport, count(*) c from scanlog where tm > date_sub(now(), interval 1 day) group by toport order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->execute();
$stmt->bind_result($toport, $count);
while($stmt->fetch()){
        echo "<tr><td align=right><a href=sbytoport.php?toport=".$toport.">".$toport."</a></td><td align=right>".$count."</td></tr>\n";
}
?>
</table>
</td>

<td align=center>
扫描目的IP排序
<table border=1 cellspacing=0>
<tr><th>目的IP</th><th>次数</th></tr>
<?php
$q = "select toip, count(*) c from scanlog where tm > date_sub(now(), interval 1 day) group by toip order by c desc limit 50";
$stmt=$mysqli->prepare($q);
$stmt->execute();
$stmt->bind_result($toip, $count);
while($stmt->fetch()){
        echo "<tr><td>".$toip."</td><td align=right>".$count."</td></tr>\n";
}
?>
</table>
</td>


</tr>
</table>

