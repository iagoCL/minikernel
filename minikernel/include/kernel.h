/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

//def para mutex
#define NO_RECURSIVO 0
#define RECURSIVO 1

#include "HAL.h"
#include "const.h"
#include "llamsis.h"

//A�ADIDO: para lectura de caracteres
char char_buffer[TAM_BUF_TERM];
int escribir;
int leer;
int char_disponibles;

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct Mutex *Mutex_ptr;

typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
    int id;                         /* ident. del proceso */
    int estado;                     /* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
    contexto_t contexto_regs;       /* copia de regs. de UCP */
    void *pila;                     /* dir. inicial de la pila */
    BCPptr siguiente;               /* puntero a otro BCP */
    void *info_mem;                 /* descriptor del mapa de memoria */
    int tiempo_bloqueado;           //A�ADIDO: ticks de reloj que el proceso espera en caso de bloqueado
    int lista_mutex[NUM_MUT_PROC];  //A�ADIDO: para mutex
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct {
    BCP *primero;
    BCP *ultimo;
} lista_BCPs;

//struct para mutex
typedef struct Mutex_t {
    char nombre[MAX_NOM_MUT];
    int recursivo;
    int contador;
    int cuenta_procesos;
    int activo;  //0 = libre; 1 = en uso
    lista_BCPs esperando;
    BCPptr usando;
} Mutex;

Mutex lista_mutex_global[NUM_MUT];

/*
 * Variable global que identifica el proceso actual
 */

BCP *p_proc_actual = NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos = {NULL, NULL};

//A�ADIDA: lista bloqueados
lista_BCPs lista_bloqueados = {NULL, NULL};

//A�ADIDA: esperando mutex
lista_BCPs lista_esperando_mutex = {NULL, NULL};

//A�ADIDA: esperando char
lista_BCPs lista_esperando_char = {NULL, NULL};

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct {
    int (*fservicio)();
} servicio;

//A�ADIDA: para round robing
int ticks_restantes;
BCPptr proc_expulsar;

/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();

//funciones a�adidas
int sis_obtener_id_pr();
int sis_dormir();
int sis_crear_mutex();
int sis_abrir_mutex();
int sis_lock();
int sis_unlock();
int sis_cerrar_mutex();
int sis_leer_caracter();

static void actualizar_bloqueados();

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS] = {
    {sis_crear_proceso},
    {sis_terminar_proceso},
    {sis_escribir},
    {sis_obtener_id_pr},
    {sis_dormir},
    {sis_crear_mutex},
    {sis_abrir_mutex},
    {sis_lock},
    {sis_unlock},
    {sis_cerrar_mutex},
    {sis_leer_caracter},
};

#endif /* _KERNEL_H */