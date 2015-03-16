#include <stdio.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <time.h> 

//using namespace std;

struct ISKRA{
	char totaal[12];
	char hoog[12];
	char laag[12];
	float f_KW_totaal;
	float f_KW_Totaal_oud;
	float f_KW_hoog_Tar;
	float f_KW_laag_Tar;
	long int verbruik;
};

ISKRA ISKRA162;

int main(){
	//declare variables
	//----- TX BYTES -----
	unsigned char tx_buffer[10];
	unsigned char *p_tx_buffer;
	memset(tx_buffer,0,sizeof(tx_buffer));//clear the data
	

	//------- RX BYTES ------
	int run=1;//run the program continuously
	int rx_amount=0;
	char rx_data[1024];
	char *p_rx_data;
	memset(rx_data,0,sizeof(rx_data));//clear the data
	p_rx_data = &rx_data[0]; 
	char rx_buffer[256];
	int step = 1;//goto step 1 to initiate 1st read

	
	time_t now, old;
	int seconds;
	/*time structures*/
	time(&now);  /* get current time; same as: now = time(NULL)  */
	old = now;

//-------------------------
	//----- SETUP USART -----
	//-------------------------
	int uart0_filestream = -1;
	
	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
	//close(uart0_filestream);

	uart0_filestream = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
		run=0;
	}
	
	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	struct termios options;
	tcgetattr(uart0_filestream, &options);
	//options.c_cflag = B300 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	//tcflush(uart0_filestream, TCIFLUSH);
	//tcsetattr(uart0_filestream, TCSANOW, &options);


	while(run>0){
		time(&now);//get time
		seconds = difftime(now, old);//calculate difference

		if (step == 0 && seconds >= 15)//it's been 15 seconds and we finished receiving the last message
		{
			old = now;
			step=1;
			//std::cout<<step<<"\r\n";					//debug
		}
			
			//----- CHECK FOR ANY RX BYTES -----
		if (uart0_filestream != -1)
		{
			// Read up to 255 characters from the port if they are there
			int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
				
			if (rx_length < 0)
			{
				//An error occured (will occur if there are no bytes)
			}
			else if (rx_length == 0)
			{
				//No data waiting
			}
			else
			{
				//Bytes received
				for(int X = 0; X < rx_length; X++){
					*p_rx_data++ = rx_buffer[X];
				}
			}
		}

		switch (step){
		case 0: break;//do nothing until it's time to do something again
		
		case 1:{//set init message
			options.c_cflag = B300 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate
			tcflush(uart0_filestream, TCIFLUSH);
			tcsetattr(uart0_filestream, TCSANOW, &options);

			p_tx_buffer = &tx_buffer[0];
			*p_tx_buffer++ = '/';
			*p_tx_buffer++ = '?';
			*p_tx_buffer++ = '!';
			*p_tx_buffer++ = '\r';
			*p_tx_buffer++ = '\n';
			step++;
			//std::cout<<tx_buffer; 					//debug
			//std::cout<<step<<"\r\n"; 					//debug
			break;
		}
		
		case 2:{//send init message
			if (uart0_filestream != -1){
				int count = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));		//Filestream, bytes to write, number of bytes to write
				if (count < 0)
				{
					printf("UART TX error\n");
					run=0;
				}
				else{
					step++;
					//std::cout<<step<<"\r\n"; 			//debug
				}
			}
			break;
		}
		
		case 3:{//received response so send ack message
			char ACK = 0x06;
			if (strstr(rx_data,"/") && strstr(rx_data,"\n")){
				p_tx_buffer = &tx_buffer[0];
				*p_tx_buffer++ = ACK;
				*p_tx_buffer++ = '0';
				*p_tx_buffer++ = rx_buffer[4];
				*p_tx_buffer++ = '0';
				*p_tx_buffer++ = '\r';
				*p_tx_buffer++ = '\n';
				step++;
				//std::cout<<step<<"\r\n"; 				//debug
			}
			//std::cout<<rx_data; 							//debug
			break;
		}
		
		case 4:{
			if (uart0_filestream != -1){
				int count = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));		//Filestream, bytes to write, number of bytes to write
				if (count < 0)
				{
					printf("UART TX error\n");
					run=0;
				}
				else{
					step++;
					//std::cout<<step<<"\r\n";			//debug
				}
			}
			break;
		}

		case 5:{//we have send an acknowledge so we need to switch to the right speed 
				int speed = rx_data[4];
				//std::cout<<"speed:"<<speed<<"\r\n";		//debug
				switch (speed){
					case 1:{
						options.c_cflag = B600 | CS7 | CLOCAL | CREAD | PARENB;			//<Set baud rate to 600 baud
						tcflush(uart0_filestream, TCIFLUSH);
						tcsetattr(uart0_filestream, TCSANOW, &options);
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;

					}
					case 2:{
						options.c_cflag = B1200 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate to 1200 baud
						tcflush(uart0_filestream, TCIFLUSH);
						tcsetattr(uart0_filestream, TCSANOW, &options);
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;

					}
					case 3:{
						options.c_cflag = B2400 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate to 2400 baud
						tcflush(uart0_filestream, TCIFLUSH);
						tcsetattr(uart0_filestream, TCSANOW, &options);
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;

					}
					case 4:{
						options.c_cflag = B4800 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate to 4800 baud
						tcflush(uart0_filestream, TCIFLUSH);
						tcsetattr(uart0_filestream, TCSANOW, &options);
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;

					}
					case 5:{
						options.c_cflag = B9600 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate to 9600 baud
						tcflush(uart0_filestream, TCIFLUSH);
						tcsetattr(uart0_filestream, TCSANOW, &options);
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;

					}
					case 6:{
						options.c_cflag = B19200 | CS7 | CLOCAL | CREAD | PARENB;		//<Set baud rate to 19200 baud
						tcflush(uart0_filestream, TCIFLUSH);
						tcsetattr(uart0_filestream, TCSANOW, &options);
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;

					}
					default:{
						step++;
						memset(rx_data,0,sizeof(rx_data));	//clear the data
						p_rx_data = &rx_data[0];			//reset the pointer
						break;
					}
				}
				//std::cout<<step<<"\r\n";					//debug
				break;
		}
		case 6:{//recieve the data
			if (strstr(rx_data,"!")){						//see if we got all the data
				memcpy(ISKRA162.totaal,strstr(rx_data,"1.8.0(")+6,sizeof(ISKRA162.totaal)-1);//search the right value and copy it into the structure
				ISKRA162.totaal[11]='\0';					//zero terminate the string so we can printf
				ISKRA162.f_KW_totaal=atof(ISKRA162.totaal);
				memcpy(ISKRA162.hoog,strstr(rx_data,"1.8.2(")+6,sizeof(ISKRA162.hoog)-1);
				ISKRA162.hoog[11]='\0';
				memcpy(ISKRA162.laag,strstr(rx_data,"1.8.1(")+6,sizeof(ISKRA162.laag)-1);
				ISKRA162.laag[11]='\0';
				//*p_rx_data++ = '\n';
				//*p_rx_data++ = '\0';
				//printf(rx_data);
				printf("Totaal verbruik: %skWh\n",ISKRA162.totaal);
				//printf("%.0fWh \n",ISKRA162.f_KW_totaal*1000);
				std::cout<<"Hoog verbruik: "<<ISKRA162.hoog<<"kWh"<<'\n';
				std::cout<<"Laag verbruik: "<<ISKRA162.laag<<"kWh"<<'\n';
				memset(rx_data,0,sizeof(rx_data));//clear the data
				p_rx_data = &rx_data[0];//reset the pointer
				step=0;//go back step 0 and wait for the next one.
			}
			break;
		}
		}
	}
	
		//----- CLOSE THE UART -----
	close(uart0_filestream);
	return 0;
}
