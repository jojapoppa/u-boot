/*
 * drivers/display/lcd/lcd_extern/lcd_extern.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <malloc.h>
#include <asm/arch/gpio.h>
#ifdef CONFIG_OF_LIBFDT
#include <libfdt.h>
#endif
#ifdef CONFIG_SYS_I2C_AML
#include <aml_i2c.h>
#endif
#include <amlogic/aml_lcd.h>
#include <amlogic/aml_lcd_extern.h>
#include "lcd_extern.h"
#include "../aml_lcd_common.h"

static char *dt_addr = NULL;

/* only probe one extern driver for uboot */
static struct aml_lcd_extern_driver_s *lcd_ext_driver;

struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(void)
{
	if (lcd_ext_driver == NULL)
		EXTERR("invalid driver\n");
	return lcd_ext_driver;
}

#define EXT_LEN_MAX   200
static void aml_lcd_extern_init_table_dynamic_size_print(
		struct lcd_extern_config_s *econf, int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char str[EXT_LEN_MAX];
	unsigned char *init_table;

	if (flag) {
		printf("power on:\n");
		init_table = econf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
	} else {
		printf("power off:\n");
		init_table = econf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
	}

	i = 0;
	while (i < max_len) {
		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;

		cmd_size = init_table[i+1];
		k = snprintf(str, EXT_LEN_MAX, "  0x%02x %d", init_table[i], cmd_size);
		if (cmd_size > 0) {
			for (j = 0; j < cmd_size; j++)
				k += snprintf(str+k, EXT_LEN_MAX, " 0x%02x", init_table[i+2+j]);
		}
		printf("%s\n", str);
		i += (cmd_size + 2);
	}
}

static void aml_lcd_extern_init_table_fixed_size_print(
		struct lcd_extern_config_s *econf, int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char str[EXT_LEN_MAX];
	unsigned char *init_table;

	cmd_size = econf->cmd_size;
	if (flag) {
		printf("power on:\n");
		init_table = econf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
	} else {
		printf("power off:\n");
		init_table = econf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
	}

	i = 0;
	while (i < max_len) {
		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;

		k = snprintf(str, EXT_LEN_MAX, " ");
		for (j = 0; j < cmd_size; j++)
			k += snprintf(str+k, EXT_LEN_MAX, " 0x%02x", init_table[i+j]);

		printf("%s\n", str);
		i += cmd_size;
	}
}

static void aml_lcd_extern_info_print(void)
{
	struct lcd_extern_config_s *econf;

	if (lcd_ext_driver == NULL) {
		EXTERR("no lcd_extern driver\n");
		return;
	}
	econf = lcd_ext_driver->config;

	LCDPR("lcd_extern info:\n");
	printf("name:           %s\n"
		"index:          %d\n"
		"type:           %d\n"
		"status:         %d\n",
		econf->name, econf->index,
		econf->type, econf->status);

	switch (econf->type) {
	case LCD_EXTERN_I2C:
		printf("cmd_size:       %d\n"
			"i2c_addr:       0x%02x\n"
			"i2c_addr2:      0x%02x\n"
			"i2c_bus:        %d\n"
			"table_loaded:   %d\n",
			econf->cmd_size, econf->i2c_addr,
			econf->i2c_addr2, econf->i2c_bus,
			econf->table_init_loaded);
		if (econf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			aml_lcd_extern_init_table_dynamic_size_print(econf, 1);
			aml_lcd_extern_init_table_dynamic_size_print(econf, 0);
		} else {
			aml_lcd_extern_init_table_fixed_size_print(econf, 1);
			aml_lcd_extern_init_table_fixed_size_print(econf, 0);
		}
		break;
	case LCD_EXTERN_SPI:
		printf("cmd_size:       %d\n"
			"spi_gpio_cs:    %d\n"
			"spi_gpio_clk:   %d\n"
			"spi_gpio_data:  %d\n"
			"spi_clk_freq:   %d\n"
			"spi_clk_pol:    %d\n"
			"table_loaded:   %d\n",
			econf->cmd_size, econf->spi_gpio_cs,
			econf->spi_gpio_clk, econf->spi_gpio_data,
			econf->spi_clk_freq, econf->spi_clk_pol,
			econf->table_init_loaded);
		if (econf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			aml_lcd_extern_init_table_dynamic_size_print(econf, 1);
			aml_lcd_extern_init_table_dynamic_size_print(econf, 0);
		} else {
			aml_lcd_extern_init_table_fixed_size_print(econf, 1);
			aml_lcd_extern_init_table_fixed_size_print(econf, 0);
		}
		break;
	case LCD_EXTERN_MIPI:
		break;
	default:
		printf("not support extern_type\n");
		break;
	}
}

