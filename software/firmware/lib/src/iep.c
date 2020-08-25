#include <stdint.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_iep.h>

#include "iep.h"

#ifdef __GNUC__
#include <pru/io.h>
#else
volatile register uint32_t __R31;
#endif

void iep_set_us(unsigned int value)
{
	iep_stop();
	iep_reset();
	/* Set compare value */
	CT_IEP.TMR_CMP0 = value * 200;

}

void iep_reset()
{
	/* Reset Count register */
	CT_IEP.TMR_CNT = 0x0;
}

void iep_start()
{
	/* Enable counter */
	CT_IEP.TMR_GLB_CFG |= (1 << 0);
}

void iep_stop()
{
	/* Disable counter */
	CT_IEP.TMR_GLB_CFG &= ~(1 << 0);
}


inline int iep_check_evt_cmp(unsigned int compare_channel)
{
	if(CT_IEP.TMR_CMP_STS & (1 << compare_channel))
		return 0;
	else
		return -1;
}

inline void iep_clear_evt_cmp(unsigned int compare_channel)
{
	CT_IEP.TMR_CMP_STS |= (1 << compare_channel);
}

void iep_enable_evt_cmp(unsigned int compare_channel)
{
	CT_IEP.TMR_CMP_CFG_bit.CMP_EN |= (1 << compare_channel);
}

void iep_disable_evt_cmp(unsigned int compare_channel)
{
	CT_IEP.TMR_CMP_CFG_bit.CMP_EN &= ~(1 << compare_channel);
}

void iep_set_cmp_val(unsigned int compare_channel, unsigned int value)
{
	/* Hack to address the CMPN registers (offset=0x48 from CT_IEP) */
	*((unsigned int *) &CT_IEP + 18 + compare_channel) = value;
}

unsigned int iep_get_cmp_val(unsigned int compare_channel)
{
	/* Hack to address the CMPN registers (offset=0x48 from CT_IEP) */
	return *((unsigned int *) &CT_IEP + 18 + compare_channel);
}

void iep_init()
{
	/* Disable counter */
	CT_IEP.TMR_GLB_CFG_bit.CNT_EN = 0;

	/* Reset Count register */
	CT_IEP.TMR_CNT = 0x0;

	/* Clear overflow status register */
	CT_IEP.TMR_GLB_STS_bit.CNT_OVF = 0x1;

	/* Clear compare status */
	CT_IEP.TMR_CMP_STS_bit.CMP_HIT = 0xFF;

	/* Enable CMP0 */
	CT_IEP.TMR_CMP_CFG_bit.CMP_EN = 0x1;

	/* Enable reset on CMP0 event */
	CT_IEP.TMR_CMP_CFG_bit.CMP0_RST_CNT_EN = 0x1;

	/* Increment by one */
	CT_IEP.TMR_GLB_CFG_bit.DEFAULT_INC = 0x01;

}
