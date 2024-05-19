#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/serdev.h>
#include <linux/slab.h>
#include <uapi/linux/sched/types.h>

#define MAX_DATA_LEN 64

#define gamepad_serdev_write_seq(serdev, seq...) \
    do {                                                               \
		static const u8 d[] = { seq };                        \
		struct device *dev = &serdev->dev;                            \
		int ret;                                                   \
		ret = serdev_device_write_buf(serdev, d, ARRAY_SIZE(d));    \
		if (ret < 0 || ret < ARRAY_SIZE(d)) {                                             \
			dev_err(dev, "Unable to write data\n");                                 \
			return;                                        \
		}                                                          \
	} while (0)

static const unsigned int keymap[] = {
	BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
    BTN_NORTH, BTN_WEST, BTN_EAST, BTN_SOUTH,
    BTN_TL, BTN_TR, BTN_SELECT, BTN_START,
    BTN_THUMBL, BTN_THUMBR, BTN_MODE, BTN_BACK
};

struct gamepad_data {
    u8 header[4];
    u8 frame_number;
    u8 command;
    u16 data_len;
    u8 data[MAX_DATA_LEN];
};

struct gamepad_device {
    struct device *dev;
    struct gpio_desc *en_gpio;
    struct input_dev *dev_input;
};

static u8 gamepad_data_checksum(const u8 *data, size_t count)
{
    const u8 *ptr = data;
    u8 ret = data[4];
    for (int i = 5; i < count - 1; i++) {
        ret ^= ptr[i];
    }
    return ret;
}

static void gamepad_send_init_sequence(struct serdev_device *serdev)
{
    gamepad_serdev_write_seq(serdev, 0xA5, 0xD3, 0x5A, 0x3D, 0x0, 0x1, 0x2, 0x0, 0x7, 0x1, 0x5);
    msleep(100);
    gamepad_serdev_write_seq(serdev, 0xA5, 0xD3, 0x5A, 0x3D, 0x1, 0x1, 0x1, 0x0, 0x6, 0x7);
    msleep(100);
    gamepad_serdev_write_seq(serdev, 0xA5, 0xD3, 0x5A, 0x3D, 0x2, 0x1, 0x1, 0x0, 0x2, 0x0);
    msleep(100);
    gamepad_serdev_write_seq(serdev, 0xa5, 0xd3, 0x5a, 0x3d, 0x3, 0x01, 0x0a, 0x00, 0x05, 0x01, 0x00, 0x00,
            0x00, 0x28, 0x00, 0x00, 0x00, 0x07, 0x23);
    msleep(100);
    gamepad_serdev_write_seq(serdev, 0xA5, 0xD3, 0x5A, 0x3D, 0x4, 0x1, 0x1, 0x0, 0x6, 0x2);
    msleep(100);
    gamepad_serdev_write_seq(serdev, 0xA5, 0xD3, 0x5A, 0x3D, 0x5, 0x1, 0x1, 0x0, 0x2, 0x7);
    msleep(100);
}

static void gamepad_input_handler(struct gamepad_device *dev, struct gamepad_data *data)
{
    static unsigned long prev_states;
    unsigned long keys = data->data[0] | (data->data[1] << 8);
    unsigned long current_states = keys, changes;
    int i;
    struct input_dev *indev;

    indev = dev->dev_input;
    if(!indev)
        return;

    bitmap_xor(&changes, &current_states, &prev_states, ARRAY_SIZE(keymap));

    for_each_set_bit(i, &changes, ARRAY_SIZE(keymap)) {
        input_report_key(indev, keymap[i], (current_states & BIT(i)));
    }

    input_report_abs(indev, ABS_HAT2X, 0x755 - (data->data[2] | (data->data[3] << 8)));
    input_report_abs(indev, ABS_HAT2Y, 0x755 - (data->data[4] | (data->data[5] << 8)));
    input_report_abs(indev, ABS_X, -(int16_t)(data->data[6] | (data->data[7] << 8)));
    input_report_abs(indev, ABS_Y, -(int16_t)(data->data[8] | (data->data[9] << 8)));
    input_report_abs(indev, ABS_RX, -(int16_t)(data->data[10] | (data->data[11] << 8)));
    input_report_abs(indev, ABS_RY, -(int16_t)(data->data[12] | (data->data[13] << 8)));

    input_sync(indev);
    prev_states = keys;
}

static void gamepad_data_handler(struct serdev_device *serdev, struct gamepad_data *data)
{
    struct gamepad_device *dev = serdev_device_get_drvdata(serdev);
    switch(data->command) {
        case 0x2:
            gamepad_input_handler(dev, data);
            break;
        default:
            break;
    }
}

