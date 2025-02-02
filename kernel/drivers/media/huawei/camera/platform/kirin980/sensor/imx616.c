 

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>

#include "hwsensor.h"
#include "sensor_commom.h"
#include "hw_csi.h"
#include <linux/pinctrl/consumer.h>
#include "../pmic/hw_pmic.h"

//lint -save -e846 -e866 -e826 -e785 -e838 -e715 -e747 -e774 -e778 -e732 -e731 -e569 -e650 -e31
#define I2S(i) container_of((i), sensor_t, intf)
#define Sensor2Pdev(s) container_of((s).dev, struct platform_device, dev)
static hwsensor_vtbl_t s_imx616_vtbl;
static bool power_on_status = false;//false: power off, true:power on
int imx616_config(hwsensor_intf_t* si, void  *argp);
static struct platform_device *s_pdev = NULL;
struct mutex imx616_power_lock;
extern int strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count);

/* imx616 cs udp project */
static struct sensor_power_setting imx616_power_setting[] = {
    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_AVDD,
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 DVDD 1.1V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P1V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST2,
        .config_val = SENSOR_GPIO_HIGH,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};

/* imx616 A&B project */
static struct sensor_power_setting imx616_power_setting_ab[] = {
    //SENSOR IOVDD 1.8V VOUT21
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },

    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_AVDD,
        .data = (void*)"front-sensor-avdd",
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },

    //MCAM1 DVDD 1.1V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_V1P1V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },

    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },

    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};

// ES UDP power up seq
static struct sensor_power_setting imx616_power_setting_udp[] = {
    //disable main camera reset
    {
        .seq_type = SENSOR_SUSPEND,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //disable sub camera reset
    {
        .seq_type = SENSOR_SUSPEND2,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_AVDD,
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 DVDD 1.2V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P2V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //VCM [2.60v]
    {
        .seq_type = SENSOR_VCM_AVDD,
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};

// Emily power up seq
static struct sensor_power_setting imx616_power_setting_emily[] = {
    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_AVDD,
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 DVDD 1.1V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P1V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};

// Neo power up seq
static struct sensor_power_setting imx616_power_setting_neo[] = {
    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_AVDD,
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 DVDD 1.1V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P1V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST2,
        .config_val = SENSOR_GPIO_HIGH,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};


// hma power up seq
static struct sensor_power_setting imx616_power_setting_hma[] = {
    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_AVDD1_EN,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 DVDD 1.1V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P1V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};


// hma power up seq
static struct sensor_power_setting imx616_power_setting_laya[] = {
    //MCAM1 AVDD 2.8V
    {
        .seq_type = SENSOR_PMIC,
        .seq_val = VOUT_LDO_1,
        .config_val = PMIC_2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 DVDD 1.1V
    {
        .seq_type = SENSOR_PMIC,
        .seq_val = VOUT_BUCK_1,
        .config_val = PMIC_1P1V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
};

atomic_t volatile imx616_powered = ATOMIC_INIT(0);
static sensor_t s_imx616 =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
            .size = ARRAY_SIZE(imx616_power_setting),
            .power_setting = imx616_power_setting,
     },
     .p_atpowercnt = &imx616_powered,
};

static sensor_t s_imx616_ab =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(imx616_power_setting_ab),
        .power_setting = imx616_power_setting_ab,
    },
    .p_atpowercnt = &imx616_powered,
};

static sensor_t s_imx616_udp =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(imx616_power_setting_udp),
        .power_setting = imx616_power_setting_udp,
    },
    .p_atpowercnt = &imx616_powered,
};

static sensor_t s_imx616_emily =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(imx616_power_setting_emily),
        .power_setting = imx616_power_setting_emily,
    },
    .p_atpowercnt = &imx616_powered,
};

static sensor_t s_imx616_neo =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(imx616_power_setting_neo),
        .power_setting = imx616_power_setting_neo,
    },
    .p_atpowercnt = &imx616_powered,
};

static sensor_t s_imx616_hma =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(imx616_power_setting_hma),
        .power_setting = imx616_power_setting_hma,
    },
    .p_atpowercnt = &imx616_powered,
};

static sensor_t s_imx616_laya =
{
    .intf = { .vtbl = &s_imx616_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(imx616_power_setting_laya),
        .power_setting = imx616_power_setting_laya,
    },
    .p_atpowercnt = &imx616_powered,
};

