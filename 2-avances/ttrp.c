/*
 * ttrp.c
 * ------
 * Implementacion del Truck and Trailer Routing Problem (TTRP, Chao 2002)
 * usando un enfoque Greedy para construir una solucion inicial.
 *
 * Uso:
 *   ./ttrp <path-relativo-de-la-instancia>
 *
 * El programa:
 *   1. Carga de datos: lee y valida la instancia indicada por argv[1],
 *      si el formato no se cumple el programa falla por completo.
 *   2. Solucion inicial: construye una solucion con un algoritmo Greedy
 *      basado en semillas y en el costo de asignacion al centro de cada
 *      ruta, tal como se describe en plan.md. Los clientes "solo
 *      camion" (TC) pueden agregarse como sub-rutas dentro de una ruta
 *      con trailer: el camion se desengancha del trailer en un cliente
 *      de la ruta principal (el ancla), visita la sub-ruta y vuelve a
 *      ese mismo cliente a recoger el trailer antes de seguir.
 *   3. Imprime el detalle de la solucion generada (rutas, sub-rutas,
 *      distancia, demanda) y su puntaje final.
 *
 * El paso 3 del plan (reparador con Tabu Search) no se implementa en
 * este archivo: aqui solo se cubren los pasos 1 y 2 (carga de datos +
 * solucion inicial, esta ultima ya con soporte de sub-rutas). Ver
 * plan.md y datos/README.md para el detalle del formato de instancia y
 * de las estructuras usadas.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <float.h>

/* ------------------------------------------------------------------ *
 * Estructuras de datos (ver plan.md, seccion "Estructuras")
 * ------------------------------------------------------------------ */

/* Costo "infinito" usado para descartar asignaciones invalidas (por
 * ejemplo, un cliente solo_camion asignado a una ruta con trailer). Se
 * usa en vez de un valor arbitrario para que nunca sea elegido por
 * AgregarCliente mientras exista una ruta valida disponible. */
#define COSTO_INFACTIBLE FLT_MAX

/*
 * Cliente: nodo de una lista enlazada simple.
 *
 * Se agrega `id` (el indice del cliente en la instancia), que no
 * aparece en plan.md pero es necesario para poder identificar a cada
 * cliente al imprimir el orden de visita de las rutas.
 *
 * El campo `sub_ruta` solo se usa una vez que el cliente forma parte de
 * una Solucion: si el cliente es el ancla de una sub-ruta (un punto de
 * la ruta principal donde el camion se desengancha del trailer para
 * atender clientes "solo camion" cercanos), aqui se guarda la lista
 * enlazada de esos clientes, en su orden de visita. En la lista
 * original (CrearClientes) y en la pila de pendientes (ClonarClientes)
 * este campo siempre vale NULL.
 */
typedef struct Cliente {
	int id;                     // Indice del cliente en la instancia (1..n)
	// Coordenadas del punto
	float x;
	float y;
	// Si solo puede ser atendida por un camion (tipo == 1, "TC" en el README)
	bool solo_camion;
	// Demanda del cliente
	float demanda;
	// Siguiente cliente en la lista
	struct Cliente *siguiente;
	// Lista de clientes dentro de una sub-ruta (solo para las soluciones)
	struct Cliente *sub_ruta;
} Cliente;

/*
 * Flota: capacidad y tamano de la flota que entrega la instancia.
 *
 * plan.md declara q_camion/q_trailer como int, pero la instancia los
 * entrega como float (ej. "50.0"), y deben compararse directamente
 * contra `demanda` (float) al calcular capacidades. Por eso se usan
 * float aqui, para no truncar valores ni mezclar tipos al comparar.
 */
typedef struct {
	// Numero de camiones (mk en el paper / instancia)
	int n_camiones;
	// Numero de trailers (mr en el paper / instancia)
	int n_trailers;
	// Capacidad de cada camion (Qk en el paper / instancia)
	float q_camion;
	// Capacidad de cada trailer (Qr en el paper / instancia)
	float q_trailer;
	// Coordenadas de el punto de inicio (deposito)
	float x;
	float y;
} Flota;

