/*
 * Copyright Pateo, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/tef6635/tef6635-core.h>

#define TEF6635_RATES SNDRV_PCM_RATE_44100
#define TEF6635_FORMATS SNDRV_PCM_FMTBIT_S16_LE


#define TEF6635_SECONDCARD_VOLUME_0DB      36
#define TEF6635_SECONDCARD_VOLUME_MAX      44
#define Vol2Gain_SecondCard(x)  ((x) - TEF6635_SECONDCARD_VOLUME_0DB)

#define TEF6635_SUPERP_ADDON_VOLUME_0DB    63
#define TEF6635_SUPERP_ADDON_VOLUME_MAX    63
#define Vol2Gain_SuperP_Addon(x)    ((x) - TEF6635_SUPERP_ADDON_VOLUME_MAX)

#define SECONDARY_VOLUME_VAL_DEF    36
#define ADDON_VOLUME_VAL_DEF        53

#define INIT_SECOND_CARD    0

enum tef6635_reg_tags {
    Asp_SecondCard_Volume = 1,
    Asp_Addon_FL_Volume = 2,
    Asp_Addon_FR_Volume = 3,
    Asp_Addon_RL_Volume = 4,
    Asp_Addon_RR_Volume = 5,
};


static struct snd_soc_dai_driver tef6635_codec_dai = {
    .name = "tef6635",
    .playback = {
        .stream_name = "playback",
        .channels_min = 1,
        .channels_max = 2,
        .rates = TEF6635_RATES,
        .formats = TEF6635_FORMATS,
    },
};


struct tef6635_control_data_t {
    int tef6635_secondcard_volume;
    unsigned int tef6635_addon_fl_volume;
    unsigned int tef6635_addon_fr_volume;
    unsigned int tef6635_addon_rl_volume;
    unsigned int tef6635_addon_rr_volume;
};

static  struct tef6635_control_data_t tef6635_control_data;

static int tef6635_set_volume(enum tef6635_reg_tags tag, int volume)
{
    int ret = 0;

    switch(tag) {
    case Asp_SecondCard_Volume:
        ret = TEF6635_Ctrl_NavigationVolumePreset(Vol2Gain_SecondCard(volume));
        if (ret) {
            pr_err("ERR: %s: fail to set secondcard volume(%d).\n",
                    __func__, volume);
            return ret;
        }
        tef6635_control_data.tef6635_secondcard_volume = volume;
        break;
    case Asp_Addon_FL_Volume:
        ret = TEF6635_Ctrl_SuperPositionPreset(TEF6635_SUPER_PRIMARY_STREAM_FRONT,
                TEF6635_ADDON_NAVI, Vol2Gain_SuperP_Addon(volume),
                Vol2Gain_SuperP_Addon(tef6635_control_data.tef6635_addon_fr_volume));
        if (ret) {
            pr_err("fail to set Asp_Addon_FL_Volume.\n");
            return ret;
        }
        tef6635_control_data.tef6635_addon_fl_volume = volume;
        break;
    case Asp_Addon_FR_Volume:
        ret = TEF6635_Ctrl_SuperPositionPreset(TEF6635_SUPER_PRIMARY_STREAM_FRONT,
                TEF6635_ADDON_NAVI,
                Vol2Gain_SuperP_Addon(tef6635_control_data.tef6635_addon_fl_volume),
                Vol2Gain_SuperP_Addon(volume));
        if (ret) {
            pr_err("fail to set Asp_Addon_FR_Volume.\n");
            return ret;
        }
        tef6635_control_data.tef6635_addon_fr_volume = volume;
        break;
    case Asp_Addon_RL_Volume:
        ret = TEF6635_Ctrl_SuperPositionPreset(TEF6635_SUPER_PRIMARY_STREAM_REAR,
                TEF6635_ADDON_NAVI,
                Vol2Gain_SuperP_Addon(volume),
                Vol2Gain_SuperP_Addon(tef6635_control_data.tef6635_addon_rr_volume));
        if (ret) {
            pr_err("fail to set Asp_Addon_RL_Volume.\n");
            return ret;
        }
        tef6635_control_data.tef6635_addon_rl_volume = volume;
        break;
    case Asp_Addon_RR_Volume:
        ret = TEF6635_Ctrl_SuperPositionPreset(TEF6635_SUPER_PRIMARY_STREAM_REAR,
                TEF6635_ADDON_NAVI,
                Vol2Gain_SuperP_Addon(tef6635_control_data.tef6635_addon_rl_volume),
                Vol2Gain_SuperP_Addon(volume));
        if (ret) {
            pr_err("fail to set Asp_Addon_RR_Volume.\n");
            return ret;
        }
        tef6635_control_data.tef6635_addon_rr_volume = volume;
        break;
    default:
        pr_err("Unkown reg(%d).\n", tag);
        return -EINVAL;
    }

    return ret;
}


static int tef6635_secondcard_volume_put(struct snd_kcontrol *kcontrol,
                        struct snd_ctl_elem_value *ucontrol)
{
    struct soc_mixer_control *mc =
                (struct soc_mixer_control *)kcontrol->private_value;

    if(ucontrol->value.integer.value[0] > mc->max) {
        pr_err("volume is larger than max(%d).\n", mc->max);
        return -EINVAL;
    }

    return tef6635_set_volume(mc->reg, ucontrol->value.integer.value[0]);
}

static int tef6635_secondcard_volume_get(struct snd_kcontrol *kcontrol,
                    struct snd_ctl_elem_value *ucontrol)
{
    struct soc_mixer_control *mc =
                        (struct soc_mixer_control *)kcontrol->private_value;
    switch(mc->reg) {
        case Asp_SecondCard_Volume:
            ucontrol->value.integer.value[0] =
                tef6635_control_data.tef6635_secondcard_volume;
            break;
        case Asp_Addon_FL_Volume:
            ucontrol->value.integer.value[0] =
                tef6635_control_data.tef6635_addon_fl_volume;
            break;
        case Asp_Addon_FR_Volume:
            ucontrol->value.integer.value[0] =
                tef6635_control_data.tef6635_addon_fr_volume;
            break;
        case Asp_Addon_RL_Volume:
            ucontrol->value.integer.value[0] =
                tef6635_control_data.tef6635_addon_rl_volume;
            break;
        case Asp_Addon_RR_Volume:
            ucontrol->value.integer.value[0] =
                tef6635_control_data.tef6635_addon_rr_volume;
            break;
    }

    return 0;
}


static const struct snd_kcontrol_new tef6635_snd_controls[] = {
    SOC_SINGLE_EXT("Asp SecondCard Volume",
        Asp_SecondCard_Volume, 0, TEF6635_SECONDCARD_VOLUME_MAX, 0,
        tef6635_secondcard_volume_get,
        tef6635_secondcard_volume_put),
    SOC_SINGLE_EXT("Asp Addon FL Volume",
        Asp_Addon_FL_Volume, 0, TEF6635_SUPERP_ADDON_VOLUME_MAX, 1,
        tef6635_secondcard_volume_get,
        tef6635_secondcard_volume_put),
    SOC_SINGLE_EXT("Asp Addon FR Volume",
        Asp_Addon_FR_Volume, 0, TEF6635_SUPERP_ADDON_VOLUME_MAX, 1,
        tef6635_secondcard_volume_get,
        tef6635_secondcard_volume_put),
    SOC_SINGLE_EXT("Asp Addon RL Volume",
        Asp_Addon_RL_Volume, 0, TEF6635_SUPERP_ADDON_VOLUME_MAX, 1,
        tef6635_secondcard_volume_get,
        tef6635_secondcard_volume_put),
    SOC_SINGLE_EXT("Asp Addon RR Volume",
        Asp_Addon_RR_Volume, 0, TEF6635_SUPERP_ADDON_VOLUME_MAX, 1,
        tef6635_secondcard_volume_get,
        tef6635_secondcard_volume_put),
};



static int tef6635_init(void)
{

#if INIT_SECOND_CARD
    int ret = 0;
    /* Choose IIS0 in Navi channel */
    ret = TEF6635_Ctrl_SourceSelectConnect(TEF6635_STREAM_NAVIGATION,
                IIS0_INPUT);
    if (ret) {
        pr_err("Set IIS0 to navi channel failed.\n");
        return ret;
    }

    /* set IIS0 format i2s */
    ret = TEF6635_PeripheralConfiguration(PID_IIS0_INPUT, FORMAT_I2S);
    if (ret) {
        pr_err("Set IIS0 to format iis failed.\n");
        return ret;
    }
