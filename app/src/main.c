#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/net/socket.h> 
#include <zephyr/net/ethernet.h>
#include <arpa/inet.h> /* Necessario para htons */
#include <errno.h>     /* Necessario para capturar o erro exato */

#define I2C_NODE DT_NODELABEL(sercom1)
#define BNO055_ADDR 0x29

#define REG_OPR_MODE 0x3D
#define REG_ACCEL_DATA_X_LSB 0x08
#define MODE_ACCONLY 0x01

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
    uint8_t chip_id = 0;
    uint8_t accel_bytes[6];

    if (!device_is_ready(i2c_dev)) {
        printk("Erro: I2C nao pronto.\n");
        return 0;
    }

    /* 1. Lê o Chip ID */
    i2c_reg_read_byte(i2c_dev, BNO055_ADDR, 0x00, &chip_id);
    printk("==> CHIP ID LIDO DO SENSOR: 0x%X\n", chip_id);

    i2c_reg_write_byte(i2c_dev, BNO055_ADDR, REG_OPR_MODE, MODE_ACCONLY);
    k_msleep(50);

    /* 2. Criação do socket corrigida para 802.15.4 */
    int sock = zsock_socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IEEE802154));
    if (sock < 0) {
        printk("==> Erro ao criar socket do radio! errno: %d\n", errno);
    } else {
        printk("==> Radio pronto!\n");
    }

    while (1) {
        int ret = i2c_burst_read(i2c_dev, BNO055_ADDR, REG_ACCEL_DATA_X_LSB, accel_bytes, 6);

        if (ret == 0) {
            tx_packet.accel_x = (int16_t)((accel_bytes[1] << 8) | accel_bytes[0]);
            tx_packet.accel_y = (int16_t)((accel_bytes[3] << 8) | accel_bytes[2]);
            tx_packet.accel_z = (int16_t)((accel_bytes[5] << 8) | accel_bytes[4]);
            tx_packet.uptime_ms = k_uptime_get_32();
            tx_packet.seq++;

            printk("Montado [%d] -> X:%d Y:%d Z:%d\n", tx_packet.seq, tx_packet.accel_x, tx_packet.accel_y, tx_packet.accel_z);

            if (sock >= 0) {
                int sent = zsock_send(sock, &tx_packet, sizeof(tx_packet), 0);
                if (sent >= 0) {
                    printk("--> %d bytes enviados pelo radio!\n", sent);
                }
            }
        }
        k_msleep(1000);
    }
    return 0;
}