int aml_lcd_extern_get_gpio(unsigned char index)
{
	int gpio;
	char *str;

	if (lcd_ext_driver == NULL) {
		EXTERR("no lcd_extern driver\n");
		return LCD_GPIO_MAX;
	}

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		return LCD_GPIO_MAX;
	}
	str = lcd_ext_driver->config->gpio_name[index];
	gpio = aml_lcd_gpio_name_map_num(str);
	return gpio;
}

int aml_lcd_extern_set_gpio(unsigned char index, int value)
{
	int gpio;
	int ret;

	gpio = aml_lcd_extern_get_gpio(index);
	ret = aml_lcd_gpio_set(gpio, value);
	return ret;
}

#ifdef CONFIG_OF_LIBFDT
#ifdef CONFIG_SYS_I2C_AML
static unsigned char aml_lcd_extern_get_i2c_bus_str(const char *str)
{
	unsigned char i2c_bus;

	if (strncmp(str, "i2c_bus_ao", 10) == 0)
		i2c_bus = AML_I2C_MASTER_AO;
	else if (strncmp(str, "i2c_bus_a", 9) == 0)
		i2c_bus = AML_I2C_MASTER_A;
	else if (strncmp(str, "i2c_bus_b", 9) == 0)
		i2c_bus = AML_I2C_MASTER_B;
	else if (strncmp(str, "i2c_bus_c", 9) == 0)
		i2c_bus = AML_I2C_MASTER_C;
	else if (strncmp(str, "i2c_bus_d", 9) == 0)
		i2c_bus = AML_I2C_MASTER_D;
	else {
		i2c_bus = AML_I2C_MASTER_A;
		EXTERR("invalid i2c_bus: %s\n", str);
	}

	return i2c_bus;
}
#endif

char *aml_lcd_extern_get_dts_prop(int nodeoffset, char *propname)
{
	char *propdata;

	propdata = (char *)fdt_getprop(dt_addr, nodeoffset, propname, NULL);
	return propdata;
}

int aml_lcd_extern_get_dts_child(int index)
{
	int nodeoffset;
	char chlid_node[30];
	char *propdata;

	sprintf(chlid_node, "/lcd_extern/extern_%d", index);
	nodeoffset = fdt_path_offset(dt_addr, chlid_node);
	if (nodeoffset < 0) {
		EXTERR("dts: not find  node %s\n", chlid_node);
		return nodeoffset;
	}

	propdata = (char *)fdt_getprop(dt_addr, nodeoffset, "index", NULL);
	if (propdata == NULL) {
		EXTERR("get index failed, exit\n");
		return -1;
	} else {
		if (be32_to_cpup((u32*)propdata) != index) {
			EXTERR("index not match, exit\n");
			return -1;
		}
	}
	return nodeoffset;
}

static int aml_lcd_extern_get_init_dts(char *dtaddr, struct lcd_extern_config_s *extconf)
{
	int parent_offset;
	char *propdata, *p;
	const char *str;
	int i;

	parent_offset = fdt_path_offset(dtaddr, "/lcd_extern");
	if (parent_offset < 0) {
		EXTERR("not find /lcd_extern node: %s\n", fdt_strerror(parent_offset));
		return -1;
	}

	propdata = (char *)fdt_getprop(dtaddr, parent_offset, "key_valid", NULL);
	if (propdata == NULL) {
		if (lcd_debug_print_flag)
			EXTPR("failed to get key_valid\n");
		extconf->lcd_ext_key_valid = 0;
	} else {
		extconf->lcd_ext_key_valid = (unsigned char)(be32_to_cpup((u32*)propdata));
	}

	i = 0;
	propdata = (char *)fdt_getprop(dtaddr, parent_offset, "extern_gpio_names", NULL);
	if (propdata == NULL) {
		if (lcd_debug_print_flag)
			EXTPR("failed to get extern_gpio_names\n");
	} else {
		p = propdata;
		while (i < LCD_EXTERN_GPIO_NUM_MAX) {
			if (i > 0)
				p += strlen(p) + 1;
			str = p;
			if (strlen(str) == 0)
				break;
			strcpy(extconf->gpio_name[i], str);
			if (lcd_debug_print_flag) {
				EXTPR("i=%d, gpio=%s\n", i, extconf->gpio_name[i]);
			}
			i++;
		}
	}
	if (i < LCD_EXTERN_GPIO_NUM_MAX)
		strcpy(extconf->gpio_name[i], "invalid");

	return 0;
}

