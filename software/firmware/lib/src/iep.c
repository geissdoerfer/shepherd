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

void iep_set_us(const uint32_t value)
{
	iep_stop();
	iep_reset();
	/* Set compare value */
	CT_IEP.TMR_CMP0 = value * 200U;
}

void iep_reset()
{
	/* Reset Count register */
	CT_IEP.TMR_CNT = 0xFFFFFFFF;
}

inline void iep_start()
{
	/* Enable counter */
	CT_IEP.TMR_GLB_CFG |= 1U;
}

inline void iep_stop()
{
	/* Disable counter */
	CT_IEP.TMR_GLB_CFG &= ~1U;
}


inline bool_ft iep_check_evt_cmp(const uint8_ft compare_channel)
{
	return (bool_ft)(CT_IEP.TMR_CMP_STS & (1U << compare_channel));
}

// allow to build external, faster iep_check_evt_cmp, for when this fn is called more often in a loop
// -> READs from IEP take 12 Cycles
inline uint32_t iep_get_tmr_cmp_sts()
{
	return CT_IEP.TMR_CMP_STS;
}

inline uint32_t iep_check_evt_cmp_fast(const uint32_t tmr_cmp_sts, const uint32_t compare_channel_mask)
{
	return (tmr_cmp_sts & compare_channel_mask);
}

inline void iep_clear_evt_cmp(const uint8_ft compare_channel)
{
	CT_IEP.TMR_CMP_STS |= (1U << compare_channel);
}

inline void iep_enable_evt_cmp(const uint8_ft compare_channel)
{
	CT_IEP.TMR_CMP_CFG_bit.CMP_EN |= (1U << compare_channel);
}

inline bool_ft iep_enable_status_evt_cmp(const uint8_ft compare_channel)
{
	return (CT_IEP.TMR_CMP_CFG_bit.CMP_EN & (1U << compare_channel));
}

inline void iep_disable_evt_cmp(const uint8_ft compare_channel)
{
	CT_IEP.TMR_CMP_CFG_bit.CMP_EN &= ~(1U << compare_channel);
}

inline void iep_set_cmp_val(const uint8_ft compare_channel, const uint32_t value)
{
	/* Hack to address the CMPN registers (offset=0x48 from CT_IEP) */
	*((uint32_t *) &CT_IEP + 18U + compare_channel) = value;
}

inline uint32_t iep_get_cmp_val(const uint8_ft compare_channel)
{
	/* Hack to address the CMPN registers (offset=0x48 from CT_IEP) */
	return *((uint32_t *) &CT_IEP + 18U + compare_channel);
}

inline uint32_t iep_get_cnt_val()
{
	return CT_IEP.TMR_CNT;
}

void iep_init()
{
	/* Disable counter */
	CT_IEP.TMR_GLB_CFG_bit.CNT_EN = 0;

	/* Reset Count register */
	CT_IEP.TMR_CNT = 0xFFFFFFFF;

	/* Clear overflow status register */
	CT_IEP.TMR_GLB_STS_bit.CNT_OVF = 0x1;

	/* Clear compare status */
	CT_IEP.TMR_CMP_STS_bit.CMP_HIT = 0xFF;

	/* Enable CMP0 */
	CT_IEP.TMR_CMP_CFG_bit.CMP_EN = 0x1;

	/* Enable counter-reset on CMP0 event */
	CT_IEP.TMR_CMP_CFG_bit.CMP0_RST_CNT_EN = 0x1;

	/* Increment by one */
	CT_IEP.TMR_GLB_CFG_bit.DEFAULT_INC = 0x01;
}
