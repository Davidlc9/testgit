/*
 * Copyright (C) Pateo, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fsl_devices.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <mach/audmux.h>
#include <mach/ssi.h>

#include "imx-ssi.h"

static struct imx_tef6635_priv {
	int sysclk;
	int hw;
	int active;
	struct platform_device *pdev;
} card_priv;


static int imx_audmux_config(int slave, int master)
{
	unsigned int ptcr, pdcr;
	slave = slave - 1;
	master = master - 1;

	ptcr = MXC_AUDMUX_V2_PTCR_SYN |
		MXC_AUDMUX_V2_PTCR_TFSDIR |
		MXC_AUDMUX_V2_PTCR_TFSEL(master) |
		MXC_AUDMUX_V2_PTCR_TCLKDIR |
		MXC_AUDMUX_V2_PTCR_TCSEL(master);
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(master);
	mxc_audmux_v2_configure_port(slave, ptcr, pdcr);

	ptcr = MXC_AUDMUX_V2_PTCR_SYN;
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(slave);
	mxc_audmux_v2_configure_port(master, ptcr, pdcr);

	return 0;
}

static int imx_tef6635_startup(struct snd_pcm_substream *substream)
{
	struct imx_tef6635_priv *priv = &card_priv;

	priv->active++;
	return 0;
}

static int imx_tef6635_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int channels = params_channels(params);
	struct imx_ssi *ssi_mode = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;
	u32 dai_format;

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
	    SND_SOC_DAIFMT_CBS_CFS;

	ssi_mode->flags |= IMX_SSI_SYN;

	/* set i.MX active slot mask */
	snd_soc_dai_set_tdm_slot(cpu_dai,
			channels == 1 ? 0xfffffffe : 0xfffffffc,
			channels == 1 ? 0xfffffffe : 0xfffffffc,
			2, 16);

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_format);
	if (ret < 0)
		return ret;

	/* set the SSI system clock as output (unused) */
	snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_OUT);

	snd_soc_dai_set_clkdiv(cpu_dai, IMX_SSI_TX_DIV_PM, 2);
	snd_soc_dai_set_clkdiv(cpu_dai, IMX_SSI_TX_DIV_2, 0);
	snd_soc_dai_set_clkdiv(cpu_dai, IMX_SSI_TX_DIV_PSR, 3);

	return 0;
}

static void imx_tef6635_shutdown(struct snd_pcm_substream *substream)
{
	struct imx_tef6635_priv *priv = &card_priv;
	priv->active--;
}


static struct snd_soc_ops imx_tef6635_ops = {
	.startup = imx_tef6635_startup,
	.hw_params = imx_tef6635_hw_params,
	.shutdown = imx_tef6635_shutdown,
};

static struct snd_soc_dai_link imx_dai = {
	.name = "imx-tef6635.1",
	.stream_name = "imx-tef6635.1",
	.codec_dai_name = "tef6635.1",
	.codec_name     = "imx-tef6635.1",
	.cpu_dai_name   = "imx-ssi.1",
	.platform_name  = "imx-pcm-audio.1",
	.ops = &imx_tef6635_ops,
};

static struct snd_soc_card snd_soc_card_imx_tef6635 = {
	.name = "imx-tef6635-ssi",
	.dai_link = &imx_dai,
	.num_links = 1,
};

static int __devinit imx_tef6635_probe(struct platform_device *pdev)
{
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;

	card_priv.pdev = pdev;
	card_priv.sysclk = plat->sysclk;
	imx_audmux_config(plat->ext_port, plat->src_port);

	return 0;

}

static int __devexit imx_tef6635_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver imx_tef6635_driver = {
	.probe = imx_tef6635_probe,
	.remove = __devexit_p(imx_tef6635_remove),
	.driver = {
		   .name = "imx-tef6635-ssi",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device *imx_snd_device;

static int __init imx_asoc_init(void)
{
	int ret;
	ret = platform_driver_register(&imx_tef6635_driver);
	if (ret < 0)
		goto exit;

	imx_snd_device = platform_device_alloc("soc-audio", 6);
	if (!imx_snd_device)
		goto err_device_alloc;

	platform_set_drvdata(imx_snd_device, &snd_soc_card_imx_tef6635);
	ret = platform_device_add(imx_snd_device);
	if (0 == ret)
		goto exit;


	platform_device_put(imx_snd_device);
err_device_alloc:
	platform_driver_unregister(&imx_tef6635_driver);
exit:
	return ret;
}

static void __exit imx_asoc_exit(void)
{
	platform_driver_unregister(&imx_tef6635_driver);
	platform_device_unregister(imx_snd_device);
}

module_init(imx_asoc_init);
module_exit(imx_asoc_exit);

/* Module information */
MODULE_AUTHOR("Pateo, Inc.");
MODULE_DESCRIPTION("ALSA SoC tef6635 imx");
MODULE_LICENSE("GPL");
