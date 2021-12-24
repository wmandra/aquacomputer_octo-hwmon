// SPDX-License-Identifier: GPL-2.0+
/*
 * hwmon driver for Aquacomputer Octo fan controller
 *
 * The Octo sends HID reports (with ID 0x01) every second to report sensor values
 * (temperatures, fan speeds, voltage, current and power). It responds to
 * Get_Report requests, but returns a dummy value of no use.
 *
 * Copyright 2021 William Mandra <wmandra@gmail.com>
 */

#include <asm/unaligned.h>
#include <linux/debugfs.h>
#include <linux/hid.h>
#include <linux/hwmon.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/seq_file.h>

#define DRIVER_NAME			"aquacomputer-octo"

#define OCTO_STATUS_REPORT_ID	0x01
#define OCTO_STATUS_UPDATE_INTERVAL	(2 * HZ) /* In seconds */

/* Register offsets for the Octo */

#define OCTO_SERIAL_FIRST_PART	3
#define OCTO_SERIAL_SECOND_PART	5
#define OCTO_FIRMWARE_VERSION	13
#define OCTO_POWER_CYCLES		24

#define OCTO_TEMP1			61
#define OCTO_TEMP2			63
#define OCTO_TEMP3			65
#define OCTO_TEMP4			67

#define OCTO_FLOW_SPEED		123
#define OCTO_FAN1_SPEED		133
#define OCTO_FAN2_SPEED		146
#define OCTO_FAN3_SPEED		159
#define OCTO_FAN4_SPEED		172
#define OCTO_FAN5_SPEED		185
#define OCTO_FAN6_SPEED		198
#define OCTO_FAN7_SPEED		211
#define OCTO_FAN8_SPEED		224
		
#define OCTO_FAN1_POWER		131
#define OCTO_FAN2_POWER		144
#define OCTO_FAN3_POWER		157
#define OCTO_FAN4_POWER		170
#define OCTO_FAN5_POWER		183
#define OCTO_FAN6_POWER		196
#define OCTO_FAN7_POWER		209
#define OCTO_FAN8_POWER		222

#define OCTO_VOLTAGE			117
#define OCTO_FAN1_VOLTAGE		127
#define OCTO_FAN2_VOLTAGE		140
#define OCTO_FAN3_VOLTAGE		153
#define OCTO_FAN4_VOLTAGE		166
#define OCTO_FAN5_VOLTAGE		179
#define OCTO_FAN6_VOLTAGE		192
#define OCTO_FAN7_VOLTAGE		205
#define OCTO_FAN8_VOLTAGE		218

#define OCTO_FAN1_CURRENT		129
#define OCTO_FAN2_CURRENT		142
#define OCTO_FAN3_CURRENT		155
#define OCTO_FAN4_CURRENT		168
#define OCTO_FAN5_CURRENT		181
#define OCTO_FAN6_CURRENT		194
#define OCTO_FAN7_CURRENT		207
#define OCTO_FAN8_CURRENT		220

/* Labels for provided values */

#define L_TEMP1				"Temp1"
#define L_TEMP2				"Temp2"
#define L_TEMP3				"Temp3"
#define L_TEMP4				"Temp4"

#define L_FLOW_SPEED		"Flow speed [l/h]"
#define L_FAN1_SPEED		"Fan1 speed"
#define L_FAN2_SPEED		"Fan2 speed"
#define L_FAN3_SPEED		"Fan3 speed"
#define L_FAN4_SPEED		"Fan4 speed"
#define L_FAN5_SPEED		"Fan5 speed"
#define L_FAN6_SPEED		"Fan6 speed"
#define L_FAN7_SPEED		"Fan7 speed"
#define L_FAN8_SPEED		"Fan8 speed"

#define L_FAN1_POWER		"Fan1 power"
#define L_FAN2_POWER		"Fan2 power"
#define L_FAN3_POWER		"Fan3 power"
#define L_FAN4_POWER		"Fan4 power"
#define L_FAN5_POWER		"Fan5 power"
#define L_FAN6_POWER		"Fan6 power"
#define L_FAN7_POWER		"Fan7 power"
#define L_FAN8_POWER		"Fan8 power"