static const struct of_device_id s_imx616_dt_match[] =
{
    {
        .compatible = "huawei,imx616",
        .data = &s_imx616.intf,
    },
    {
        .compatible = "huawei,imx616_ab",
        .data = &s_imx616_ab.intf,
    },
    {
        .compatible = "huawei,imx616_udp",
        .data = &s_imx616_udp.intf,
    },
    {
        .compatible = "huawei,imx616_emily",
        .data = &s_imx616_emily.intf,
    },
    {
        .compatible = "huawei,imx616_neo",
        .data = &s_imx616_neo.intf,
    },
    {
        .compatible = "huawei,imx616_hma",
        .data = &s_imx616_hma.intf,
    },
    {
        .compatible = "huawei,imx616_laya",
        .data = &s_imx616_laya.intf,
    },
    {

    },/* terminate list */
};

MODULE_DEVICE_TABLE(of, s_imx616_dt_match);

static struct platform_driver s_imx616_driver =
{
    .driver =
    {
        .name = "huawei,imx616",
        .owner = THIS_MODULE,
        .of_match_table = s_imx616_dt_match,
    },
};

char const* imx616_get_name(hwsensor_intf_t* si)
{
    sensor_t* sensor = NULL;
    if (NULL == si) {
        cam_err("%s. si is NULL.", __func__);
        return NULL;
    }
    sensor = I2S(si);
    return sensor->board_info->name;
}

int imx616_power_up(hwsensor_intf_t* si)
{
    int ret = 0;
    sensor_t* sensor = NULL;

    if (NULL == si) {
        cam_err("%s. si is NULL.", __func__);
        return -EINVAL;
    }

    sensor = I2S(si);
    cam_info("enter %s. index = %d name = %s", __func__, sensor->board_info->sensor_index, sensor->board_info->name);

    if (hw_is_fpga_board()) {
        ret = do_sensor_power_on(sensor->board_info->sensor_index, sensor->board_info->name);
    } else {
        ret = hw_sensor_power_up(sensor);
    }

    if (0 == ret) {
        cam_info("%s. power up sensor success.", __func__);
    } else {
        cam_err("%s. power up sensor fail.", __func__);
    }
    return ret;
}

int
imx616_power_down(
        hwsensor_intf_t* si)
{
    int ret = 0;
    sensor_t* sensor = NULL;

    if (NULL == si) {
        cam_err("%s. si is NULL.", __func__);
        return -EINVAL;
    }

    sensor = I2S(si);
    cam_info("enter %s. index = %d name = %s", __func__, sensor->board_info->sensor_index, sensor->board_info->name);
    if (hw_is_fpga_board()) {
        ret = do_sensor_power_off(sensor->board_info->sensor_index, sensor->board_info->name);
    } else {
        ret = hw_sensor_power_down(sensor);
    }

    if (0 == ret) {
        cam_info("%s. power down sensor success.", __func__);
    } else {
        cam_err("%s. power down sensor fail.", __func__);
    }

    return ret;
}

int imx616_csi_enable(hwsensor_intf_t* si)
{
    return 0;
}

int imx616_csi_disable(hwsensor_intf_t* si)
{
    return 0;
}

static int imx616_match_id(
        hwsensor_intf_t* si, void * data)
{
    sensor_t* sensor = I2S(si);
    struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
    char * sensor_name[] = {"IMX616_SUNNY"};
	int ret;

    cam_info("%s enter.", __func__);

    cdata->data = SENSOR_INDEX_INVALID;

    if(!strncmp(sensor->board_info->name,"IMX616_SUNNY",strlen("IMX616_SUNNY")))
    {
		ret = strncpy_s(cdata->cfg.name,
			DEVICE_NAME_SIZE - 1,
			sensor_name[0],
			DEVICE_NAME_SIZE - 1);
		if (ret != 0)
			cam_err("%s.strncpy failed %d", __func__, __LINE__);
        cdata->data = sensor->board_info->sensor_index;
    }
    else
    {
		ret = strncpy_s(cdata->cfg.name,
			DEVICE_NAME_SIZE - 1,
			sensor->board_info->name,
			DEVICE_NAME_SIZE - 1);
		if (ret != 0)
			cam_err("%s.strncpy failed %d", __func__, __LINE__);
        cdata->data = sensor->board_info->sensor_index;
    }

    if (cdata->data != SENSOR_INDEX_INVALID)
    {
        cam_info("%s, cdata->cfg.name = %s", __func__,cdata->cfg.name );
    }
    cam_info("%s TODO.  cdata->data=%d", __func__, cdata->data);
    return 0;
}

