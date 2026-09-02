#include <cstdio>  // Para std::remove
#include <cstring> // Para memcpy y memset
#include <fstream>
#include <iostream>

int main() {
    const char *nombreArchivo = "memoria_secundaria.bin";
    const int TAM_ORIGINAL = 1000;
    const int TAM_DESTINO = 100;
    const int INDICE_INICIO = 500;

    // --- Preparar datos en memoria secundaria (archivo binario) ---
    std::ofstream archivoSalida(nombreArchivo, std::ios::binary);
    if (!archivoSalida) {
        std::cerr << "Error al crear el archivo binario." << std::endl;
        return 1;
    }

    // Escribir 1000 enteros secuenciales (1 a 1000)
    for (int i = 1; i <= TAM_ORIGINAL; ++i) {
        archivoSalida.write(reinterpret_cast<const char *>(&i), sizeof(int));
    }
    archivoSalida.close();

    // --- 1. Leer el arreglo de 1000 elementos desde memoria secundaria ---
    int arregloOriginal[TAM_ORIGINAL];
    std::ifstream archivoEntrada(nombreArchivo, std::ios::binary);
    if (!archivoEntrada) {
        std::cerr << "Error al abrir el archivo binario para lectura."
                  << std::endl;
        return 1;
    }

    archivoEntrada.read(reinterpret_cast<char *>(arregloOriginal),
                        sizeof(arregloOriginal));
    archivoEntrada.close();
    std::remove(nombreArchivo); // Eliminar archivo temporal

    // --- 3a. Mostrar primeros y últimos cinco elementos antes del traslado ---
    std::cout << "-- ARREGLO INICIAL (ANTES DEL TRASLADO) --" << std::endl;
    std::cout << "Primeros 5: ";
    for (int i = 0; i < 5; ++i) {
        std::cout << arregloOriginal[i] << " ";
    }
    std::cout << "\nUltimos 5:  ";
    for (int i = TAM_ORIGINAL - 5; i < TAM_ORIGINAL; ++i) {
        std::cout << arregloOriginal[i] << " ";
    }
    std::cout << "\n\n";

    // --- 2. Segundo arreglo, transferencia con memcpy y limpieza con memset
    // ---
    int segundoArreglo[TAM_DESTINO];

    // Copiar 100 enteros a partir del índice 500
    std::memcpy(segundoArreglo, &arregloOriginal[INDICE_INICIO],
                TAM_DESTINO * sizeof(int));

    // Limpiar a cero los elementos transferidos en el arreglo original
    std::memset(&arregloOriginal[INDICE_INICIO], 0, TAM_DESTINO * sizeof(int));

    // --- 3b. Mostrar los 100 elementos transferidos ---
    std::cout << "-- 100 ELEMENTOS TRANSFERIDOS --" << std::endl;
    for (int i = 0; i < TAM_DESTINO; ++i) {
        std::cout << segundoArreglo[i] << " ";
        if ((i + 1) % 20 == 0)
            std::cout << "\n"; // Salto cada 20 para legibilidad
    }
    std::cout << "\n";

    // --- 3c. Validación de que los índices 500 al 599 contengan ceros ---
    bool sonTodosCeros = true;
    for (int i = INDICE_INICIO; i < INDICE_INICIO + TAM_DESTINO; ++i) {
        if (arregloOriginal[i] != 0) {
            sonTodosCeros = false;
            break;
        }
    }

    std::cout << "-- VALIDACION (INDICES 500 AL 599) --" << std::endl;
    if (sonTodosCeros) {
        std::cout << "Validacion EXITOSA: El rango contiene unicamente ceros."
                  << std::endl;
    } else {
        std::cout << "Validacion FALLIDA: Existen valores diferentes de cero."
                  << std::endl;
    }

    return 0;
}
