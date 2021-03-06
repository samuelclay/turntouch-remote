/*********************************************************************
*              SEGGER MICROCONTROLLER SYSTEME GmbH                   *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996-2014 SEGGER Microcontroller Systeme GmbH           *
*                                                                    *
* Internet: www.segger.com Support: support@segger.com               *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : SEGGER_RTT.h
Date    : 17 Dec 2014
Purpose : Implementation of SEGGER real-time terminal which allows
          real-time terminal communication on targets which support
          debugger memory accesses while the CPU is running.
---------------------------END-OF-HEADER------------------------------
*/
#include <stdarg.h>

/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
#define SEGGER_RTT_MODE_MASK                  (3 << 0)

#define SEGGER_RTT_MODE_NO_BLOCK_SKIP         (0)
#define SEGGER_RTT_MODE_NO_BLOCK_TRIM         (1 << 0)
#define SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL    (1 << 1)

#define RTT_CTRL_RESET                "[0;39;49m"

#define RTT_CTRL_CLEAR                "[2J"

#define RTT_CTRL_TEXT_BLACK           "[0;30m"
#define RTT_CTRL_TEXT_RED             "[0;31m"
#define RTT_CTRL_TEXT_GREEN           "[0;32m"
#define RTT_CTRL_TEXT_YELLOW          "[0;33m"
#define RTT_CTRL_TEXT_BLUE            "[0;34m"
#define RTT_CTRL_TEXT_MAGENTA         "[0;35m"
#define RTT_CTRL_TEXT_CYAN            "[0;36m"
#define RTT_CTRL_TEXT_WHITE           "[0;37m"

#define RTT_CTRL_TEXT_BRIGHT_BLACK    "[1;30m"
#define RTT_CTRL_TEXT_BRIGHT_RED      "[1;31m"
#define RTT_CTRL_TEXT_BRIGHT_GREEN    "[1;32m"
#define RTT_CTRL_TEXT_BRIGHT_YELLOW   "[1;33m"
#define RTT_CTRL_TEXT_BRIGHT_BLUE     "[1;34m"
#define RTT_CTRL_TEXT_BRIGHT_MAGENTA  "[1;35m"
#define RTT_CTRL_TEXT_BRIGHT_CYAN     "[1;36m"
#define RTT_CTRL_TEXT_BRIGHT_WHITE    "[1;37m"

#define RTT_CTRL_BG_BLACK             "[40m"
#define RTT_CTRL_BG_RED               "[41m"
#define RTT_CTRL_BG_GREEN             "[42m"
#define RTT_CTRL_BG_YELLOW            "[43m"
#define RTT_CTRL_BG_BLUE              "[44m"
#define RTT_CTRL_BG_MAGENTA           "[45m"
#define RTT_CTRL_BG_CYAN              "[46m"
#define RTT_CTRL_BG_WHITE             "[47m"

#define RTT_CTRL_BG_BRIGHT_BLACK      "[1;40m"
#define RTT_CTRL_BG_BRIGHT_RED        "[1;41m"
#define RTT_CTRL_BG_BRIGHT_GREEN      "[1;42m"
#define RTT_CTRL_BG_BRIGHT_YELLOW     "[1;43m"
#define RTT_CTRL_BG_BRIGHT_BLUE       "[1;44m"
#define RTT_CTRL_BG_BRIGHT_MAGENTA    "[1;45m"
#define RTT_CTRL_BG_BRIGHT_CYAN       "[1;46m"
#define RTT_CTRL_BG_BRIGHT_WHITE      "[1;47m"


/*********************************************************************
*
*       RTT API functions
*
**********************************************************************
*/

int     SEGGER_RTT_Read             (unsigned BufferIndex,       char* pBuffer, unsigned BufferSize);
int     SEGGER_RTT_Write            (unsigned BufferIndex, const char* pBuffer, unsigned NumBytes);
int     SEGGER_RTT_WriteString      (unsigned BufferIndex, const char* s);

int     SEGGER_RTT_GetKey           (void);
int     SEGGER_RTT_WaitKey          (void);
int     SEGGER_RTT_HasKey           (void);

int     SEGGER_RTT_ConfigUpBuffer   (unsigned BufferIndex, const char* sName, char* pBuffer, int BufferSize, int Flags);
int     SEGGER_RTT_ConfigDownBuffer (unsigned BufferIndex, const char* sName, char* pBuffer, int BufferSize, int Flags);

void    SEGGER_RTT_Init             (void);

/*********************************************************************
*
*       RTT "Terminal" API functions
*
**********************************************************************
*/
void    SEGGER_RTT_SetTerminal        (char TerminalId);
int     SEGGER_RTT_TerminalOut        (char TerminalId, const char* s);

/*********************************************************************
*
*       RTT printf functions (require SEGGER_RTT_printf.c)
*
**********************************************************************
*/
int SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...);
int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList);

/*************************** End of file ****************************/