static hwsensor_vtbl_t s_imx616_vtbl =
{
    .get_name = imx616_get_name,
    .config = imx616_config,
    .power_up = imx616_power_up,
    .power_down = imx616_power_down,
    .match_id = imx616_match_id,
    .csi_enable = imx616_csi_enable,
    .csi_disable = imx616_csi_disable,
};

int imx616_config(hwsensor_intf_t* si, void  *argp)
{
    struct sensor_cfg_data *data = NULL;
    int ret = 0;

    if ((NULL == si) || (NULL == argp)) {
        cam_err("%s : si or argp is null", __func__);
        return -EINVAL;
    }

    data = (struct sensor_cfg_data *)argp;
    cam_debug("imx616 cfgtype = %d",data->cfgtype);
    switch(data->cfgtype) {
        case SEN_CONFIG_POWER_ON:
            mutex_lock(&imx616_power_lock);
            if (false == power_on_status) {
                ret = si->vtbl->power_up(si);
                if (0 == ret) {
                    power_on_status = true;
                } else {
                    cam_err("%s. power up fail.", __func__);
                }
            }
            /*lint -e455 -esym(455,*)*/
            mutex_unlock(&imx616_power_lock);
            /*lint -e455 -esym(455,*)*/
            break;
        case SEN_CONFIG_POWER_OFF:
            mutex_lock(&imx616_power_lock);
            if (true == power_on_status) {
                ret = si->vtbl->power_down(si);
                if (0 != ret) {
                    cam_err("%s. power_down fail.", __func__);
                }
                power_on_status = false;
            }
            /*lint -e455 -esym(455,*)*/
            mutex_unlock(&imx616_power_lock);
            /*lint -e455 -esym(455,*)*/

            break;
        case SEN_CONFIG_WRITE_REG:
        case SEN_CONFIG_READ_REG:
        case SEN_CONFIG_WRITE_REG_SETTINGS:
        case SEN_CONFIG_READ_REG_SETTINGS:
        case SEN_CONFIG_ENABLE_CSI:
        case SEN_CONFIG_DISABLE_CSI:
            break;
        case SEN_CONFIG_MATCH_ID:
            ret = si->vtbl->match_id(si,argp);
            break;
        default:
            cam_warn("%s cfgtype(%d) is unknow", __func__, data->cfgtype);
            break;
    }

    return ret;
}

static int32_t imx616_platform_probe(struct platform_device* pdev)
{
    int rc = 0;
    const struct of_device_id *id = NULL;
    hwsensor_intf_t *intf = NULL;
    sensor_t *sensor = NULL;
    struct device_node *np = NULL;
    cam_notice("enter %s",__func__);

    if (NULL == pdev) {
        cam_err("%s pdev is NULL", __func__);
        return -EINVAL;
    }

    np = pdev->dev.of_node;
    if (NULL == np) {
        cam_err("%s of_node is NULL", __func__);
        return -ENODEV;
    }

    id = of_match_node(s_imx616_dt_match, np);
    if (!id) {
        cam_err("%s none id matched", __func__);
        return -ENODEV;
    }

    intf = (hwsensor_intf_t*)id->data;
    sensor = I2S(intf);

    rc = hw_sensor_get_dt_data(pdev, sensor);
    if (rc < 0) {
        cam_err("%s no dt data rc %d", __func__, rc);
        return -ENODEV;
    }
    sensor->dev = &pdev->dev;
    mutex_init(&imx616_power_lock);
    rc = hwsensor_register(pdev, intf);
    if (rc < 0) {
        cam_err("%s hwsensor_register failed rc %d\n", __func__, rc);
        return -ENODEV;
    }
    s_pdev = pdev;
    rc = rpmsg_sensor_register(pdev, (void*)sensor);
    if (rc < 0) {
        hwsensor_unregister(s_pdev);
        s_pdev = NULL;
        cam_err("%s rpmsg_sensor_register failed rc %d\n", __func__, rc);
        return -ENODEV;
    }

    return rc;
}

static int __init imx616_init_module(void)
{
    cam_info("Enter: %s", __func__);
    return platform_driver_probe(&s_imx616_driver,
            imx616_platform_probe);
}

static void __exit imx616_exit_module(void)
{
    if (NULL != s_pdev) {
        hwsensor_unregister(s_pdev);
        s_pdev = NULL;
    }

    platform_driver_unregister(&s_imx616_driver);
}

module_init(imx616_init_module);
module_exit(imx616_exit_module);
MODULE_DESCRIPTION("imx616");
MODULE_LICENSE("GPL v2");
//lint -restore
