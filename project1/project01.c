/* Copyright (c) 2011, RidgeRun
 * All rights reserved.
 *
From https://www.ridgerun.com/developer/wiki/index.php/Gpio-int-test.c

 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the RidgeRun.
 * 4. Neither the name of the RidgeRun nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY RIDGERUN ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL RIDGERUN BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>	// Defines signal-handling functions (i.e. trap Ctrl-C)
#include "i2c-dev.h"    // Included all the i2c control related configuration and functions


 /****************************************************************
 * Constants
 ****************************************************************/
 
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT (3 * 1000) /* 3 seconds */
#define MAX_BUF 64
#define SWITCH 1
#define PWM    2 
#define I2C    3
/****************************************************************
 * Global variables
 ****************************************************************/
int keepgoing = 1;	// Set to 0 when ctrl-c is pressed
char check;		// Set to 0 when ctrl-c is pressed
int check_pwm = 1;
int ctrl;
int check_temp = 1;

/****************************************************************
 * signal_handler
 ****************************************************************/
// Callback called when SIGINT is sent to the process (Ctrl-C)
void signal_handler(int sig)
{
	printf( "Ctrl-C pressed, cleaning up and exiting..\n" );
	keepgoing = 0;
	check = '0'; 	//terminate led blink
	check_pwm = 0;
	check_temp = 0;
	ctrl = 1;
	
	FILE *file_pwm;
	file_pwm = fopen("/sys/class/pwm/ehrpwm.1\:0/run","w+");
	fprintf(file_pwm, "%d", 1);
	fclose(file_pwm);

}

int read_analog(){
	FILE* file_analog;
	file_analog = fopen("/sys/devices/platform/omap/tsc/ain6","r");
	int analog_value;
	fscanf(file_analog, "%d", &analog_value);
	fclose(file_analog);
//	printf("the analog value is:%d\n",analog_value);
	return analog_value;
}


void set_pinMux(){

	FILE *file_pinMux;
	file_pinMux = fopen("/sys/kernel/debug/omap_mux/gpmc_a2","w+");
	fprintf(file_pinMux, "%d", 6);
	fclose(file_pinMux);
}

void ini_pwm_run(){

	FILE *file_pwm;
	file_pwm = fopen("/sys/class/pwm/ehrpwm.1\:0/run","w+");
	fprintf(file_pwm, "%d", 1);
	fclose(file_pwm);
}


void ini_pwm_period(){

	FILE *file_pwm;
	file_pwm = fopen("/sys/class/pwm/ehrpwm.1\:0/period_freq","w+");
	fprintf(file_pwm, "%d", 25);
	fclose(file_pwm);
}
void set_pwm(int analog){

	int duty;
	FILE *file_pwm;
	file_pwm = fopen("/sys/class/pwm/ehrpwm.1\:0/duty_percent","w+");
	duty = analog/40;
	fprintf(file_pwm, "%d", duty);
	fclose(file_pwm);
}

int readTemp() {
	char *end;
	int res, i2cbus, address, size, file;
	int daddress;
	char filename[20];

	i2cbus = 3;
	address = 73;
	daddress = 0;
	size = I2C_SMBUS_BYTE;

	sprintf(filename, "/dev/i2c-%d", i2cbus);
	file = open(filename, O_RDWR);
	if (file<0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Error: Could not open file "
					"/dev/i2c-%d: %s\n", i2cbus, strerror(ENOENT));
		} else {
			fprintf(stderr, "Error: Could not open file "
					"`%s': %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
		//exit(1);
		}
		
		if (ioctl(file, I2C_SLAVE, address) < 0) {
		fprintf(stderr,
		"Error: Could not set address to 0x%02x: %s\n",
		address, strerror(errno));
		return -errno;
		}
		
		res = i2c_smbus_write_byte(file, daddress);
		if (res < 0) {
		fprintf(stderr, "Warning - write failed, filename=%s, daddress=%d\n",
		filename, daddress);
		}
		res = i2c_smbus_read_byte_data(file, daddress);
		close(file);
		
		if (res < 0) {
		fprintf(stderr, "Error: Read failed, res=%d\n", res);
		exit(2);
		}
		
		printf("0x%02x (%d)\n", res, res);
		
		float f = (9.0/5.0)*res + 32;
		
		return f;
		}
/****************************************************************
 * gpio_export
 ****************************************************************/
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
 
	return 0;
}

/****************************************************************
 * gpio_unexport
 ****************************************************************/
int gpio_unexport(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_set_dir
 ****************************************************************/
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
	int fd, len;
	char buf[MAX_BUF];
 
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/direction");
		return fd;
	}
 
	if (out_flag)
		write(fd, "out", 4);
	else
		write(fd, "in", 3);
 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_set_value
 ****************************************************************/
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd, len;
	char buf[MAX_BUF];
 
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-value");
		return fd;
	}
 
	if (value)
		write(fd, "1", 2);
	else
		write(fd, "0", 2);
 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_get_value
 ****************************************************************/
int gpio_get_value(unsigned int gpio, unsigned int *value)
{
	int fd, len;
	char buf[MAX_BUF];
	char ch;

	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("gpio/get-value");
		return fd;
	}
 
	read(fd, &ch, 1);

	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}
 
	close(fd);
	return 0;
}


/****************************************************************
 * gpio_set_edge
 ****************************************************************/

