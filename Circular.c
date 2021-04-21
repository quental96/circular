/***************************************************************************
*
* Circular.c
*
* This program demonstrates many of the functions used with the 
* circular buffer portion of the BI API.
* 
* This function gives the user the ability to run all the BI control
* parameters from the keyboard. The function will prompt the user
* to choose the following control options:
*	G - To Go or Start the circular acquisition
*	S - To Stop the current circular acqusition
*	A - To Abort the current circular acqusition
*	P - To Pause the current circular acquisition
*	C - To Continue after a pause of the circular acqusition
*
* Copyright (C) 2002 by BitFlow, Inc.  All Rights Reserved.
*
***************************************************************************/

#include <stdio.h>
#include <conio.h>
#include "BiApi.h"
#include "DSapi.h"
#include "BFErApi.h"

/* Threads */
UINT WaitForBufferDone(LPVOID lpdwParam);
UINT CirErrorThread(LPVOID lpdwParam);

Bd		Board;
BFBOOL	EndTest = FALSE;
int		hDspSrf = -1;	// handle to display surface
PBFVOID	m_bitmap;
BFBOOL	LookAtBuffers = FALSE; // Set not to check buffers
PBFU32	pSrc32;
PBFU32  pS32;

main()
{
	
	BIBA	BufArray;
	BIRC	ReturnVal;
	MSG		Msg;
	char	ch;
	HANDLE	IntThread, IntThread1;
	DWORD	dwThreadId, dwThreadId1;
	BFU32	ErrorCheck = 1;
	BFU32	xsize,ysize,pixdepthdisplay;
	BFU32	Init, SerNum;

	BFU32	BoardType, BoardNum, NumBuffers;
	BFU32	CirSetupOptions = BiAqEngJ|NoResetOnError;
	BFU32	ErrorMode = CirErStop;
	BFU32	FramesCaptured=0, FramesMissed=0;


	if( DoBrdOpenDialog(TRUE, FF_BITFLOW_MODERN, &BoardType, &BoardNum, &Init, &SerNum) == IDCANCEL )
	{
		return -1;
	}

	/* Open board */
	ReturnVal = BiBrdOpen(BoardType, BoardNum, &Board);
	if(ReturnVal != BI_OK)
	{
		BiErrorShow(Board,ReturnVal);
		return 1;
	}

	printf("The board was successfully opened\n");
	printf("How many buffers would you like to allocate: ");
	scanf_s("%d", &NumBuffers);

	/* Allocate memory for buffers */
	ReturnVal = BiBufferAllocCam(Board, &BufArray, NumBuffers);
	if(ReturnVal != BI_OK)
	{
		BiErrorShow(Board,ReturnVal);
		BiBrdClose(Board);
		return 1;
	}

	/* find out camera info */
	BiBrdInquire(Board, BiCamInqXSize, &xsize);
	BiBrdInquire(Board, BiCamInqYSize0, &ysize);
	BiBrdInquire(Board, BiCamInqBitsPerPixDisplay, &pixdepthdisplay);

	/* create display surface to view what is in the bitmap in buffers */
	if(!DispSurfCreate((PBFS32)&hDspSrf,xsize,ysize,pixdepthdisplay,NULL))
	{
		printf("Couldn't open display surface\n\n");
		EndTest = TRUE;
		return 1;
	}
	
	/* get pointer to bitmap data memory */
	if(!DispSurfGetBitmap(hDspSrf,&m_bitmap))
	{
		printf("No display surface available\n\n");
		EndTest = TRUE;
		return 1;
	}

	/* offset Display surface */
	DispSurfOffset(hDspSrf,100,100);

	/* Display the bitmap */
	if (!DispSurfBlit(hDspSrf))
	{
		printf("Could not update the display surface\n\n");
		EndTest = TRUE;
		return 1;
	}

	/* Update windows */
	if(PeekMessage(&Msg,NULL,0,0,PM_REMOVE))
		DispatchMessage(&Msg);


	/* Setup for circular buffers */
	ReturnVal = BiCircAqSetup(Board, &BufArray, ErrorMode, CirSetupOptions);
	if(ReturnVal != BI_OK)
	{
		BiErrorShow(Board,ReturnVal);
		BiBufferFree(Board,&BufArray);
		BiBrdClose(Board);
		return 1;
	}


	// Create thread to wait for errors
	IntThread1 = CreateThread(BFNULL,0,(LPTHREAD_START_ROUTINE)CirErrorThread,(LPDWORD)&BufArray, 0, &dwThreadId1);
	if (IntThread1 == BFNULL) 
	{
		BiCircCleanUp(Board, &BufArray);
		BiBufferFree(Board, &BufArray);
		BiBrdClose(Board);
		return 1;
	}
	SetThreadPriority(IntThread1, THREAD_PRIORITY_HIGHEST);


	/* Create thread to wait for acquisition done */
	IntThread = CreateThread(BFNULL,0,(LPTHREAD_START_ROUTINE)WaitForBufferDone,(LPDWORD)&BufArray, 0, &dwThreadId);
	if (IntThread == BFNULL) 
	{
		BiCircCleanUp(Board, &BufArray);
		BiBufferFree(Board, &BufArray);
		BiBrdClose(Board);
		return 1;
	}
	SetThreadPriority(IntThread, THREAD_PRIORITY_HIGHEST);


	printf("\nPress G (as in Go) to start Acquisition ");
	printf("	Press S to Stop Acquisition \n");
	printf("Press P to Pause				Press C to Continue\n");
	printf("Press A to Abort\n");
	printf("Press X to exit test\n\n");

	while(!EndTest)
	{
		// Wait here for a keyboard stroke
		while(!BFkbhit() && !EndTest)
		{
			if(PeekMessage(&Msg,NULL,0,0,PM_REMOVE))
				DispatchMessage(&Msg);
			else
				Sleep(10);
		}
	
		if(!EndTest)
			ch = BFgetch();
		else
			ch = 'X';

		switch(toupper(ch))
		{
		case 'G':
			// Start Circular Acquisition
			ReturnVal = BiCirControl(Board, &BufArray, BISTART, BiAsync);
			if(ReturnVal != BI_OK)
			{
				if(ReturnVal < BI_WARNINGS)
				{
					/* Show Error and Shutdown */
					BiErrorShow(Board,ReturnVal);
					BiCircCleanUp(Board, &BufArray);
					BiBufferFree(Board, &BufArray);
					BiBrdClose(Board);
					return 1;
				}
			}
			
			if(ReturnVal == BI_OK)
				printf("Circular Acquisition Started.\n");
		
			break;

		case 'P':
			ReturnVal = BiCirControl(Board, &BufArray, BIPAUSE, BiAsync);
			if(ReturnVal != BI_OK)
			{
				if(ReturnVal < BI_WARNINGS)
				{
					/* Show Error and Shutdown */
					BiErrorShow(Board,ReturnVal);
					BiCircCleanUp(Board, &BufArray);
					BiBufferFree(Board, &BufArray);
					BiBrdClose(Board);
					return 1;
				}
			}

			if(ReturnVal == BI_OK)
				printf("Circular Acquisition Paused\n");

			break;

		case 'C':
			ReturnVal = BiCirControl(Board, &BufArray, BIRESUME, BiAsync);
			if(ReturnVal != BI_OK)
			{
				if(ReturnVal < BI_WARNINGS)
				{
					/* Show Error and Shutdown */
					BiErrorShow(Board,ReturnVal);
					BiCircCleanUp(Board, &BufArray);
					BiBufferFree(Board, &BufArray);
					BiBrdClose(Board);
					return 1;
				}
			}

			if(ReturnVal == BI_OK)
				printf("Circular Acquisition Resumed\n");

			break;

		case 'S':
			ReturnVal = BiCirControl(Board, &BufArray, BISTOP, BiAsync);
			if(ReturnVal != BI_OK)
			{
				if(ReturnVal < BI_WARNINGS)
				{
					/* Show Error and Shutdown */
					BiErrorShow(Board,ReturnVal);
					BiCircCleanUp(Board, &BufArray);
					BiBufferFree(Board, &BufArray);
					BiBrdClose(Board);
					return 1;
				}
			}
			break;
	
		case 'A':
			ReturnVal = BiCirControl(Board, &BufArray, BIABORT, BiAsync);
			if(ReturnVal != BI_OK)
			{
				if(ReturnVal < BI_WARNINGS)
				{
					/* Show Error and Shutdown */
					BiErrorShow(Board,ReturnVal);
					BiCircCleanUp(Board, &BufArray);
					BiBufferFree(Board, &BufArray);
					BiBrdClose(Board);
					return 1;
				}
			}
			break;

		case 'X':
			BiCirControl(Board, &BufArray, BISTOP, BiAsync);
			EndTest = TRUE;
			break;
	
		default:
			printf("Key not Recognized, Try Again\n");
			break;
		}
	}

	BiCaptureStatusGet(Board, &BufArray, &FramesCaptured, &FramesMissed);
	printf("\nMissed %d Frames\n", FramesMissed);
	printf("Captured %d Frames\n", FramesCaptured);
	

	// Look through the Error stack
	printf("Looping through error stack\n");
	while(ErrorCheck != BI_OK)
	{
		ErrorCheck = BiCirErrorCheck(Board, &BufArray);
		BiErrorShow(Board, ErrorCheck);
	}
	
	
	printf("\nPress Any Key to Continue\n");
	while(!BFkbhit())
	{
		/* needed for display window */
		if(PeekMessage(&Msg,NULL,0,0,PM_REMOVE))
			DispatchMessage(&Msg);
		else
			Sleep(0);
	}

	/* absorb key stroke */
	if (BFkbhit()) BFgetch();

	/* Close Display window */
	DispSurfClose(hDspSrf);

	/* Clean things up */
	ReturnVal = BiCircCleanUp(Board, &BufArray);
	if(ReturnVal != BI_OK)
		BiErrorShow(Board,ReturnVal);

	ReturnVal = BiBufferFree(Board, &BufArray);
	if(ReturnVal != BI_OK)
		BiErrorShow(Board,ReturnVal);
		
	/* Close the board */
	ReturnVal = BiBrdClose(Board);
	if(ReturnVal != BI_OK)
		BiErrorShow(Board,ReturnVal);


	return BI_OK;
}


