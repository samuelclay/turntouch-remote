/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x18000, LENGTH = 0x1D000
  RAM (rwx) :  ORIGIN = 0x20002000, LENGTH = 0x2000
}

INCLUDE "gcc_nrf51_common.ld"


/*

   text	   data	    bss	    dec	    hex	filename
  23732	    256	   3232	  27220	   6a54	build/nrf51422_xxac.out

   text	   data	    bss	    dec	    hex	filename
  24464	    256	   3260	  27980	   6d4c	build/nrf51422_xxac.out

   text	   data	    bss	    dec	    hex	filename
  39924	    252	   3900	  44076	   ac2c	build/nrf51.out

*/