static int aml_lcd_extern_init_table_dynamic_size_load_dts(
		char *dtaddr, int nodeoffset,
		struct lcd_extern_config_s *extconf, int flag)
{
	unsigned char cmd_size, type;
	int i, j, max_len;
	unsigned char *init_table;
	char propname[20];
	char *propdata;

	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}

	i = 0;
	propdata = (char *)fdt_getprop(dtaddr, nodeoffset, propname, NULL);
	if (propdata == NULL) {
		EXTERR("get %s %s failed\n", extconf->name, propname);
		init_table[0] = LCD_EXTERN_INIT_END;
		return -1;
	}
	while (i < max_len) {
		/* step1: type */
		init_table[i] = (unsigned char)(be32_to_cpup((((u32*)propdata)+i)));
		type = init_table[i];
		if (type == LCD_EXTERN_INIT_END)
			break;
		/* step2: cmd_size */
		init_table[i+1] = (unsigned char)(be32_to_cpup((((u32*)propdata)+i+1)));
		cmd_size = init_table[i+1];
		if (cmd_size == 0) {
			i += 2;
			continue;
		}
		/* step3: data */
		for (j = 0; j < cmd_size; j++)
			init_table[i+2+j] = (unsigned char)(be32_to_cpup((((u32*)propdata)+i+2+j)));

		i += (cmd_size + 2);
	}

	return 0;
}

static int aml_lcd_extern_init_table_fixed_size_load_dts(
		char *dtaddr, int nodeoffset,
		struct lcd_extern_config_s *extconf, int flag)
{
	unsigned char cmd_size;
	int i, j, max_len;
	unsigned char *init_table;
	char propname[20];
	char *propdata;

	cmd_size = extconf->cmd_size;
	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}

	i = 0;
	propdata = (char *)fdt_getprop(dtaddr, nodeoffset, propname, NULL);
	if (propdata == NULL) {
		EXTERR("get %s %s failed\n", extconf->name, propname);
		init_table[0] = LCD_EXTERN_INIT_END;
		return -1;
	}
	while (i < max_len) {
		for (j = 0; j < cmd_size; j++)
			init_table[i+j] = (unsigned char)(be32_to_cpup((((u32*)propdata)+i+j)));

		if (extconf->table_init_on[i] == LCD_EXTERN_INIT_END)
			break;

		i += cmd_size;
	}

	return 0;
}