UINT WaitForBufferDone(LPVOID lpdwParam)
{
	BIBA		*pBufArray = (BIBA *)lpdwParam;
	BIRC		ReturnVal = 0xFFFF;
	BiCirHandle	CirHandle;
	BFU32		xsize,ysize,pixdepth,BytesPerPix;
	BFU32		j,k,shift;
	PBFU8		pSrc8,pDest,pD;
	PBFU16		pSrc16,pS16;
	MSG			Msg;
	BFBOOL		Start,Stop,Abort,Pause,Cleanup;


	/* find out camera info */
	BiBrdInquire(Board, BiCamInqXSize, &xsize);
	BiBrdInquire(Board, BiCamInqYSize0, &ysize);
	BiBrdInquire(Board, BiCamInqBitsPerPix, &pixdepth);
	BiBrdInquire(Board, BiCamInqBytesPerPix, &BytesPerPix);


	/*
	* Loop until clean up is called. Don't display image to screen
	* if an error occurs.
	*/
	BiControlStatusGet(Board,pBufArray,&Start,&Stop,&Abort,&Pause,&Cleanup);
	while(!Cleanup)
	{
		/* Wait until the user stops, aborts or pauses acquisition */
		ReturnVal = BiCirWaitDoneFrame(Board, pBufArray, INFINITE, &CirHandle);
		
		/*
		* If BiCirWaitDoneFrame was bumped BiCircCleanUp, don't print any messages.
		*/
		BiControlStatusGet(Board,pBufArray,&Start,&Stop,&Abort,&Pause,&Cleanup);
		if(!Cleanup)
		{
			if(ReturnVal == BI_CIR_ABORTED)
				printf("Acquisition has been aborted\n");
			else if(ReturnVal == BI_CIR_STOPPED)
				printf("Acquisition has been stopped\n");
			else if(ReturnVal == BI_ERROR_CIR_WAIT_TIMEOUT)
				printf("BiSeqWaitDone has timed out\n");
			else if(ReturnVal == BI_ERROR_CIR_WAIT_FAILED)
				printf("The wait in BiSeqWaitDone Failed\n");
			else if(ReturnVal == BI_ERROR_QEMPTY)
				printf("The queue was empty\n");
		
			/* buffer must be full */
			else if(ReturnVal == BI_OK)
			{
				/* 
				* Display the current buffer 
				*/
				
				if(pixdepth == 8 || pixdepth == 16 || pixdepth == 24 || pixdepth == 32)	/* display data as is */
				{	
					pSrc8 = (PBFU8) CirHandle.pBufData;
					memcpy(m_bitmap, pSrc8, ysize*xsize*BytesPerPix);
				}
				else if(pixdepth < 16)	/* display MSB of data */
				{
					pSrc16 = (PBFU16) CirHandle.pBufData;
					pDest = (PBFU8) m_bitmap;

					shift = pixdepth - 8;
					/* copy image to bitmap */
					for(j=0; j<ysize; j++)
					{
						pS16 = pSrc16;
						pD = pDest;
						/* shift MSB down */
						for (k=0; k<xsize; k++)
							*pD++ = (BFU8) ((*pS16++) >> shift);
						pDest += xsize;
						pSrc16 += xsize;
					}
				}
				else if(pixdepth == 30)
				{
					// Initilize source/destination to turn image upside-down for windows 
					pSrc32 = CirHandle.pBufData;
					pDest = (PBFU8) m_bitmap;

					// 30 bit data (3x10 RGB) is packed into a 32 bit word
					// we need to repack it displaying the MSB.
					shift = 2;
					pS32 = pSrc32;
					for(j=0; j<ysize*xsize; j++)
					{
						*pDest = (BFU8)((*pS32 >> 2) & 0x000000FF);
						pDest++;
						*pDest = (BFU8)((*pS32 >> 12) & 0x000000FF);
						pDest++;
						*pDest = (BFU8)((*pS32 >> 22) & 0x000000FF);
						pDest++;
						pS32++;
					}
				}
				else if(pixdepth > 32 && pixdepth <=48)
				{
					// Initilize source/destination to turn image upside-down for windows 
					pSrc16 = (PBFU16) CirHandle.pBufData;
					pDest = (PBFU8) m_bitmap;

					shift = pixdepth % 8;
					for (j=0;j<ysize;j++)
					{
						pS16 = pSrc16;
						pD = pDest;
						// shift MSB down
						for (k=0; k<xsize*3; k++)
							*pD++ = (BFU8) ((*pS16++) >> shift);
						pDest += xsize*3;
						pSrc16 += xsize*3;
					}

				}
				else
				{
					printf("Pixel depth not supported\n\n");
					EndTest = TRUE;
					return 1;
				}


				/* Display the bitmap */
				if (!DispSurfBlit(hDspSrf))
				{
					printf("Could not update the display surface\n\n");
					EndTest = TRUE;
					return 1;
				}

				/* Mark the buffer as AVAILABLE after processing */
				ReturnVal = BiCirStatusSet(Board,pBufArray,CirHandle,BIAVAILABLE);
				if(ReturnVal != BI_OK)
				{
					printf("Error changing buffer status\n\n");
					EndTest = TRUE;
				}

				/* Update windows */
				if(PeekMessage(&Msg,NULL,0,0,PM_REMOVE))
					DispatchMessage(&Msg);

				/* Get Cleanup status */
				BiControlStatusGet(Board,pBufArray,&Start,&Stop,&Abort,&Pause,&Cleanup);

			}/* End if ReturnVal == BI_OK */

		}/* End of if cleaned up */

	}/* End while cleaned up */
	

	return 0;
}


UINT CirErrorThread(LPVOID lpdwParam)
{
	BIBA	*pBufArray = (BIBA *)lpdwParam;
	BIRC	rv;
	BFBOOL	Start,Stop,Abort,Pause,Cleanup;

	BiControlStatusGet(Board,pBufArray,&Start,&Stop,&Abort,&Pause,&Cleanup);
	while(!Cleanup)
	{
		// Wait here until a acquisition error occurs
		rv = BiCirErrorWait(Board, pBufArray);

		/* If a error is returned by BiCirWaitError, stop the program */
		if(rv == BI_ERROR_CIR_ACQUISITION)
		{
			printf("An circular acquisition error has occured.\n\n");
			EndTest = TRUE;
		}

		/* Get Cleanup status */
		BiControlStatusGet(Board,pBufArray,&Start,&Stop,&Abort,&Pause,&Cleanup);

	}

	return 0;
}
