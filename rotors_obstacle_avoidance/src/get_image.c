#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <linux/videodev2.h>
#include <fcntl.h>

#include <getopt.h>
#include <syslog.h>
#include <math.h>

#include "camera.h"

#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <linux/videodev2.h>

flushBuffer()
{
	uint8_t mode = 0;
	uint8_t bits = 8;
	uint32_t speed = 25000000 / 4;
	uint16_t delay = 0;
	int ret = 0;
	int fd;
	int exposure = 2; // milisec
	char *gpioName = "/sys/class/gpio/gpio237/value";
	char *spidevName = "/dev/spidev0.0";

	int gpio_fd = -1;
	//pull gpio high in case it was previously low
	gpio_fd = fopen(gpioName, "w");
	if (gpio_fd == NULL)
	{
		printError("Can't open GPIO, you may not have enough privilege");
	}

	fwrite("1", sizeof(char), 1, gpio_fd);
	fflush(gpio_fd);
	fclose(gpio_fd);
	usleep(20000); //wait for 20 milliseconds

	fd = open(spidevName, O_RDWR);
	if (fd < 0)
		printError("can't open device");

	/*
	* spi mode
	*/
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		printError("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		printError("can't get spi mode");

	/*
	* bits per word
	*/
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		printError("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		printError("can't get bits per word");

	/*
	* max speed hz
	*/
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printError("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printError("can't get max speed hz");

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed / 1000);

	//buffers
	uint8_t *tx = (uint8_t *)malloc(4 * sizeof(uint8_t));
	uint8_t expoHigh = exposure >> 8;
	uint8_t expoLow = exposure % 256;
	uint8_t *rx = (uint8_t *)malloc(4 * sizeof(uint8_t));

	gpio_fd = -1;

	gpio_fd = fopen(gpioName, "w");
	if (gpio_fd == NULL)
	{
		printError("Can't open GPIO, you may not have enough privilege");
	}

	fwrite("0", sizeof(char), 1, gpio_fd);
	fflush(gpio_fd);
	fclose(gpio_fd);

	usleep(20000); //wait for 20 milliseconds

	tx[0] = 0x90;	 //command, 0x90 is set trigger.
	tx[1] = 0x00;	 //trigger offset
	tx[2] = expoHigh; // tx[2][3:0] is exposure[11:8].
	tx[3] = expoLow;  // tx[3][7:0] is exposure[7:0].
	struct spi_ioc_transfer tr{};
		tr.tx_buf = (unsigned long)tx;
		tr.rx_buf = (unsigned long)rx;
		tr.len = 4;
		tr.delay_usecs = delay;
		tr.speed_hz = speed;
		tr.bits_per_word = bits;
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	usleep(20000); //wait for 20 milliseconds

	gpio_fd = -1;

	gpio_fd = fopen(gpioName, "w");
	if (gpio_fd == NULL)
	{
		printError("Can't open GPIO, you may not have enough privilege");
	}

	fwrite("1", sizeof(char), 1, gpio_fd);
	fflush(gpio_fd);
	fclose(gpio_fd);

	openlog("remote", LOG_PID, LOG_USER);
	syslog(LOG_INFO, "SPI sent");
	closelog();

	free(tx);
	free(rx);

	close(fd);
}

int main(int argc, char **argv)
{
	int i,j,k;
	char *dev_name = "/dev/video0";
	char *dev_name2 = "/dev/video1";
	//resolution
	//2432x1842
	cameraState *c1 = init_camera(dev_name, 2432, 1842, 1, 3, 2);
	cameraState *c2 = init_camera(dev_name2, 2432, 1842, 1, 3, 2);

	//V4L2_CID_EXPOSURE
	//V4L2_CID_GAIN

	// V4L2_CID_CONTRAST = 0 always take
	// V4L2_CID_CONTRAST = 1 take a picture when signal received

	//v4l2ctrl thingies
	int control_val, control_val2;
	v4l2SetControl(c1, V4L2_CID_EXPOSURE, 10);
	control_val = v4l2GetControl(c1, V4L2_CID_EXPOSURE);
	fprintf(stderr, "set value:%d\n", control_val);

	v4l2SetControl(c1, V4L2_CID_GAIN, 1);
	control_val = v4l2GetControl(c1, V4L2_CID_GAIN);
	fprintf(stderr, "set value:%d\n", control_val);

	v4l2SetControl(c2, V4L2_CID_EXPOSURE, 10);
	control_val2 = v4l2GetControl(c2, V4L2_CID_EXPOSURE);
	fprintf(stderr, "set value:%d\n", control_val2);

	v4l2SetControl(c2, V4L2_CID_GAIN, 1);
	control_val2 = v4l2GetControl(c1, V4L2_CID_GAIN);
	fprintf(stderr, "set value:%d\n", control_val2);

	// Set Contrast to 1
	v4l2SetControl(c1, V4L2_CID_CONTRAST, 1);
	control_val = v4l2GetControl(c1, V4L2_CID_CONTRAST);
	fprintf(stderr, "set value:%d\n", control_val);

	v4l2SetControl(c2, V4L2_CID_CONTRAST, 1);
	control_val2 = v4l2GetControl(c2, V4L2_CID_CONTRAST);
	fprintf(stderr, "set value:%d\n", control_val2);

	void *hostBuffer = malloc(c1->width * c1->height * c1->bytePerPixel * 5);
	void *hostBuffer2 = malloc(c2->width * c2->height * c2->bytePerPixel * 5);

	struct v4l2_buffer buff1;
	struct v4l2_buffer buff2;
	int k;

	for (i = 0; i < 3; i++)
	{
		// remove image from buff first
		getBufferTimeOut(c1, &buff1, 1);
		getBufferTimeOut(c2, &buff2, 1);
	}

	fprintf(stdout, "Starting taking pictures\n");
	for (k = 0; k < 10; k++)
	{
		flushBuffer();

		getBuffer(c1, &buff1);
		getBuffer(c2, &buff2);

		FILE *f1 = fopen("img.raw", "w");
		FILE *f2 = fopen("img2.raw", "w");

		fwrite(buff1.m.userptr, c1->bytePerPixel, c1->width * c1->height, f1);
		fwrite(buff2.m.userptr, c2->bytePerPixel, c2->width * c2->height, f2);

		pushBuffer(c1, &buff1);
		pushBuffer(c2, &buff2);

		fprintf(stdout, "Waiting 1 sec before next image...\n");
		usleep(1000000); // sleep for 1 sec
	}
	fflush(stdout);

	uninit_device(c1);
	uninit_device(c2);
	return 0;
}
