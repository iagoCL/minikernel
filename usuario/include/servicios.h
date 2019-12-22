/*
 *
 * Fichero de cabecera que contiene los prototipos de funciones de
 * biblioteca que proporcionan la interfaz de llamadas al sistema.
 *
 */

#ifndef SERVICIOS_H
#define SERVICIOS_H

//def a�adidas
#define NO_RECURSIVO 0
#define RECURSIVO 1

/* Evita el uso del printf de la bilioteca est�ndar */
#define printf escribirf

/* Funcion de biblioteca */
int escribirf(const char *formato, ...);

/* Llamadas al sistema proporcionadas */
int crear_proceso(char *prog);
int terminar_proceso();
int escribir(char *texto, unsigned int longi);

//func a�adidas
int obtener_id_pr();
int dormir(unsigned int segundos);
int crear_mutex(char *nombre, int tipo);
int abrir_mutex(char *nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);
int leer_caracter();

#endif /* SERVICIOS_H */