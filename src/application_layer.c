#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "application_layer.h"
#include "link_layer.h"
#include <fcntl.h>


#define BUF_SIZE 256

//Controlo 
#define DADOS (0X01)
#define START (0X02)
#define END (0X03)

#define SIZE_FILE (0X00);
#define NAME_FILE (0X01);

extern int total_bytes_read;
extern unsigned char* final_content;
extern int seq_num;
extern int seq_num_expected;
extern int fd;
int seq_number = 0;
int num_read = 0;



void switch_seq() {
	if (seq_num == 1) {
		seq_num = 0;
	}
	else {
		seq_num = 1;
	}
}

void switch_expected() {
	if (seq_num_expected == 1) {
		seq_num_expected = 0;
	}
	else {
		seq_num_expected = 1;
	}
}


void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename){

    LinkLayer connectionParameters;
    LinkLayerRole r;

    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    strcpy(connectionParameters.serialPort, serialPort);

     fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(argv[1]);
        exit(-1);
    }
    

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
        fseek(pinguim, 0L, SEEK_END);
        long unsigned int tamanho_file = ftell(pinguim);
        fseek(pinguim, 0L, SEEK_SET);
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


        for(int i = 3; i <= (2 + number_octetos); i++){
            pacote_controlo[i] = tamanho_file;
            tamanho_file = tamanho_file << 8;
        }
        pacote_controlo[ 2 + number_octetos + 1] = NAME_FILE;
        pacote_controlo[ 2 + number_octetos + 2] = strlen(filename);
        for(int i = 0 ; i < strlen(filename);i++){
            pacote_controlo[2+number_octetos+3+i] = filename[i];
        }
        llwrite(pacote_controlo,sizeof(pacote_controlo));
        switch_seq();
        sleep(1);

        //envio de dados -----------------------------------------
        unsigned char pacote_dados[1500];
        unsigned char dados_efetivos[1496];
        while (!feof(pinguim)){
            pacote_dados[0] = DADOS;
            pacote_dados[1] = seq_number; // ver melhor
            int len = fread(dados_efetivos,1,1496,pinguim);
            printf("Numero de dados enviados: %d \n",len);
	        unsigned char l1 = len % 256;
            unsigned char l2 = len / 256;
            pacote_dados[2] = l2;
            pacote_dados[3] = l1;
            memcpy(&pacote_dados[4],dados_efetivos,sizeof(dados_efetivos));
            llwrite(pacote_dados,1500);
            switch_seq();
            sleep(1);
            
            num_read = len;
            
            printf("\n\n\n\n\n\n\n\n\n\n\n pacote_dados[2]: %d, pacote_dados[3]: %d\n\n\n\n\n\n\n\n\n\n\n", pacote_dados[2], pacote_dados[3]);
        }

        fclose(pinguim);
        //pacote controlo End--------------------------------------
        pacote_controlo[0] = END;
        //switch_seq();
        //llwrite(pacote_controlo,sizeof(pacote_controlo));
        //switch_seq();
        sleep(1);
        free(pacote_controlo);

        llclose(0);
    }
    
    else{
        FILE *pinguim2 = fopen(*filename,"wb");
        int check = 0;
        unsigned char res[2000];
        int a = 0;
        while (check == 0) {
            check = llread(res);
            if (check == 3) {
                check = 0;
            }
            else if (check == 0) {
            	a++;
            	int final_size = 256 * (int) final_content[2] + (int) final_content[3];
            	
            	unsigned char final[1496];
            	
            	printf("\n\n\n\n\n");
                printf("SIZE: %d\n", 256 * (int) final_content[2] + (int) final_content[3]);
                printf("\n\n\n\n\n");
                
                if (final_content[0] == START){
                    switch_expected();
                    switch_arq();
                    check = 0;
                    continue;
                }
                if (final_content[0] == END){
                    switch_expected();
                    switch_arq();
                    check = 0;
                    continue;
                }
            	
                int acum = 0;
                for (int i = 4; i < total_bytes_read - 6; i++) {
                	final[acum] = final_content[i];
                	acum++;
                }

                printf("RECEIVED: ");
                for (int i = 0; i < final_size; i++) {
                    printf("%x", final[i]);
                }
                printf("\n");
                
                
                printf("\n\n\n\n\n");
                printf("ACUM: %d\n", acum);
                printf("\n\n\n\n\n");
                
                
                fwrite(final,1,final_size, pinguim2);
                	
                
                switch_expected();
                switch_arq();
                
            }
            else if (check == 1) {
            	llread(res);
            }
            else if (check == 6) {
            	check = 0;
            }
        }
        fclose(pinguim2);
    }
    return;
}
