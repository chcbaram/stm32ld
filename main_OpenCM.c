// Loader driver

#include "stm32ld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include "serial.h"
#include "type.h"
#include "stm32ld.h"



#define _DEBUG_PRINT_MSG  0


static FILE *OpenCM_fp;
static u32   OpenCM_fpsize;

extern ser_handler stm32_ser_id;



int OpenCM_Cmd_Init( const char *portname, u32 baud );
int OpenCM_Cmd_SendCommand( char *CmdStr );
int OpenCM_Cmd_ReadResponse( char *CmdStr, int TimeOut );
int OpenCM_Cmd_SendCmdRecvResponse( char *CmdStr, char *RecvStr, int TimeOut );

int  OpenCM_WriteFlash( p_read_data read_data_func, p_progress progress_func );
void OpenCM_Wait_ms( int WaitTime );

// ****************************************************************************
// Helper functions and macros

// Get data function
static u32 OpenCM_writeh_read_data( u8 *dst, u32 len )
{
  size_t readbytes = 0;

  if( !feof( OpenCM_fp ) )
  {
    readbytes = fread( dst, 1, len, OpenCM_fp );
  }
  return ( u32 )readbytes;
}

// Progress function
static void OpenCM_writeh_progress( u32 wrote )
{
  unsigned pwrite = ( wrote * 100 ) / OpenCM_fpsize;
  static int expected_next = 10;

  if( pwrite >= expected_next )
  {
    printf( "%d%% ", expected_next );
    expected_next += 10;
  }
}



/*---------------------------------------------------------------------------
     TITLE   : OpenCM_main
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
int OpenCM_main( int argc, const char **argv )
{
	u8 not_flashing=0;
	u8 send_go_command=0;
	u8 boot_mode = 0;
	long baud;
	char RecvStr[100];	 

	errno = 0;
	baud = strtol( argv[ 2 ], NULL, 10 );
	

	if( argc >= 5 && strlen(argv[ 4 ])==1 && strncmp(argv[ 4 ], "1", 1)==0 )
	{
		send_go_command=1;
	}


	if( ( OpenCM_fp = fopen( argv[ 3 ], "rb" ) ) == NULL )
	{
		fprintf( stderr, "Unable to open %s\n", argv[ 3 ] );
		exit( 1 );
	}
	else
	{
		fseek( OpenCM_fp, 0, SEEK_END );
		OpenCM_fpsize = ftell( OpenCM_fp );
		fseek( OpenCM_fp, 0, SEEK_SET );
	}
  
	printf("OpenCM Download Ver 1.0.0 \n");


	while(1)
	{
		//-- Com Port Open
		//
  		if( OpenCM_Cmd_Init( argv[ 1 ], baud ) != TRUE )
  		{
    		fprintf( stderr, "Unable to connect to bootloader\n" );
    		break;;
  		}


  		if( OpenCM_Cmd_SendCmdRecvResponse("AT&LD", RecvStr, 500 ) == TRUE )
  		{
  			printf("Ready To download \n");
  			OpenCM_Wait_ms(5);

  			if( OpenCM_WriteFlash( OpenCM_writeh_read_data, OpenCM_writeh_progress ) != TRUE )
    		{
				fprintf( stderr, "Unable to program FLASH memory.\n" );
				exit( 1 );
			}
			else
			{
				printf("\nFlash OK\n");
				OpenCM_Wait_ms(50);

				RecvStr[0] = 0;
				OpenCM_Cmd_ReadResponse( RecvStr, 500 );
				printf("CheckSum : %s\n", RecvStr);

				if( strncmp(RecvStr, "Success", 7) == 0 )
				{
					if( send_go_command == 1 )
					{
						OpenCM_Wait_ms(200);
						printf("Go Application\n");
						OpenCM_Cmd_SendCommand("AT&GO");
					}
				}

			}

  		}
  		else
  		{
  			fprintf( stderr, "Fail to be ready.\n" );
  		}


		break;
	}



	fclose( OpenCM_fp );
  
  return 0;
}


/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Wait_ms
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
void OpenCM_Wait_ms( int WaitTime )
{
	#ifdef WIN32_BUILD
	Sleep(WaitTime);
	#else
	usleep(WaitTime*1000);
	#endif
}



/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Cmd_WriteBytes
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
int OpenCM_Cmd_WriteBytes( char *CmdStr, int len )
{
	int written_len;

	written_len = ser_write( stm32_ser_id, CmdStr, len );

#if _DEBUG_PRINT_MSG == 1
	printf("Txd : %d\n", written_len);
#endif

	return written_len;
}




/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Cmd_ReadByte
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
char OpenCM_Cmd_ReadByte( void )
{
	return ser_read_byte( stm32_ser_id );
}



/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Cmd_SendCommand
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
int OpenCM_Cmd_SendCommand( char *CmdStr )
{
	int cnt;
	int retry = 3;
	int i;

	cnt = 0;


	i = OpenCM_Cmd_WriteBytes( CmdStr, strlen(CmdStr) );

	/*
	while(1)
	{

		i = ser_write_byte( stm32_ser_id, CmdStr[cnt] );
		printf("Txd : %d\n", i);

		if( CmdStr[cnt] == 0x00 || cnt > 100 )
		{
			break;
		}
		cnt++;
	}
	*/
	if( cnt > 100 ) return STM32_COMM_ERROR;


	return STM32_OK;
}



