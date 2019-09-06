#include <project.h>

#define USBFS_DEVICE  (0u)
/* Active endpoints of USB device. */
#define IN_EP_NUM     (1u)
#define OUT_EP_NUM    (2u)

/* Place your initialization/startup code here (e.g. MyInst_Start()) */
    /* Defines for DMA_1 */
#define DMA_1_BYTES_PER_BURST 1
#define DMA_1_REQUEST_PER_BURST 1
#define DMA_1_SRC_BASE (CYDEV_PERIPH_BASE)
#define DMA_1_DST_BASE (CYDEV_SRAM_BASE)

/* Variable declarations for DMA_1 */
/* Move these variable declarations to the top of the function */
uint8 DMA_1_Chan;
uint8 DMA_1_TD[2]; 

    /* Defines for DMA_2 */
#define DMA_2_BYTES_PER_BURST 1
#define DMA_2_REQUEST_PER_BURST 1
#define DMA_2_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_2_DST_BASE (CYDEV_PERIPH_BASE)

/* Variable declarations for DMA_2 */
/* Move these variable declarations to the top of the function */
uint8 DMA_2_Chan;
uint8 DMA_2_TD[2];

uint8 first[64];
uint8 second[64];
uint8 third[64];
uint8 fourth[64];

uint8 dma_1_flag = 1;
uint8 dma_2_flag = 1;

uint8 data_loss = 0;

CY_ISR(DMA1)
{
	//Once we enter this ISR we can flip the flag to say which buffer to fill
    dma_1_flag = dma_1_flag ^ 1;
	
	//Check for DATA LOSS by checking if we go into this ISR twice before a full transfer occurs
    data_loss++;
    if(data_loss >= 2)
    {
        Control_Reg_1_Write(0);   
    } else {
        Control_Reg_1_Write(1);   
    }
    DMA1_ISR_ClearPending();
}

int main()
{
	//The length which should always be 64. Depends on the input data from the Pi to PSoC
    uint16 length;
    CyGlobalIntEnable;
	
    VDAC8_1_Start();    
    ADC_DelSig_1_Start();
    ADC_DelSig_1_StartConvert();
    DMA1_ISR_StartEx(DMA1);
    /* Start USBFS operation with 5V operation. */
    USBFS_Start(USBFS_DEVICE, USBFS_5V_OPERATION);

    /* Wait until device is enumerated by host. */
    while (0u == USBFS_GetConfiguration())
    {
    }

    /* Enable OUT endpoint to receive data from host. */
    USBFS_EnableOutEP(OUT_EP_NUM);
    
    /* DMA Configuration for DMA_1 */
    DMA_1_Chan = DMA_1_DmaInitialize(DMA_1_BYTES_PER_BURST, DMA_1_REQUEST_PER_BURST, 
        HI16(DMA_1_SRC_BASE), HI16(DMA_1_DST_BASE));
    DMA_1_TD[0] = CyDmaTdAllocate();
    DMA_1_TD[1] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_1_TD[0], 64, DMA_1_TD[1], DMA_1__TD_TERMOUT_EN | CY_DMA_TD_INC_DST_ADR);
    CyDmaTdSetConfiguration(DMA_1_TD[1], 64, DMA_1_TD[0], DMA_1__TD_TERMOUT_EN | CY_DMA_TD_INC_DST_ADR);
    CyDmaTdSetAddress(DMA_1_TD[0], LO16((uint32)ADC_DelSig_1_DEC_SAMP_PTR), LO16((uint32)&first[0]));
    CyDmaTdSetAddress(DMA_1_TD[1], LO16((uint32)ADC_DelSig_1_DEC_SAMP_PTR), LO16((uint32)&second[0]));
    CyDmaChSetInitialTd(DMA_1_Chan, DMA_1_TD[0]);
    
    
    /* DMA Configuration for DMA_2 */
    DMA_2_Chan = DMA_2_DmaInitialize(DMA_2_BYTES_PER_BURST, DMA_2_REQUEST_PER_BURST, 
        HI16(DMA_2_SRC_BASE), HI16(DMA_2_DST_BASE));
    DMA_2_TD[0] = CyDmaTdAllocate();
    DMA_2_TD[1] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_2_TD[0], 64, DMA_2_TD[1], CY_DMA_TD_INC_SRC_ADR);
    CyDmaTdSetConfiguration(DMA_2_TD[1], 64, DMA_2_TD[0], CY_DMA_TD_INC_SRC_ADR );
    CyDmaTdSetAddress(DMA_2_TD[0], LO16((uint32)&third[0]), LO16((uint32)VDAC8_1_Data_PTR));
    CyDmaTdSetAddress(DMA_2_TD[1], LO16((uint32)&fourth[0]), LO16((uint32)VDAC8_1_Data_PTR));
    CyDmaChSetInitialTd(DMA_2_Chan, DMA_2_TD[0]);
    
    CyDmaChEnable(DMA_1_Chan, 1);
    CyDmaChEnable(DMA_2_Chan, 1);
    CyDelay(10);
    for(;;)
    {
		//Set an initial length and ensure that the USB configuration hasn't changed
        length = 64;
        if (USBFS_IsConfigurationChanged())
        {
            if (USBFS_GetConfiguration())
            {
                
                USBFS_EnableOutEP(OUT_EP_NUM);
            }
        }
		
		//If the Pi has sent a "go" signal start the program
        if (USBFS_OUT_BUFFER_FULL == USBFS_GetEPState(OUT_EP_NUM))
        {
            // Read the number of bytes from the OUT
            length = USBFS_GetEPCount(OUT_EP_NUM);
			
			//Check the dma_2_flag to check and see what buffer to fill up
            if(dma_2_flag == 0)
            {
               USBFS_ReadOutEP(OUT_EP_NUM, third, length); 
            } else {
               USBFS_ReadOutEP(OUT_EP_NUM, fourth, length);
            }
            

            //Wait till the Out buffer no longer has data to read
            while (USBFS_OUT_BUFFER_FULL == USBFS_GetEPState(OUT_EP_NUM))
            {
            }
			//Flip the DMA flag to choose which buffer to write to next
            dma_2_flag = dma_2_flag ^ 1;
            //Enable the out once again.
            USBFS_EnableOutEP(OUT_EP_NUM);
        
            // Wait till we have some data to read in the IN buffer
            while (USBFS_IN_BUFFER_EMPTY != USBFS_GetEPState(IN_EP_NUM))
            {
            }

			//Check the flag from the DMA ISR and write to the Pi from the correct buffer(either first or second)
            if(dma_2_flag == 0)
            {
                USBFS_LoadInEP(IN_EP_NUM, first, length);
            } else {
                USBFS_LoadInEP(IN_EP_NUM, second, length);
            }
			
			//Reset data loss if this entire process has happened before the DMA has entered the ISR twice
            data_loss = 0;
        }
    }
}


/* [] END OF FILE */
