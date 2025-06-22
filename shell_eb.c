//****************************************************************
// minishell_eb.c
// V 3.0 Junio 2025
// Autores:
//      - Rodríguez Leal Marco Cesar
//      - Rosas Jímenez Brenda Sibhet
//      - Suarez Lara Angel Uriel
// Grupo: 4CM2
// Curso: Sistemas Operativos 25/2
// ESCOM-IPN
// Simulación de una minishell con comandos internos y operadores 
// especiales como '|', '<','<<','>>','>', etc.
// Compilación: 
//      "gcc minishell.c -o minishell"
// Ejecución: 
//      "./minishell"
//********************************************************************


//*****************************************************************
// BIBLIOTECAS USADAS
//*****************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <glob.h>

//*****************************************************************
// CONSTANTES
//*****************************************************************
#define MAX_ENTRADA 200
#define MAX_ARGUMENTOS 20

//*****************************************************************
// PROTOTIPOS DE FUNCIONES
//*****************************************************************
void mostrar_prompt();
void ejecutar_comando(char **args);
void cambiar_directorio(char **args);
int interpretar_entrada(char *entrada, char **args);
void ejecutar_con_pipes(char *comandos[][MAX_ARGUMENTOS], int num_comandos);
void manejar_redirecciones(char **args);