static int aml_lcd_extern_get_config_dts(char *dtaddr, int index, struct lcd_extern_config_s *extconf)
{
	int nodeoffset;
	char *propdata;
	const char *str;
	int ret = 0;

	extconf->table_init_loaded = 0;
	nodeoffset = aml_lcd_extern_get_dts_child(index);
	if (nodeoffset < 0)
		return -1;

	propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "index", NULL);
	if (propdata == NULL) {
		extconf->index = LCD_EXTERN_INDEX_INVALID;
		EXTERR("get index failed, exit\n");
		return -1;
	} else {
		extconf->index = (unsigned char)(be32_to_cpup((u32*)propdata));
	}
	if (lcd_debug_print_flag)
		EXTPR("index = %d\n", extconf->index);

	propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "extern_name", NULL);
	if (propdata == NULL) {
		str = "invalid_name";
		strcpy(extconf->name, str);
		EXTERR("get extern_name failed\n");
	} else {
		memset(extconf->name, 0, LCD_EXTERN_NAME_LEN_MAX);
		strcpy(extconf->name, propdata);
	}

	propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "status", NULL);
	if (propdata == NULL) {
		EXTERR("get status failed, default to disabled\n");
		extconf->status = 0;
	} else {
		if (strncmp(propdata, "okay", 2) == 0)
			extconf->status = 1;
		else
			extconf->status = 0;
	}
	if (lcd_debug_print_flag)
		EXTPR("%s: status = %d\n", extconf->name, extconf->status);

	propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "type", NULL);
	if (propdata == NULL) {
		extconf->type = LCD_EXTERN_MAX;
		EXTERR("get type failed, exit\n");
		return -1;
	} else {
		extconf->type = be32_to_cpup((u32*)propdata);
	}
	if (lcd_debug_print_flag)
		EXTPR("%s: type = %d\n", extconf->name, extconf->type);

	switch (extconf->type) {
	case LCD_EXTERN_I2C:
#ifdef CONFIG_SYS_I2C_AML
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "i2c_address", NULL);
		if (propdata == NULL) {
			EXTERR("get %s i2c_address failed, exit\n", extconf->name);
			extconf->i2c_addr = 0xff;
			return -1;
		} else {
			extconf->i2c_addr = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		if (lcd_debug_print_flag)
			EXTPR("%s: i2c_address=0x%02x\n", extconf->name, extconf->i2c_addr);
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "i2c_second_address", NULL);
		if (propdata == NULL) {
			if (lcd_debug_print_flag)
				EXTPR("get %s i2c_second_address failed, exit\n", extconf->name);
			extconf->i2c_addr2 = 0xff;
		} else {
			extconf->i2c_addr2 = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		if (lcd_debug_print_flag)
			EXTPR("%s: i2c_second_address=0x%02x\n", extconf->name, extconf->i2c_addr2);

		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "i2c_bus", NULL);
		if (propdata == NULL) {
			EXTERR("get %s i2c_bus failed, exit\n", extconf->name);
			extconf->i2c_bus = AML_I2C_MASTER_A;
			return -1;
		} else {
			extconf->i2c_bus = aml_lcd_extern_get_i2c_bus_str(propdata);
		}
		if (lcd_debug_print_flag)
			EXTPR("%s: i2c_bus=%s[%d]\n", extconf->name, propdata, extconf->i2c_bus);
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "cmd_size", NULL);
		if (propdata == NULL) {
			EXTERR("get %s cmd_size failed\n", extconf->name);
			extconf->cmd_size = 0;
		} else {
			extconf->cmd_size = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		if (lcd_debug_print_flag)
			EXTPR("%s: cmd_size=%d\n", extconf->name, extconf->cmd_size);
		if (extconf->cmd_size <= 1) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}

		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = aml_lcd_extern_init_table_dynamic_size_load_dts(
				dtaddr, nodeoffset, extconf, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_dynamic_size_load_dts(
				dtaddr, nodeoffset, extconf, 0);
		} else {
			ret = aml_lcd_extern_init_table_fixed_size_load_dts(
				dtaddr, nodeoffset, extconf, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_fixed_size_load_dts(
				dtaddr, nodeoffset, extconf, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
#else
		EXTERR("system has no i2c support\n");
#endif
		break;
	case LCD_EXTERN_SPI:
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "gpio_spi_cs", NULL);
		if (propdata == NULL) {
			EXTERR("get %s gpio_spi_cs failed, exit\n", extconf->name);
			extconf->spi_gpio_cs = LCD_EXTERN_GPIO_NUM_MAX;
			return -1;
		} else {
			extconf->spi_gpio_cs = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "gpio_spi_clk", NULL);
		if (propdata == NULL) {
			EXTERR("get %s gpio_spi_clk failed, exit\n", extconf->name);
			extconf->spi_gpio_clk = LCD_EXTERN_GPIO_NUM_MAX;
			return -1;
		} else {
			extconf->spi_gpio_clk = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "gpio_spi_data", NULL);
		if (propdata == NULL) {
			EXTERR("get %s gpio_spi_data failed, exit\n", extconf->name);
			extconf->spi_gpio_data = LCD_EXTERN_GPIO_NUM_MAX;
			return -1;
		} else {
			extconf->spi_gpio_data = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s: gpio_spi cs=%d, clk=%d, data=%d\n",
				extconf->name, extconf->spi_gpio_cs,
				extconf->spi_gpio_clk, extconf->spi_gpio_data);
		}
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "spi_clk_freq", NULL);
		if (propdata == NULL) {
			EXTERR("get %s spi_clk_freq failed, default to %dHz\n",
				extconf->name, LCD_EXTERN_SPI_CLK_FREQ_DFT);
			extconf->spi_clk_freq = LCD_EXTERN_SPI_CLK_FREQ_DFT;
		} else {
			extconf->spi_clk_freq = be32_to_cpup((u32*)propdata);
		}
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "spi_clk_pol", NULL);
		if (propdata == NULL) {
			EXTERR("get %s spi_clk_pol failed, default to 1\n", extconf->name);
			extconf->spi_clk_pol = 1;
		} else {
			extconf->spi_clk_pol = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s: spi clk=%dHz, clk_pol=%d\n",
				extconf->name, extconf->spi_clk_freq, extconf->spi_clk_pol);
		}
		propdata = (char *)fdt_getprop(dtaddr, nodeoffset, "cmd_size", NULL);
		if (propdata == NULL) {
			EXTERR("get %s cmd_size failed\n", extconf->name);
			extconf->cmd_size = 0;
			return -1;
		} else {
			extconf->cmd_size = (unsigned char)(be32_to_cpup((u32*)propdata));
		}
		if (lcd_debug_print_flag)
			EXTPR("%s: cmd_size=%d\n", extconf->name, extconf->cmd_size);
		if (extconf->cmd_size <= 1) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}

		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = aml_lcd_extern_init_table_dynamic_size_load_dts(
				dtaddr, nodeoffset, extconf, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_dynamic_size_load_dts(
				dtaddr, nodeoffset, extconf, 0);
		} else {
			ret = aml_lcd_extern_init_table_fixed_size_load_dts(
				dtaddr, nodeoffset, extconf, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_fixed_size_load_dts(
				dtaddr, nodeoffset, extconf, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_MIPI:
		break;
	default:
		break;
	}

	return 0;
}
#endif

static unsigned char aml_lcd_extern_i2c_bus_table[][2] = {
	{LCD_EXTERN_I2C_BUS_AO, AML_I2C_MASTER_AO},
	{LCD_EXTERN_I2C_BUS_A, AML_I2C_MASTER_A},
	{LCD_EXTERN_I2C_BUS_B, AML_I2C_MASTER_B},
	{LCD_EXTERN_I2C_BUS_C, AML_I2C_MASTER_C},
	{LCD_EXTERN_I2C_BUS_D, AML_I2C_MASTER_D},
};

static unsigned char aml_lcd_extern_get_i2c_bus_unifykey(unsigned char val)
{
	unsigned char i2c_bus = LCD_EXTERN_I2C_BUS_INVALID;
	int i;

	for (i = 0; i < ARRAY_SIZE(aml_lcd_extern_i2c_bus_table); i++) {
		if (aml_lcd_extern_i2c_bus_table[i][0] == val) {
			i2c_bus = aml_lcd_extern_i2c_bus_table[i][1];
			break;
		}
	}

	return i2c_bus;
}

static int aml_lcd_extern_init_table_dynamic_size_load_unifykey(
		struct lcd_extern_config_s *extconf, unsigned char *p,
		int key_len, int len, int flag)
{
	unsigned char cmd_size;
	int i, j, max_len, ret = 0;
	unsigned char *init_table;
	char propname[20];

	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}

	i = 0;
	while (i < max_len) {
		/* step1: type */
		len += 1;
		ret = aml_lcd_unifykey_len_check(key_len, len);
		if (ret) {
			EXTERR("get %s %s failed\n", extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			return -1;
		}
		init_table[i] = *p;
		p++;
		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;

		/* step2: cmd_size */
		len += 1;
		ret = aml_lcd_unifykey_len_check(key_len, len);
		if (ret) {
			EXTERR("get %s %s failed\n", extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			return -1;
		}
		init_table[i+1] = *p;
		cmd_size = init_table[i+1];
		p++;
		if (cmd_size == 0) {
			i += 2;
			continue;
		}

		/* step3: data */
		len += cmd_size;
		ret = aml_lcd_unifykey_len_check(key_len, len);
		if (ret) {
			EXTERR("get %s %s failed\n", extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			for (j = 0; j < cmd_size; j++)
				init_table[i+2+j] = 0x0;
			return -1;
		}
		for (j = 0; j < cmd_size; j++) {
			init_table[i+2+j] = *p;
			p++;
		}
		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;

		i += (cmd_size + 2);
	}

	return 0;
}

static int aml_lcd_extern_init_table_fixed_size_load_unifykey(
		struct lcd_extern_config_s *extconf, unsigned char *p,
		int key_len, int len, int flag)
{
	unsigned char cmd_size;
	int i, j, max_len, ret = 0;
	unsigned char *init_table;
	char propname[20];

	cmd_size = extconf->cmd_size;
	if (flag) {
		init_table = extconf->table_init_on;
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		init_table = extconf->table_init_off;
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}

	i = 0;
	while (i < max_len) {
		len += cmd_size;
		ret = aml_lcd_unifykey_len_check(key_len, len);
		if (ret) {
			EXTERR("get %s %s failed\n", extconf->name, propname);
			init_table[i] = LCD_EXTERN_INIT_END;
			return -1;
		}
		for (j = 0; j < cmd_size; j++) {
			init_table[i+j] = *p;
			p++;
		}
		if (init_table[i] == LCD_EXTERN_INIT_END)
			break;

		i += cmd_size;
	}

	return 0;
}

static int aml_lcd_extern_get_config_unifykey(int index, struct lcd_extern_config_s *extconf)
{
	unsigned char *para, *p;
	int key_len, len;
	const char *str;
	struct aml_lcd_unifykey_header_s ext_header;
	int ret;

	extconf->table_init_loaded = 0;
	para = (unsigned char *)malloc(sizeof(unsigned char) * LCD_UKEY_LCD_EXT_SIZE);
	if (!para) {
		EXTERR("%s: Not enough memory\n", __func__);
		return -1;
	}
	key_len = LCD_UKEY_LCD_EXT_SIZE;
	memset(para, 0, (sizeof(unsigned char) * key_len));
	ret = aml_lcd_unifykey_get("lcd_extern", para, &key_len);
	if (ret) {
		free(para);
		return -1;
	}

	/* check lcd_extern unifykey length */
	len = 10 + 33 + 10;
	ret = aml_lcd_unifykey_len_check(key_len, len);
	if (ret) {
		EXTERR("unifykey length is not correct\n");
		free(para);
		return -1;
	}

	/* header: 10byte */
	aml_lcd_unifykey_header_check(para, &ext_header);
	if (lcd_debug_print_flag) {
		EXTPR("unifykey header:\n");
		EXTPR("crc32             = 0x%08x\n", ext_header.crc32);
		EXTPR("data_len          = %d\n", ext_header.data_len);
		EXTPR("version           = 0x%04x\n", ext_header.version);
		EXTPR("reserved          = 0x%04x\n", ext_header.reserved);
	}

	/* basic: 33byte */
	p = para + LCD_UKEY_HEAD_SIZE;
	*(p + LCD_UKEY_EXT_NAME - 1) = '\0'; /* ensure string ending */
	str = (const char *)p;
	strcpy(extconf->name, str);
	p += LCD_UKEY_EXT_NAME;
	extconf->index = *p;
	p += LCD_UKEY_EXT_INDEX;
	extconf->type = *p;
	p += LCD_UKEY_EXT_TYPE;
	extconf->status = *p;
	p += LCD_UKEY_EXT_STATUS;

	if (index != extconf->index) {
		EXTERR("index %d err, unifykey config index %d\n", index, extconf->index);
		free(para);
		return -1;
	}

	/* type: 10byte */
	switch (extconf->type) {
	case LCD_EXTERN_I2C:
		extconf->i2c_addr = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_0;
		extconf->i2c_addr2 = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_1;
		extconf->i2c_bus = aml_lcd_extern_get_i2c_bus_unifykey(*p);
		p += LCD_UKEY_EXT_TYPE_VAL_2;
		extconf->cmd_size = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_3;
		/* dummy pointer */
		p += LCD_UKEY_EXT_TYPE_VAL_4;
		p += LCD_UKEY_EXT_TYPE_VAL_5;
		p += LCD_UKEY_EXT_TYPE_VAL_6;
		p += LCD_UKEY_EXT_TYPE_VAL_7;
		p += LCD_UKEY_EXT_TYPE_VAL_8;
		p += LCD_UKEY_EXT_TYPE_VAL_9;

		/* init */
		if (extconf->cmd_size <= 1) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}
		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = aml_lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 0);
		} else {
			ret = aml_lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_SPI:
		extconf->spi_gpio_cs = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_0;
		extconf->spi_gpio_clk = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_1;
		extconf->spi_gpio_data = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_2;
		extconf->spi_clk_freq = (*p | ((*(p + 1)) << 8) |
					((*(p + 2)) << 16) |
					((*(p + 3)) << 24));
		p += LCD_UKEY_EXT_TYPE_VAL_3;
		p += LCD_UKEY_EXT_TYPE_VAL_4;
		p += LCD_UKEY_EXT_TYPE_VAL_5;
		p += LCD_UKEY_EXT_TYPE_VAL_6;
		extconf->spi_clk_pol = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_7;
		extconf->cmd_size = *p;
		p += LCD_UKEY_EXT_TYPE_VAL_8;
		/* dummy pointer */
		p += LCD_UKEY_EXT_TYPE_VAL_9;

		/* init */
		if (extconf->cmd_size <= 1) {
			EXTERR("cmd_size %d is invalid\n", extconf->cmd_size);
			break;
		}
		if (extconf->cmd_size == LCD_EXTERN_CMD_SIZE_DYNAMIC) {
			ret = aml_lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_dynamic_size_load_unifykey(
				extconf, p, key_len, len, 0);
		} else {
			ret = aml_lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 1);
			if (ret)
				break;
			ret = aml_lcd_extern_init_table_fixed_size_load_unifykey(
				extconf, p, key_len, len, 0);
		}
		if (ret == 0)
			extconf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_MIPI:
		/* dummy pointer */
		p += LCD_UKEY_EXT_TYPE_VAL_0;
		p += LCD_UKEY_EXT_TYPE_VAL_1;
		p += LCD_UKEY_EXT_TYPE_VAL_2;
		p += LCD_UKEY_EXT_TYPE_VAL_3;
		p += LCD_UKEY_EXT_TYPE_VAL_4;
		p += LCD_UKEY_EXT_TYPE_VAL_5;
		p += LCD_UKEY_EXT_TYPE_VAL_6;
		p += LCD_UKEY_EXT_TYPE_VAL_7;
		p += LCD_UKEY_EXT_TYPE_VAL_8;
		p += LCD_UKEY_EXT_TYPE_VAL_9;

		/* init */
		/* to do */
		break;
	default:
		break;
	}

	free(para);
	return 0;
}