/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Cmd_ReadResponse
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
int OpenCM_Cmd_ReadResponse( char *CmdStr, int TimeOut )
{
	int cnt;
	int recv = FALSE;
	char Ch;
	int TimeCnt;

	cnt = 0;


	while(1)
	{
		Ch = OpenCM_Cmd_ReadByte();

		//printf("Rxd : %x \n", Ch);
		if( Ch == -1 )
		{
			break;
		}
		else
		if( Ch == 0x0A )
		{
			CmdStr[cnt] = 0;
			recv = TRUE;
			break;
		}
		else
		{
			CmdStr[cnt] = Ch;
		}

		cnt++;
	}

	return recv;
}



/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Cmd_SendCmdRecvResponse
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
int OpenCM_Cmd_SendCmdRecvResponse( char *CmdStr, char *RecvStr, int TimeOut )
{
	int Ret = FALSE;


	#if _DEBUG_PRINT_MSG == 1
	printf("Cmd  : %s\n", CmdStr);
	#endif

	if( OpenCM_Cmd_SendCommand(CmdStr) == STM32_OK )
	{
		if( OpenCM_Cmd_ReadResponse( RecvStr, TimeOut ) == TRUE )
		{
			#if _DEBUG_PRINT_MSG == 1
			printf("Recv : %s\n", RecvStr);
			#endif

			Ret = TRUE;
		}
	}
	else
	{
		printf("Fail to send AT&NAME\n");
	}

	return Ret;	
}



/*---------------------------------------------------------------------------
     TITLE   : OpenCM_Cmd_Init
     WORK    :
     ARG     : void
     RET     : void
---------------------------------------------------------------------------*/
int OpenCM_Cmd_Init( const char *portname, u32 baud )
{
	char buf[100];

	// Open port
	if( ( stm32_ser_id = ser_open( portname ) ) == ( ser_handler )-1 )
	{
		printf("Fail to open port 1\n");
		return STM32_PORT_OPEN_ERROR;
	}
	
	// Setup port
	ser_setup( stm32_ser_id, baud, SER_DATABITS_8, SER_PARITY_NONE, SER_STOPBITS_1 );


	// Flush all incoming data
	ser_set_timeout_ms( stm32_ser_id, SER_NO_TIMEOUT );
	while( OpenCM_Cmd_ReadByte() != -1 );
	ser_set_timeout_ms( stm32_ser_id, STM32_COMM_TIMEOUT );


	//-- Boart Reset
	//
	OpenCM_Cmd_SendCommand("CM9X");
	OpenCM_Wait_ms(1000);

	ser_close( stm32_ser_id );

	// Open port
	if( ( stm32_ser_id = ser_open( portname ) ) == ( ser_handler )-1 )
	{
		printf("Fail to open port 2\n");
		return STM32_PORT_OPEN_ERROR;
	}

	// Setup port
	ser_setup( stm32_ser_id, baud, SER_DATABITS_8, SER_PARITY_NONE, SER_STOPBITS_1 );


	// Flush all incoming data
	ser_set_timeout_ms( stm32_ser_id, SER_NO_TIMEOUT );
	while( OpenCM_Cmd_ReadByte() != -1 );
	ser_set_timeout_ms( stm32_ser_id, STM32_COMM_TIMEOUT );


	//-- 보드 연결 확인
	//
	if( OpenCM_Cmd_SendCmdRecvResponse("AT&NAME\n", buf, 500) == TRUE )
	{
		printf("Board Name : %s\n", buf);
	}
	else
	{
		printf("Fail to connect OpenCM\n");
		return FALSE;
	}


	return TRUE;
}           


#define OPENCM_WRITE_BUFSIZE  1024

int OpenCM_WriteFlash( p_read_data read_data_func, p_progress progress_func )
{
 
	u32 wrote = 0;
	u8  data[ OPENCM_WRITE_BUFSIZE + 1 ];
	u32 datalen;
	u8  CheckSum = 0;
	int i;

	printf("Flash : ");
	
	while( 1 )
	{
		// Read data to program
		if( ( datalen = read_data_func( data, OPENCM_WRITE_BUFSIZE ) ) == 0 )
		{
      		break;
      	}

		for( i=0; i<datalen; i++ )
		{
			CheckSum += data[i];
		}  	
	
	
		//-- 데이터 전송
		//
	  	OpenCM_Cmd_WriteBytes( data, datalen );
	

	    wrote += datalen;
	    if( progress_func )
	    {
	      progress_func( wrote );
	    }
	}

  	data[0] = CheckSum;  	

	OpenCM_Cmd_WriteBytes( data, 1 );


	printf("\nWrite Size : %d\n", wrote);
	return TRUE;
}


