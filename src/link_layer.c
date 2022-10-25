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
#include <limits.h>


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
#define REJ (0x01)


#define N_TRIES 3
#define TIMEOUT 4


volatile int STOP = FALSE;
     
unsigned char* last_trama;
unsigned char* received_trama;
int total_bytes_read;
unsigned char* final_content;
int stuffed_size;
int seq_num = 0;
int arq_num = 0;

int seq_num_expected;


unsigned char* stuffed;
unsigned char* unstuffed;

void switch_arq() {
	if (arq_num == 1) {
		arq_num = 0;
	}
	else {
		arq_num = 1;
	}
}

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



unsigned char read_noncanonical (unsigned int size, unsigned char* res)
{   
    // Loop for input
    unsigned char old_trama[size]; // +1: Save space for the final '\0' char

    
    int	bytes;
    unsigned char buf[1];
    int numr = 0;
    while (TRUE) {
    	bytes = read(fd, buf, 1);
    	
    	if (bytes == 0 && numr == 0) {
    		continue;
    	}
    	else if (bytes == 0) {
    		break;
    	}
    	
    	old_trama[numr] = buf[0];
    	numr++;
    }
    
    
    printf("RECEIVED PACKET\n");
    
    int iter = 1;
    for (int i = 1; i < 2000; i++) {
    	if (old_trama[i] == FLAG && old_trama[i + 1] != 0x5d && old_trama[i + 1] != FLAG) {
    		break;
    	}
    	iter++;
    }
    
    unsigned char trama[iter];
    
    for (int i = 0; i <= iter; i++) {
    	trama[i] = old_trama[i];
    } 
    

    	
    int new_size = iter + 1;
    

    if (trama[0] == FLAG && trama[new_size - 1] == FLAG) {
        if (new_size != 5) {
            if (checksum(trama, 4) == 0) {
                if (trama[1] == A_EMISSOR) {
                    unsigned char buf[bytes - 6];
                    int it = 0;
                    
                    if (seq_num_expected == 0 && trama[2] != 0x00) {
                    	printf("DUPLICATE ERROR\n");
                    	switch_arq();
                    	return 6;
                    }
                    else if (seq_num_expected == 1 && trama[2] != 0x40) {
                    	printf("DUPLICATE ERROR\n");
                    	switch_arq();
                    	return 6;
                    }
                    
                    
                    for (int i = 4; i < bytes - 1; i++) {
                        buf[it] = trama[i];
                        it++;
                    }
                    
                    if (checksum(buf, it) == 0) {
                    
                        received_trama = trama;
                        total_bytes_read = numr;
                        
                        
                        return 5;
                    }

                }  
		    }
        }

        else {
            if (checksum(trama, 5) == 0) {
                if (trama[1] == A_RECETOR && trama[2] == UA) return 0; 

                else if (trama[1] == A_EMISSOR && trama[2] == DISC) return 2;

                else if (trama[1] == A_RECETOR && trama[2] == DISC) return 0; 
                
                else if (trama[1] == A_EMISSOR && trama[2] == SET) return 3; 

                else if (trama[1] == A_EMISSOR && trama[2] == UA) return 2;

                else if (trama[1] == A_RECETOR && (trama[2] == 0x85 || trama[2] == 0x05)) {
                    return 0;
                }
                else if (trama[1] == A_RECETOR && trama[2] == REJ) {
                    alarm(0); 
    		    	alarmEnabled = FALSE;
                    return 1;
                }
        
            }
        
        }
    	
    }
    
    return 1;
}

int write_noncanoical(unsigned char* trama, unsigned int size)
{


    // Create string to send

    write(fd, trama, size);
    
    printf("PACKET SENT\n");
    

    // Wait until all bytes have been written to the serial port
    sleep(1);


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

    unsigned char res[2000];

    int check = 1;
    while (check == 1) {
	if (alarmEnabled == FALSE) {
		alarmEnabled = TRUE;
		write_noncanoical(trama, sizeof(trama));
		alarm(3);
		//sleep(1);
		check = read_noncanonical(2000, res);
	}
    }
    alarm(0);
    alarmEnabled = FALSE;
    printf("Connection established!\n");

    return 0;
}


void stuff_bytes(const unsigned char *buf, int bufSize) {
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

    unsigned char res[it * 2];


    for (int i = 0; i < it; i++) {
        res[i] = stuff[i];
    }
    
    stuffed_size = it;

    stuffed = res;
    return;
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

    stuff_bytes(buf, bufSize);
    unsigned char* buf_stuffed = stuffed;
    size_t n = stuffed_size + 6;

    unsigned char trama[n];
    trama[0] = FLAG;
    trama[1] = A_EMISSOR;
    trama[2] = 0x00;
    
    if (seq_num == 1) {
    	trama[2] = 0x40;
    }
    
    
    trama[3] = 0x00; //checksum po Lab3
    trama[3] = checksum(trama, 4); // pode correr mal
    trama[n - 1] = FLAG;
    trama[n - 2] = checksum(buf_stuffed, stuffed_size);

    int it = 0;
    for (int i = 4; i < n - 2; i++) {
        trama[i] = buf_stuffed[it];
        it++;
    }
    

    unsigned char* res = malloc(sizeof(char) * 5);

    int check = 1;
    while (check == 1) {
        if (alarmEnabled == FALSE) {
            alarmEnabled = TRUE;
            write_noncanoical(trama, n);
            alarm(3);
            //sleep(1);
            check = read_noncanonical(2000, res);
        }
    }
    alarm(0); 
    alarmEnabled = FALSE;
    
    return 0;
}

void destuff_bytes(const unsigned char *buf, int bufSize) {
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

    unsigned char res[it];

    for (int i = 0; i < it; i++) {
        res[i] = stuff[i];
    }

    unstuffed = res;
    return;
}



////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char* res = malloc(sizeof(char) * 4000);
    int check = read_noncanonical(4000, res);
        

    if (check == 1) {
        printf("llread deu mal. Recetor envia REJ!\n");
        
        unsigned char trama_rej[5];
		trama_rej[0] = FLAG;
		trama_rej[4] = FLAG;
		trama_rej[1] = A_RECETOR;
		trama_rej[2] = REJ;
		trama_rej[3] = 0x00;
		trama_rej[3] = checksum(trama_rej, 5);
        
        
        sleep(1);
    	write_noncanoical(trama_rej, 5); 
        
        return 3;
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
    else if (check == 5) {
    	trama_envio[2] = 0x05;
    	
    	if (arq_num == 1) {
    		trama_envio[2] = 0x85;
    	}
    	
    	trama_envio[3] = 0x00;
           trama_envio[3] = checksum(trama_envio, 5);
    }
    else if (check == 6) {
    	trama_envio[2] = 0x05;
    	
    	if (arq_num == 1) {
    		trama_envio[2] = 0x85;
    	}
    	
    	trama_envio[3] = 0x00;
        trama_envio[3] = checksum(trama_envio, 5);
           
        sleep(1);
    	write_noncanoical(trama_envio, 5); 
    	return 6;
    }
    
    
    sleep(1);
    write_noncanoical(trama_envio, 5); 
    
      

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
    
    
    
    destuff_bytes(buf, it); 
    unsigned char* content = unstuffed; 
    
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
		write_noncanoical(trama, 5);
		alarm(3);
		//sleep(1);
		check = read_noncanonical(5, res);
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
    write_noncanoical(envio_trama, 5);
    
    
    printf("Connection CLOSED!\n");

    return 0;
}
