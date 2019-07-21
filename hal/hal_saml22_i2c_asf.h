/*
 * hal_saml22_i2c_asf.h
 *
 * Created: 7/20/2019 3:53:12 PM
 *  Author: pvallone
 */ 


#ifndef HAL_SAML22_I2C_ASF_H_
#define HAL_SAML22_I2C_ASF_H_

#include <asf.h>

/** \defgroup hal_ Hardware abstraction layer (hal_)
 *
 * \brief
 * These methods define the hardware abstraction layer for communicating with a CryptoAuth device
 * using I2C driver of ASF.
 *
   @{ */


#define MAX_I2C_BUSES    6   // SAML22 has up to 6 SERCOMS that can be configured as I2C

/** \brief this is the hal_data for ATCA HAL for ASF SERCOM
 */
typedef struct atcaI2Cmaster
{
    Sercom*                  i2c_sercom;
    struct i2c_master_module i2c_master_instance;
    int                      ref_ct;
    // for conveniences during interface release phase
    int bus_index;
} ATCAI2CMaster_t;

void change_i2c_speed(ATCAIface iface, uint32_t speed);




#endif /* HAL_SAML22_I2C_ASF_H_ */