/***************************************************************************//**
 * @file     main.c
 * @brief    Demonstrate how to update chip flash data through UART interface
             between chip UART and PC UART.
             Nuvoton NuMicro ISP Programming Tool is also required in this
             sample code to connect with chip UART and assign update file
             of Flash.
 * @version  0x32
 *
 * @note
 * Copyright (C) 2017-2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "targetdev.h"
#include "uart_transfer.h"


void ProcessHardFault(void){}
void SH_Return(void){}


void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    
    /* Enable HIRC clock */
    CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;

    /* Wait for HIRC clock ready */
    while (!(CLK->STATUS & CLK_STATUS_HIRCSTB_Msk));

    /* Set HCLK source to HIRC first */    
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | CLK_CLKSEL0_HCLKSEL_HIRC; 
    
    /* Enable PLL */
    CLK->PLLCTL = CLK_PLLCTL_128MHz_HIRC;

    /* Wait for PLL stable */
    while (!(CLK->STATUS & CLK_STATUS_PLLSTB_Msk));

    /* Select HCLK clock source as PLL and HCLK source divider as 2 */
    CLK->CLKDIV0 = (CLK->CLKDIV0 & (~CLK_CLKDIV0_HCLKDIV_Msk)) | CLK_CLKDIV0_HCLK(2);    
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | CLK_CLKSEL0_HCLKSEL_PLL;
    
    /* Update System Core Clock */
    PllClock        = 128000000;
    SystemCoreClock = 128000000 / 2;
    CyclesPerUs     = SystemCoreClock / 1000000;  /* For SYS_SysTickDelay() */
    
    /* Enable UART module clock */
    CLK->APBCLK0 |= CLK_APBCLK0_UART0CKEN_Msk;
    
    /* Select UART module clock source */
    CLK->CLKSEL1 = (CLK->CLKSEL1 & (~CLK_CLKSEL1_UART0SEL_Msk)) | CLK_CLKSEL1_UART0SEL_HIRC;
    
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    
    /* Set multi-function pins for UART0 RXD and TXD */
    SYS->GPB_MFPH = (SYS->GPB_MFPH & (~(UART0_RXD_PB12_Msk | UART0_TXD_PB13_Msk))) | UART0_RXD_PB12 | UART0_TXD_PB13;
}

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();
    
    /* Init System, peripheral clock and multi-function I/O */
    SYS_Init();
    
    /* Init UART */
    UART_Init();
    
    /* Enable ISP */
    CLK->AHBCLK |= CLK_AHBCLK_ISPCKEN_Msk;
    FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk;
    
    /* Get Secure and Non-secure Flash size */
    g_u32apromSize = BL_EnableFMC();
    g_u32dataFlashAddr = SCU->FNSADDR;

    if (g_u32dataFlashAddr < g_u32apromSize) {
        g_u32dataFlashSize = (g_u32apromSize - g_u32dataFlashAddr);
    } else {
        g_u32dataFlashSize = 0;
    }

    /* Set Systick time-out for 300ms */
    SysTick->LOAD = 300000 * CyclesPerUs;
    SysTick->VAL  = (0x00);
    SysTick->CTRL = SysTick->CTRL | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;   /* Use CPU clock */

    /* Wait for CMD_CONNECT command until Systick time-out */
    while (1)
    {       
        /* Wait for CMD_CONNECT command */        
        if ((u8bufhead >= 4) || (u8bUartDataReady == TRUE))
        {
            uint32_t u32lcmd;
            u32lcmd = inpw(au8uart_rcvbuf);

            if (u32lcmd == CMD_CONNECT)
            {
                goto _ISP;
            }
            else
            {
                u8bUartDataReady = FALSE;
                u8bufhead = 0;
            }
        }

        /* Systick time-out, then go to APROM */
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {
            goto _APROM;
        }
    }

_ISP:

    /* Prase command from master and send response back */
    while (1)
    {
        if (u8bUartDataReady == TRUE)
        {
            u8bUartDataReady = FALSE;       /* Reset UART data ready flag */
            ParseCmd(au8uart_rcvbuf, 64);   /* Parse command from master */
            PutString();                    /* Send response to master */
        }
    }

_APROM:
    
    /* Reset system and boot from APROM */
    FMC_SetVectorPageAddr(FMC_APROM_BASE);
    NVIC_SystemReset();

    /* Trap the CPU */
    while (1);
}

/*** (C) COPYRIGHT 2017-2018 Nuvoton Technology Corp. ***/