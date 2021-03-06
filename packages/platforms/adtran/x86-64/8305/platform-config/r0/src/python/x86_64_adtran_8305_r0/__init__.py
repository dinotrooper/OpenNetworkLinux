from onl.platform.base import *
from onl.platform.adtran import *

class OnlPlatform_x86_64_adtran_8305_r0(OnlPlatformAdtran,
                                              OnlPlatformPortConfig_20x100):
    PLATFORM='x86-64-adtran-8305-r0'
    MODEL="ADTRAN-8305"
    SYS_OBJECT_ID=".8305"

    def baseconfig(self):
        # self.insmod('optoe')
        # self.insmod('ym2651y')
        # self.insmod('adtran_i2c_cpld')
        # for m in [ 'fan', 'cpld1', 'psu', 'leds' ]:
        #     self.insmod("x86-64-adtran-8305-%s.ko" % m)

        self.insmod("drv_ucext_usb.ko")

        # ########### initialize I2C bus 0 ###########
        # self.new_i2c_devices([

        #     # initialize multiplexer (PCA9548)
        #     ('pca9548', 0x76, 0),

        #     # initiate chassis fan
        #     ('8305_fan', 0x66, 2),

        #     # inititate LM75
        #     ('lm75', 0x48, 3),
        #     ('lm75', 0x49, 3),
        #     ('lm75', 0x4a, 3),
        #     ('lm75', 0x4b, 3),

        #     ('8305_cpld1', 0x60, 4),
        #     ('adtran_i2c_cpld', 0x62, 5),
        #     ('adtran_i2c_cpld', 0x64, 6),
        #     ])

        # ########### initialize I2C bus 1 ###########
        # self.new_i2c_devices(
        #     [
        #         # initiate multiplexer (PCA9548)
        #         ('pca9548', 0x71, 1),

        #         # initiate PSU-1
        #         ('8305_psu1', 0x53, 11),
        #         ('ym2651', 0x5b, 11),

        #         # initiate PSU-2
        #         ('8305_psu2', 0x50, 10),
        #         ('ym2651', 0x58, 10),

        #         # initiate multiplexer (PCA9548)
        #         ('pca9548', 0x72, 1),
        #         ('pca9548', 0x73, 1),
        #         ('pca9548', 0x74, 1),
        #         ('pca9548', 0x75, 1),
        #         ]
        #     )

        # # initialize QSFP port 1~32
        # self.new_i2c_devices([
        #         ('optoe1', 0x50, 18),
        #         ('optoe1', 0x50, 19),
        #         ('optoe1', 0x50, 20),
        #         ('optoe1', 0x50, 21),
        #         ('optoe1', 0x50, 22),
        #         ('optoe1', 0x50, 23),
        #         ('optoe1', 0x50, 24),
        #         ('optoe1', 0x50, 25),
        #         ('optoe1', 0x50, 26),
        #         ('optoe1', 0x50, 27),
        #         ('optoe1', 0x50, 28),
        #         ('optoe1', 0x50, 29),
        #         ('optoe1', 0x50, 30),
        #         ('optoe1', 0x50, 31),
        #         ('optoe1', 0x50, 32),
        #         ('optoe1', 0x50, 33),
        #         ('optoe1', 0x50, 34),
        #         ('optoe1', 0x50, 35),
        #         ('optoe1', 0x50, 36),
        #         ('optoe1', 0x50, 37),
        #         ('optoe1', 0x50, 38),
        #         ('optoe1', 0x50, 39),
        #         ('optoe1', 0x50, 40),
        #         ('optoe1', 0x50, 41),
        #         ('optoe1', 0x50, 42),
        #         ('optoe1', 0x50, 43),
        #         ('optoe1', 0x50, 44),
        #         ('optoe1', 0x50, 45),
        #         ('optoe1', 0x50, 46),
        #         ('optoe1', 0x50, 47),
        #         ('optoe1', 0x50, 48),
        #         ('optoe1', 0x50, 49),
        #         ])

        # subprocess.call('echo port9 > /sys/bus/i2c/devices/18-0050/port_name', shell=True)
        # subprocess.call('echo port10 > /sys/bus/i2c/devices/19-0050/port_name', shell=True)
        # subprocess.call('echo port11 > /sys/bus/i2c/devices/20-0050/port_name', shell=True)
        # subprocess.call('echo port12 > /sys/bus/i2c/devices/21-0050/port_name', shell=True)
        # subprocess.call('echo port1 > /sys/bus/i2c/devices/22-0050/port_name', shell=True)
        # subprocess.call('echo port2 > /sys/bus/i2c/devices/23-0050/port_name', shell=True)
        # subprocess.call('echo port3 > /sys/bus/i2c/devices/24-0050/port_name', shell=True)
        # subprocess.call('echo port4 > /sys/bus/i2c/devices/25-0050/port_name', shell=True)
        # subprocess.call('echo port6 > /sys/bus/i2c/devices/26-0050/port_name', shell=True)
        # subprocess.call('echo port5 > /sys/bus/i2c/devices/27-0050/port_name', shell=True)
        # subprocess.call('echo port8 > /sys/bus/i2c/devices/28-0050/port_name', shell=True)
        # subprocess.call('echo port7 > /sys/bus/i2c/devices/29-0050/port_name', shell=True)
        # subprocess.call('echo port13 > /sys/bus/i2c/devices/30-0050/port_name', shell=True)
        # subprocess.call('echo port14 > /sys/bus/i2c/devices/31-0050/port_name', shell=True)
        # subprocess.call('echo port15 > /sys/bus/i2c/devices/32-0050/port_name', shell=True)
        # subprocess.call('echo port16 > /sys/bus/i2c/devices/33-0050/port_name', shell=True)
        # subprocess.call('echo port17 > /sys/bus/i2c/devices/34-0050/port_name', shell=True)
        # subprocess.call('echo port18 > /sys/bus/i2c/devices/35-0050/port_name', shell=True)
        # subprocess.call('echo port19 > /sys/bus/i2c/devices/36-0050/port_name', shell=True)
        # subprocess.call('echo port20 > /sys/bus/i2c/devices/37-0050/port_name', shell=True)
        # subprocess.call('echo port25 > /sys/bus/i2c/devices/38-0050/port_name', shell=True)
        # subprocess.call('echo port26 > /sys/bus/i2c/devices/39-0050/port_name', shell=True)
        # subprocess.call('echo port27 > /sys/bus/i2c/devices/40-0050/port_name', shell=True)
        # subprocess.call('echo port28 > /sys/bus/i2c/devices/41-0050/port_name', shell=True)
        # subprocess.call('echo port29 > /sys/bus/i2c/devices/42-0050/port_name', shell=True)
        # subprocess.call('echo port30 > /sys/bus/i2c/devices/43-0050/port_name', shell=True)
        # subprocess.call('echo port31 > /sys/bus/i2c/devices/44-0050/port_name', shell=True)
        # subprocess.call('echo port32 > /sys/bus/i2c/devices/45-0050/port_name', shell=True)
        # subprocess.call('echo port21 > /sys/bus/i2c/devices/46-0050/port_name', shell=True)
        # subprocess.call('echo port22 > /sys/bus/i2c/devices/47-0050/port_name', shell=True)
        # subprocess.call('echo port23 > /sys/bus/i2c/devices/48-0050/port_name', shell=True)
        # subprocess.call('echo port24 > /sys/bus/i2c/devices/49-0050/port_name', shell=True)

        # self.new_i2c_device('24c02', 0x57, 1)


        return True