#define L_OCTO_VOLTAGE		"VCC"
#define L_FAN1_VOLTAGE		"Fan1 voltage"
#define L_FAN2_VOLTAGE		"Fan2 voltage"
#define L_FAN3_VOLTAGE		"Fan3 voltage"
#define L_FAN4_VOLTAGE		"Fan4 voltage"
#define L_FAN5_VOLTAGE		"Fan5 voltage"
#define L_FAN6_VOLTAGE		"Fan6 voltage"
#define L_FAN7_VOLTAGE		"Fan7 voltage"
#define L_FAN8_VOLTAGE		"Fan8 voltage"

#define L_FAN1_CURRENT		"Fan1 current"
#define L_FAN2_CURRENT		"Fan2 current"
#define L_FAN3_CURRENT		"Fan3 current"
#define L_FAN4_CURRENT		"Fan4 current"
#define L_FAN5_CURRENT		"Fan1 current"
#define L_FAN6_CURRENT		"Fan2 current"
#define L_FAN7_CURRENT		"Fan3 current"
#define L_FAN8_CURRENT		"Fan4 current"

static const char *const label_temps[] = {
	L_TEMP1,
	L_TEMP2,
	L_TEMP3,
	L_TEMP4,
};

static const char *const label_speeds[] = {
	L_FLOW_SPEED,
	L_FAN1_SPEED,
	L_FAN2_SPEED,
	L_FAN3_SPEED,
	L_FAN4_SPEED,
	L_FAN5_SPEED,
	L_FAN6_SPEED,
	L_FAN7_SPEED,
	L_FAN8_SPEED,
};

static const char *const label_power[] = {
	L_FAN1_POWER,
	L_FAN2_POWER,
	L_FAN3_POWER,
	L_FAN4_POWER,
	L_FAN5_POWER,
	L_FAN6_POWER,
	L_FAN7_POWER,
	L_FAN8_POWER,
};

static const char *const label_voltages[] = {
	L_OCTO_VOLTAGE,
	L_FAN1_VOLTAGE,
	L_FAN2_VOLTAGE,
	L_FAN3_VOLTAGE,
	L_FAN4_VOLTAGE,
	L_FAN5_VOLTAGE,
	L_FAN6_VOLTAGE,
	L_FAN7_VOLTAGE,
	L_FAN8_VOLTAGE,
};

static const char *const label_current[] = {
	L_FAN1_CURRENT,
	L_FAN2_CURRENT,
	L_FAN3_CURRENT,
	L_FAN4_CURRENT,
	L_FAN5_CURRENT,
	L_FAN6_CURRENT,
	L_FAN7_CURRENT,
	L_FAN8_CURRENT,
};

struct octo_data {
	struct hid_device *hdev;
	struct device *hwmon_dev;
	struct dentry *debugfs;
	s32 temp_input[4];
	u16 speed_input[9];
	u32 power_input[8];
	u16 voltage_input[9];
	u16 current_input[8];
	u32 serial_number[2];
	u16 firmware_version;
	u32 power_cycles; /* How many times the device was powered on */
	unsigned long updated;
};

static umode_t octo_is_visible(const void *data, enum hwmon_sensor_types type, u32 attr,
				 int channel)
{
	return 0444;
}

