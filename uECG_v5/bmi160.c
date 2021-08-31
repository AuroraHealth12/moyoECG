#include "bmi160.h"
#include "leds.h"
#include "urf_timer.h"
#include "spim_functions.h"

#define BMI_ERR_REG			0x02
#define BMI_PMU_STATUS		0x03
#define BMI_DATA_MAG		0x04
#define BMI_DATA_GYRO		0x0C
#define BMI_DATA_ACC		0x12
#define BMI_SENSORTIME		0x18
#define BMI_STATUS			0x1B
#define BMI_INT_STATUS		0x1C
#define BMI_TEMPERATURE		0x20
#define BMI_FIFO_LENGTH		0x22
#define BMI_FIFO_DATA		0x24
#define BMI_ACC_CONF		0x40
#define BMI_ACC_RANGE		0x41
#define BMI_GYR_CONF		0x42
#define BMI_GYR_RANGE		0x43
#define BMI_MAG_CONF		0x44
#define BMI_FIFO_DOWNS		0x45
#define BMI_FIFO_CONF		0x46
#define BMI_INT_EN			0x50
#define BMI_INT_OUT_CTRL	0x53
#define BMI_INT_LATCH		0x54
#define BMI_INT_MAP			0x55
#define BMI_INT_DATA		0x58

#define BMI_INT_TAP			0x63
//...
#define BMI_STEP_CNT		0x78
#define BMI_STEP_CONF		0x7A
#define BMI_CMD				0x7E

sBMI160 bmi;

void bmi_write_reg8(uint8_t reg, uint8_t value)
{
	spi_write_reg8(reg, value, bmi.CS);
}

uint8_t bmi_read_reg8(uint8_t reg)
{
	return spi_read_reg8(reg | (1<<7), bmi.CS);
}

uint16_t bmi_read_reg16(uint8_t reg)
{
	return spi_read_reg16(reg | (1<<7), bmi.CS);
}

//uint8_t raw_data[12];
uint8_t stat;

uint8_t bmi160_get_status()
{
	return bmi_read_reg8(0x00);
}

uint8_t bmi_spi_buf[32];
void bmi160_read_cplt()
{
	int16_t v;
	uint8_t *pbuf = bmi_spi_buf+1;
	v = *pbuf++; v += (*pbuf++) << 8;
	bmi.rwX = v;
	bmi.wX = bmi.rawG2SI * (float)v;
	v = *pbuf++; v += (*pbuf++) << 8;
	bmi.rwY = v;
	bmi.wY = bmi.rawG2SI * (float)v;
	v = *pbuf++; v += (*pbuf++) << 8;
	bmi.rwZ = v;
	bmi.wZ = bmi.rawG2SI * (float)v;
	v = *pbuf++; v += (*pbuf++) << 8;
	bmi.raX = v;
	bmi.aX = bmi.rawA2SI * (float)v;
	v = *pbuf++; v += (*pbuf++) << 8;
	bmi.raY = v;
	bmi.aY = bmi.rawA2SI * (float)v;
	v = *pbuf++; v += (*pbuf++) << 8;
	bmi.raZ = v;
	bmi.aZ = bmi.rawA2SI * (float)v;

	bmi.data_id++;
}

uint8_t bmi160_read()
{
//	NVIC_DisableIRQ(GPIOTE_IRQn);
	uint8_t status = bmi_read_reg8(BMI_STATUS);
//	NVIC_EnableIRQ(GPIOTE_IRQn);
	stat = status;
	if((status & 0b11000000) == 0) return 0;
	spi_read_buf(BMI_DATA_GYRO | (1<<7), 12, bmi160_read_cplt, bmi_spi_buf, bmi.CS);
	return 1;
}

void bmi160_read_steps()
{
	bmi.step_cnt = bmi_read_reg16(BMI_STEP_CNT);
}

float bmi160_read_temp()
{
	int16_t temp_val;
	temp_val = bmi_read_reg16(BMI_TEMPERATURE);
	bmi.T = 23 + (float)temp_val / 512.0;
	return bmi.T;
}

void bmi160_stop()
{
//	lsm_write_reg8(LSM_CTRL1_XL, 0b0); //disabled
//	lsm_write_reg8(LSM_CTRL2_G, 0b0); //disabled

	bmi_write_reg8(BMI_CMD, 0b00010000); //set acc power mode - suspend
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010100); //set gyro power mode - suspend
	delay_ms(100);
}

void bmi160_resume()
{
	NRF_SPI0->ENABLE = 1;
	delay_ms(1);
	bmi_write_reg8(BMI_CMD, 0b00010001); //set acc power mode - normal 01
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010101); //set gyro power mode - normal 01
	delay_ms(100);
}

