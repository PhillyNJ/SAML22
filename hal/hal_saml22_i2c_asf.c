/*
 * hal_saml22_i2c_asf.c
 *
 * Created: 7/20/2019 3:53:21 PM
 *  Author: pvallone
 */ 
#include <asf.h>
#include <string.h>
#include <stdio.h>

#include "atca_hal.h"
#include "hal_saml22_i2c_asf.h"
#include "atca_device.h"
#include "atca_execution.h"
#include "atca_status.h"
/** \defgroup hal_ Hardware abstraction layer (hal_)
 *
 * \brief
 * These methods define the hardware abstraction layer for communicating with a CryptoAuth device
 * using I2C driver of ASF.
 *
   @{ */

/** \brief logical to physical bus mapping structure */
static ATCAI2CMaster_t i2c_hal_data[MAX_I2C_BUSES];   // map logical, 0-based bus number to index
static struct i2c_master_config config_i2c_master;

/** \brief discover i2c buses available for this hardware
 * this maintains a list of logical to physical bus mappings freeing the application
 * of the a-priori knowledge
 * \param[in] i2c_buses - an array of logical bus numbers
 * \param[in] max_buses - maximum number of buses the app wants to attempt to discover
 * \return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_discover_buses(int i2c_buses[], int max_buses)
{

    /* if every SERCOM was a likely candidate bus, then would need to initialize the entire array to all SERCOM n numbers.
     * As an optimization and making discovery safer, make assumptions about bus-num / SERCOM map based on D21 Xplained Pro board
     * If you were using a raw D21 on your own board, you would supply your own bus numbers based on your particular hardware configuration.
     */
#ifdef __SAMR21G18A__
    i2c_buses[0] = 1;   // default r21 for xplained pro dev board
#else
    i2c_buses[0] = 2;   // default d21 for xplained pro dev board
#endif

    return ATCA_SUCCESS;
}

/** \brief discover any CryptoAuth devices on a given logical bus number
 * \param[in]  bus_num  logical bus number on which to look for CryptoAuth devices
 * \param[out] cfg     pointer to head of an array of interface config structures which get filled in by this method
 * \param[out] found   number of devices found on this bus
 * \return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_discover_devices(int bus_num, ATCAIfaceCfg cfg[], int *found)
{
    ATCAIfaceCfg *head = cfg;
    uint8_t slaveAddress = 0x01;
    ATCADevice device;

#ifdef ATCA_NO_HEAP
    struct atca_device disc_device;
    struct atca_command disc_command;
    struct atca_iface disc_iface;
#endif
    ATCAPacket packet;
    ATCA_STATUS status;
    uint8_t revs608[][4] = { { 0x00, 0x00, 0x60, 0x01 }, { 0x00, 0x00, 0x60, 0x02 } };
    uint8_t revs508[][4] = { { 0x00, 0x00, 0x50, 0x00 } };
    uint8_t revs108[][4] = { { 0x80, 0x00, 0x10, 0x01 } };
    uint8_t revs204[][4] = { { 0x00, 0x02, 0x00, 0x08 }, { 0x00, 0x02, 0x00, 0x09 }, { 0x00, 0x04, 0x05, 0x00 } };
    int i;

    /** \brief default configuration, to be reused during discovery process */
    ATCAIfaceCfg discoverCfg = {
        .iface_type             = ATCA_I2C_IFACE,
        .devtype                = ATECC508A,
        .atcai2c.slave_address  = 0x07,
        .atcai2c.bus            = bus_num,
        .atcai2c.baud           = 400000,
        //.atcai2c.baud = 100000,
        .wake_delay             = 800,
        .rx_retries             = 3
    };

    if (bus_num < 0)
    {
        return ATCA_COMM_FAIL;
    }

#ifdef ATCA_NO_HEAP
    disc_device.mCommands = &disc_command;
    disc_device.mIface    = &disc_iface;
    status = initATCADevice(&discoverCfg, &disc_device);
    if (status != ATCA_SUCCESS)
    {
        return status;
    }
    device = &disc_device;
#else
    device = newATCADevice(&discoverCfg);
    if (device == NULL)
    {
        return ATCA_COMM_FAIL;
    }
