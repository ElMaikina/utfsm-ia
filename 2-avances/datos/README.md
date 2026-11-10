# Instancias TTRP — Proyecto IA 2026-2

35 instancias en 7 tamaños (n = 10, 20, 30, 50, 75, 100, 150), 5 por tamaño.

## Formato (el del enunciado)
    Qk Qr n mk mr
    0 x y 0.0 0                    <- depósito (demanda y tipo se ignoran)
    i x y demanda tipo             <- un cliente por línea; tipo: 1 = TC, 0 = VC
La matriz de costos no se entrega: c_ij es la distancia euclidiana entre coordenadas.

## Nombres: `ttrp_n<n>_<geometría><%TC>.txt`
- Geometría **A**: TC = 25, 50 y 75 % (misma ubicación y demanda de clientes; cambia solo qué clientes son TC).
- Geometría **B**: TC = 25 y 75 %.
- Los TC se eligen como en Chao (2002): el floor(p·n) de clientes con menor distancia a su vecino más cercano.

## Origen
- **Directas de Chao (2002)**, sin cambiar coordenadas, demandas, tipos ni capacidades: n=50 (A, Chao 1-3), n=75 (A, Chao 4-6),
  n=100 (A, Chao 7-9; B, Chao 19 y 21), n=150 (A, Chao 10-12). Solo se agregan `mk` y `mr` (Lin et al., 2009, Tabla 2) a la primera línea.
- **Derivadas** (n=10, 20, 30 y las B de 50, 75 y 150): submuestra aleatoria con semilla fija de clientes de una instancia de Chao.
  Flota y capacidades recalculadas para que la razón demanda/capacidad total quede en ~0,9–0,93, como en Chao.

## Instancias para comparar con la literatura
Solo las 14 instancias directas de Chao (2002) tienen resultados publicados contra los cuales comparar. Las otras 21
(n=10, 20, 30 y las geometrías B de n=50, 75 y 150) son para desarrollo y depuración: **no tienen valor de referencia**.

Referencia: Lin, Yu y Chou (2009), *Solving the truck and trailer routing problem based on a simulated annealing heuristic*,
Computers & Operations Research 36(5), Tabla 3. "Mejor conocida" es la mejor solución reportada hasta esa fecha;
"SA mín./prom." son los resultados de su SA en 10 ejecuciones.

| Archivo | Chao | Mejor conocida | SA mín. / prom. |
|---|---|---|---|
| ttrp_n050_A25 | 1 | 564,68 | 566,82 / 568,86 |
| ttrp_n050_A50 | 2 | 611,53 | 612,75 / 617,48 |
| ttrp_n050_A75 | 3 | 618,04 | 618,04 / 620,50 |
| ttrp_n075_A25 | 4 | 798,53 | 808,84 / 817,71 |
| ttrp_n075_A50 | 5 | 839,62 | 839,62 / 858,95 |
| ttrp_n075_A75 | 6 | 930,64 | 934,11 / 942,60 |
| ttrp_n100_A25 | 7 | 830,48 | 830,48 / 838,50 |
| ttrp_n100_A50 | 8 | 872,56 | 875,76 / 882,70 |
| ttrp_n100_A75 | 9 | 912,02 | 912,64 / 921,97 |
| ttrp_n100_B25 | 19 | 813,30 | 814,07 / 823,01 |
| ttrp_n100_B75 | 21 | 909,06 | 909,06 / 915,38 |
| ttrp_n150_A25 | 10 | 1039,07 | 1053,90 / 1074,38 |
| ttrp_n150_A50 | 11 | 1093,57 | 1093,57 / 1108,88 |
| ttrp_n150_A75 | 12 | 1154,73 | 1155,44 / 1166,59 |

Consideraciones al comparar:
- Los valores son de 2009; existen resultados posteriores (por ejemplo, Pérez-Rodríguez y Frausto-Hernández, 2023) que pueden ser mejores.
- La distancia es euclidiana a partir de las coordenadas, sin redondear, como en el enunciado.