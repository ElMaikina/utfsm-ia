# 🚚 TTRP — Avance 2

Implementación en C del *Truck and Trailer Routing Problem* (TTRP, Chao 2002).
El diseño completo (estructuras y funciones) está detallado en [`plan.md`](plan.md);
este avance implementa los pasos **1. Carga de datos** y **2. Solución inicial**
de ese plan (el reparador con Tabu Search queda para un avance posterior).

## Compilar

```sh
make
```

Genera el ejecutable `ttrp` (requiere `gcc` y la librería matemática estándar, `-lm`).

Para limpiar el binario generado:

```sh
make clean
```

## Ejecutar

El programa recibe como único argumento el *path relativo* al archivo de
la instancia a resolver:

```sh
./ttrp datos/instancias/ttrp_n050_A25.txt
```

Si el archivo no existe o no cumple el formato descrito en
[`datos/README.md`](datos/README.md), el programa termina con un mensaje de
error y código de salida distinto de cero, sin intentar construir ninguna
solución.

Si la instancia es válida, el programa:

1. Carga y valida la instancia (`CrearClientes`), mostrando un resumen de la
   flota (depósito, número y capacidad de camiones/trailers).
2. Construye una solución inicial con un algoritmo Greedy
   (`CrearRutas` + `CrearSemillas` + `AgregarCliente`, usando `CostoAsignacion`
   como criterio) e imprime, por cada ruta: si usa trailer, su capacidad,
   demanda cubierta, distancia recorrida, el orden de visita de los clientes
   y, si corresponde, sus sub-rutas.
3. Imprime el puntaje total de la solución y si esta resulta factible.

## Sub-rutas

Un cliente marcado como `solo_camion` (tipo `1`/"TC" en la instancia) no
puede ser atendido por la combinación camión+trailer, pero sí puede ser
atendido por una ruta con trailer **como sub-ruta**: el camión deja el
trailer estacionado en un cliente de la ruta principal (el "ancla"), recorre
los clientes TC cercanos y vuelve a ese mismo cliente a recoger el trailer
antes de seguir la ruta principal. Reglas:

- Una sub-ruta solo puede **abrirse** en el último cliente agregado a la
  ruta principal (es el único punto donde el camión+trailer está
  efectivamente parado en ese momento de la construcción).
- Una sub-ruta ya abierta se puede **extender** en cualquier cliente de la
  ruta principal, sin importar si ya quedó "atrás" en el recorrido: ir y
  volver de esa sub-ruta no afecta al resto de la ruta principal.
- `CostoAsignacion` evalúa, para un cliente TC sobre una ruta con trailer,
  el costo marginal (de ida y vuelta) de colgarlo en la mejor ancla
  disponible de esa ruta (abrir una nueva o extender una existente).
  `AgregarCliente` reutiliza ese mismo cálculo para decidir dónde insertarlo
  realmente.
- Si la ruta principal de una ruta con trailer todavía no tiene ningún
  cliente (no hay ancla posible), la asignación es inválida; `CrearSemillas`
  ya evita este caso en la práctica, exigiendo que la primera semilla de toda
  ruta con trailer sea un cliente VC.

## Notas de implementación

- Las distancias son euclidianas, calculadas directamente desde las
  coordenadas de la instancia (no se redondean), como indica
  `datos/README.md`.
- El puntaje de la solución es la distancia total recorrida por todas las
  rutas (que ya incluye las idas y vueltas de cada sub-ruta) más el exceso
  de capacidad de cada ruta. `plan.md` define el puntaje como la suma del
  costo de asignación de cada punto, pero volver a evaluar ese costo contra
  el estado final deja de tener sentido para un cliente que ya forma parte
  de una sub-ruta (la propia estructura que se evalúa); por eso se usa esta
  versión consolidada a nivel de solución, que conserva la misma idea
  (distancia + exceso de capacidad). Esta función queda disponible para que
  el futuro reparador (Tabu Search) compare soluciones.
- El campo `id` de `Cliente` no aparece en `plan.md`, pero es necesario para
  poder identificar al cliente en el orden de visita y en las sub-rutas
  impresas al final.