//*****************************************************************
// PROGRAMA PRINCIPAL
//*****************************************************************
int main() {
    char entrada[MAX_ENTRADA];
    char *argumentos[MAX_ARGUMENTOS];

    while (1) {
        mostrar_prompt();
        // Si fgets recibe NULL --> Salir de la Shell
        if (fgets(entrada, sizeof(entrada), stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // Eliminar salto de línea
        entrada[strcspn(entrada, "\n")] = '\0';

        // Llamar a la función interpretar_entrada para detectar pipes o redirecciones,
        // continua si devuelve un 0.
        if (interpretar_entrada(entrada, argumentos) == 0) {
            continue;
        }

        // Si no hay argumentos, se pide otro comando
        if (argumentos[0] == NULL) continue;

        // Se detecta el comando 'cd' para cambiar de directorio
        if (strcmp(argumentos[0], "cd") == 0) {
            cambiar_directorio(argumentos);
            continue;
        }

        // Comandos internos para salir del shell
        if (strcmp(argumentos[0], "salir") == 0 || strcmp(argumentos[0], "exit") == 0) {
            break;
        }

        // Comando interno para mostrar ayuda sobre otros comandos internos básicos
        if (strcmp(argumentos[0], "ayuda") == 0 || strcmp(argumentos[0], "help") == 0) {
            printf("\nMINISHELL - Ayuda básica\n");
            printf("-----------------------\n");
            printf("Comandos internos:\n");
            printf("  cd <directorio>    Cambia el directorio actual.\n");
            printf("  salir o exit       Sale de la minishell.\n");
            printf("  ayuda o help       Muestra esta ayuda.\n\n");

            printf("Operadores soportados:\n");
            printf("  |      Pipe: conecta la salida de un comando con la entrada de otro.\n");
            printf("  >      Redirige la salida estándar a un archivo (sobrescribe).\n");
            printf("  >>     Redirige la salida estándar a un archivo (agrega al final).\n");
            printf("  <      Redirige la entrada estándar desde un archivo.\n");
            printf("  <<     Heredoc: entrada multilínea terminada con un delimitador.\n\n");

            printf("Ejemplos:\n");
            printf("  ls -l\n");
            printf("  cat archivo.txt > salida.txt\n");
            printf("  grep 'texto' < entrada.txt\n");
            printf("  cat archivo1.txt | grep 'error' | wc -l\n");
            printf("  cd ..\n\n");

            printf("Nota: El límite máximo por línea es de 200 caracteres.\n\n");
            continue;
        }

        // El último caso, para ejecutar comandos externos
        ejecutar_comando(argumentos);
    }

    return 0;
}

//*****************************************************************
// Función: mostrar_prompt
//*****************************************************************
// Descripción: Muestra el prompt del shell con información del usuario
// y directorio actual en colores rojo y amarillo.
//
// Input: Ninguno
// Output: Ninguno.
//*****************************************************************
void mostrar_prompt() {
    char directorio_actual[PATH_MAX];
    char *usuario = getenv("USER");
    if (!usuario) usuario = "usuario"; //Si no hay un usuario registrado, se coloca usuario.

    //Obtener directorio atual y mostrar el prompt con color
    if (getcwd(directorio_actual, sizeof(directorio_actual))) {
        printf("\033[1;31m%s@minishell:\033[0m", usuario);
        printf("\033[1;33m%s$\033[0m ", directorio_actual);
    } else {
        perror("getcwd: ERROR AL OBTENER LA RUTA DE DIRECTORIO ACTUAL");
    }
}


//*****************************************************************
// Función: interpretar_entrada
//*****************************************************************
// Descripción: Analiza la entrada del usuario para detectar operadores
// especiales (|, >, <, << o >>.) y divide los comandos y argumentos.
//
// Input:
//  - entrada: Cadena con el comando completo ingresado por el usuario
//  - args: Array donde se almacenarán los argumentos separados
//
// Output:
//  - Retorna 1 si debe ejecutarse un comando simple
//  - Retorna 0 si se detectó un operador especial.
//*****************************************************************
int interpretar_entrada(char *entrada, char **args) {
    
    //Buscar si hay | (pipes) en la entrada
    char *pipe_posicion = strchr(entrada, '|');
    
    //Separar los comandos cuando se encontró un |
    if (pipe_posicion != NULL) {
        char *comandos[MAX_ARGUMENTOS]; //Arreglo para separar los comandos individuales
        int num_comandos = 0;

        //Separar utilizando '|' como separador
        char *token = strtok(entrada, "|");
        while (token != NULL && num_comandos < MAX_ARGUMENTOS) {
            comandos[num_comandos++] = token;
            token = strtok(NULL, "|");
        }

        //Arreglo para guardar a entrada de la forma comando -- argumentos
        char *args_comandos[MAX_ARGUMENTOS][MAX_ARGUMENTOS];
        
        //Para cada comando separado por pipes
        for (int i = 0; i < num_comandos; i++) {

            //Separar argumentos usando " " como separador.
            char *arg_token = strtok(comandos[i], " ");
            int j = 0;
            while (arg_token != NULL && j < MAX_ARGUMENTOS - 1) {
                args_comandos[i][j++] = arg_token;
                arg_token = strtok(NULL, " ");
            }
            args_comandos[i][j] = NULL;
        }

        //Ejecutar las cadenas de comandos que contienen pipes.
        ejecutar_con_pipes(args_comandos, num_comandos);
        return 0;
    }

    //Si no hay pipes:
    int i = 0;

    //Separar argumentos usando " " como separador.
    char *token = strtok(entrada, " ");
    while (token != NULL && i < MAX_ARGUMENTOS - 1) {
        //Detectar si se tienen los comodines '*' o '?'
        if (strchr(token, '*') || strchr(token, '?')) {
            glob_t glob_result;
            if (glob(token, GLOB_NOCHECK, NULL, &glob_result) == 0) {
                //Agregar al arreglo de args por cada coincidencia
                for (size_t j = 0; j < glob_result.gl_pathc && i < MAX_ARGUMENTOS - 1; j++) {
                    args[i++] = strdup(glob_result.gl_pathv[j]);
                }
                globfree(&glob_result);
            }
        } else {
            //Si es una separación normal, solo agregar a args.
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    //Comando simple
    return 1;
}

//*****************************************************************
// Función: cambiar_directorio
//*****************************************************************
// Descripción: Implemenatación del comando interno 'cd' para cambiar el
// directorio actual del shell.
//
// Input:
//  - args: Arreglo de cadenas donde args[1] contiene la ruta destino.
//
// Output: Ninguno
//*****************************************************************
void cambiar_directorio(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Uso: cd <directorio>\n");
    } else {
        //Cambiar directorio con chdir, '0' indica que el cambio fue exitoso
        if (chdir(args[1]) != 0) {
            perror("Error al cambiar directorio");
        }
    }
}


//*****************************************************************
// Función: manejar_redirecciones
//*****************************************************************
// Descripción: Detecta y procesa operadores de redirección (> >> < <<)
// en los argumentos de un comando.
//
// Input:
//  - args: Array de argumentos que puede contener operadores
//
// Output: Ninguno
//*****************************************************************
void manejar_redirecciones(char **args) {
    int i = 0; //Para recorrer el arreglo
    int fd; //Descriptor de archivo

    //Analizar los argumentos uno por uno
    while (args[i] != NULL) {
        //Redirección de salida '>'
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta archivo después de '>'\n");
                exit(1);
            }

            //Abrir el archivo para escritura
            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open >");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO); //Redirige la salida estándar al archivo
            close(fd); //Cerrar el descriptor de archivo
            //Limpiar argumentos para evitar que execvp los reconozca
            args[i] = NULL;
            args[i + 1] = NULL;
            i += 2;
            continue;
            
            //Redirección de salida con append: '>>'
        } else if (strcmp(args[i], ">>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta archivo después de '>>'\n");
                exit(1);
            }
            //En vez de sobrescribir, agrega al final
            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
                perror("open >>");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            //Limpiar argumentos para evitar que execvp los reconozca
            args[i] = NULL;
            args[i + 1] = NULL;
            i += 2;
            continue;

            //Redirección de entrada: '<'
        } else if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta archivo después de '<'\n");
                exit(1);
            }
            //Arbir el archivo solo en modo lectura
            fd = open(args[i + 1], O_RDONLY);
            if (fd == -1) {
                perror("open <");
                exit(1);
            }
            dup2(fd, STDIN_FILENO); //Redirigir la entrada estandar al archivo abierto
            close(fd);//Cerra el descriptor de archivo
            //Limpiar argumentos para evitar que execvp los reconozca
            args[i] = NULL;
            args[i + 1] = NULL;
            i += 2;
            continue;

        //Redirección: '<<'
        } else if (strcmp(args[i], "<<") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta delimitador para '<<'\n");
                exit(1);
            }

            int pipefd[2];
            pipe(pipefd);  // Se crea el pipe

            pid_t heredoc_pid = fork();

            if (heredoc_pid == 0) {
                // Proceso hijo: escribe en el pipe
                close(pipefd[0]);  // Cerrar lectura

                char *delimitador = args[i + 1];
                char linea[MAX_ENTRADA];

                while (1) {
                    printf("heredoc> ");
                    fflush(stdout);
                    if (fgets(linea, sizeof(linea), stdin) == NULL) break;
                    linea[strcspn(linea, "\n")] = 0;
                    if (strcmp(linea, delimitador) == 0) break;
                    dprintf(pipefd[1], "%s\n", linea);
                }
                close(pipefd[1]);
                exit(0);
            } else {
                // Proceso padre: usará el pipe como entrada estándar
                wait(NULL);           // Espera a que el hijo termine de escribir
                close(pipefd[1]);     // Cerrar escritura
                dup2(pipefd[0], STDIN_FILENO);  // Redirige stdin al pipe
                close(pipefd[0]);
            }

            //Limpiar argumentos para evitar que execvp los reconozca
            args[i] = NULL;
            args[i + 1] = NULL;
            i += 2;
            continue;
        }
        i++;
    }

        // Limpiar argumentos eliminando NULLs intermedios
        int j = 0;
        for (int k = 0; k < MAX_ARGUMENTOS && args[k] != NULL; k++) {
            if (args[k] != NULL) {
                args[j++] = args[k];
            }
        }
        args[j] = NULL;
}

