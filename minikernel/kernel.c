/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */
#include <string.h>

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	printk(/*"-> NO HAY LISTOS. ESPERA INT\n"*/ "");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	ticks_restantes  = TICKS_POR_RODAJA;
	proc_expulsar = NULL;
	while (lista_listos.primero==NULL) {
		espera_int();		/* No hay nada que hacer */
	}
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	int prioridad = fijar_nivel_int(NIVEL_2);
	
	char car;
	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL: %c\n", car);

	if (char_disponibles < TAM_BUF_TERM) {
		char_buffer[escribir] = car;
		escribir = (escribir + 1) % TAM_BUF_TERM;
		char_disponibles++;
	
		if (lista_esperando_char.primero != NULL) {
			
			BCPptr proc = lista_esperando_char.primero;	
			eliminar_primero(&lista_esperando_char);
	
			proc->estado = LISTO;
	
			insertar_ultimo(&lista_listos, proc);
		}	

	} else {
		printk("Error al leer: Buffer de lectura de caracteres lleno\n");
	}

	fijar_nivel_int(prioridad);
        return;
}

//funci�n para reducir ticks en rodaja RR y comprobar si se debe expulsar el proceso actual
static void round_robin () {
	if (p_proc_actual->estado == LISTO) {
		ticks_restantes--;
		if (ticks_restantes <= 0) {
			proc_expulsar = p_proc_actual;			
			activar_int_SW();
		}
	}
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){
	int interrupcion=fijar_nivel_int(NIVEL_3);
	printk(/*"-> TRATANDO INT. DE RELOJ\n"*/"");
	actualizar_bloqueados();
	round_robin();
	fijar_nivel_int(interrupcion);
        return;
}

//funci�n para reducir la cuenta en procesos bloqueados y devolver a listo si procede
static void actualizar_bloqueados (){
	BCPptr bcp_act = lista_bloqueados.primero;
	while (bcp_act != NULL) {
		bcp_act->tiempo_bloqueado--;
		BCPptr bcp_aux = bcp_act->siguiente;
		if (bcp_act->tiempo_bloqueado <= 0) {
			eliminar_elem(&lista_bloqueados, bcp_act);
			bcp_act->estado=LISTO;
			insertar_ultimo(&lista_listos, bcp_act);
		}
		bcp_act = bcp_aux;
	}
	return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	int interrupcion = fijar_nivel_int(NIVEL_1);
	printk("-> TRATANDO INT. SW\n");

	if (p_proc_actual != NULL && p_proc_actual == proc_expulsar) {
		
		BCPptr proc = p_proc_actual;		
		int interrupcion = fijar_nivel_int(NIVEL_3);

		eliminar_elem(&lista_listos, proc);
		insertar_ultimo(&lista_listos, proc);		
		p_proc_actual = planificador();
		printk("-> C.CONTEXTO POR FIN DE RODAJA: de %d a %d\n", proc->id, p_proc_actual->id);

		fijar_nivel_int(interrupcion);
		cambio_contexto(&(proc->contexto_regs), &(p_proc_actual->contexto_regs));

	}

	fijar_nivel_int(interrupcion);
	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;
		
		for (int i = 0; i <	NUM_MUT_PROC; i++) {
			p_proc->lista_mutex[i] = -1;
		}
		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

//A�ADIDA: CERRAR MUTEX
int buscar_mutex_proceso(int id) {

	for (int i = 0; i < NUM_MUT_PROC; i++) {
		if (p_proc_actual->lista_mutex[i] == id) {
			return i;
		}	
	}

	return -1;	
}

int cerrar_mutex_aux(int mutexid){	
	int prioridad = fijar_nivel_int(NIVEL_1);

	int mutex_pos = buscar_mutex_proceso(mutexid);
	if (mutex_pos < 0) {
		printk ("Error al cerrar mutex: el proceso %i no contiene el mutex indicado\n", p_proc_actual->id);

		fijar_nivel_int(prioridad);
		return -2;
	}

	p_proc_actual->lista_mutex[mutex_pos] = -1;
	lista_mutex_global[mutexid].cuenta_procesos--;
	if (lista_mutex_global[mutexid].usando == p_proc_actual) {
		lista_mutex_global[mutexid].contador = 0;
		lista_mutex_global[mutexid].usando = NULL;

		if (lista_mutex_global[mutexid].esperando.primero != NULL) {
			BCPptr proc = lista_mutex_global[mutexid].esperando.primero;	
			eliminar_primero(&lista_mutex_global[mutexid].esperando);
	
			proc->estado = LISTO;
	
			insertar_ultimo(&lista_listos, proc);
		}
	}

	if (lista_mutex_global[mutexid].cuenta_procesos == 0) {
		lista_mutex_global[mutexid].activo = 0;
		if (lista_esperando_mutex.primero != NULL) {
			BCPptr proc_aux = lista_esperando_mutex.primero;
			eliminar_primero(&lista_esperando_mutex);
			insertar_ultimo(&lista_listos, proc_aux);
			proc_aux->estado = LISTO;
		}
	}

	fijar_nivel_int(prioridad);	
	return 0;

}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);
	
	for (int i = 0; i < NUM_MUT_PROC; i++) {
		if (p_proc_actual->lista_mutex[i] >= 0) {
			cerrar_mutex_aux(p_proc_actual->lista_mutex[i]);
		}
	}

	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw);

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	//lista mutex a 0
	for (int i = 0; i < NUM_MUT; i++) {
		lista_mutex_global[i].activo = 0;
	}

	//inicializamos buffer de caracteres
	leer = 0;
	escribir = 0;
	char_disponibles = 0;

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}

