### 端口扫描记录

本程序用来记录TCP端口扫描行为，工作过程如下：

1. 命令行指定接口和IP地址

```
接口为专用接口，不能由操作系统处理数据包。

接口不需要在操作系统设置IP地址，以免与操作系统的处理互相干扰。

接口需要是up状态。
```

2. RAW方式打开接口，接收和发送数据包

```
程序处理过程与操作系统无关。
```

3. 如果收到ARP包，且目的IP是指定的IP地址
 
```
发送ARP响应包。
```

4. 如果收到TCP SYN包

```
发送SYN+ACK包，目前应答包tcp序号是固定的，需要改为随机的。
```

5. 如果收到有TCP ACK包

```
检查是否是自己刚发出的SYN+ACK包的应答包，如果是，假定对方是在扫描。

由于这样的包可能重复，直接记录日志会出现重复日志。因此查找最近若干条的日志，如果没有记录过，将相关信息记录。

可选：返回一段字符串给对方。
```

### 我的使用

```
./whoisscanme -i eth1 -a 210.45.*.6 
```

然后在路由器上，把很多蜜罐IP地址加静态路由到 210.45.*.6，这样一台机器可以记录很多个蜜罐IP的扫描行为行为。

展示请见 http://whoisscanme.ustc.edu.cn
