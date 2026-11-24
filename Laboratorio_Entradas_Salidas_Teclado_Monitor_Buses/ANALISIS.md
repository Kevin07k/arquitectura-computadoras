# Análisis del práctico: Entradas, salidas, teclado, monitor y buses

## 1. Objetivo

El práctico estudia cómo se intercambia información entre CPU, memoria y periféricos. Para ello se utilizan programas en C que representan la escritura y lectura de una cola de teclado, el acceso a memoria de video y las transferencias a través de los buses del sistema.

El desarrollo toma como referencia el documento **Laboratorio de Arquitectura de Computadoras: Entradas, Salidas y Memoria de Video (VRAM)**. El orden de presentación de las evidencias es: escritor (`prog1`), lector (`prog2`), monitor (`monitor_linux`) y simulador de buses (`buses`).

**Alcance de las evidencias:** las cuatro imágenes fueron proporcionadas como enlaces de GitHub. Al redactar este archivo no fue posible recuperar su contenido. Se conservan los enlaces originales y el orden indicado; el análisis se basa en el código de la práctica, sin atribuir a las capturas opciones, caracteres o valores que no pudieron comprobarse. Los ejemplos numéricos siguientes son explicativos.

## 2. Programa 1: escritura en la cola de teclado

![Figura 1. Ejecución de prog1, según el orden proporcionado](https://github.com/user-attachments/assets/4c950568-9639-491b-880a-ca2e8e886317)

*Figura 1. Evidencia proporcionada para el programa escritor.*

El archivo `prog1_escribir_cola.c` ofrece tres métodos de entrada: inyección mediante `TIOCSTI`, simulación del servicio BIOS `INT 16h / AH=05h` y escritura directa sobre la estructura que representa la BIOS Data Area (BDA).

La BDA se conserva en un archivo, por defecto `/tmp/bios_keyboard_bda.bin`. Esto permite que el escritor termine y que otro proceso lea después la información. El archivo contiene una representación compacta de los punteros, las palabras del teclado y los registros de puerto simulados; no es una imagen completa del segmento físico de BIOS.

Cada tecla ocupa una palabra de 16 bits:

```c
palabra = ((uint16_t)scan_code << 8) | (uint8_t)ascii;
```

El byte bajo representa el código ASCII y el alto representa el scan-code. Por ejemplo, con ASCII `A = 0x41` y scan-code `0x1E`, la palabra resultante es `0x1E41`. Ambos códigos tienen funciones diferentes: ASCII identifica el carácter y el scan-code representa una tecla según una convención de teclado.

Los punteros `head` y `tail` recorren los offsets pares desde `0x001E` hasta `0x003C`. El avance es de dos bytes porque cada entrada es una palabra. Después de la última posición se vuelve al inicio.

```c
next_tail = (tail >= 0x003C) ? 0x001E : tail + 2;
```

La cola está vacía cuando `head == tail` y llena cuando `next_tail == head`. Aunque existen 16 celdas físicas, este esquema permite almacenar **15 teclas**, porque reserva una posición para distinguir ambos estados. No utiliza el contador de la práctica anterior.

La opción de BIOS muestra el ensamblador equivalente y reproduce su efecto sobre la cola mediante C; no ejecuta una interrupción BIOS real desde Linux. La escritura directa también trabaja sobre la BDA simulada. En cambio, `TIOCSTI` intenta actuar sobre la entrada del terminal del sistema operativo y puede estar deshabilitado por el kernel. Su fallo no demuestra un error de la cola simulada: se trata de mecanismos distintos.

## 3. Programa 2: lectura del teclado y la memoria

![Figura 2. Ejecución de prog2, según el orden proporcionado](https://github.com/user-attachments/assets/1cf2ecc5-b1cc-4fe5-bbdd-edac0d9fde71)

*Figura 2. Evidencia proporcionada para el programa lector.*

El archivo `prog2_leer_teclado.c` permite comparar cinco caminos de lectura: `scanf`, `getc`, puerto simulado `0x60`, acceso directo a la BDA y simulación de `INT 16h / AH=00h`.

`scanf` y `getc` leen el flujo estándar `stdin`. El primero obtiene una palabra y el segundo un carácter. Por sí solos no consumen la cola almacenada en el archivo BDA. Para recuperar lo insertado con las opciones 2 o 3 del escritor se utilizan las opciones 4 o 5 del lector.

La lectura directa obtiene la palabra apuntada por `head` y separa sus dos componentes:

```c
ascii = palabra & 0xFF;
scan_code = palabra >> 8;
```

Después avanza `head`, dejando disponible esa posición para futuras inserciones. La lectura BIOS simulada realiza una extracción equivalente y presenta el resultado como registros de CPU: `AL` contiene ASCII, `AH` contiene el scan-code y `AX` contiene la palabra completa. Estos nombres describen la interfaz BIOS reproducida por el programa; no indican que se hayan modificado registros físicos mediante una interrupción real.

La lectura de puerto consulta el bit 0 del estado en `0x64`. Si está activo, obtiene el scan-code de `0x60` y limpia el estado para evitar presentar repetidamente el mismo dato como pendiente.

**Leer el puerto y extraer de la cola son operaciones independientes en esta implementación.** La lectura del puerto no avanza `head`. Asimismo, la cola puede almacenar varias teclas, mientras que el registro de datos representa solamente el último scan-code depositado por el escritor. No se modela toda la temporización ni el comportamiento del controlador físico 8042.

La relación entre ambos programas demuestra persistencia entre procesos: puede ejecutarse primero el escritor y luego el lector en una sola terminal. Deben usar la misma ruta BDA y ejecutarse secuencialmente; el código no incorpora bloqueo para actualizaciones simultáneas.

## 4. Programa 3: escritura en memoria de video

![Figura 3. Ejecución del monitor, según el orden proporcionado](https://github.com/user-attachments/assets/7e0f1236-2e85-480e-ae95-1433e6844cd8)

*Figura 3. Evidencia proporcionada para el programa de monitor.*

El archivo `escribir_monitor_linux.c` presenta cuatro opciones: framebuffer `/dev/fb0`, consola virtual `/dev/vcsa`, ejemplo de acceso a `/dev/mem` y VRAM didáctica mediante `mmap`.

La opción de framebuffer intenta mapear memoria del dispositivo y dibujar un cuadrado de 100×100 píxeles. Su funcionamiento depende de que exista un framebuffer compatible y de que el proceso tenga permiso de acceso. El programa comprueba el formato requerido y los límites del área mapeada.

La opción de consola virtual utiliza celdas de texto compuestas por carácter y atributo. `/dev/vcsa` incluye una cabecera de cuatro bytes; después de ella se escriben los pares de datos de pantalla. Este recurso corresponde a consolas virtuales de Linux y no equivale a cualquier ventana de terminal gráfica.

La opción `/dev/mem` presenta el código explicativo de C y ensamblador del PDF. **No ejecuta ese acceso a memoria física.** Por lo tanto, su salida demuestra la explicación del mecanismo, no una escritura real en la dirección VGA `0xB8000`.

La opción didáctica crea `vram_monitor.bin` y lo mapea con `mmap` y `MAP_SHARED`. Representa una pantalla de 10 filas por 40 columnas mediante celdas de dos bytes:

```c
typedef struct {
    uint8_t ascii;
    uint8_t attribute;
} CeldaVram;
```

El tamaño de esta VRAM es:

```text
10 filas × 40 columnas × 2 bytes = 800 bytes
```

La posición se calcula con `fila * 40 + columna`. La escritura mediante punteros modifica las celdas del área mapeada y la rutina de visualización recorre esas celdas, convirtiendo el carácter y los bits de color en salida ANSI.

El atributo conserva la organización VGA de color de texto y fondo. Para mostrarlo correctamente se traduce el orden de colores VGA al orden ANSI. La representación visible se genera en la terminal; no es un monitor CRT físico ni una aplicación interactiva de edición. La rutina produce una proyección del contenido de VRAM al ejecutarse.

Esta parte permite distinguir el almacenamiento del carácter de su presentación: el dato permanece en memoria, mientras que otra rutina lo interpreta y lo muestra.

## 5. Simulador de CPU y buses

![Figura 4. Ejecución de buses, según el orden proporcionado](https://github.com/user-attachments/assets/ab207144-c75e-4a55-82f4-b1400e10c9dc)

*Figura 4. Evidencia proporcionada para el simulador de buses.*

El archivo `simulador_buses.c` desarrolla la sección metodológica del PDF mediante una CPU didáctica. Representa `PC`, `MAR`, `MDR`, `IR` y `AX`, además de los buses de direcciones, datos y control. Su conjunto de instrucciones permite ejecutar `LOAD`, `STORE`, `IN`, `OUT` y `HALT`; no reproduce la codificación de instrucciones de x86.

### 5.1 Micropasos de ejecución

Cada instrucción muestra las siguientes etapas:

1. **T0:** `MAR` recibe el valor de `PC` mediante una transferencia interna.
2. **T1:** el bus de direcciones recibe `MAR`, se activa `MEM_READ`, la RAM coloca el opcode en el bus de datos y se incrementa `PC`.
3. **T2:** `MDR` captura el bus de datos e `IR` recibe el opcode.
4. **T3:** la unidad de control decodifica `IR`.
5. **T4:** se obtiene el operando de dirección mediante una lectura de memoria que pasa por el bus y `MDR`.
6. **T5:** se realiza el acceso externo de memoria o puerto indicado por la instrucción.
7. **T6:** en las lecturas se transfiere el dato desde `MDR` hacia `AX` y se desactivan las señales de control.

`HALT` finaliza después de la decodificación. La traza sirve para estudiar transferencias lógicas; las operaciones agrupadas dentro de cada etapa no representan una medición de tiempos de hardware real.

### 5.2 Función de cada bus

El **bus de direcciones** recibe su valor de `MAR` y selecciona una posición de memoria o un puerto. El **bus de datos** transporta el byte transferido; el registro de frontera de la CPU es `MDR`. El **bus de control** indica qué recurso debe responder y en qué sentido se mueve la información.

Las señales son:

- `MEM_READ`: lectura de RAM.
- `MEM_WRITE`: escritura en RAM.
- `IO_READ`: lectura de puerto.
- `IO_WRITE`: escritura en puerto.

La coincidencia numérica de una dirección de RAM con un puerto no genera por sí sola una colisión. Por ejemplo, RAM `0x60` y puerto `0x60` están separados por el tipo de operación. `MEM_READ` selecciona memoria; `IO_READ` selecciona el puerto.

El programa demostrativo ejecuta `LOAD 0x80`, `STORE 0xF0`, `IN 0x60` y `OUT 0x61`. Luego comprueba que la escritura de memoria, la transferencia por puertos y el reconocimiento de lectura del teclado produjeron los resultados esperados, conservando intacta RAM `0x60`.

### 5.3 Detección de conflictos

Antes de cada transferencia externa se valida la regla del PDF:

```text
MEM_READ + MEM_WRITE + IO_READ + IO_WRITE ≤ 1
```

Permitir cero señales activas corresponde a etapas internas. Permitir una corresponde a una transferencia externa válida. Si hay más de una activa, el simulador informa un conflicto y termina con error.

El comando `./buses conflicto` activa deliberadamente `MEM_READ` e `IO_READ` para comprobar esta detección. Esa excepción es el resultado esperado de la prueba. La validación aplica el modelo simplificado del práctico; no representa todas las posibilidades de arbitraje de un bus real.

## 6. Ajustes al código del documento

Se mantuvo la organización de los tres programas y se realizaron correcciones para obtener una ejecución más consistente:

- Se inicializa completamente la BDA y se validan sus punteros al cargar un archivo existente.
- Se comprueban errores de escritura del archivo compartido.
- Se evita agregar un salto de línea fuera del arreglo de entrada del escritor.
- Se utiliza `int` al leer con `getchar`, para distinguir correctamente los caracteres de `EOF`.
- Se corrige la correspondencia de scan-codes de las letras, que no siguen el orden alfabético en el teclado.
- Se comprueba el bit de disponibilidad antes de leer el puerto y se limpia el dato consumido.
- Se muestra el offset realmente escrito antes de actualizar `tail`.
- La VRAM didáctica utiliza un archivo mapeado con `mmap`, en lugar de anunciar ese mecanismo mientras trabaja solamente con un arreglo local.
- Se incorporan comprobaciones de acceso y límites para framebuffer y consola virtual.
- Se añade el programa de micropasos y detección de conflictos solicitado por la sección 5.

## 7. Validación y límites

La validación funcional realizada durante el desarrollo cubrió inserción y extracción entre procesos, lectura de puerto sin repetición del dato, entrada con `scanf` y `getc`, capacidad útil de 15 teclas, desbordamiento, vuelta circular de punteros y recuperación ante una BDA truncada. También se comprobó el archivo de VRAM de 800 bytes, la ejecución de las cuatro instrucciones de transferencia y el rechazo de señales de bus incompatibles.

Estas comprobaciones se distinguen de las capturas enlazadas: no se transcriben salidas concretas de las imágenes porque no fue posible inspeccionarlas. Tampoco se afirma haber validado inyección TTY, escritura en framebuffer físico o consola virtual en el equipo del usuario.

La BDA y los puertos son estructuras simuladas. El simulador de buses es una demostración independiente: no intercepta ni registra automáticamente las operaciones de `prog1`, `prog2` o `monitor_linux`. Los cuatro programas muestran conceptos relacionados, pero no forman una única computadora virtual conectada de extremo a extremo.

## 8. Conclusiones

El práctico permite relacionar la entrada de teclado con una cola circular, interpretar palabras de 16 bits como ASCII y scan-code y distinguir la lectura de un puerto de la extracción de un dato almacenado en RAM. La persistencia mediante un archivo explica por qué escritor y lector pueden ejecutarse consecutivamente en una misma terminal.

La memoria de video muestra que una pantalla puede representarse mediante posiciones direccionables que contienen caracteres y atributos. La visualización interpreta esos datos para producir una salida visible.

Finalmente, la traza de micropasos explica el papel de `MAR` y `MDR` y demuestra que el destino de una transferencia depende tanto de la dirección como de las señales de control. La detección de conflictos hace explícita la necesidad de que cada transferencia tenga un recurso seleccionado y un único emisor de datos.