//funciones a�adidas
int sis_obtener_id_pr(){
        return p_proc_actual->id;
}

int sis_dormir(){

	unsigned int segundos = (unsigned int)leer_registro(1);
		
	int interrupcion = fijar_nivel_int(NIVEL_3);
	
	BCPptr proc = p_proc_actual;	
	proc->estado = BLOQUEADO;
	eliminar_elem(&lista_listos, proc);
	
	proc->tiempo_bloqueado = segundos*TICK;
	insertar_ultimo(&lista_bloqueados, proc);	
	
	p_proc_actual = planificador();
	printk("-> C.CONTEXTO POR BLOQ: de %d a %d\n", proc->id, p_proc_actual->id);
	fijar_nivel_int(interrupcion);
	cambio_contexto(&(proc->contexto_regs), &(p_proc_actual->contexto_regs));
	
	return 0;
}

int get_mutex_libre () {
	for (int i = 0; i < NUM_MUT; i++) {
		if (lista_mutex_global[i].activo == 0) {
			lista_mutex_global[i].activo = 1;
			return i;
		}	
	}
	return -1;
}

int get_mutex_libre_proc () {
	for (int i = 0; i < NUM_MUT_PROC; i++) {
		if (p_proc_actual->lista_mutex[i] < 0) {
			return i;
		}	
	}
	return -1;
}

int abrir_mutex_aux(char* nombre){	
	int prioridad = fijar_nivel_int(NIVEL_1);	

	int mutex_proc_asignar = -1;
	
		mutex_proc_asignar = get_mutex_libre_proc();

		if(mutex_proc_asignar<0){
			printk("Error al abrir mutex: el proceso %i ya tiene todos sus mutex abiertos.\n",p_proc_actual->id);
	
			fijar_nivel_int(prioridad);
			return -1;
		}else{
	
			for (int i = 0; i < NUM_MUT; i++) {
				if (lista_mutex_global[i].activo == 1 &&
						strcmp (lista_mutex_global[i].nombre, nombre) == 0) {

					p_proc_actual->lista_mutex[mutex_proc_asignar] = i;
					lista_mutex_global[i].cuenta_procesos++;
			
					fijar_nivel_int(prioridad);		
					return i;
				}
			}

			printk("Error al abrir mutex: el nombre \"%s\" no corresponde a ning�n mutex activo\n", nombre);
			
			fijar_nivel_int(prioridad);	
			return -2;
		}
}

int sis_crear_mutex(){	
	int prioridad = fijar_nivel_int(NIVEL_1);

	char* nombre = (char*)leer_registro(1);
	int tipo = (int)leer_registro(2);
	int mutex_asignado = -1;
	
	if (strlen(nombre) > (MAX_NOM_MUT - 1)) {
		nombre[MAX_NOM_MUT] = '\0';
	}

	do {
		for (int i = 0; i < NUM_MUT; i++) {
			if (lista_mutex_global[i].activo == 1 &&
					strcmp (lista_mutex_global[i].nombre, nombre) == 0) {
				printk("Error en creaci�n de mutex: el nombre \"%s\" ya esta tomado\n", nombre);

				fijar_nivel_int(prioridad);
				return -1;
			}
		}

		mutex_asignado = get_mutex_libre();
		if (mutex_asignado < 0){
		
			BCPptr proc = p_proc_actual;			
			proc->estado = BLOQUEADO;

			int interrupcion = fijar_nivel_int(NIVEL_3);
				
			eliminar_elem(&lista_listos, proc);
		
			
			insertar_ultimo(&lista_esperando_mutex, proc);	
				
			p_proc_actual = planificador();
			printk("-> C.CONTEXTO POR BLOQ (MUTEX): de %d a %d\n", proc->id, p_proc_actual->id);	
			
			fijar_nivel_int(interrupcion);
			cambio_contexto(&(proc->contexto_regs), &(p_proc_actual->contexto_regs));
		} else {
			strcpy(lista_mutex_global[mutex_asignado].nombre, nombre);
			lista_mutex_global[mutex_asignado].recursivo = tipo;
			lista_mutex_global[mutex_asignado].contador = 0;
			lista_mutex_global[mutex_asignado].cuenta_procesos = 0;
			lista_mutex_global[mutex_asignado].activo = 1;
			lista_mutex_global[mutex_asignado].usando = NULL;
			abrir_mutex_aux(nombre);

		}
	} while (mutex_asignado == -1);	
	
	fijar_nivel_int(prioridad);
	return mutex_asignado;
}

