#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

/* Pega a referencia do barramento sercom1 onde ligamos o sensor */
#define I2C_NODE DT_NODELABEL(sercom1)

int main(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    uint8_t chip_id = 0;

    if (!device_is_ready(i2c_dev)) {
        printk("Erro: Barramento I2C (sercom1) nao esta pronto.\n");
        return 0;
    }

    printk("I2C pronto! A procurar o BNO055 no endereco 0x28...\n");

    /* O endereco I2C do BNO055 e 0x28. O registrador de CHIP_ID e 0x00 */
    int ret = i2c_reg_read_byte(i2c_dev, 0x28, 0x00, &chip_id);

    if (ret != 0) {
        printk("Falha na comunicacao I2C (Erro: %d).\n", ret);
    } else {
        printk("Sucesso! BNO055 encontrado. CHIP ID: 0x%X (Esperado: 0xA0)\n", chip_id);
    }

    while (1) {
        /* Aqui adicionaremos a leitura dos eixos X, Y e Z no futuro */
        k_msleep(1000);
    }
    
    return 0;
}