#ifdef CONFIG_SYS_I2C_AML
static int aml_lcd_extern_add_i2c(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (ext_drv->config->index == 0) {
		ret = aml_lcd_extern_default_probe(ext_drv);
		return ret;
	}

	if (strcmp(ext_drv->config->name, "i2c_T5800Q") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_T5800Q
		ret = aml_lcd_extern_i2c_T5800Q_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config->name, "i2c_tc101") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_TC101
		ret = aml_lcd_extern_i2c_tc101_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config->name, "i2c_anx6345") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_ANX6345
		ret = aml_lcd_extern_i2c_anx6345_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config->name, "i2c_DLPC3439") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_DLPC3439
		ret = aml_lcd_extern_i2c_DLPC3439_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config->name);
		ret = -1;
	}
	return ret;
}
#endif

static int aml_lcd_extern_add_spi(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (ext_drv->config->index == 0) {
		ret = aml_lcd_extern_default_probe(ext_drv);
		return ret;
	}

	if (strcmp(ext_drv->config->name, "spi_LD070WS2") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_SPI_LD070WS2
		ret = aml_lcd_extern_spi_LD070WS2_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config->name);
		ret = -1;
	}
	return ret;
}

static int aml_lcd_extern_add_mipi(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (ext_drv->config->index == 0) {
		ret = aml_lcd_extern_default_probe(ext_drv);
		return ret;
	}

	if (strcmp(ext_drv->config->name, "mipi_N070ICN") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_MIPI_N070ICN
		ret = aml_lcd_extern_mipi_N070ICN_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config->name, "mipi_KD080D13") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_MIPI_KD080D13
		ret = aml_lcd_extern_mipi_KD080D13_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config->name);
		ret = -1;
	}
	return ret;
}

