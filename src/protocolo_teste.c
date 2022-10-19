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


//Controlo 
#define DADOS (0X01)
#define START (0X02)
#define END (0X03)

#define SIZE_FILE (0X00);
#define NAME_FILE (0X01);


#define N_TRIES 3
#define TIMEOUT 4


volatile int STOP = FALSE;
char seq_number=0;
     
unsigned char* last_trama;
unsigned char* received_trama;
int total_bytes_read;
unsigned char* final_content;
int stuffed_size;
int seq_num;
int arq_num;

int num_read;


void switch_seq() {
	if (seq_num == 1) {
		seq_num = 0;
	}
	else {
		seq_num = 1;
	}
}

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
    unsigned char *old_trama = malloc(sizeof(char) * size); // +1: Save space for the final '\0' char

    
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
                    
                        received_trama = trama;
                        total_bytes_read = numr;
                        
                        return 5;
                    }

                }  
		    }
        }

        else {
            printf("checksum: %x\n", checksum(trama, 5));
            printf("trama[2]: %x\n", trama[2]);
            if (checksum(trama, 5) == 0) {
                if (trama[1] == A_RECETOR && trama[2] == UA) return 0; 

                else if (trama[1] == A_EMISSOR && trama[2] == DISC) return 2;

                else if (trama[1] == A_RECETOR && trama[2] == DISC) return 0; 
                
                else if (trama[1] == A_EMISSOR && trama[2] == SET) return 3; 

                else if (trama[1] == A_EMISSOR && trama[2] == UA) return 2;

                else if (trama[1] == A_RECETOR && (trama[2] == 0x85 || trama[2] == 0x05)) {
               	    printf("HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    return 0;
                }
        
            }
        
        }
    	
    }

    return 1;
}