#endif

    // iterate through all addresses on given i2c bus
    // all valid 7-bit addresses go from 0x07 to 0x78
    for (slaveAddress = 0x07; slaveAddress <= 0x78; slaveAddress++)
    {
        discoverCfg.atcai2c.slave_address = slaveAddress << 1;  // turn it into an 8-bit address which is what the rest of the i2c HAL is expecting when a packet is sent

        memset(&packet, 0x00, sizeof(packet));
        // build an info command
        packet.param1 = INFO_MODE_REVISION;
        packet.param2 = 0;
        // get devrev info and set device type accordingly
        atInfo(device->mCommands, &packet);
        if ((status = atca_execute_command(&packet, device)) != ATCA_SUCCESS)
        {
            continue;
        }

        // determine device type from common info and dev rev response byte strings... start with unknown
        discoverCfg.devtype = ATCA_DEV_UNKNOWN;
        for (i = 0; i < (int)sizeof(revs608) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs608[i], 4) == 0)
            {
                discoverCfg.devtype = ATECC608A;
                break;
            }
        }

        for (i = 0; i < (int)sizeof(revs508) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs508[i], 4) == 0)
            {
                discoverCfg.devtype = ATECC508A;
                break;
            }
        }

        for (i = 0; i < (int)sizeof(revs204) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs204[i], 4) == 0)
            {
                discoverCfg.devtype = ATSHA204A;
                break;
            }
        }

        for (i = 0; i < (int)sizeof(revs108) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs108[i], 4) == 0)
            {
                discoverCfg.devtype = ATECC108A;
                break;
            }
        }

        if (discoverCfg.devtype != ATCA_DEV_UNKNOWN)
        {
            // now the device type is known, so update the caller's cfg array element with it
            (*found)++;
            memcpy( (uint8_t*)head, (uint8_t*)&discoverCfg, sizeof(ATCAIfaceCfg));
            head->devtype = discoverCfg.devtype;
            head++;
        }

        atca_delay_ms(15);
    }

#ifdef ATCA_NO_HEAP
    releaseATCADevice(device);
#else
    deleteATCADevice(&device);

#endif


    return ATCA_SUCCESS;
}

/** \brief
    - this HAL implementation assumes you've included the ASF SERCOM I2C libraries in your project, otherwise,
    the HAL layer will not compile because the ASF I2C drivers are a dependency *
 */

/** \brief hal_i2c_init manages requests to initialize a physical interface.  it manages use counts so when an interface
 * has released the physical layer, it will disable the interface for some other use.
 * You can have multiple ATCAIFace instances using the same bus, and you can have multiple ATCAIFace instances on
 * multiple i2c buses, so hal_i2c_init manages these things and ATCAIFace is abstracted from the physical details.
 */

/** \brief initialize an I2C interface using given config
 * \param[in] hal - opaque ptr to HAL data
 * \param[in] cfg - interface configuration
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS hal_i2c_init(void *hal, ATCAIfaceCfg *cfg)
{
    if (cfg->atcai2c.bus >= MAX_I2C_BUSES)
    {
        return ATCA_COMM_FAIL;
    }
    ATCAI2CMaster_t* data = &i2c_hal_data[cfg->atcai2c.bus];

    if (data->ref_ct <= 0)
    {
        i2c_master_get_config_defaults(&config_i2c_master);
        config_i2c_master.buffer_timeout = 10000;

        switch (cfg->atcai2c.bus)
        {
        case 0:
            data->i2c_sercom = SERCOM0;
            break;
        case 1:
            data->i2c_sercom = SERCOM1;
            break;
        case 2:
            data->i2c_sercom = SERCOM2;
            break;
        case 3:
            data->i2c_sercom = SERCOM3;
            break;
        case 4:
            data->i2c_sercom = SERCOM4;
            break;
        case 5:
            data->i2c_sercom = SERCOM5;
            break;
        default:
            return ATCA_COMM_FAIL;
        }

        i2c_master_init(&data->i2c_master_instance, data->i2c_sercom, &config_i2c_master);
        i2c_master_enable(&data->i2c_master_instance);

        // store this for use during the release phase
        data->bus_index = cfg->atcai2c.bus;
        // buses are shared, this is the first instance
        data->ref_ct = 1;
    }
    else
    {
        // Bus is already is use, increment reference counter
        data->ref_ct++;
    }

    ((ATCAHAL_t*)hal)->hal_data = data;

    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C post init
 * \param[in] iface  instance
 * \return ATCA_SUCCESS
 */