static int aml_lcd_extern_add_invalid(struct aml_lcd_extern_driver_s *ext_drv)
{
	return -1;
}

static int aml_lcd_extern_add_driver(struct lcd_extern_config_s *extconf)
{
	struct aml_lcd_extern_driver_s *ext_drv;
	int ret = -1;

	if (extconf->status == 0) {
		EXTERR("%s(%d) is disabled\n", extconf->name, extconf->index);
		return -1;
	}
	lcd_ext_driver = (struct aml_lcd_extern_driver_s *)malloc(sizeof(struct aml_lcd_extern_driver_s));
	if (lcd_ext_driver == NULL) {
		EXTERR("failed to alloc driver %s[%d], not enough memory\n", extconf->name, extconf->index);
		return -1;
	}

	/* fill config parameters */
	ext_drv = lcd_ext_driver;
	ext_drv->config = extconf;
	ext_drv->info_print = aml_lcd_extern_info_print;

	/* fill config parameters by different type */
	switch (ext_drv->config->type) {
	case LCD_EXTERN_I2C:
#ifdef CONFIG_SYS_I2C_AML
		ret = aml_lcd_extern_add_i2c(ext_drv);
#else
		EXTERR("system has no i2c support\n");
#endif
		break;
	case LCD_EXTERN_SPI:
		ret = aml_lcd_extern_add_spi(ext_drv);
		break;
	case LCD_EXTERN_MIPI:
		ret = aml_lcd_extern_add_mipi(ext_drv);
		break;
	default:
		ret = aml_lcd_extern_add_invalid(ext_drv);
		EXTERR("don't support type %d\n", ext_drv->config->type);
		break;
	}
	if (ret) {
		EXTERR("add driver failed\n");
		free(lcd_ext_driver);
		lcd_ext_driver = NULL;
		return -1;
	}

	EXTPR("add driver %s(%d)\n", ext_drv->config->name, ext_drv->config->index);
	return ret;
}