void bmi160_normal_mode()
{
	bmi_write_reg8(BMI_CMD, 0xB6); //reset
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010001); //set acc power mode - normal 01
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010101); //set gyro power mode - normal 01
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010000); //set acc power mode - off 00
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010100); //set gyro power mode - off 00
	delay_ms(100);
	

	bmi_write_reg8(BMI_FIFO_DOWNS, 0);
	bmi_write_reg8(BMI_FIFO_CONF+1, 0); //turn off

	bmi_write_reg8(BMI_ACC_CONF, 0b10100111); //010 bwp, 50 Hz, undersampling enabled
	delay_ms(1);
	bmi_write_reg8(BMI_ACC_RANGE, 0b00000101); //+-4g
	delay_ms(1);
	bmi_write_reg8(BMI_GYR_CONF, 0b00100111); //10 bwp, 50 Hz
	delay_ms(1);
	bmi_write_reg8(BMI_GYR_RANGE, 0b00000010); //+-500 dps
	delay_ms(1);
	bmi_write_reg8(BMI_STEP_CONF, 0b00010101); //steps
	delay_ms(1);
	bmi_write_reg8(BMI_STEP_CONF+1, 0b00001011); //steps
	delay_ms(1);

	bmi_write_reg8(BMI_INT_EN+1, 1<<4); //drdy interrupt
	bmi_write_reg8(BMI_INT_OUT_CTRL, 0b00001010); //int 1 enabled, active high
	bmi_write_reg8(BMI_INT_MAP+1, 1<<7); //drdy on int1
//	bmi_write_reg8(BMI_INT_LATCH, 0b1000); //40 ms latch
	bmi_write_reg8(BMI_INT_LATCH, 0); //no latch

	bmi_write_reg8(BMI_CMD, 0b00010001); //set acc power mode - normal 01
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010101); //set gyro power mode - normal 01
	delay_ms(100);
}


void bmi160_lp_mode()
{
	bmi_write_reg8(BMI_CMD, 0b00010000); //set acc power mode - suspend
	delay_ms(10);
	bmi_write_reg8(BMI_CMD, 0b00010100); //set gyro power mode - suspend
	delay_ms(10);

	bmi_write_reg8(BMI_CMD, 0b00010010); //set acc power mode - low
	delay_ms(10);

	bmi_write_reg8(BMI_FIFO_DOWNS , 0b01010000); //pre-filtered acc data at 50hz
	
	bmi_write_reg8(BMI_FIFO_CONF+1, 0b01000000); //only acc data

}

void bmi160_acc_mode()
{
	bmi_write_reg8(BMI_CMD, 0xB6); //reset
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010001); //set acc power mode - normal 01
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010100); //set gyro power mode - off 00
	delay_ms(100);
	bmi_write_reg8(BMI_CMD, 0b00010001); //set acc power mode - normal 01
	delay_ms(100);

	bmi_write_reg8(BMI_FIFO_DOWNS, 0);
	bmi_write_reg8(BMI_FIFO_CONF+1, 0); //turn off

	bmi_write_reg8(BMI_ACC_CONF, 0b10100111); //010 bwp, 50 Hz, undersampling enabled
	delay_ms(1);
	bmi_write_reg8(BMI_ACC_RANGE, 0b00000101); //+-4g
	bmi.rawA2SI = 9.8 * 4.0 / 32768.0;// 4g mode
	delay_ms(1);
	bmi_write_reg8(BMI_GYR_CONF, 0b00100111); //10 bwp, 50 Hz
	delay_ms(1);
	bmi_write_reg8(BMI_GYR_RANGE, 0b00000010); //+-500 dps
	delay_ms(1);
	bmi_write_reg8(BMI_INT_EN+1, 1<<4); //drdy interrupt
	bmi_write_reg8(BMI_INT_OUT_CTRL, 0b00001010); //int 1 enabled, active high
	bmi_write_reg8(BMI_INT_MAP+1, 1<<7); //drdy on int1
//	bmi_write_reg8(BMI_INT_LATCH, 0b1000); //40 ms latch
	bmi_write_reg8(BMI_INT_LATCH, 0); //no latch

	bmi_write_reg8(BMI_STEP_CONF, 0b00010101); //steps
	bmi_write_reg8(BMI_STEP_CONF+1, 0b00001011); //steps
	
}

//expects that SPI hardware is already initialized
void bmi160_init(uint8_t pin_CS, uint8_t pin_INT)
{
	NRF_GPIO->DIRSET = 1<<pin_CS;
	bmi.CS = 1<<pin_CS;
	NRF_GPIO->OUTSET = bmi.CS;
	bmi.data_id = 0;

	delay_ms(100);
	
	bmi.rawA2SI = 9.8 * 4.0 / 32768.0;// 4g mode
	bmi.rawG2SI = 0.000266048;// 500 dps mode

	bmi160_normal_mode();
//	bmi160_acc_mode();
}

int bmi160_is_ok()
{
	uint8_t who_am_i = bmi_read_reg8(0);
	if(who_am_i == 0b11010001)
		return 1;
	return 0;
}
