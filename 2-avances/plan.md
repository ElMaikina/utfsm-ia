# 🚚 Planificación - Proyecto IA

## ️🚦 Resumen

La idea de este proyecto es crear una implementacion del algoritmo _Truck and Trailer Routing Problem (TTRP) (Chao, 2002)_ usando un enfoque **Greedy** e implementando **Tabu Search**.

El proceso general debe contar con los siguientes pasos:

1. Carga de datos: Leerá los datos de entrada entregados por
los ayudantes, parsearlo y convertirlo en una estructura de
datos conveniente.
2. Solución inicial: Genera una solución usando algún algorimo
Greedy. De antemano creo que lo mejor es simplemente elegir
el punto más cercano y agregarlo a la ruta / sub-ruta.
3. Reparador: Tomar la solución inicial e iterar con Tabu-Search
para encontrar una solución mejor. Los movimientos van a ser:
Mover cliente de una ruta a otra, cambiar posición de dos clientes,
cambiar punto de estacionado.
4. Resultados: Mostrar los resultados finales. Compara la función
de evaluación entre la solución Greedy y la optimizada.

El codigo para esta solucion va a estar desarrollado en **C** y va a constar de un unico archivo llamado `ttrp.c`, con su `Makefile` y `README.md` correspondiente.

Los datos de entrada estaran en la carpeta `./datos/instancias`, para el cual existe un `README.md` con el formato explicado. Las instancias empleadas para este codigo son las mismas que se uso en el paper original del 2002.

## 📚 Estructuras

Aqui se definen las estructuras de datos que se van a usar para implementar el algoritmo, fueron elegidas con mucho cuidado con el fin de representar de forma mas precisa el problema a solucionar.

Para guardar los detalles de los clientes:

```
typedef struct {
	// Coordenadas del punto
	float x;
	float y;
	// Si solo puede ser atendida por un camion
	bool solo_camion;
	// Demanda del cliente
	float demanda;
	// Siguiente cliente en la lista
	Cliente *siguiente;
	// Lista de clientes dentro de una sub-ruta (solo para las soluciones)
	Cliente *sub_ruta;
} Cliente;
```

En donde los clientes se guardaran en una lista enlazada, de esta forma la memoria es mas facil de manipular y posteriormente podemos hacer un stack para insertar los clientes en las rutas de forma conveniente. Para la capacidad de la flota que entrega la instancia, la estructuramos asi:

```
typedef struct {
	// Numero de camiones (mk en el paper / instancia)
	int n_camiones;
	// Numero de trailers (mr en el paper / instancia)
	int n_trailers;
	// Capacidad de cada camion (Qk en el paper / instancia)
	int q_camion;
	// Capacidad de cada trailer (Qr en el paper / instancia)
	int q_trailer;
	// Coordenadas de el punto de inicio (deposito)
	float x;
	float y;
} Flota;
```

Luego, las rutas las armamos asi:

```
typedef struct {
	// Si la ruta es recorrida por un vehiculo con trailer
	bool con_trailer;
	// Capacidad total de el vehiculo empleado para la ruta
	float capacidad;
	// Demanda de los clientes cubierta en la ruta
	float demanda;
	// Distancia total recorrida por el vehiculo al atender
	float distancia;
	// Una lista enlazada con los clientes agregados a la ruta
	Cliente *clientes;
	// Centro de la ruta, se actualiza cada vez que se agrega un cliente
	// y toma en consideracion el punto de inicio (deposito)
	float x;
	float y;
} Ruta;
```

En donde la capacidad de el vehiculo que atiente la ruta se debe calcular en base al tipo de ruta y las condiciones iniciales establecidas por la instancia.

Las rutas se crean desde un inicio en base a la cantidad de camiones y trailers disponibles. Primero se crean rutas con trailer para cuantos trailers hayan y posteriormente se crean rutas sin trailer para los camiones restantes (siempre habran mas camiones).

Adicionalmente, agregue las coordenadas centrales de la ruta, de esta forma
insertar nuevos clientes es mas facil ya que no necesitamos re-calcular la distancia de un cliente a cada punto de las rutas; solo se toma el centro de cada una.

Finalmente, las soluciones las guardaremos de esta forma:

```
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
```

## ⌛ Funciones

Para el algoritmo serán necesarias las siguientes funciones:

* **CrearClientes**: Genera la lista enlazada de clientes a partir del input que entregue el ayudante. Recibira la path de la instancia a ocupar, si esta no cumple el formato el programa completo debe fallar.

* **ClonarClientes**: Arma una copia de la lista de clientes para armar la pila de puntos no agregados a la solución sin alterar los datos originales.

* **SacarCliente**: Quita el ultimo cliente de la lista de clientes y lo devuelve. Sirve para agregar clientes a una solucion. Debe aplicarse solo desde la pila y no desde la lista leida original.

* **CrearRutas**: Crea las rutas en base a las condiciones de la instancia. Las rutas se crean vacias y se van llenando dinamicamente.

* **CrearSemillas**: Agrega los clientes iniciales (semillas) a cada ruta. Primero elige el cliente mas lejano al punto inicial, para las ruta siguiente, elige el cliente mas lejano a la semilla anterior y asi se eligen los clientes mas lejanos a las semillas anteriores hasta que todas las rutas tengan un cliente inicial.

* **CostoAsignacion**: Calcula el costo de asignar un nuevo cliente a la ruta. Para que el algoritmo sea Greedy, la calculamos sumando la distancia al centro de la ruta mas el exceso de capacidad, osea si los clientes atendidos (mas el que se va a agregar) sobrepasan la capacidad del vehiculo, se agrega dicho exceso al costo de asignacion. Debe permitir agregar clientes de tipo "solo camion" a rutas con trailer, las cuales deben agregarse como sub-ruta al ultimo cliente agregado. El costo de asignacion debe asumir que si hay una sub-ruta, el camion debe volver al cliente donde esta comenzo para recoger el trailer.

* **AgregarCliente**: Agrega un cliente a una solucion. Se agrega a la ruta que minimice el costo de asignacion. Se debe agregar el cliente a la posicion de la ruta que minimice el costo de asignacion. Debe permitir agregar clientes de tipo "solo camion" a una ruta con trailer, lo cual debe generar una nueva sub-ruta o agregar el cliente a una sub-ruta existente, dependiendo de cual minimice el costo.