static int aml_lcd_extern_add_driver_default(int index, struct lcd_extern_config_s *extconf)
{
	int drv_index = extconf->index;
	int ret = -1;
	struct aml_lcd_extern_driver_s *ext_drv;

	if (index != drv_index) {
		EXTERR("index %d err, default config index %d\n", index, drv_index);
		return -1;
	}

	if (extconf->status == 0) {
		EXTERR("%s(%d) is disabled\n", extconf->name, drv_index);
		return -1;
	}

	lcd_ext_driver = (struct aml_lcd_extern_driver_s *)malloc(sizeof(struct aml_lcd_extern_driver_s));
	if (lcd_ext_driver == NULL) {
		EXTERR("failed to alloc driver %d, not enough memory\n", index);
		return -1;
	}

	drv_index = LCD_EXTERN_INDEX_INVALID;
	ext_drv = lcd_ext_driver;
	ext_drv->config = extconf;
	ext_drv->config->table_init_loaded = 1;
	ext_drv->info_print  = aml_lcd_extern_info_print;

	/* add ext_default driver */
	if (index == 0) {
		ret = aml_lcd_extern_default_probe(ext_drv);
		goto add_driver_default_end;
	}
#ifdef CONFIG_SYS_I2C_AML
#ifdef CONFIG_AML_LCD_EXTERN_I2C_T5800Q
	drv_index = aml_lcd_extern_i2c_T5800Q_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_i2c_T5800Q_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif
#ifdef CONFIG_AML_LCD_EXTERN_I2C_TC101
	drv_index = aml_lcd_extern_i2c_tc101_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_i2c_tc101_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif
#ifdef CONFIG_AML_LCD_EXTERN_I2C_ANX6345
	drv_index = aml_lcd_extern_i2c_anx6345_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_i2c_anx6345_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif
#ifdef CONFIG_AML_LCD_EXTERN_I2C_DLPC3439
	drv_index = aml_lcd_extern_i2c_DLPC3439_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_i2c_DLPC3439_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif
#endif
#ifdef CONFIG_AML_LCD_EXTERN_SPI_LD070WS2
	drv_index = aml_lcd_extern_spi_LD070WS2_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_spi_LD070WS2_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif
#ifdef CONFIG_AML_LCD_EXTERN_MIPI_N070ICN
	drv_index = aml_lcd_extern_mipi_N070ICN_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_mipi_N070ICN_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif
#ifdef CONFIG_AML_LCD_EXTERN_MIPI_KD080D13
	drv_index = aml_lcd_extern_mipi_KD080D13_get_default_index();
	if (drv_index == index) {
		ret = aml_lcd_extern_mipi_KD080D13_probe(ext_drv);
		goto add_driver_default_end;
	}
#endif

add_driver_default_end:
	if (ret) {
		EXTERR("add driver failed\n");
		free(lcd_ext_driver);
		lcd_ext_driver = NULL;
		return -1;
	}
	EXTPR("add default driver %d\n", index);
	return ret;
}

