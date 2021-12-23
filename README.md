# aquacomputer_octo-hwmon
*A hwmon Linux kernel driver for exposing sensors of the Aquacomputer Octo fan controller.*

Supports reading temperatures and speed, power, voltage and current of attached fans. The speed of the flowmeter is reported in l/h and is only correct after configuration in aquasuite. Being a standard `hwmon` driver, it provides readings via `sysfs`, which are easily accessible through `lm-sensors` as usual:

```shell
bill@debian:~> sensors
octo-hid-3-14
Adapter: HID adapter
VCC:               12.11 V  
Fan1 voltage:      12.07 V  
Fan2 voltage:      12.11 V  
Fan3 voltage:      12.11 V  
Fan4 voltage:      12.11 V  
Fan5 voltage:      12.11 V  
Fan6 voltage:      12.11 V  
Fan7 voltage:      12.11 V  
Fan8 voltage:      12.11 V  
Flow speed [l/h]:    0 RPM
Fan1 speed:       3028 RPM
Fan2 speed:       1167 RPM
Fan3 speed:       1161 RPM
Fan4 speed:       1155 RPM
Fan5 speed:       1154 RPM
Fan6 speed:       1137 RPM
Fan7 speed:       1129 RPM
Fan8 speed:          0 RPM
Temp1:             +24.8°C  
Temp2:             +29.7°C  
Temp3:             +30.9°C  
Temp4:            +327.7°C  
Fan1 power:         4.88 W  
Fan2 power:       260.00 mW 
Fan3 power:       260.00 mW 
Fan4 power:       300.00 mW 
Fan5 power:       290.00 mW 
Fan6 power:       300.00 mW 
Fan7 power:       300.00 mW 
Fan8 power:         0.00 W  
Fan1 current:     405.00 mA 
Fan2 current:      22.00 mA 
Fan3 current:      22.00 mA 
Fan4 current:      25.00 mA 
Fan1 current:      24.00 mA 
Fan2 current:      25.00 mA 
Fan3 current:      25.00 mA 
Fan4 current:       0.00 A  
```

## Install

Go into the directory and simply run
```
make
```
and load the module by running (as a root)
```
insmod aquacomputer-quadro.ko
```

To remove the module simply run
```
rmmod aquacomputer-quadro.ko
```


based on [aquacomputer_quadro-hwmon](https://github.com/leoratte/aquacomputer_quadro-hwmon)