/* Ruta: una ruta de reparto, con o sin trailer. */
typedef struct {
	// Si la ruta es recorrida por un vehiculo con trailer
	bool con_trailer;
	// Capacidad total de el vehiculo empleado para la ruta
	float capacidad;
	// Demanda de los clientes cubierta en la ruta
	float demanda;
	// Distancia total recorrida por el vehiculo al atender
	float distancia;
	// Una lista enlazada con los clientes agregados a la ruta. El orden
	// de la lista es tambien el orden de visita usado para calcular
	// `distancia` (deposito -> primer cliente -> ... -> ultimo cliente)
	Cliente *clientes;
	// Centro de la ruta, se actualiza cada vez que se agrega un cliente
	// y toma en consideracion el punto de inicio (deposito)
	float x;
	float y;
} Ruta;

/* Solucion: conjunto de rutas que cubre a todos los clientes. */
typedef struct {
	// Si la solucion es factible
	bool es_factible;
	// Puntaje de la solucion, se calcula usando la funcion de
	// costo de asignacion para todas los puntos de las rutas.
	float puntaje;
	// Cantidad de rutas que tiene la solucion
	int n_rutas;
	// Rutas que tiene la solucion
	Ruta *rutas;
} Solucion;

/* ------------------------------------------------------------------ *
 * Utilidades generales
 * ------------------------------------------------------------------ */

/* Distancia euclidiana entre dos puntos, sin redondear (la matriz de
 * costos no se entrega en las instancias, se calcula a partir de las
 * coordenadas, como indica datos/README.md). */
static float DistanciaEuclidiana(float x1, float y1, float x2, float y2) {
	float dx = x1 - x2;
	float dy = y1 - y2;
	return sqrtf(dx * dx + dy * dy);
}

/* Termina el programa con un mensaje de error. Se usa durante la carga
 * y validacion de la instancia: si el formato no se cumple, el
 * programa completo debe fallar (asi lo pide plan.md). */