#endif
    memset(&tef6635_control_data, 0x0, sizeof(struct tef6635_control_data_t));

    tef6635_control_data.tef6635_secondcard_volume = SECONDARY_VOLUME_VAL_DEF;
    tef6635_control_data.tef6635_addon_fl_volume = ADDON_VOLUME_VAL_DEF;
    tef6635_control_data.tef6635_addon_fr_volume = ADDON_VOLUME_VAL_DEF;
    tef6635_control_data.tef6635_addon_rl_volume = ADDON_VOLUME_VAL_DEF;
    tef6635_control_data.tef6635_addon_rr_volume = ADDON_VOLUME_VAL_DEF;

    tef6635_set_volume(Asp_SecondCard_Volume,
                tef6635_control_data.tef6635_secondcard_volume);
    tef6635_set_volume(Asp_Addon_FL_Volume,
                tef6635_control_data.tef6635_addon_fl_volume);
    tef6635_set_volume(Asp_Addon_FR_Volume,
                tef6635_control_data.tef6635_addon_fr_volume);
    tef6635_set_volume(Asp_Addon_RL_Volume,
                tef6635_control_data.tef6635_addon_rl_volume);
    tef6635_set_volume(Asp_Addon_RR_Volume,
                tef6635_control_data.tef6635_addon_rr_volume);

    return 0;
}


static int mxc_tef6635_codec_soc_probe(struct snd_soc_codec *codec)
{
    tef6635_init();

    snd_soc_add_controls(codec, tef6635_snd_controls,
            ARRAY_SIZE(tef6635_snd_controls));

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_tef6635 = {
	.probe = mxc_tef6635_codec_soc_probe,
};

static int tef6635_probe(struct platform_device *pdev)
{
	int ret = 0;
	dev_info(&pdev->dev, "TEF6635 Audio codec\n");

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tef6635,
					&tef6635_codec_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register TEF6635 SSI codec\n");
		return ret;
	}

	return 0;

}

static int tef6635_remove(struct platform_device *pdev)
{
	return 0;
}

static int tef6635_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int tef6635_resume(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver tef6635_driver = {
    .driver = {
        .name = "imx-tef6635",
        .owner = THIS_MODULE,
    },
	.probe = tef6635_probe,
	.remove = tef6635_remove,
	.suspend = tef6635_suspend,
	.resume = tef6635_resume,
};

static int __init tef6635_codec_init(void)
{
	return platform_driver_register(&tef6635_driver);
}

static void __exit tef6635_codec_exit(void)
{
	return platform_driver_unregister(&tef6635_driver);
}

module_init(tef6635_codec_init);
module_exit(tef6635_codec_exit);

MODULE_DESCRIPTION("ASoC tef6635 codec driver");
MODULE_AUTHOR("Pateo, Inc.");
MODULE_LICENSE("GPL");