int sis_abrir_mutex(){
	char* nombre = (char*)leer_registro(1);
	return abrir_mutex_aux(nombre);	
}


int sis_cerrar_mutex(){
	unsigned int mutexid = (unsigned int)leer_registro(1);
	return 	cerrar_mutex_aux(mutexid);
}

int sis_lock(){	
	int prioridad = fijar_nivel_int(NIVEL_1);

	unsigned int mutexid = (unsigned int)leer_registro(1);

	if (buscar_mutex_proceso(mutexid) < 0) {
		printk("Error en lock: el mutex no ha sido abierto por el proceso\n", lista_mutex_global[mutexid].nombre);
		fijar_nivel_int(prioridad);
		return -1;
	}

	int aux;
	do {
		aux = 1;
		if (lista_mutex_global[mutexid].usando == NULL) {
			lista_mutex_global[mutexid].usando = p_proc_actual;
			lista_mutex_global[mutexid].contador = 1;
		} else if (lista_mutex_global[mutexid].usando == p_proc_actual && lista_mutex_global[mutexid].recursivo == RECURSIVO) {
			lista_mutex_global[mutexid].contador++;
		} else if (lista_mutex_global[mutexid].usando == p_proc_actual && lista_mutex_global[mutexid].recursivo == NO_RECURSIVO) {
			printk("Error en lock: mutex \"%s\" no es recursivo y ya est� bloqueado\n", lista_mutex_global[mutexid].nombre);
			fijar_nivel_int(prioridad);
			return -2;
		} else {
			aux = 0;
			BCPptr proc = p_proc_actual;
			
			proc->estado = BLOQUEADO;
			

			int interrupcion = fijar_nivel_int(NIVEL_3);	
			eliminar_elem(&lista_listos, proc);
			
			insertar_ultimo(&lista_mutex_global[mutexid].esperando, proc);	
				
			p_proc_actual = planificador();
			printk("-> C.CONTEXTO POR LOCK (MUTEX): de %d a %d\n", proc->id, p_proc_actual->id);	
			
			fijar_nivel_int(interrupcion);
			cambio_contexto(&(proc->contexto_regs), &(p_proc_actual->contexto_regs));
		}
	} while (!aux);

	fijar_nivel_int(prioridad);
	return 0;
}

int sis_unlock(){
	int prioridad = fijar_nivel_int(NIVEL_1);
	
	unsigned int mutexid = (unsigned int)leer_registro(1);

	if (buscar_mutex_proceso(mutexid) < 0) {
		printk("Error en unlock: el mutex no ha sido abierto por el proceso\n", lista_mutex_global[mutexid].nombre);
		fijar_nivel_int(prioridad);
		return -1;
	}

	if (lista_mutex_global[mutexid].usando != p_proc_actual) {
		printk("Error en unlock: el mutex \"%s\" no ha sido bloqueado por el proceso\n", lista_mutex_global[mutexid].nombre);
		fijar_nivel_int(prioridad);
		return -2;
	} else if (lista_mutex_global[mutexid].contador == 0) {
		printk("Error en unlock: error inesperado; el mutex\"%s\" esta bloqueado con valor 0\n", lista_mutex_global[mutexid].nombre);
		fijar_nivel_int(prioridad);
		return -3;
	}

	lista_mutex_global[mutexid].contador--;
	
	if(lista_mutex_global[mutexid].contador == 0) {
		lista_mutex_global[mutexid].usando = NULL;

		if (lista_mutex_global[mutexid].esperando.primero != NULL) {
			BCPptr proc = lista_mutex_global[mutexid].esperando.primero;	
			eliminar_primero(&lista_mutex_global[mutexid].esperando);
		
			proc->estado = LISTO;
		
			insertar_ultimo(&lista_listos, proc);	
		}
	}
	fijar_nivel_int(prioridad);
	return 0;
}

int sis_leer_caracter(){
	int prioridad = fijar_nivel_int(NIVEL_1);
	char aux = -1;

	do {
		if (char_disponibles > 0) {
			aux = char_buffer[leer];
			leer = (leer + 1) % TAM_BUF_TERM;	
			char_disponibles--;
		} else {
			BCPptr proc = p_proc_actual;			
			proc->estado = BLOQUEADO;

			int interrupcion = fijar_nivel_int(NIVEL_3);
				
			eliminar_elem(&lista_listos, proc);
		
			
			insertar_ultimo(&lista_esperando_char, proc);
				
			p_proc_actual = planificador();
			printk("-> C.CONTEXTO POR BLOQ (ESPERANDO CHAR): de %d a %d\n", proc->id, p_proc_actual->id);	
			
			fijar_nivel_int(interrupcion);
			cambio_contexto(&(proc->contexto_regs), &(p_proc_actual->contexto_regs));
		}
	} while (aux < 0);
		
	fijar_nivel_int(prioridad);
	return aux;
}