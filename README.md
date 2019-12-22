# mini-kernel

This is a simple and basic mini-kernel developed as part of a course-work. The original code was created by [Fernando Perez Costoya](http://laurel.datsi.fi.upm.es/~fperez/) and can be found [here](http://laurel.datsi.fi.upm.es/_media/docencia/asignaturas/soa/practicas/minikernel.tgz). The instructions of the coursework can be found  [here](https://laurel.datsi.fi.upm.es/docencia/asignaturas/dso/practicas/minikernel#el_entorno_de_desarrollo).

## Instructions

To compile the code just go to the source root directory and execute make. The program can be started calling the boot program giving the kernel as an argument.

The file *usuario/init.c* can be edited to select which functions will be executed in each iteration.

```
make
boot/boot minikernel/kernel
```

## Description
### Get process id
A system call that allows us to obtain the id of a running program. Implementing this is quite simple since a variable one will be used where it is stored.

### Sleep
For this second function, we choose to create a new list that collects the BCPs of the blocked processes, similar to what occurs in a ready process. It will also be necessary to modify the structure of the BCPs so that they store the remaining time they have to remain locked.

When the call is invoked, the corresponding BCP field must be adjusted so that it stores the remaining ticks that the process must sleep; then, the process BCP will be modified so that its status becomes blocked and, finally, the process will be removed from the list of ready processes and passed to the list of blocked processes. When a running process is taken out, it will be necessary to change the context.

To be able to update the count of the remaining ticks so that a process wakes up, it will be necessary to activate the clock interruptions, since these are the ones in charge of measuring the passed time. After executing the routine, the previous interruption level will be restored.

### Mutex
The mutex is the component that offers greater flexibility in terms of implementation decisions. The creation of a system static mutex array with the maximum number of mutexes allowed simultaneously in the system has been adopted.

As for the structure of a mutex, these will contain:
* A field with information on the state of the mutex (which will let us know if this position in the array can be used or is already being used).
* A buffer with the name given to the mutex.
* A field to know if it is recursive.
* A field to know if it has been blocked (or the times it has been blocked in case of a recursive mutex).
* A pointer to the process that is currently blocking it.
* A list of processes waiting for the mutex to be unlocked.
* The number of processes that keep said mutex open.
* As identifier of the mutex, its position in the array will be used.

Instead of including a list of the processes that had the mutex open without using it; it was decided to store in the processes what mutex they have opened, creating the corresponding array in the BCP of each process.

Instead of having a global list with the processes that are
waiting for their mutex to be released, the option was chosen for the mutex to store a list of the processes blocked by them, allowing to directly unlock the waiting processes from the mutex.

#### Mutex creation.
The first function related to the mutex is responsible for creating them, doing the following steps:
* Check if the name is the right size (otherwise it will be left with only the first characters)
* Check if there is any mutex already created with that name (in which case it will return an error)
* Search for a free position to store it and mark said mutex as busy, adjusting its variables
* Call the function to open the mutex, which ensures that it can already be used by the process.

When all the mutexes provided by the system are in use, the function to search for a free mutex will return a negative value, so the process will go to a new list of process BCPs waiting for a new mutex to be released. When this happens they will search for a free mutex again, avoiding errors if the mutex is assigned to another process before it can be claimed.

### Opening a mutex
The function intended to open a mutex will call an auxiliary function to find a free position in which to store the mutex in the BCP and, in case of not finding any, return an error. In case no error is returned, the number of processes that have it open will be updated and the mutex identifier will be loaded in the corresponding position of the mutex array of the process BCP.


#### Closing to mutex
In the function of closing mutex, the identifier of the mutex to be closed will be obtained from the process; the account of processes that have the mutex open will be reduced and the position of the mutex array of the BCP of the process in question will be released. If the process that closes it was the one with the mutex locked, the locks count will be passed to 0 and the mutex will be released so that another can take it.

If the mutex happens to have no process that keeps it open associated, the mutex will be destroyed and new mutexes will be allowed to be created in that position of the system array.

#### Mutex lock and unlock
The last function to be implemented will be responsible for locking and unlocking the mutexes. The lock will be responsible for blocking the mutex and will begin by checking that the process has the mutex opened. If the mutex is not being locked by other processes, the summoner will block the mutex and update the blocking account
from 0 to 1. In case of additional blockages for a recursive mutex, the account will continue increasing and will descend with calls to unlock.

However, in case the mutex is being used by another process, the current execution process must be removed and passed to the list of processes waiting to use said mutex, contained in the mutex itself. Note that this is done in a loop to ensure that concurrent access to the mutex does not occur, as explained above.

The implementation of unlocking is much simpler, returning an error in case the process does not have the mutex locked and reducing the block count otherwise. Finally, if the blocking account has reached 0, the process must release the mutex, passing it to one of the processes waiting for it.

### Round-robin planning implementation
To implement the round-robin planning algorithm, you must start by creating a variable responsible for counting the ticks that the current process can remain in execution. If the planner designates a new process to execute, this tick account must be reset to the value of the corresponding constant.

Second, we will proceed to modify the routine of clock interruption to include the function of checking and updating the previous variable. In case the slice has been completely consumed, the software interrupt will be activated expelling the process and putting it in the last position of the ready queue.

There are certain particularities about the implementation that are noteworthy: first, it is observed that the slice will not be consumed if the process executes the role of null process, so the slice check function will only subtract ticks if the corresponding process is ready. It should also be noted that, before activating the software interruption, the process to be ejected is stored in a variable, since there is a possibility that by the time the software interruption takes time to take place, this process has already abandoned the processor.

Finally, it should be noted that the software interrupt sets its priority level to one, causing the scheduler to be non-ejector. Thus, in the case that a call is being processed
the system, it will not be ejected until you exit the kernel and return to user mode.

### Basic operation of the keyboard input
To implement character reading, we will begin by implementing a static array that will serve as a character buffer, with two pointers that will indicate the position in which one must read and write. When characters are received from the keyboard, they will be stored in the writing position, while reading from the reading position when the functions request characters. 
We also add an account of the characters that are stored in the buffer at any given time, preventing non-existent characters from being read.

To be able to store the received characters, we will modify the terminal interrupt treatment routine so that it is responsible for getting the new characters into the array to be filled, at which point you will begin to ignore them. As for reading, a call will be implemented that allows characters to be passed to the processes in order, paying special attention in the case that the array is empty, it will be passed to a list of processes that expect characters, updated when a character is entered by keyboard.