int aml_lcd_extern_probe(char *dtaddr, int index)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_extern_config_s *ext_config;
	int ret, load_id = 0;

	if (index >= LCD_EXTERN_INDEX_INVALID) {
		EXTERR("invalid index, %s exit\n", __func__);
		return -1;
	}

	dt_addr = NULL;
	ext_config = &ext_config_dtf;
	ext_config->table_init_loaded = 0;

	/* check dts config */
#ifndef DTB_BIND_KERNEL
#ifdef CONFIG_OF_LIBFDT
	if (dtaddr)
		dt_addr = dtaddr;
	if (fdt_check_header(dtaddr) < 0) {
		EXTERR("check dts: %s, use default parameters\n",
			fdt_strerror(fdt_check_header(dt_addr)));
	} else {
		load_id = 1;
	}
#endif
#endif

	if (lcd_debug_test)
		load_id = 0;

	switch (load_id) {
	case 1: /* dts */
		aml_lcd_extern_get_init_dts(dtaddr, ext_config);
		if (lcd_drv->unifykey_test_flag) {
			ext_config->lcd_ext_key_valid = 1;
			LCDPR("force lcd_ext_key_valid to 1\n");
		}
		/* check unifykey config */
		if (ext_config->lcd_ext_key_valid) {
			ret = aml_lcd_unifykey_check("lcd_extern");
			if (ret == 0) {
				EXTPR("load config from unifykey\n");
				ret = aml_lcd_extern_get_config_unifykey(index, ext_config);
				if (ret == 0)
					ret = aml_lcd_extern_add_driver(ext_config);
			}
		} else {
			EXTPR("load config from dts\n");
			ret = aml_lcd_extern_get_config_dts(dtaddr, index, ext_config);
			if (ret == 0)
				ret = aml_lcd_extern_add_driver(ext_config);
		}
		break;
	default: /* default */
		if (lcd_drv->unifykey_test_flag) {
			ext_config->lcd_ext_key_valid = 1;
			LCDPR("force lcd_ext_key_valid to 1\n");
		}
		if (ext_config->lcd_ext_key_valid) {
			ret = aml_lcd_unifykey_check("lcd_extern");
			if (ret == 0) {
				EXTPR("load config from unifykey\n");
				ret = aml_lcd_extern_get_config_unifykey(index, ext_config);
				if (ret == 0)
					ret = aml_lcd_extern_add_driver(ext_config);
			}
		} else {
			EXTPR("load config from bsp\n");
			ret = aml_lcd_extern_add_driver_default(index, ext_config);
		}
		break;
	}

	EXTPR("%s %s\n", __func__, (ret ? "failed" : "ok"));
	return ret;
}

int aml_lcd_extern_remove(void)
{
	if (lcd_ext_driver)
		free(lcd_ext_driver);
	lcd_ext_driver = NULL;
	return 0;
}

