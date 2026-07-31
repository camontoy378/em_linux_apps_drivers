#include <linux/module.h>
#include <linux/i2c.h>

#define I2C_BUS_NUM     1
#define BME_280_ADDR    0x76

//I2C kernel setup
struct i2c_adapter * adapter;
struct i2c_client * client;

//Register address
#define REG_F4_ADDR     0xF4
#define REG_F5_ADDR     0xF5

//Register settings

//Config for 1 sample every 1 second
//Set bit 7:5 to b'101'
#define REG_F5_SET      (5 << 5)

//Config Power mode to Normal
//Set bit 1:0 to b'11'
//Config temperature data oversampling to highest resolution
//Set bit 7:5 to b'101
#define REG_F4_SET      ( (5 << 5) | (3 << 0) )

/******************************************************************************
*
******************************************************************************/
unsigned short convert_to_unsigned(unsigned short num)
{
    if( num > 32767)
    {
        return num - 65536;
    }
    else
    {
        return num;
    }
}

/******************************************************************************
*
******************************************************************************/
void delay(void)
{
    for(int i = 0; i < 5000000; i++);
}

/******************************************************************************
*
******************************************************************************/
int get_temperature(int adc_t, unsigned short dig_t1, unsigned short dig_t2, unsigned short dig_t3)
{
	unsigned int    var1, var2, t_fine;
	int             temp_celcius, temp_farenheit;

	var1 = ( ( ( ( adc_t >> 3 ) - ( ( unsigned int ) dig_t1 << 1 ) ) ) * ( (unsigned int) dig_t2)) >> 11;
	var2 = ( ( ( ( ( adc_t >> 4 ) - ( ( unsigned int ) dig_t1) ) * (( adc_t >> 4) - ( (unsigned int) dig_t1 ))) >> 12) * ((unsigned int) dig_t3)) >> 14;

	t_fine = var1 + var2;

	//Temperature is in Celcius and is off by 100.
	temp_celcius = ( t_fine * 5 + 128) >> 8;
	temp_farenheit 	= ((temp_celcius * 9)/5) + 32 * 100;


	return temp_farenheit;
}

/******************************************************************************
*
******************************************************************************/
unsigned short read_register_data(struct i2c_client *client, char *tx_buf, char *rx_buf, int data_len)
{
    if (i2c_master_send(client, tx_buf, 1) < 0)
    {
        return 0;
    }

    if (i2c_master_recv(client, rx_buf, data_len) < 0)
    {
        return 0;
    }

    return ( (rx_buf[1] << 8) | rx_buf[0] );


}

/******************************************************************************
*
******************************************************************************/
int my_init(void)
{
    printk("BME280 kernel initialize...\n");

    //Setup
    struct i2c_board_info info =
    {
        I2C_BOARD_INFO("my_bme280_device", BME_280_ADDR)
    };

    //Get adapter via know bus num
    adapter = i2c_get_adapter(I2C_BUS_NUM);
    if (!adapter)
    {
        printk("Can't find I2C bus %d\n", I2C_BUS_NUM);
        return -ENODEV;
    }

    //Bind device to adapter
    client = i2c_new_client_device(adapter, &info);

    //Release adapter
    i2c_put_adapter(adapter);

    char setup_reg_data[] = {REG_F5_ADDR,
                            REG_F5_SET,
                            REG_F4_ADDR,
                            REG_F4_SET
                            };

    
    printk("Configure registers...\n");
    
    int array_length        = sizeof(setup_reg_data);    
    int err_bytes_written   = i2c_master_send(client, setup_reg_data, array_length);
    

    printk("Get calibration data...\n");

    char recv_buf[2]        = {0,10};
    char reg_addr           = 0x88;
    int recv_buf_size       = sizeof(recv_buf) / sizeof(recv_buf[0]);

    unsigned short dig_t1   = read_register_data(client, &reg_addr, recv_buf, recv_buf_size);
    printk("Buffer contents: 0x%hx\n", dig_t1);
    

    reg_addr   = 0x8A;

    unsigned short dig_t2   = read_register_data(client, &reg_addr, recv_buf, recv_buf_size);
    dig_t2                  = convert_to_unsigned(dig_t2);
    printk("Buffer contents: 0x%hx\n", dig_t2);


    reg_addr   = 0x8C;

    unsigned short dig_t3   = read_register_data(client, &reg_addr, recv_buf, recv_buf_size);
    dig_t3                  = convert_to_unsigned(dig_t3);
    printk("Buffer contents: 0x%hx\n", dig_t3);


    //Show temperature
    int raw_reg_temp;
    int temperature;

    int count = 10;

    while (count > 0)
    {
        printk("Read temperature from register...\n");
        
        reg_addr            = 0xFA;
        char d1             = read_register_data(client, &reg_addr, recv_buf, recv_buf_size);
        
        reg_addr            = 0xFB;
        char d2             = read_register_data(client, &reg_addr, recv_buf, recv_buf_size);
        
        reg_addr            = 0xFC;
        char d3             = read_register_data(client, &reg_addr, recv_buf, recv_buf_size);

        //d3[0:3] is not valid so adjustment is made
        raw_reg_temp        = ( (d1 << 16) | (d2 << 8) | d3 ) >> 4;

        temperature         = get_temperature(raw_reg_temp, dig_t1, dig_t2, dig_t3);

        printk("Temperature (F) = %d.%d\n", temperature / 100, temperature % 100);
        
        count--;
        delay();

    }
    
    return 0;
}

/******************************************************************************
*
******************************************************************************/
void my_exit(void)
{
    printk("BME280 kernel exit...\n");
    i2c_unregister_device(client);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