int gpio_set_edge(unsigned int gpio, char *edge)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-edge");
		return fd;
	}
 
	write(fd, edge, strlen(edge) + 1); 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_fd_open
 ****************************************************************/

int gpio_fd_open(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY | O_NONBLOCK );
	if (fd < 0) {
		perror("gpio/fd_open");
	}
	return fd;
}

/****************************************************************
 * gpio_fd_close
 ****************************************************************/

int gpio_fd_close(int fd)
{
	return close(fd);
}

/****************************************************************
 * Main
 ****************************************************************/
int main(int argc, char **argv, char **envp)
{
	struct pollfd fdset[2];
	int nfds = 2;
	
	//int analog;

	int gpio_fd, timeout, rc;

	int analog_value_is;

	char buf[MAX_BUF];
	char compair[5];
	unsigned int gpio;
	unsigned int gpio_blink_60;
	int gpio_blink_60_fd;
	//int gpio_
	int len;
	int toggle = 0;
	int mode;
	int act;
	ctrl = 0;

	if (argc < 3) {
		printf("Usage: input gpio, output gpio\n\n");
		printf("Waits for a change in the GPIO pin voltage level or input on stdin\n");
		exit(-1);
	}

	// Set the signal callback for Ctrl-C
	signal(SIGINT, signal_handler);

	gpio = atoi(argv[1]);
	gpio_blink_60 = atoi(argv[2]);

	gpio_export(gpio);
	gpio_set_dir(gpio, 0);
	gpio_set_edge(gpio, "both");  // Can be rising, falling or both
	gpio_fd = gpio_fd_open(gpio);

	gpio_export(gpio_blink_60);
	gpio_set_dir(gpio_blink_60, 1);
	gpio_blink_60_fd = gpio_fd_open(gpio_blink_60);

	timeout = POLL_TIMEOUT;
	ini_pwm_run();
	ini_pwm_period();
 
	while (keepgoing) {
		memset((void*)fdset, 0, sizeof(fdset));

		fdset[0].fd = STDIN_FILENO;
		fdset[0].events = POLLIN;
      
		fdset[1].fd = gpio_fd;
		fdset[1].events = POLLPRI;

		rc = poll(fdset, nfds, timeout);      

		if (rc < 0) {
			printf("\npoll() failed!\n");
			return -1;
		}
      
		if (rc == 0) {
			printf(".");
		}
            
		if (fdset[1].revents & POLLPRI) {
			lseek(fdset[1].fd, 0, SEEK_SET);  // Read from the start of the file
			len = read(fdset[1].fd, buf, MAX_BUF);
			//printf("\npoll() GPIO %d interrupt occurred, value=%c, len=%d\n",
			//	 gpio, buf[0], len);
			
			//read(fdset[1].fd, "%d", &check);
			
			check = buf[0];	//Blink LED when switch value turn to '1'
	  		//read(gpio_fd, &check, 1);
			//printf("check's value: %d\n",check);
			//printf("Buf's value is:%c\n",buf[0]);

			printf("Please choose the mode:\n1: switch control\n2: PWM control\n3: I2C control\nEnter your choice:");
			scanf("%d",&mode);
			switch(mode){

				case SWITCH:
					while(1){
					memset((void*)fdset, 0, sizeof(fdset));

					fdset[0].fd = STDIN_FILENO;		
					fdset[0].events = POLLIN;
      
					fdset[1].fd = gpio_fd;
					fdset[1].events = POLLPRI;
	
					rc = poll(fdset, nfds, timeout);      
	
					if (rc < 0) {
						printf("\npoll() failed!\n");
						return -1;
					}
      
					if (rc == 0) {
						printf(".");
					}
            
					if (fdset[1].revents & POLLPRI) {
					lseek(fdset[1].fd, 0, SEEK_SET);  // Read from the start of the file
					len = read(fdset[1].fd, buf, MAX_BUF);
			
					check = buf[0];	//Blink LED when switch value turn to '1'
					if(check == '1'){
						printf("The switch has been activated.\nThe green LED is blinking.\n");
						printf("---------------------------------------------------------------------\n");
						printf("Ctrl+C to stop blinking\n");
					}
					if(check == '0'){
						printf("If you would like blink the LED again, please press the switch again.\n");
						printf("Another Ctrl+C to the end of this program.\n");
						printf("---------------------------------------------------------------------\n");
					}
					
					while(check == '1')
					{
						toggle = !toggle;
					
						gpio_set_value(gpio_blink_60,toggle);
						usleep(100000);
					}
					}
					}
					break;

				case PWM:
					check_pwm = 1;
					while(check_pwm){
						set_pinMux();
						analog_value_is = read_analog();
						set_pwm(analog_value_is);
						usleep(100000);
					}
					break;

				case I2C:
					check_temp = 1;
					while(check_temp){
						act = readTemp();
						if(act > 75){
							gpio_set_value(gpio_blink_60,1);
						}
					}
					break;

				default:
					break;
			}
			
			gpio_fd_close(gpio_blink_60_fd);


		}

		if (fdset[0].revents & POLLIN) {
			(void)read(fdset[0].fd, buf, 1);
			printf("\npoll() stdin read 0x%2.2X\n", (unsigned int) buf[0]);
		}

		fflush(stdout);
	}

	gpio_fd_close(gpio_fd);
	//gpio_fd_close(gpio_48_fd);
	return 0;
}

