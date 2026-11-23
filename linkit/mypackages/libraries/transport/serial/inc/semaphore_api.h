#ifndef SEMAPHORE_API_H
#define SEMAPHORE_API_H

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <time.h>
#include <signal.h>

//#include <ossdn_global.h>

//semaphore
#define SEM_PERMS				(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define INITIAL_VALUE			1


//static uint8_t g_ui8_debug      = 0;                //debug flag

//semaphore function

/**
 *  @brief   Create Semaphore
 *			 Create Semaphore Set LOCK and UNLOCK Mechanism
 *
 *  @param instrument_sem_name Semaphore name specific to instrument
 *
 *  @return  On Success 0 and error code otherwise.
 *
*/
int8_t create_sem(uint8_t *);

/**
 *
 *  @brief   Lock a Semaphore
 *			 Lock a Semaphore and decrement semaphore value to zero
 *
 *  @return  On Success returns 0 and error code otherwise.
 *
 */
int8_t wait_sem();

/**
 *  @brief   Unlock a Semaphore
 *			 Unlock semaphore and increment semaphore value to 1
 *
 *  @return  return void
 *
 */
int post_sem();

/**
 *  @brief   destroy Semaphore
 * 			 destroy Semaphore and remove it
 *
 *  @return  On Success return 0 and error code otherwise.
 *
 */
int8_t destroy_sem();
#endif