static void ErrorFatal(const char *mensaje, const char *detalle) {
	if (detalle != NULL) {
		fprintf(stderr, "Error: %s (%s)\n", mensaje, detalle);
	} else {
		fprintf(stderr, "Error: %s\n", mensaje);
	}
	exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------ *
 * Paso 1: Carga de datos
 * ------------------------------------------------------------------ */

/*
 * CrearClientes
 * -------------
 * Lee el archivo de instancia en `path`, valida su formato segun
 * datos/README.md y construye la lista enlazada de clientes (sin el
 * deposito). Deja en `flota` la capacidad/tamano de la flota y las
 * coordenadas del deposito.
 *
 * Formato esperado:
 *   Qk Qr n mk mr
 *   0 x y 0.0 0            <- deposito (demanda y tipo se ignoran)
 *   i x y demanda tipo     <- un cliente por linea, i = 1..n en orden
 *
 * Si el archivo no existe o el formato no se cumple en algun punto, el
 * programa falla por completo (ErrorFatal no retorna).
 */
static Cliente *CrearClientes(const char *path, Flota *flota) {
	FILE *archivo = fopen(path, "r");
	if (archivo == NULL) {
		ErrorFatal("no se pudo abrir el archivo de instancia", path);
	}

	// Primera linea: Qk Qr n mk mr
	int n;
	if (fscanf(archivo, "%f %f %d %d %d", &flota->q_camion, &flota->q_trailer,
	           &n, &flota->n_camiones, &flota->n_trailers) != 5) {
		fclose(archivo);
		ErrorFatal("la primera linea no tiene el formato 'Qk Qr n mk mr'", path);
	}
	if (n <= 0) {
		fclose(archivo);
		ErrorFatal("el numero de clientes (n) debe ser positivo", path);
	}
	if (flota->n_camiones <= 0) {
		fclose(archivo);
		ErrorFatal("el numero de camiones (mk) debe ser positivo", path);
	}
	if (flota->n_trailers < 0 || flota->n_trailers > flota->n_camiones) {
		fclose(archivo);
		ErrorFatal("el numero de trailers (mr) es invalido respecto a mk", path);
	}
	if (flota->q_camion <= 0.0f || flota->q_trailer < 0.0f) {
		fclose(archivo);
		ErrorFatal("las capacidades Qk/Qr deben ser positivas", path);
	}

	// Segunda linea: el deposito. Su demanda y tipo se ignoran (README).
	int id_deposito, tipo_deposito;
	float demanda_deposito;
	if (fscanf(archivo, "%d %f %f %f %d", &id_deposito, &flota->x, &flota->y,
	           &demanda_deposito, &tipo_deposito) != 5) {
		fclose(archivo);
		ErrorFatal("la linea del deposito no tiene el formato esperado", path);
	}
	if (id_deposito != 0) {
		fclose(archivo);
		ErrorFatal("la linea del deposito debe comenzar con indice 0", path);
	}

	// Lineas siguientes: un cliente por linea "i x y demanda tipo".
	Cliente *lista = NULL;   // cabeza de la lista
	Cliente *ultimo = NULL;  // cola de la lista (para agregar en orden)
	for (int i = 1; i <= n; i++) {
		int id, tipo;
		float x, y, demanda;
		if (fscanf(archivo, "%d %f %f %f %d", &id, &x, &y, &demanda, &tipo) != 5) {
			fclose(archivo);
			ErrorFatal("linea de cliente mal formada o faltante", path);
		}
		if (id != i) {
			fclose(archivo);
			ErrorFatal("los clientes deben estar numerados de forma consecutiva (1..n)", path);
		}
		if (demanda < 0.0f) {
			fclose(archivo);
			ErrorFatal("la demanda de un cliente no puede ser negativa", path);
		}
		if (tipo != 0 && tipo != 1) {
			fclose(archivo);
			ErrorFatal("el tipo de cliente debe ser 0 (VC) o 1 (TC)", path);
		}

		Cliente *nuevo = malloc(sizeof(Cliente));
		if (nuevo == NULL) {
			fclose(archivo);
			ErrorFatal("no hay memoria suficiente para crear los clientes", NULL);
		}
		nuevo->id = id;
		nuevo->x = x;
		nuevo->y = y;
		nuevo->demanda = demanda;
		nuevo->solo_camion = (tipo == 1);
		nuevo->siguiente = NULL;
		nuevo->sub_ruta = NULL;

		if (lista == NULL) {
			lista = nuevo;
		} else {
			ultimo->siguiente = nuevo;
		}
		ultimo = nuevo;
	}

	fclose(archivo);
	return lista;
}

/* ------------------------------------------------------------------ *
 * Paso 2: Solucion inicial (Greedy)
 * ------------------------------------------------------------------ */

/*
 * ClonarClientes
 * --------------
 * Arma una copia independiente de la lista de clientes, para usarla
 * como la pila de clientes que aun no se han agregado a la solucion,
 * sin alterar la lista original que entrego CrearClientes.
 */
static Cliente *ClonarClientes(const Cliente *lista) {
	Cliente *clon = NULL;
	Cliente *ultimo = NULL;

	for (const Cliente *actual = lista; actual != NULL; actual = actual->siguiente) {
		Cliente *nuevo = malloc(sizeof(Cliente));
		if (nuevo == NULL) {
			ErrorFatal("no hay memoria suficiente para clonar los clientes", NULL);
		}
		*nuevo = *actual;          // copia id, x, y, demanda, solo_camion
		nuevo->siguiente = NULL;
		nuevo->sub_ruta = NULL;    // la pila no tiene sub-rutas, son de la solucion

		if (clon == NULL) {
			clon = nuevo;
		} else {
			ultimo->siguiente = nuevo;
		}
		ultimo = nuevo;
	}

	return clon;
}

/*
 * SacarCliente
 * ------------
 * Quita el cliente que esta en el tope de la pila (la cabeza de la
 * lista, para que la operacion sea O(1)) y lo devuelve. Solo debe
 * usarse sobre la pila construida con ClonarClientes, nunca sobre la
 * lista original que entrega CrearClientes. Devuelve NULL si la pila
 * ya esta vacia.
 */
static Cliente *SacarCliente(Cliente **pila) {
	if (*pila == NULL) {
		return NULL;
	}

	Cliente *tope = *pila;
	*pila = tope->siguiente;
	tope->siguiente = NULL;
	return tope;
}

/*
 * CrearRutas
 * ----------
 * Crea el arreglo de rutas vacias segun las condiciones de la
 * instancia: primero se crean rutas con trailer (una por cada trailer
 * disponible) y luego rutas sin trailer para los camiones restantes
 * (siempre hay mas camiones que trailers). La capacidad de cada ruta
 * depende de su tipo:
 *   - con trailer: capacidad = Qk + Qr (camion mas trailer)
 *   - sin trailer: capacidad = Qk      (solo camion)
 * Todas las rutas parten vacias, con su centro en el deposito.
 */
static Ruta *CrearRutas(const Flota *flota) {
	int n_rutas = flota->n_camiones;
	Ruta *rutas = malloc((size_t) n_rutas * sizeof(Ruta));
	if (rutas == NULL) {
		ErrorFatal("no hay memoria suficiente para crear las rutas", NULL);
	}

	for (int i = 0; i < n_rutas; i++) {
		rutas[i].con_trailer = (i < flota->n_trailers);
		rutas[i].capacidad = flota->q_camion + (rutas[i].con_trailer ? flota->q_trailer : 0.0f);
		rutas[i].demanda = 0.0f;
		rutas[i].distancia = 0.0f;
		rutas[i].clientes = NULL;
		rutas[i].x = flota->x;
		rutas[i].y = flota->y;
	}

	return rutas;
}

/* Recalcula el centro de una ruta como el promedio de las coordenadas
 * del deposito, de todos los clientes de la ruta principal y de todos
 * los clientes dentro de sus sub-rutas (el deposito siempre se
 * considera, como pide plan.md; las sub-rutas se consideran porque
 * tambien son atendidas por el vehiculo de esta ruta). */
static void ActualizarCentroRuta(Ruta *ruta, const Flota *flota) {
	float suma_x = flota->x;
	float suma_y = flota->y;
	int n = 1;   // el deposito cuenta como un punto mas

	for (Cliente *c = ruta->clientes; c != NULL; c = c->siguiente) {
		suma_x += c->x;
		suma_y += c->y;
		n++;
		for (Cliente *s = c->sub_ruta; s != NULL; s = s->siguiente) {
			suma_x += s->x;
			suma_y += s->y;
			n++;
		}
	}

	ruta->x = suma_x / (float) n;
	ruta->y = suma_y / (float) n;
}

/* Agrega `cliente` al final de la secuencia de visita de `ruta` (queda
 * como el ultimo cliente visitado) y actualiza su demanda, la
 * distancia recorrida y su centro. La usan tanto CrearSemillas como
 * AgregarCliente, ya que ambas terminan insertando un cliente en una
 * ruta de la misma forma. */
static void InsertarEnRuta(Ruta *ruta, Cliente *cliente, const Flota *flota) {
	Cliente *ultimo = ruta->clientes;

	if (ultimo == NULL) {
		// Ruta vacia: el tramo recorrido viene directo desde el deposito.
		ruta->distancia += DistanciaEuclidiana(flota->x, flota->y, cliente->x, cliente->y);
		ruta->clientes = cliente;
	} else {
		while (ultimo->siguiente != NULL) {
			ultimo = ultimo->siguiente;
		}
		ruta->distancia += DistanciaEuclidiana(ultimo->x, ultimo->y, cliente->x, cliente->y);
		ultimo->siguiente = cliente;
	}
	cliente->siguiente = NULL;

	ruta->demanda += cliente->demanda;
	ActualizarCentroRuta(ruta, flota);
}

/*
 * CrearSemillas
 * -------------
 * Agrega el cliente inicial (semilla) de cada ruta, sacandolo de la
 * pila `pila`. La primera semilla es el cliente mas lejano al deposito;
 * cada semilla siguiente es el cliente mas lejano a la semilla
 * anterior (no al deposito), tal como lo describe plan.md.
 *
 * Si la ruta es con trailer, solo se consideran como candidatas las
 * clientes que no sean solo_camion. No es que un TC no pueda ir en una
 * ruta con trailer (ahora puede, como sub-ruta), sino que la semilla
 * se vuelve el primer cliente de la ruta principal, y una sub-ruta
 * necesita un cliente de la ruta principal ya puesto como ancla antes
 * de poder existir. Si no queda ningun candidato VC para una ruta con
 * trailer, esa ruta simplemente queda sin semilla y se llenara mas
 * adelante con AgregarCliente si corresponde (si eso ocurre, cualquier
 * TC que le llegue despues no tendra ancla disponible todavia).
 *
 * `mejor` se saca de la pila a mano (no con SacarCliente) porque
 * SacarCliente solo puede sacar el tope de la pila, y la semilla puede
 * estar en cualquier posicion de ella.
 */
static void CrearSemillas(Ruta *rutas, int n_rutas, Cliente **pila, const Flota *flota) {
	float ref_x = flota->x;
	float ref_y = flota->y;

	for (int i = 0; i < n_rutas; i++) {
		Cliente *mejor = NULL;
		Cliente *mejor_anterior = NULL;
		float mejor_distancia = -1.0f;

		Cliente *anterior = NULL;
		for (Cliente *actual = *pila; actual != NULL; actual = actual->siguiente) {
			bool compatible = !(rutas[i].con_trailer && actual->solo_camion);
			if (compatible) {
				float d = DistanciaEuclidiana(ref_x, ref_y, actual->x, actual->y);
				if (d > mejor_distancia) {
					mejor_distancia = d;
					mejor = actual;
					mejor_anterior = anterior;
				}
			}
			anterior = actual;
		}

		if (mejor == NULL) {
			continue;
		}

		// Se desengancha `mejor` de la pila.
		if (mejor_anterior == NULL) {
			*pila = mejor->siguiente;
		} else {
			mejor_anterior->siguiente = mejor->siguiente;
		}

		InsertarEnRuta(&rutas[i], mejor, flota);

		// La siguiente semilla se busca respecto a esta, no al deposito.
		ref_x = mejor->x;
		ref_y = mejor->y;
	}
}

/*
 * Busca, dentro de `ruta`, el punto donde conviene mas "colgar" a
 * `cliente` (un solo_camion) como parte de una sub-ruta, y devuelve la
 * distancia MARGINAL que eso agrega (no la distancia total de la
 * sub-ruta, solo lo que aumenta al agregar este cliente).
 *
 * Una sub-ruta se arma dejando el trailer en un cliente de la ruta
 * principal (el ancla) y recorriendo con el camion solo los clientes
 * "solo camion" que cuelgan de ese ancla, para volver despues al
 * mismo ancla a recoger el trailer. Por eso hay dos formas validas de
 * colgar un cliente nuevo:
 *
 *   (a) Extender una sub-ruta que ya existe en cualquier cliente de la
 *       ruta principal: no importa si ese cliente quedo "atras" en el
 *       recorrido, porque ir y volver de su sub-ruta no afecta al
 *       resto de la ruta principal.
 *   (b) Abrir una sub-ruta nueva, pero solo en el ULTIMO cliente
 *       agregado a la ruta principal: es el unico punto donde el
 *       camion+trailer esta actualmente, asi que es el unico lugar
 *       donde recien se puede dejar el trailer y empezar un desvio.
 *
 * Se evaluan todas las anclas validas (todas las que ya tienen
 * sub-ruta, mas el ultimo cliente de la ruta principal) y se devuelve
 * el costo de la mas barata. Si `ancla_elegida` no es NULL, ahi se
 * deja un puntero a esa ancla (lo usa InsertarEnSubRuta para saber
 * donde colgar realmente al cliente).
 *
 * Devuelve COSTO_INFACTIBLE si la ruta principal todavia esta vacia
 * (no hay ningun ancla posible).
 */
static float CostoMarginalSubRuta(const Cliente *cliente, const Ruta *ruta, Cliente **ancla_elegida) {
	if (ruta->clientes == NULL) {
		if (ancla_elegida != NULL) {
			*ancla_elegida = NULL;
		}
		return COSTO_INFACTIBLE;
	}

	float mejor_costo = COSTO_INFACTIBLE;
	Cliente *mejor_ancla = NULL;

	for (Cliente *ancla = ruta->clientes; ancla != NULL; ancla = ancla->siguiente) {
		bool es_el_ultimo = (ancla->siguiente == NULL);
		bool tiene_sub_ruta = (ancla->sub_ruta != NULL);

		// Solo se puede abrir una sub-ruta nueva en el ultimo cliente;
		// una ya existente se puede extender en cualquier ancla.
		if (!es_el_ultimo && !tiene_sub_ruta) {
			continue;
		}

		// Punto desde el que se llegaria a `cliente`: el ultimo cliente
		// de la sub-ruta si ya existe, o el ancla misma si es nueva.
		const Cliente *origen = ancla;
		float vuelta_previa = 0.0f;
		if (tiene_sub_ruta) {
			origen = ancla->sub_ruta;
			while (origen->siguiente != NULL) {
				origen = origen->siguiente;
			}
			// Se descuenta el tramo de vuelta al ancla que ya estaba
			// contado, porque el nuevo cliente se inserta antes de el.
			vuelta_previa = DistanciaEuclidiana(origen->x, origen->y, ancla->x, ancla->y);
		}

		float costo = DistanciaEuclidiana(origen->x, origen->y, cliente->x, cliente->y)
		            + DistanciaEuclidiana(cliente->x, cliente->y, ancla->x, ancla->y)
		            - vuelta_previa;

		if (costo < mejor_costo) {
			mejor_costo = costo;
			mejor_ancla = ancla;
		}
	}

	if (ancla_elegida != NULL) {
		*ancla_elegida = mejor_ancla;
	}
	return mejor_costo;
}

/*
 * CostoAsignacion
 * ---------------
 * Calcula el costo de asignar `cliente` a `ruta`, mas el exceso de
 * capacidad que se produciria si el cliente se agregara (0 si no se
 * supera la capacidad de la ruta completa, incluyendo sus sub-rutas).
 * Esto hace que el algoritmo sea Greedy: siempre se prefiere la opcion
 * mas cercana, pero penalizando sobrecargar un vehiculo.
 *
 * La parte de la distancia depende del caso:
 *   - Cliente VC, o cliente TC en una ruta sin trailer: la distancia
 *     entre el cliente y el centro de la ruta (como antes).
 *   - Cliente TC (solo_camion) en una ruta con trailer: la distancia
 *     marginal de colgarlo como sub-ruta en la mejor ancla disponible
 *     (ver CostoMarginalSubRuta), que ya asume que el camion debe
 *     volver al cliente donde comenzo la sub-ruta para recoger el
 *     trailer. Si la ruta principal todavia esta vacia (no hay ancla),
 *     la asignacion es invalida y se devuelve COSTO_INFACTIBLE.
 */
static float CostoAsignacion(const Cliente *cliente, const Ruta *ruta) {
	float distancia_extra;

	if (ruta->con_trailer && cliente->solo_camion) {
		distancia_extra = CostoMarginalSubRuta(cliente, ruta, NULL);
		if (distancia_extra >= COSTO_INFACTIBLE) {
			return COSTO_INFACTIBLE;
		}
	} else {
		distancia_extra = DistanciaEuclidiana(cliente->x, cliente->y, ruta->x, ruta->y);
	}

	float demanda_total = ruta->demanda + cliente->demanda;
	float exceso = demanda_total > ruta->capacidad ? demanda_total - ruta->capacidad : 0.0f;

	return distancia_extra + exceso;
}

/* Agrega `cliente` (un solo_camion) como sub-ruta de `ruta`, colgandolo
 * de la ancla que minimiza el costo (la misma que encuentra
 * CostoMarginalSubRuta) y actualiza la demanda, la distancia y el
 * centro de la ruta. Solo debe llamarse cuando ya se sabe que la ruta
 * tiene al menos un cliente principal (ver AgregarCliente). */
static void InsertarEnSubRuta(Ruta *ruta, Cliente *cliente, const Flota *flota) {
	Cliente *ancla = NULL;
	CostoMarginalSubRuta(cliente, ruta, &ancla);

	Cliente *ultimo_sub = ancla->sub_ruta;
	if (ultimo_sub == NULL) {
		// Primera vez que se usa esta ancla: ida y vuelta completas.
		ruta->distancia += DistanciaEuclidiana(ancla->x, ancla->y, cliente->x, cliente->y)
		                  + DistanciaEuclidiana(cliente->x, cliente->y, ancla->x, ancla->y);
		ancla->sub_ruta = cliente;
	} else {
		while (ultimo_sub->siguiente != NULL) {
			ultimo_sub = ultimo_sub->siguiente;
		}
		float vuelta_previa = DistanciaEuclidiana(ultimo_sub->x, ultimo_sub->y, ancla->x, ancla->y);
		ruta->distancia += DistanciaEuclidiana(ultimo_sub->x, ultimo_sub->y, cliente->x, cliente->y)
		                  + DistanciaEuclidiana(cliente->x, cliente->y, ancla->x, ancla->y)
		                  - vuelta_previa;
		ultimo_sub->siguiente = cliente;
	}
	cliente->siguiente = NULL;

	ruta->demanda += cliente->demanda;
	ActualizarCentroRuta(ruta, flota);
}

/*
 * AgregarCliente
 * --------------
 * Agrega `cliente` a la solucion, en la ruta que minimice
 * CostoAsignacion. Dentro de esa ruta, si el cliente es "solo camion"
 * y la ruta tiene trailer, se agrega como sub-ruta (nueva o existente,
 * la que resulte mas barata); en cualquier otro caso se agrega como un
 * cliente mas de la ruta principal.
 *
 * Caso extremo: si para este cliente ninguna ruta resulto valida (por
 * ejemplo, un TC sin ninguna ruta sin trailer disponible y sin ningun
 * ancla todavia en las rutas con trailer), se elige igualmente la
 * primera ruta y se agrega a su lista principal; la solucion quedara
 * marcada como no factible al evaluarla.
 */
static void AgregarCliente(Solucion *solucion, Cliente *cliente, const Flota *flota) {
	int mejor_indice = 0;
	float mejor_costo = CostoAsignacion(cliente, &solucion->rutas[0]);

	for (int i = 1; i < solucion->n_rutas; i++) {
		float costo = CostoAsignacion(cliente, &solucion->rutas[i]);
		if (costo < mejor_costo) {
			mejor_costo = costo;
			mejor_indice = i;
		}
	}

	Ruta *elegida = &solucion->rutas[mejor_indice];
	if (elegida->con_trailer && cliente->solo_camion && elegida->clientes != NULL) {
		InsertarEnSubRuta(elegida, cliente, flota);
	} else {
		InsertarEnRuta(elegida, cliente, flota);
	}
}

/* Agrega a cada ruta no vacia el tramo final de regreso al deposito,
 * de forma que `distancia` represente el recorrido completo (deposito
 * -> clientes -> deposito), tal como pide plan.md: "Distancia total
 * recorrida por el vehiculo al atender [los clientes]". */
static void CerrarRutas(Ruta *rutas, int n_rutas, const Flota *flota) {
	for (int i = 0; i < n_rutas; i++) {
		if (rutas[i].clientes == NULL) {
			continue;
		}

		Cliente *ultimo = rutas[i].clientes;
		while (ultimo->siguiente != NULL) {
			ultimo = ultimo->siguiente;
		}
		rutas[i].distancia += DistanciaEuclidiana(ultimo->x, ultimo->y, flota->x, flota->y);
	}
}

/*
 * Evalua la solucion ya construida.
 *
 * plan.md define el puntaje como "la funcion de costo de asignacion
 * para todas los puntos de las rutas", es decir, distancia mas exceso
 * de capacidad. Esa definicion funcionaba bien cuando cada cliente se
 * podia evaluar de forma independiente contra el centro de su ruta,
 * pero con sub-rutas eso ya no tiene sentido: CostoAsignacion busca la
 * mejor ancla disponible en toda la ruta, y volver a llamarla sobre un
 * cliente que ya esta colgado de una ancla (parte de la propia
 * estructura que se esta evaluando) no da un numero significativo.
 *
 * Por eso aqui el puntaje se calcula a nivel de toda la solucion, con
 * la misma idea de "distancia mas exceso de capacidad" pero ya
 * consolidada: la suma de la distancia real recorrida por cada ruta
 * (que ya incluye, gracias a InsertarEnRuta/InsertarEnSubRuta, las
 * idas y vueltas de cada sub-ruta) mas el exceso de capacidad de cada
 * ruta (0 si no la supera). Esta es la funcion de evaluacion que luego
 * podra usar el reparador (Tabu Search) para comparar soluciones.
 *
 * La solucion es factible si ninguna ruta supera su capacidad y ningun
 * cliente solo_camion quedo en la ruta principal de una ruta con
 * trailer (eso solo pasaria en el caso extremo sin ancla disponible
 * que describe AgregarCliente; un solo_camion dentro de una sub-ruta
 * es, en cambio, el caso normal y no afecta la factibilidad).
 */
static void EvaluarSolucion(Solucion *solucion) {
	float distancia_total = 0.0f;
	float exceso_total = 0.0f;
	bool factible = true;

	for (int i = 0; i < solucion->n_rutas; i++) {
		Ruta *ruta = &solucion->rutas[i];

		distancia_total += ruta->distancia;
		if (ruta->demanda > ruta->capacidad) {
			exceso_total += ruta->demanda - ruta->capacidad;
			factible = false;
		}

		for (Cliente *c = ruta->clientes; c != NULL; c = c->siguiente) {
			if (ruta->con_trailer && c->solo_camion) {
				factible = false;
			}
		}
	}

	solucion->puntaje = distancia_total + exceso_total;
	solucion->es_factible = factible;
}

/*
 * ConstruirSolucionInicial
 * ------------------------
 * Orquesta el algoritmo Greedy completo descrito en plan.md:
 *   1. Clona la lista de clientes en la pila de pendientes.
 *   2. Crea las rutas vacias segun la flota de la instancia.
 *   3. Agrega las semillas (un cliente inicial por ruta).
 *   4. Vacia la pila, agregando cada cliente restante a la ruta que
 *      minimice su costo de asignacion.
 *   5. Cierra las rutas (regreso al deposito) y evalua la solucion.
 */
static Solucion ConstruirSolucionInicial(Cliente *clientes, const Flota *flota) {
	Solucion solucion;
	solucion.n_rutas = flota->n_camiones;
	solucion.rutas = CrearRutas(flota);

	Cliente *pila = ClonarClientes(clientes);

	CrearSemillas(solucion.rutas, solucion.n_rutas, &pila, flota);

	Cliente *actual;
	while ((actual = SacarCliente(&pila)) != NULL) {
		AgregarCliente(&solucion, actual, flota);
	}

	CerrarRutas(solucion.rutas, solucion.n_rutas, flota);
	EvaluarSolucion(&solucion);

	return solucion;
}

/* ------------------------------------------------------------------ *
 * Paso 4 (parcial): impresion de resultados
 * ------------------------------------------------------------------ */

static void ImprimirSolucion(const Solucion *solucion) {
	printf("=== Solucion inicial (Greedy) ===\n\n");

	for (int i = 0; i < solucion->n_rutas; i++) {
		const Ruta *ruta = &solucion->rutas[i];

		printf("Ruta %d [%s] - capacidad: %.2f, demanda: %.2f, distancia: %.2f\n",
		       i + 1, ruta->con_trailer ? "camion+trailer" : "solo camion",
		       ruta->capacidad, ruta->demanda, ruta->distancia);

		if (ruta->clientes == NULL) {
			printf("  (sin clientes)\n");
		} else {
			printf("  Orden de visita: deposito");
			for (Cliente *c = ruta->clientes; c != NULL; c = c->siguiente) {
				printf(" -> %d", c->id);
			}
			printf(" -> deposito\n");

			// Sub-rutas: tramos donde el camion deja el trailer en un
			// cliente de la ruta principal para atender clientes
			// "solo camion" cercanos, y vuelve a ese mismo cliente.
			bool con_sub_rutas = false;
			for (Cliente *c = ruta->clientes; c != NULL; c = c->siguiente) {
				if (c->sub_ruta != NULL) {
					if (!con_sub_rutas) {
						printf("  Sub-rutas (trailer estacionado):\n");
						con_sub_rutas = true;
					}
					printf("    En cliente %d: %d", c->id, c->id);
					for (Cliente *s = c->sub_ruta; s != NULL; s = s->siguiente) {
						printf(" -> %d", s->id);
					}
					printf(" -> %d\n", c->id);
				}
			}
		}
		printf("\n");
	}

	printf("Puntaje total (distancia recorrida + exceso de capacidad): %.4f\n", solucion->puntaje);
	printf("Solucion factible: %s\n", solucion->es_factible ? "si" : "no");
}

/* ------------------------------------------------------------------ *
 * Liberacion de memoria
 * ------------------------------------------------------------------ */

static void LiberarClientes(Cliente *lista) {
	while (lista != NULL) {
		Cliente *siguiente = lista->siguiente;
		LiberarClientes(lista->sub_ruta);   // libera la sub-ruta anidada, si tiene
		free(lista);
		lista = siguiente;
	}
}

static void LiberarSolucion(Solucion *solucion) {
	for (int i = 0; i < solucion->n_rutas; i++) {
		LiberarClientes(solucion->rutas[i].clientes);
	}
	free(solucion->rutas);
}

/* ------------------------------------------------------------------ *
 * Programa principal
 * ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Uso: %s <path-relativo-de-la-instancia>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Paso 1: carga de datos (falla todo el programa si el formato no
	// se cumple, dentro de CrearClientes).
	Flota flota;
	Cliente *clientes = CrearClientes(argv[1], &flota);

	printf("Instancia cargada: %s\n", argv[1]);
	printf("  Deposito:  (%.2f, %.2f)\n", flota.x, flota.y);
	printf("  Camiones:  %d (capacidad %.2f c/u)\n", flota.n_camiones, flota.q_camion);
	printf("  Trailers:  %d (capacidad %.2f c/u)\n", flota.n_trailers, flota.q_trailer);
	printf("\n");

	// Paso 2: solucion inicial (Greedy).
	Solucion solucion = ConstruirSolucionInicial(clientes, &flota);
	ImprimirSolucion(&solucion);

	LiberarSolucion(&solucion);
	LiberarClientes(clientes);

	return EXIT_SUCCESS;
}