static size_t gamepad_mcu_uart_rx_bytes(struct serdev_device *serdev,
                 const u8 *data,
                 size_t count)
{
    struct device *dev = &serdev->dev;
    struct gamepad_data recv_data_buffer;
    u8 checksum;

    if (!data || count < 7) {
        dev_err(dev, "Invalid packet.\n");
        return count;
    }

    checksum = gamepad_data_checksum(data, count);
    if (checksum != *(data + count - 1)) {
        dev_err(dev, "Packet checksum failed.\n");
        return count;
    }

    memcpy(recv_data_buffer.header, data, 4);
    recv_data_buffer.frame_number = *(data + 4);
    recv_data_buffer.command = *(data + 5);
    recv_data_buffer.data_len = *(data + 6) | (*(data + 7) << 8);
    
    if (recv_data_buffer.data_len) {
        memset(&recv_data_buffer.data[0], 0, sizeof(recv_data_buffer.data));
        memcpy(&recv_data_buffer.data[0], data + 8, recv_data_buffer.data_len);
    }

    gamepad_data_handler(serdev, &recv_data_buffer);
    return count;
}

static const struct serdev_device_ops gamepad_mcu_uart_client_ops = {
    .receive_buf = gamepad_mcu_uart_rx_bytes,
};

static int gamepad_mcu_uart_probe(struct serdev_device *serdev)
{
    struct device *dev = &serdev->dev;
    struct gamepad_device *gamepad_dev;
    int ret;

    gamepad_dev = devm_kzalloc(dev, sizeof(*gamepad_dev), GFP_KERNEL);
    if (!gamepad_dev) {
		dev_err(dev, "could not allocate memory for device data\n");
		return -ENOMEM;
	}

    gamepad_dev->en_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_HIGH);
    if (IS_ERR(gamepad_dev->en_gpio)) {
        ret = PTR_ERR(gamepad_dev->en_gpio);
        goto err_free_dev;
    }

    ret = devm_serdev_device_open(dev, serdev);
    if (ret) {
		dev_err(dev, "Unable to open UART device");
		goto err_free_dev;
	}
    gamepad_dev->dev = dev;

    serdev_device_set_drvdata(serdev, gamepad_dev);

    ret = serdev_device_set_baudrate(serdev, 115200);
	if (ret < 0) {
		dev_err(dev, "Failed to set up host baud rate (%d)", ret);
		goto err_free_dev;
	}

	serdev_device_set_flow_control(serdev, false);

    gamepad_dev->dev_input = input_allocate_device();
    if (!gamepad_dev->dev_input) {
        dev_err(dev,
			"Not enough memory for input input device\n");
		ret = -ENOMEM;
        goto err_free_dev;
    }

    gamepad_dev->dev_input->name = "Ayn Odin2 Gamepad";
    gamepad_dev->dev_input->phys = "odin2-gamepad/input0";
    __set_bit(EV_ABS, gamepad_dev->dev_input->evbit);

    for (int i = 0; i < ARRAY_SIZE(keymap); i++)
        input_set_capability(gamepad_dev->dev_input, EV_KEY, keymap[i]);

    input_set_capability(gamepad_dev->dev_input, EV_KEY, BTN_JOYSTICK);

    for (int i = ABS_X; i <= ABS_RZ; i++)
        input_set_abs_params(gamepad_dev->dev_input, i, -0x580, 0x580, 0, 0);

    input_set_abs_params(gamepad_dev->dev_input, ABS_HAT2X, 0, 1830, 0, 30);
    input_set_abs_params(gamepad_dev->dev_input, ABS_HAT2Y, 0, 1830, 0, 30);

    ret = input_register_device(gamepad_dev->dev_input);
	if (ret) {
		dev_err(dev, "Could not register input device");
		goto err_free_dev;
	}

    serdev_device_set_client_ops(serdev, &gamepad_mcu_uart_client_ops);

    gamepad_send_init_sequence(serdev);
    
    return 0;

err_free_dev:
    if (gamepad_dev->dev_input) {
        input_free_device(gamepad_dev->dev_input);
    }

    if (gamepad_dev) {
        devm_kfree(dev, gamepad_dev);
    }
    return ret;
}

static void gamepad_mcu_uart_remove(struct serdev_device *serdev)
{

    return;
}


static const struct of_device_id gamepad_mcu_uart_of_match[] = {
    { .compatible = "ayn,odin2-gamepad"},
    {}
};
MODULE_DEVICE_TABLE(of, gamepad_mcu_uart_of_match);

static struct serdev_device_driver gamepad_mcu_uart_driver = {
    .driver = {
        .name = "odin2-gamepad",
        .of_match_table = gamepad_mcu_uart_of_match,
    },
    .probe = gamepad_mcu_uart_probe,
    .remove = gamepad_mcu_uart_remove,
};

module_serdev_device_driver(gamepad_mcu_uart_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Gamepad driver for Ayn Odin2");
MODULE_AUTHOR("Molly Sophia <mollysophia379@gmail.com>");