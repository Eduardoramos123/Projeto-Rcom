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
unsigned char* received_trama;
int total_bytes_read;
unsigned char* final_content;
int stuffed_size;


unsigned char checksum (unsigned char *trama, size_t sz) {
	unsigned char res = 0;
	for (int i = 0; i < sz; i++) {
		res -= trama[i];
	}
	return res;
}

char* global_port;

int alarmEnabled = FALSE;

LinkLayer global_var;

int fd;


unsigned char read_noncanonical(const char *port, unsigned int size, unsigned char* res)
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = port;

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    //int fd = open(serialPortName, O_RDWR | O_NOCTTY);
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
    unsigned char *old_trama = malloc(sizeof(char) * size); // +1: Save space for the final '\0' char

    
    int	bytes = read(fd, old_trama, size);
    
    
    
    
    printf("bytes read: %d\n", bytes);
    
    int iter = 1;
    for (int i = 1; i < size; i++) {
    	if (old_trama[i] == FLAG && old_trama[i + 1] != 0x5d && old_trama[i + 1] != FLAG) {
    		break;
    	}
    	iter++;
    }
    
    unsigned char *trama = malloc(sizeof(char) * iter);
    
    for (int i = 0; i <= iter; i++) {
    	trama[i] = old_trama[i];
    } 
    for (int i = 0; i < iter + 1; i++) {
    	printf("%x", trama[i]);
    }
    printf("\n");
    printf("%d\n", size);
    	
    int new_size = iter + 1;
    

    if (trama[0] == FLAG && trama[new_size - 1] == FLAG) {
        if (new_size != 5) {
            if (checksum(trama, 4) == 0) {
                if (trama[1] == A_EMISSOR) {
                    unsigned char buf[bytes - 6];
                    int it = 0;
                    for (int i = 4; i < bytes - 1; i++) {
                        buf[it] = trama[i];
                        it++;
                    }
                    
                    if (checksum(buf, it) == 0) {
                        if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                        {
                            perror("tcsetattr");
                            exit(-1);
                        }

                        //close(fd);
                        received_trama = trama;
                        total_bytes_read = bytes;
                        

                        return 0;
                    }

                }  
		    }
        }
        else {
            printf("checksum: %x\n", checksum(trama, 5));
            if (checksum(trama, 5) == 0) {
                if (trama[1] == A_RECETOR && trama[2] == UA) {

                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    //close(fd);

                    return 0;

                } 
                else if (trama[1] == A_EMISSOR && trama[2] == DISC) {

                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    //close(fd);

                    return 2;

                }
                else if (trama[1] == A_RECETOR && trama[2] == DISC) {

                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    //close(fd);

                    return 0;

                }
                else if (trama[1] == A_EMISSOR && trama[2] == SET) {
                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    //close(fd);

                    return 3;
               }
               else if (trama[1] == A_EMISSOR && trama[2] == UA) {
                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
                    {
                        perror("tcsetattr");
                        exit(-1);
                    }

                    //close(fd);

                    return 2;
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

    //close(fd);


    return 1;
}

int write_noncanoical(const char *port, unsigned char* trama, unsigned int size)
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = port;

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    //int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    
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
    
    printf("bytes writen: %d\n", bytes);
    
    
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

    //close(fd);

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
    trama[3] = checksum(trama, 5); // pode correr mal
    last_trama = trama;

    unsigned char* res = malloc(sizeof(char) * 5);

    int check = 1;
    while (check == 1) {
	if (alarmEnabled == FALSE) {
		alarmEnabled = TRUE;
		write_noncanoical(global_port, trama, sizeof(trama));
		alarm(3);
		//sleep(1);
		check = read_noncanonical(global_port, 5, res);
	}
    }
    alarm(0);
    alarmEnabled = FALSE;
    printf("Connection established!\n");

    return 0;
}


unsigned char* stuff_bytes(const unsigned char *buf, int bufSize) {
    unsigned char stuff[bufSize * 2];
    int it = 0;

    for (int i = 0; i < bufSize; i++) {
        stuff[it] = buf[i];  
        if (buf[i] == FLAG) {
            it++;
            stuff[it] = 0x5d;
        }
        it++;
    }

    unsigned char* res = malloc(sizeof(char) * it * 2);


    for (int i = 0; i < it; i++) {
        res[i] = stuff[i];
    }
    
    stuffed_size = it;

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
    size_t n = stuffed_size + 6;

    unsigned char trama[n];
    trama[0] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = 0x00;
    trama[3] = 0x00; //checksum po Lab3
    trama[3] = checksum(trama, 4); // pode correr mal
    trama[n - 1] = FLAG;
    trama[n - 2] = checksum(buf_stuffed, stuffed_size);

    int it = 0;
    for (int i = 4; i < n - 2; i++) {
        trama[i] = buf_stuffed[it];
        it++;
    }
    
    printf("TRAMA A ENVIAR: ");
    for (int i = 0; i < stuffed_size; i++) {
    	printf("%x", buf_stuffed[i]);
    }
    printf("\n");
    printf("\n");
    printf("\n");
    
    
    printf("TRAMA A ENVIAR: ");
    for (int i = 0; i < n; i++) {
    	printf("%x", trama[i]);
    }
    printf("\n");

    unsigned char* res = malloc(sizeof(char) * 5);

    int check = 1;
    while (check == 1) {
        if (alarmEnabled == FALSE) {
            alarmEnabled = TRUE;
            write_noncanoical(global_port, trama, n);
            alarm(3);
            //sleep(1);
            check = read_noncanonical(global_port, 5, res);
        }
    }
    alarm(0); 
    alarmEnabled = FALSE;
    
    return 0;
}

unsigned char* destuff_bytes(const unsigned char *buf, int bufSize) {
    unsigned char stuff[bufSize];
    int it = 0;

    for (int i = 0; i < bufSize; i++) {
        if (buf[i] != FLAG) {
            stuff[it] = buf[i];
        }
        else {
            stuff[it] = buf[i];
            i++;
        }
        it++;
    }

    unsigned char *res = malloc(sizeof(char) * it);

    for (int i = 0; i < it; i++) {
        res[i] = stuff[i];
    }

    return res;
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet, const char *port)
{
    unsigned char* res = malloc(sizeof(char) * 4000);
    int check = read_noncanonical(port, 4000, res);

    if (check == 1) {
        printf("llread deu mal\n");
        return 0;
    }
    

    unsigned char trama_envio[5];
    trama_envio[0] = FLAG;
    trama_envio[4] = FLAG;
    trama_envio[1] = A_RECETOR;
    trama_envio[2] = UA;
    trama_envio[3] = 0x00;
    trama_envio[3] = checksum(trama_envio, 5);
        
    
    if (check == 2) {
        trama_envio[2] = DISC;
        trama_envio[3] = 0x00;
        trama_envio[3] = checksum(trama_envio, 5);
    }
    
    
    sleep(1);
    write_noncanoical(port, trama_envio, 5); 
    
      

    if (check == 2) {
        return 1;
    }
    else if (check == 3) {
        return 3;
    }
   
    unsigned char buf[total_bytes_read - 5];
    int it = 0;
    for (int i = 4; i < total_bytes_read - 2; i++) {
        buf[it] = received_trama[i];
        it++;
    }
    
    printf("CONTENT: ");
    for (int i = 0; i < it; i++) {
    	printf("%x", buf[i]);
    }
    printf("\n");
	
    unsigned char* content = destuff_bytes(buf, it); 
    
    final_content = content;


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
    

    unsigned char trama[5];
    trama[0] = FLAG;
    trama[4] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = DISC;
    trama[3] = 0x00; //checksum po Lab3
    trama[3] = checksum(trama, 5); // pode correr mal
    last_trama = trama;

    unsigned char* res = malloc(sizeof(char) * 5);
  
    int check = 1;
    while (check == 1) {
	if (alarmEnabled == FALSE) {
		alarmEnabled = TRUE;
		write_noncanoical(global_port, trama, 5);
		alarm(3);
		//sleep(1);
		check = read_noncanonical(global_port, 5, res);
	}
    }
    alarm(0);
    alarmEnabled = FALSE;
    
    unsigned char envio_trama[5];
    envio_trama[0] = FLAG;
    envio_trama[4] = FLAG;
    envio_trama[1] = A_EMISSOR;
    envio_trama[2] = UA;
    envio_trama[3] = 0x00; //checksum po Lab3
    envio_trama[3] = checksum(envio_trama, 5); // pode correr mal
    last_trama = envio_trama;
    
    
    sleep(1);
    write_noncanoical(global_port, envio_trama, 5);
    
    printf("Connection CLOSED!\n");

    return 0;
}

int main(int argc, char *argv[])
{
    LinkLayer connectionParameters;
    LinkLayerRole r;

    connectionParameters.baudRate = 0;
    connectionParameters.nRetransmissions = 0;
    connectionParameters.timeout = 0;
    strcpy(connectionParameters.serialPort, argv[1]);

    fd = open(argv[1], O_RDWR | O_NOCTTY);

    char str1[] = "tx";
    if (strcmp(argv[2], str1) == 0) {
        r = LlTx;
    }
    else {
        r = LlRx;
    }

    if (r == LlTx) {

        llopen(connectionParameters);
        sleep(1);

        unsigned char* res = malloc(sizeof(char) * 5);
        res[0] = 0x71;
        res[1] = FLAG;
        res[2] = 0x5d;
        res[3] = 0x31;
        res[4] = 0x11;

        printf("SENDING1: ");
        for (int i = 0; i < sizeof(res); i++) {
            printf("%x", res[i]);
        }
        printf("\n");

        llwrite(res, 5);
        sleep(1);
        
        unsigned char* res2 = malloc(sizeof(char) * 10);
        res2[0] = FLAG;
        res2[1] = FLAG;
        res2[2] = FLAG;
        res2[3] = FLAG;
        res2[4] = FLAG;
        res2[5] = FLAG;
        
        
        printf("SENDING2: ");
        for (int i = 0; i < sizeof(res2); i++) {
            printf("%x", res2[i]);
        }
        printf("\n");
        
        
        
        llwrite(res2, 10);
        sleep(1);


        llclose(0);
    }
    else {
        int check = 0;
        unsigned char* res = malloc(sizeof(char) * 4000);
        while (check == 0) {
            check = llread(res, argv[1]);
            if (check == 3) {
                check = 0;
            }
            else if (check == 0) {
                printf("RECEIVED: ");
                for (int i = 0; i < total_bytes_read; i++) {
                    printf("%x", final_content[i]);
                }
                printf("\n");
            }
            else if (check == 1) {
            	llread(res, argv[1]);
            }
        }
    }
    
    close(fd);

    return 0;
}