ATCA_STATUS hal_i2c_post_init(ATCAIface iface)
{
    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C send over ASF
 * \param[in] iface     instance
 * \param[in] txdata    pointer to space to bytes to send
 * \param[in] txlength  number of bytes to send
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_send(ATCAIface iface, uint8_t *txdata, int txlength)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);

    struct i2c_master_packet packet = {
        .address            = cfg->atcai2c.slave_address >> 1,
        .data_length        = txlength,
        .data               = txdata,
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    // for this implementation of I2C with CryptoAuth chips, txdata is assumed to have ATCAPacket format
    // other device types that don't require i/o tokens on the front end of a command need a different hal_i2c_send and wire it up instead of this one
    // this covers devices such as ATSHA204A and ATECCx08A that require a word address value pre-pended to the packet
    // txdata[0] is using _reserved byte of the ATCAPacket
    txdata[0] = 0x03;   // insert the Word Address Value, Command token
    txlength++;         // account for word address value byte.
    packet.data_length = txlength;

    if (i2c_master_write_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet) != STATUS_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C receive function for ASF I2C
 * \param[in]    iface     Device to interact with.
 * \param[out]   rxdata    Data received will be returned here.
 * \param[inout] rxlength  As input, the size of the rxdata buffer.
 *                         As output, the number of bytes received.
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS hal_i2c_receive(ATCAIface iface, uint8_t *rxdata, uint16_t *rxlength)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int retries = cfg->rx_retries;
    int status = !ATCA_SUCCESS;
    uint16_t rxdata_max_size = *rxlength;

    struct i2c_master_packet packet = {
        .address            = cfg->atcai2c.slave_address >> 1,
        .data_length        = 1,
        .data               = rxdata,
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    *rxlength = 0;
    if (rxdata_max_size < 1)
    {
        return ATCA_SMALL_BUFFER;
    }

    while (retries-- > 0 && status != ATCA_SUCCESS)
    {
        if (i2c_master_read_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet) != STATUS_OK)
        {
            status = ATCA_COMM_FAIL;
        }
        else
        {
            status = ATCA_SUCCESS;
        }
    }
    if (status != ATCA_SUCCESS)
    {
        return status;
    }
    if (rxdata[0] < ATCA_RSP_SIZE_MIN)
    {
        return ATCA_INVALID_SIZE;
    }
    if (rxdata[0] > rxdata_max_size)
    {
        return ATCA_SMALL_BUFFER;
    }

    //Update receive length with first byte received and set to read rest of the data
    packet.data_length = rxdata[0] - 1;
    packet.data = &rxdata[1];

    if (i2c_master_read_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet) != STATUS_OK)
    {
        status = ATCA_COMM_FAIL;
    }
    else
    {
        status = ATCA_SUCCESS;
    }
    if (status != ATCA_SUCCESS)
    {
        return status;
    }

    *rxlength = rxdata[0];

    return ATCA_SUCCESS;
}

/** \brief method to change the bus speec of I2C
 * \param[in] iface  interface on which to change bus speed
 * \param[in] speed  baud rate (typically 100000 or 400000)
 */

void change_i2c_speed(ATCAIface iface, uint32_t speed)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    ATCAI2CMaster_t* data = &i2c_hal_data[cfg->atcai2c.bus];

    i2c_master_disable(&data->i2c_master_instance);
    config_i2c_master.buffer_timeout = 10000;   
    i2c_master_init(&data->i2c_master_instance, data->i2c_sercom, &config_i2c_master);
    i2c_master_enable(&data->i2c_master_instance);
}

/** \brief wake up CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to wakeup
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_wake(ATCAIface iface)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int retries = cfg->rx_retries;
    uint32_t bdrt = cfg->atcai2c.baud;
    int status = !STATUS_OK;
    uint8_t data[4];

    if (bdrt != 100000)    // if not already at 100KHz, change it
    {
        change_i2c_speed(iface, 100000);
    }

    // Send the wake by writing to an address of 0x00
    struct i2c_master_packet packet = {
        .address            = 0x00,
        .data_length        = 0,
        .data               = &data[0],
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    // Send the 00 address as the wake pulse
    // part will NACK, so don't check for status
    i2c_master_write_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet);

    // wait tWHI + tWLO which is configured based on device type and configuration structure
    atca_delay_us(cfg->wake_delay);

    packet.address = cfg->atcai2c.slave_address >> 1;
    packet.data_length = 4;
    packet.data = data;

    while (retries-- > 0 && status != STATUS_OK)
    {
        status = i2c_master_read_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet);
    }

    // if necessary, revert baud rate to what came in.
    if (bdrt != 100000)
    {
        change_i2c_speed(iface, bdrt);
    }

    if (status != STATUS_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return hal_check_wake(data, 4);
}

/** \brief idle CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to idle
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_idle(ATCAIface iface)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    uint8_t data[4];

    struct i2c_master_packet packet = {
        .address            = cfg->atcai2c.slave_address >> 1,
        .data_length        = 1,
        .data               = &data[0],
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    data[0] = 0x02;  // idle word address value
    if (i2c_master_write_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet) != STATUS_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;

}

/** \brief sleep CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to sleep
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_sleep(ATCAIface iface)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    uint8_t data[4];

    struct i2c_master_packet packet = {
        .address            = cfg->atcai2c.slave_address >> 1,
        .data_length        = 1,
        .data               = &data[0],
        .ten_bit_address    = false,
        .high_speed         = false,
        .hs_master_code     = 0x0,
    };

    data[0] = 0x01;  // sleep word address value
    if (i2c_master_write_packet_wait(&i2c_hal_data[cfg->atcai2c.bus].i2c_master_instance, &packet) != STATUS_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

/** \brief manages reference count on given bus and releases resource if no more refences exist
 * \param[in] hal_data - opaque pointer to hal data structure - known only to the HAL implementation
 * return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_release(void *hal_data)
{
    ATCAI2CMaster_t *hal = (ATCAI2CMaster_t*)hal_data;

    // if the use count for this bus has gone to 0 references, disable it.  protect against an unbracketed release
    if (hal && --(hal->ref_ct) <= 0)
    {
        i2c_master_reset(&(hal->i2c_master_instance));
        hal->ref_ct = 0;
    }

    return ATCA_SUCCESS;
}