//*****************************************************************
// Función: ejecutar_comando
//*****************************************************************
// Descripción: Crea un proceso hijo para ejecutar el comando con
// posibles redirecciones. El proceso hijo maneja las redirecciones
// y luego ejecuta el comando con execvp.
//
// Input:
//  - args: Array de cadenas con el comando y sus argumentos.
//
// Output: Ninguno
//*****************************************************************
void ejecutar_comando(char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        // Proceso hijo

        // Manejar redirecciones (>, >>, <, <<) y limpiar args
        manejar_redirecciones(args);

        // Ejecutar el comando con sus argumentos ya limpios
        execvp(args[0], args);

        // Si execvp falla, imprimir error y salir
        perror("Error al ejecutar comando");
        exit(1);
    } else if (pid > 0) {
        // Proceso padre espera a que el hijo termine
        wait(NULL);
    } else {
        // Error al crear proceso
        perror("Error al crear proceso");
    }
}


//*****************************************************************
// Función: ejecutar_con_pipes
//*****************************************************************
// Descripción: Ejecuta una secuencia de comandos conectados con pipes.
//
// Input:
//  - comandos: Matriz de cadenas con los comandos a ejecutar
//  - num_comandos: Cantidad de comandos en la secuencia
//
// Output: Ninguno
//*****************************************************************
void ejecutar_con_pipes(char *comandos[][MAX_ARGUMENTOS], int num_comandos) {
    int i;
    int fd[2];          // Pipe: fd[0] para lectura, fd[1] para escritura
    int fd_entrada = 0; // Descriptor para la entrada estándar 
    pid_t pid;

    for (i = 0; i < num_comandos; i++) {
        pipe(fd);  // Crear un pipe 

        pid = fork();  // Crear proceso hijo para ejecutar el comando actual
        if (pid == 0) {  // Proceso hijo
            // Si no es el primer comando, redirige entrada estándar al lado de lectura del pipe anterior
            if (i > 0) {
                dup2(fd_entrada, STDIN_FILENO);
                close(fd_entrada);
            }

            // Si no es el último comando, redirige salida estándar al lado de escritura del pipe actual
            if (i < num_comandos - 1) {
                dup2(fd[1], STDOUT_FILENO);
            }

            // Cierra ambos extremos del pipe actual
            close(fd[0]);
            close(fd[1]);

            // Maneja redirecciones especiales (> < >> <<) 
            manejar_redirecciones(comandos[i]);

            // Ejecuta el comando con sus argumentos
            execvp(comandos[i][0], comandos[i]);

            // Si execvp falla, muestra error y termina el proceso hijo
            perror("Error al ejecutar comando");
            exit(1);
        } else {
            // Proceso padre espera a que el hijo termine
            wait(NULL);

            // Cierra el extremo de escritura del pipe actual
            close(fd[1]);

            // Si no es el primer comando, cierra el descriptor de entrada previo
            if (i > 0) close(fd_entrada);

            // El descriptor de lectura actual, se convierte en la entrada para el siguiente comando
            fd_entrada = fd[0];
        }
    }
}