int write_noncanoical(unsigned char* trama, unsigned int size)
{


    // Create string to send

    int bytes = write(fd, trama, size);
    
    printf("bytes writen: %d\n", bytes);
    
    
    for (int i = 0; i < size; i++) {
    	printf("%x", trama[i]);
    }
    
    printf("\n");

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

    unsigned char* res = malloc(sizeof(char) * 5);

    int check = 1;
    while (check == 1) {
	if (alarmEnabled == FALSE) {
		alarmEnabled = TRUE;
		write_noncanoical(trama, sizeof(trama));
		alarm(3);
		//sleep(1);
		check = read_noncanonical(5, res);
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
            write_noncanoical(trama, n);
            alarm(3);
            //sleep(1);
            check = read_noncanonical(5, res);
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
int llread(unsigned char *packet)
{
    unsigned char* res = malloc(sizeof(char) * 4000);
    int check = read_noncanonical(4000, res);

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
    else if (check == 5) {
    	trama_envio[2] = 0x05;
    	
    	if (arq_num == 1) {
    		trama_envio[2] = 0x85;
    	}
    	
    	trama_envio[3] = 0x00;
           trama_envio[3] = checksum(trama_envio, 5);
    }
    
    
    sleep(1);
    write_noncanoical(trama_envio, 5); 
    
      

    if (check == 2) {
        return 1;
    }
    else if (check == 3) {
        return 3;
    }
    
    printf("\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("RECEIVED_TRAMA: \n");
   
    unsigned char buf[total_bytes_read - 5];
    int it = 0;
    for (int i = 4; i < total_bytes_read - 2; i++) {
        buf[it] = received_trama[i];
        it++;
        printf("%x", received_trama[i]);
    }
    
    printf("\n\n\n\n\n\n\n\n\n\n\n\n");
    
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



void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename){

    LinkLayer connectionParameters;
    LinkLayerRole r;

    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    strcpy(connectionParameters.serialPort, serialPort);
    

    char str1[] = "tx";
    char str2[] = "rx";
     
    if (strcmp(role, str1) == 0) r = LlTx;
    
    else if (strcmp(role, str2) == 0) r = LlRx;

    else {
        printf("Argv[2] has an error");
        exit(1);
    }

    if (r == LlTx){
    
    	llopen(connectionParameters);
    	sleep(1);
    	
    	printf("filename: %s\n", filename);

        FILE *pinguim = fopen(filename,"rb");
        
        if (pinguim == NULL) {
        	printf("Erro ao abrir o ficheiro\n");
        	
        	return;
        }
        
        //const char *name_file = filename;
        //fseek(pinguim, 0L, SEEK_END);
        long int tamanho_file = ftell(pinguim);
        printf("size of file is equal to : %ld \n",tamanho_file);

        //pacote controlo Start-------------------------------------
        int number_octetos;
        if(tamanho_file <= UCHAR_MAX) number_octetos = 1;
        else if(tamanho_file <= USHRT_MAX) number_octetos = 2;
        else if(tamanho_file <= INT_MAX) number_octetos = 4;
        else perror("file is to big");
        printf("octetos %d",number_octetos);

        unsigned char* pacote_controlo = malloc(5 + number_octetos + strlen(filename));

        pacote_controlo[0] = START;
        pacote_controlo[1] = SIZE_FILE;
        pacote_controlo[2] = number_octetos;

        if(number_octetos == 1) tamanho_file = tamanho_file << 56;
        if(number_octetos == 2) tamanho_file = tamanho_file << 48;
        if(number_octetos == 4) tamanho_file = tamanho_file << 32;


        for(int i = 3; i <= (2 + number_octetos); i++){
            pacote_controlo[i] = tamanho_file;
            tamanho_file = tamanho_file << 8;
        }
        pacote_controlo[ 2 + number_octetos + 1] = NAME_FILE;
        pacote_controlo[ 2 + number_octetos + 2] = strlen(filename);
        for(int i = 0 ; i < strlen(filename);i++){
            pacote_controlo[2+number_octetos+3+i] = filename[i];
        }
        //llwrite(pacote_controlo,sizeof(pacote_controlo));
        //sleep(1);

        //envio de dados -----------------------------------------
        unsigned char pacote_dados[1500];
        unsigned char dados_efetivos[1496];
        while (!feof(pinguim)){
            pacote_dados[0] = DADOS;
            pacote_dados[1] = seq_number; // ver melhor
            int len = fread(dados_efetivos,1,1496,pinguim);
            printf("Numero de dados enviados: %d \n",len);
            char l1 = (char)(len / 256);
            char l2 = (char)(len % 256);
            pacote_dados[2] = l2;
            pacote_dados[3] = l1;
            memcpy(&pacote_dados[4],dados_efetivos,sizeof(dados_efetivos));
            llwrite(pacote_dados,1500);
            sleep(1);
            
            num_read = len;

            
        }
        fclose(pinguim);
        //pacote controlo End--------------------------------------
        pacote_controlo[0] = END;
        llwrite(pacote_controlo,sizeof(pacote_controlo));
        sleep(1);
        free(pacote_controlo);

        llclose(0);
    }
    
    else{
        FILE *pinguim2 = fopen("penguin2.gif","wb");
        int check = 0;
        unsigned char* res = malloc(sizeof(char) * 2000);
        int a = 0;
        while (check == 0) {
            check = llread(res);
            if (check == 3) {
                check = 0;
            }
            else if (check == 0) {
            	a++;
            	
            	unsigned char final[1496];
            	
            	printf("\n\n\n\n\n");
                printf("SIZE: %d\n", 256 * final_content[2] + final_content[3]);
                printf("\n\n\n\n\n");
            	
                
                int acum = 0;
                for (int i = 4; i < total_bytes_read - 6; i++) {
                	final[acum] = final_content[i];
                	acum++;
                }
                
                //unsigned char buf[acum];
            	
                printf("RECEIVED: ");
                for (int i = 0; i < 1496; i++) {
                    printf("%x", final[i]);
                }
                printf("\n");
                
                
                printf("\n\n\n\n\n");
                printf("ACUM: %d\n", acum);
                printf("\n\n\n\n\n");
                
                
                fwrite(final,1,1496, pinguim2);
                	
                
                
                switch_arq();
                
            }
            else if (check == 1) {
            	llread(res);
            }
        }
        fclose(pinguim2);
    }
    return;
}


int main(int argc, char *argv[])
{

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(argv[1], O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(argv[1]);
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
    newtio.c_cc[VTIME] = 10; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

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

    if (argc < 4)
    {
        printf("Usage: %s /dev/ttySxx tx|rx filename\n", argv[0]);
        exit(1);
    }

    const char *serialPort = argv[1];
    const char *role = argv[2];
    const char *filename = argv[3];

    printf("Starting link-layer protocol application\n"
           "  - Serial port: %s\n"
           "  - Role: %s\n"
           "  - Baudrate: %d\n"
           "  - Number of tries: %d\n"
           "  - Timeout: %d\n"
           "  - Filename: %s\n",
           serialPort,
           role,
           BAUDRATE,
           N_TRIES,
           TIMEOUT,
           filename);

    applicationLayer(serialPort, role, BAUDRATE, N_TRIES, TIMEOUT, filename);
    
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    
    close(fd);

    return 0;
}