static int octo_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel,
		       long *val)
{
	struct octo_data *priv = dev_get_drvdata(dev);

	if (time_after(jiffies, priv->updated + OCTO_STATUS_UPDATE_INTERVAL))
		return -ENODATA;

	switch (type) {
	case hwmon_temp:
		*val = priv->temp_input[channel];
		break;
	case hwmon_fan:
		*val = priv->speed_input[channel];
		break;
	case hwmon_power:
		*val = priv->power_input[channel];
		break;
	case hwmon_in:
		*val = priv->voltage_input[channel];
		break;
	case hwmon_curr:
		*val = priv->current_input[channel];
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int octo_read_string(struct device *dev, enum hwmon_sensor_types type, u32 attr,
			      int channel, const char **str)
{
	switch (type) {
	case hwmon_temp:
		*str = label_temps[channel];
		break;
	case hwmon_fan:
		*str = label_speeds[channel];
		break;
	case hwmon_power:
		*str = label_power[channel];
		break;
	case hwmon_in:
		*str = label_voltages[channel];
		break;
	case hwmon_curr:
		*str = label_current[channel];
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct hwmon_ops octo_hwmon_ops = {
	.is_visible = octo_is_visible,
	.read = octo_read,
	.read_string = octo_read_string,
};

static const struct hwmon_channel_info *octo_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
	 			HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL),
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL, 
				HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL, 
				HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL, 
				HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL, 
				HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(power, HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL, 
				HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL, 
				HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
				HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL),
	HWMON_CHANNEL_INFO(in, HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL, 
				HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL, 
				HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL,
			   	HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL, 
			   	HWMON_I_INPUT | HWMON_I_LABEL),
	HWMON_CHANNEL_INFO(curr, HWMON_C_INPUT | HWMON_C_LABEL, HWMON_C_INPUT | HWMON_C_LABEL, 
				HWMON_C_INPUT | HWMON_C_LABEL, HWMON_C_INPUT | HWMON_C_LABEL, 
				HWMON_C_INPUT | HWMON_C_LABEL, HWMON_C_INPUT | HWMON_C_LABEL,
				HWMON_C_INPUT | HWMON_C_LABEL, HWMON_C_INPUT | HWMON_C_LABEL),
	NULL
};

static const struct hwmon_chip_info octo_chip_info = {
	.ops = &octo_hwmon_ops,
	.info = octo_info,
};

static int octo_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size)
{
	struct octo_data *priv;

	if (report->id != OCTO_STATUS_REPORT_ID)
		return 0;

	priv = hid_get_drvdata(hdev);

	/* Info provided with every report */

	priv->serial_number[0] = get_unaligned_be16(data + OCTO_SERIAL_FIRST_PART);
	priv->serial_number[1] = get_unaligned_be16(data + OCTO_SERIAL_SECOND_PART);

	priv->firmware_version = get_unaligned_be16(data + OCTO_FIRMWARE_VERSION);
	priv->power_cycles = get_unaligned_be32(data + OCTO_POWER_CYCLES);

	/* Sensor readings */

	priv->temp_input[0] = get_unaligned_be16(data + OCTO_TEMP1) * 10;
	priv->temp_input[1] = get_unaligned_be16(data + OCTO_TEMP2) * 10;
	priv->temp_input[2] = get_unaligned_be16(data + OCTO_TEMP3) * 10;
	priv->temp_input[3] = get_unaligned_be16(data + OCTO_TEMP4) * 10;

	priv->speed_input[0] = get_unaligned_be16(data + OCTO_FLOW_SPEED) / 10;
	priv->speed_input[1] = get_unaligned_be16(data + OCTO_FAN1_SPEED);
	priv->speed_input[2] = get_unaligned_be16(data + OCTO_FAN2_SPEED);
	priv->speed_input[3] = get_unaligned_be16(data + OCTO_FAN3_SPEED);
	priv->speed_input[4] = get_unaligned_be16(data + OCTO_FAN4_SPEED);
	priv->speed_input[5] = get_unaligned_be16(data + OCTO_FAN5_SPEED);
	priv->speed_input[6] = get_unaligned_be16(data + OCTO_FAN6_SPEED);
	priv->speed_input[7] = get_unaligned_be16(data + OCTO_FAN7_SPEED);
	priv->speed_input[8] = get_unaligned_be16(data + OCTO_FAN8_SPEED);


	priv->power_input[0] = get_unaligned_be16(data + OCTO_FAN1_POWER) * 10000;
	priv->power_input[1] = get_unaligned_be16(data + OCTO_FAN2_POWER) * 10000;
	priv->power_input[2] = get_unaligned_be16(data + OCTO_FAN3_POWER) * 10000;
	priv->power_input[3] = get_unaligned_be16(data + OCTO_FAN4_POWER) * 10000;
	priv->power_input[4] = get_unaligned_be16(data + OCTO_FAN5_POWER) * 10000;
	priv->power_input[5] = get_unaligned_be16(data + OCTO_FAN6_POWER) * 10000;
	priv->power_input[6] = get_unaligned_be16(data + OCTO_FAN7_POWER) * 10000;
	priv->power_input[7] = get_unaligned_be16(data + OCTO_FAN8_POWER) * 10000;

	priv->voltage_input[0] = get_unaligned_be16(data + OCTO_VOLTAGE) * 10;
	priv->voltage_input[1] = get_unaligned_be16(data + OCTO_FAN1_VOLTAGE) * 10;
	priv->voltage_input[2] = get_unaligned_be16(data + OCTO_FAN2_VOLTAGE) * 10;
	priv->voltage_input[3] = get_unaligned_be16(data + OCTO_FAN3_VOLTAGE) * 10;
	priv->voltage_input[4] = get_unaligned_be16(data + OCTO_FAN4_VOLTAGE) * 10;
	priv->voltage_input[5] = get_unaligned_be16(data + OCTO_FAN5_VOLTAGE) * 10;
	priv->voltage_input[6] = get_unaligned_be16(data + OCTO_FAN6_VOLTAGE) * 10;
	priv->voltage_input[7] = get_unaligned_be16(data + OCTO_FAN7_VOLTAGE) * 10;
	priv->voltage_input[8] = get_unaligned_be16(data + OCTO_FAN8_VOLTAGE) * 10;

	priv->current_input[0] = get_unaligned_be16(data + OCTO_FAN1_CURRENT);
	priv->current_input[1] = get_unaligned_be16(data + OCTO_FAN2_CURRENT);
	priv->current_input[2] = get_unaligned_be16(data + OCTO_FAN3_CURRENT);
	priv->current_input[3] = get_unaligned_be16(data + OCTO_FAN4_CURRENT);
	priv->current_input[4] = get_unaligned_be16(data + OCTO_FAN5_CURRENT);
	priv->current_input[5] = get_unaligned_be16(data + OCTO_FAN6_CURRENT);
	priv->current_input[6] = get_unaligned_be16(data + OCTO_FAN7_CURRENT);
	priv->current_input[7] = get_unaligned_be16(data + OCTO_FAN8_CURRENT);

	priv->updated = jiffies;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int serial_number_show(struct seq_file *seqf, void *unused)
{
	struct octo_data *priv = seqf->private;

	seq_printf(seqf, "%05u-%05u\n", priv->serial_number[0], priv->serial_number[1]);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(serial_number);

static int firmware_version_show(struct seq_file *seqf, void *unused)
{
	struct octo_data *priv = seqf->private;

	seq_printf(seqf, "%u\n", priv->firmware_version);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(firmware_version);

static int power_cycles_show(struct seq_file *seqf, void *unused)
{
	struct octo_data *priv = seqf->private;

	seq_printf(seqf, "%u\n", priv->power_cycles);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(power_cycles);

static void octo_debugfs_init(struct octo_data *priv)
{
	char name[32];

	scnprintf(name, sizeof(name), "%s-%s", DRIVER_NAME, dev_name(&priv->hdev->dev));

	priv->debugfs = debugfs_create_dir(name, NULL);
	debugfs_create_file("serial_number", 0444, priv->debugfs, priv, &serial_number_fops);
	debugfs_create_file("firmware_version", 0444, priv->debugfs, priv, &firmware_version_fops);
	debugfs_create_file("power_cycles", 0444, priv->debugfs, priv, &power_cycles_fops);
}

#else

static void octo_debugfs_init(struct octo_data *priv)
{
}

#endif

static int octo_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct octo_data *priv;
	int ret;

	priv = devm_kzalloc(&hdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->hdev = hdev;
	hid_set_drvdata(hdev, priv);

	priv->updated = jiffies - OCTO_STATUS_UPDATE_INTERVAL;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret)
		goto fail_and_stop;

	priv->hwmon_dev = hwmon_device_register_with_info(&hdev->dev, "octo", priv,
							  &octo_chip_info, NULL);

	if (IS_ERR(priv->hwmon_dev)) {
		ret = PTR_ERR(priv->hwmon_dev);
		goto fail_and_close;
	}

	octo_debugfs_init(priv);

	return 0;

fail_and_close:
	hid_hw_close(hdev);
fail_and_stop:
	hid_hw_stop(hdev);
	return ret;
}

static void octo_remove(struct hid_device *hdev)
{
	struct octo_data *priv = hid_get_drvdata(hdev);

	debugfs_remove_recursive(priv->debugfs);
	hwmon_device_unregister(priv->hwmon_dev);

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id octo_table[] = {
	{ HID_USB_DEVICE(0x0c70, 0xf011) }, /* Aquacomputer Octo */
	{},
};

MODULE_DEVICE_TABLE(hid, octo_table);

static struct hid_driver octo_driver = {
	.name = DRIVER_NAME,
	.id_table = octo_table,
	.probe = octo_probe,
	.remove = octo_remove,
	.raw_event = octo_raw_event,
};

static int __init octo_init(void)
{
	return hid_register_driver(&octo_driver);
}

static void __exit octo_exit(void)
{
	hid_unregister_driver(&octo_driver);
}

/* Request to initialize after the HID bus to ensure it's not being loaded before */

late_initcall(octo_init);
module_exit(octo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("William Mandra <wmandra@gmail.com>");
MODULE_DESCRIPTION("Hwmon driver for Aquacomputer Octo");
