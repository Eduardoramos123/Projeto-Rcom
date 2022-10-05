#include "link_layer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>



// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define FLAG (0xFE)
#define SET (0x03)
#define UA (0x07)
#define A_EMISSOR (0x03)
#define A_RECETOR (0x01)
#define DISC (0x0B)


volatile int STOP = FALSE;

unsigned char* last_trama;


unsigned char checksum (unsigned char *trama, size_t sz) {
	unsigned char res = 0;
	for (int i = 0; i < sz; i++) {
		res -= trama[i];
	}
	return res;
}

unsigned char* global_port;

int alarmEnabled = FALSE;

LinkLayer global_var;


unsigned char read_noncanonical(char *port, unsigned int size)
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = port;

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char *trama = malloc(sizeof(char) * size); // +1: Save space for the final '\0' char

    int bytes = read(fd, trama, size);
    
    printf("%d\n", size);
    	

    

    if (trama[0] == FLAG && trama[size - 1] == FLAG) {
        if (size != 5) {
            if (checksum(trama, 4) == 0) {
                if (trama[1] == A_EMISSOR) {
                    unsigned char buf[sizeof(trama) - 5];
                    int it = 0;
                    for (int i = 4; i < sizeof(trama) - 1; i++) {
                        buf[it] = trama[i];
                        it++;
                    } 
                    
                    if (checksum(buf, sizeof(buf)) == 0) {
                        if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                        {
                            perror("tcsetattr");
                            exit(-1);
                        }

                        close(fd);

                        return 0;
                    }

                }  
		    }
        }
        else {
            if (checksum(trama, 5) == 0) {
                if (trama[1] == A_RECETOR && trama[2] == UA) {

                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    close(fd);

                    return 0;

                } 
                else if (trama[1] == A_EMISSOR && trama[2] == DISC) {

                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    close(fd);

                    return 0;

                } 
            }
            
        }
    	
    }



    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);


    return 1;
}

int write_noncanoical(char *port, unsigned char* trama, unsigned int size)
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = port;

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send




    int bytes = write(fd, trama, size);
    
    for (int i = 0; i < size; i++) {
    	printf("%x", trama[i]);
    }
    
    printf("\n");

    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

void alarmHandler(int signal)
{   
    alarmEnabled = FALSE;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarmHandler;
    sigaction(SIGALRM,&action,NULL);
    
    global_port = connectionParameters.serialPort;
    global_var = connectionParameters;

    unsigned char trama[5];
    trama[0] = FLAG;
    trama[4] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = SET;
    trama[3] = 0x00; //checksum po Lab3
    trama[3] = checksum(trama, 4); // pode correr mal
    last_trama = trama;

    int check = 1;
    while (check == 1) {
	if (alarmEnabled == FALSE) {
		alarmEnabled = TRUE;
		write_noncanoical(global_port, trama);
		alarm(3);
		//sleep(1);
		check = read_noncanonical(global_port, 5);
	}
    }
    alarm(0);
    printf("Connection established!\n");

    return 0;
}


unsigned char* stuff_bytes(const unsigned char *buf, int bufSize) {
    unsigned char stuff[bufSize * 2];
    int it = 0;

    for (int i = 0; i < bufSize; i++) {
        stuff[it] == buf[i];  
        if (buf[i] == FLAG) {
            stuff[it + 1] == 0x5d;
            it++;
        }
        it++;
    }

    unsigned char res[it];

    for (int i = 0; i < it; i++) {
        res[i] = stuff[i];
    }

    return res;
}



////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarmHandler;
    sigaction(SIGALRM,&action,NULL);


    unsigned char* buf_stuffed = stuff_bytes(buf, bufSize);
    size_t n = sizeof(buf_stuffed) + 6;

    unsigned char trama[5];
    trama[0] = FLAG;
    trama[4] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = SET;
    last_trama = trama;

    unsigned char* buf_stuffed = stuff_bytes(buf, bufSize);
    size_t n = sizeof(buf_stuffed) + 6;

    unsigned char trama[n];
    trama[0] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = 0x00;
    trama[3] = 0x00; //checksum po Lab3
    trama[3] = checksum(trama, 4); // pode correr mal
    trama[n - 1] = FLAG;
    trama[n - 2] = checksum(buf_stuffed, sizeof(buf_stuffed));

    int it = 0;
    for (int i = 4; i < n - 2; i++) {
        trama[i] = buf_stuffed[it];
        it++;
    }

    int check = 1;
    while (check == 1) {
        if (alarmEnabled == FALSE) {
            alarmEnabled = TRUE;
            write_noncanoical(global_port, trama);
            alarm(3);
            //sleep(1);
            check = read_noncanonical(global_port, 5);
        }
    }
    alarm(0); 

    return 0;
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    *packet = read_noncanonical(global_port, 4000);



    unsigned char trama_envio[5];
    trama_envio[0] = FLAG;
	trama_envio[4] = FLAG;
	trama_envio[1] = A_RECETOR;
	trama_envio[2] = UA;
	trama_envio[3] = 0x00;
	trama_envio[3] = checksum(trama_envio, 5);

    write_noncanoical(global_port, trama_envio);   

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarmHandler;
    sigaction(SIGALRM,&action,NULL);
    
    global_port = connectionParameters.serialPort;
    global_var = connectionParameters;

    unsigned char trama[5];
    trama[0] = FLAG;
    trama[4] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = DISC;
    trama[3] = 0x00; //checksum po Lab3
    trama[3] = checksum(trama, 5); // pode correr mal
    last_trama = trama;

    int check = 1;
    while (check == 1) {
	if (alarmEnabled == FALSE) {
		alarmEnabled = TRUE;
		write_noncanoical(global_port, trama);
		alarm(3);
		//sleep(1);
		check = read_noncanonical(global_port, 5);
	}
    }
    alarm(0);

    return 0;
}
