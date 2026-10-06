#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/ethernet.h>
#include <arpa/inet.h>
#include <errno.h>

#define I2C_NODE DT_NODELABEL(sercom1)
#define BNO055_ADDR 0x29
#define TEMP_SENSOR_ADDR 0x4f
#define TEMP_REGISTER 0x00
#define ADC_NODE DT_ALIAS(light_adc)

#define REG_OPR_MODE 0x3D
#define REG_ACCEL_DATA_X_LSB 0x08
#define MODE_ACCONLY 0x01
#define ADC_CHANNEL_ID 6U

struct __attribute__((packed)) sensor_packet {
    uint8_t  node_id;
    uint16_t seq;
    uint16_t light;
    uint16_t temp;
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    uint32_t uptime_ms;
    uint8_t  flags;
};

struct sensor_packet tx_packet = {
    .node_id = 1,
    .seq = 0
};

int main(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
    struct adc_channel_cfg channel_cfg = {
        .channel_id = ADC_CHANNEL_ID,
        .gain = ADC_GAIN_1,
        .reference = ADC_REF_VDD_1,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    };
    uint16_t light_raw = 0;
    struct adc_sequence sequence = {
        .buffer = &light_raw,
        .buffer_size = sizeof(light_raw),
        .resolution = 12,
        .oversampling = 0,
        .channels = BIT(ADC_CHANNEL_ID),
    };
    uint8_t chip_id = 0;
    uint8_t accel_bytes[6];
    uint8_t temp_bytes[2];
    int16_t temp_raw;
    int32_t light_mv;
    int ret;

    if (!device_is_ready(i2c_dev)) {
        printk("Erro: I2C nao pronto.\n");
        return 0;
    }

    if (!device_is_ready(adc_dev)) {
        printk("Erro: ADC nao pronto.\n");
        return 0;
    }

    ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret < 0) {
        printk("Erro ao configurar canal ADC (%d).\n", ret);
        return 0;
    }

    i2c_reg_read_byte(i2c_dev, BNO055_ADDR, 0x00, &chip_id);
    printk("==> CHIP ID LIDO DO SENSOR: 0x%X\n", chip_id);

    i2c_reg_write_byte(i2c_dev, BNO055_ADDR, REG_OPR_MODE, MODE_ACCONLY);
    k_msleep(50);

    int sock = zsock_socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IEEE802154));
    if (sock < 0) {
        printk("==> Erro ao criar socket do radio! errno: %d\n", errno);
    } else {
        printk("==> Radio pronto!\n");
    }

    while (1) {
        ret = i2c_burst_read(i2c_dev, BNO055_ADDR, REG_ACCEL_DATA_X_LSB, accel_bytes, 6);
        if (ret == 0) {
            tx_packet.accel_x = (int16_t)((accel_bytes[1] << 8) | accel_bytes[0]);
            tx_packet.accel_y = (int16_t)((accel_bytes[3] << 8) | accel_bytes[2]);
            tx_packet.accel_z = (int16_t)((accel_bytes[5] << 8) | accel_bytes[4]);
        }

        ret = i2c_burst_read(i2c_dev, TEMP_SENSOR_ADDR, TEMP_REGISTER,
                             temp_bytes, sizeof(temp_bytes));
        if (ret == 0) {
            temp_raw = (int16_t)((temp_bytes[0] << 8) | temp_bytes[1]);
            tx_packet.temp = (uint16_t)((temp_raw * 625) / 1000);
        }

        ret = adc_read(adc_dev, &sequence);
        if (ret == 0) {
            light_mv = (int32_t)light_raw;
            ret = adc_raw_to_millivolts(3300, ADC_GAIN_1, 12, &light_mv);
            if (ret == 0) {
                tx_packet.light = (uint16_t)light_mv;
            }
        }

        tx_packet.uptime_ms = k_uptime_get_32();
        tx_packet.seq++;

        printk("Montado [%d] -> Luz:%u Temp:%u.%uC X:%d Y:%d Z:%d\n",
               tx_packet.seq, tx_packet.light, tx_packet.temp / 10,
               tx_packet.temp % 10, tx_packet.accel_x, tx_packet.accel_y, tx_packet.accel_z);

        if (sock >= 0) {
            int sent = zsock_send(sock, &tx_packet, sizeof(tx_packet), 0);
            if (sent >= 0) {
                printk("--> %d bytes enviados pelo radio!\n", sent);
            }
        }
        k_msleep(1000);
    }
    return 0;
}
