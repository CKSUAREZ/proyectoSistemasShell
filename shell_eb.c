#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>

//*****************************************************************
// CONSTANTES
//*****************************************************************
#define MAX_ENTRADA 200    // Longitud máxima para entrada de usuario
#define MAX_ARGUMENTOS 20  // Número máximo de argumentos por comando

//*****************************************************************
// PROTOTIPOS DE FUNCIONES
//*****************************************************************

//*****************************************************************
// Función: mostrar_prompt
//*****************************************************************
// Descripción: Muestra el prompt del shell con información del usuario
// y directorio actual en color azul brillante.
//
// Input: Ninguno
//
// Output: Ninguno (escribe directamente a stdout)
//*****************************************************************
void mostrar_prompt();

//*****************************************************************
// Función: ejecutar_comando
//*****************************************************************
// Descripción: Ejecuta un comando externo en un proceso hijo utilizando
// la familia de funciones exec.
//
// Input:
//  - args: Array de cadenas que contiene el comando y sus argumentos
//
// Output: Ninguno
//*****************************************************************
void ejecutar_comando(char **args);

//*****************************************************************
// Función: cambiar_directorio
//*****************************************************************
// Descripción: Implementa el comando interno 'cd' para cambiar el
// directorio de trabajo actual.
//
// Input:
//  - args: Array de cadenas donde args[1] contiene la ruta destino
//
// Output: Ninguno
//*****************************************************************
void cambiar_directorio(char **args);

//*****************************************************************
// Función: interpretar_entrada
//*****************************************************************
// Descripción: Analiza la entrada del usuario para detectar operadores
// especiales (|, >, <, etc.) y divide los comandos y argumentos.
//
// Input:
//  - entrada: Cadena con el comando completo ingresado por el usuario
//  - args: Array donde se almacenarán los argumentos parseados
//
// Output:
//  - Retorna 1 si debe ejecutarse un comando simple
//  - Retorna 0 si se detectó un operador especial (ya se manejó)
//*****************************************************************
int interpretar_entrada(char *entrada, char **args);

//*****************************************************************
// Función: manejar_redirecciones
//*****************************************************************
// Descripción: Detecta y procesa operadores de redirección (> >> <)
// en los argumentos de un comando.
//
// Input:
//  - args: Array de argumentos que puede contener operadores
//
// Output:
//  - Retorna 1 si se detectó y manejó una redirección
//  - Retorna 0 si no hay redirecciones
//*****************************************************************
int manejar_redirecciones(char **args);

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
void ejecutar_con_pipes(char *comandos[][MAX_ARGUMENTOS], int num_comandos);

//*****************************************************************
// Función principal
//*****************************************************************
int main() {
    char entrada[MAX_ENTRADA];          // Buffer para entrada del usuario
    char *argumentos[MAX_ARGUMENTOS];   // Array para argumentos parseados
    
    // Bucle principal del shell
    while (1) {
        mostrar_prompt();
        
        // Leer entrada del usuario
        if (fgets(entrada, sizeof(entrada), stdin) == NULL) {
            printf("\n");
            break;  // Salir si hay EOF (Ctrl+D)
        }
        
        // Eliminar salto de línea final
        entrada[strcspn(entrada, "\n")] = '\0';
        
        // Parsear y procesar la entrada
        if (interpretar_entrada(entrada, argumentos) == 0) {
            continue;  // Ya se manejó un operador especial
        }
        
        // Comandos internos
        if (strcmp(argumentos[0], "cd") == 0) {
            cambiar_directorio(argumentos);
            continue;
        }
        if (strcmp(argumentos[0], "salir") == 0 || strcmp(argumentos[0], "exit") == 0) {
            break;  // Terminar el shell
        }
        if (strcmp(argumentos[0], "ayuda") == 0 || strcmp(argumentos[0], "help") == 0) {
            printf("Comandos internos:\n");
            printf("  cd <directorio> - Cambiar directorio\n");
            printf("  exit/salir     - Terminar el shell\n");
            printf("  ayuda/help     - Mostrar esta ayuda\n");
            continue;
        }
        
        // Comandos externos con posible redirección
        if (!manejar_redirecciones(argumentos)) {
            ejecutar_comando(argumentos);
        }
    }
    
    return 0;
}

//*****************************************************************
// Implementación de funciones
//*****************************************************************

//*****************************************************************
// Función: mostrar_prompt
//*****************************************************************
void mostrar_prompt() {
    char directorio_actual[PATH_MAX];  // Buffer para el directorio actual
    char *usuario = getenv("USER");    // Obtener nombre de usuario
    
    // Obtener directorio actual y mostrar prompt con color
    if (getcwd(directorio_actual, sizeof(directorio_actual))) {
        // \033[1;34m = Azul brillante, \033[0m = Resetear color
        printf("\033[1;34m%s@minishell:%s\033[0m$ ", usuario, directorio_actual);
    } else {
        perror("getcwd");  // Mostrar error si falla
    }
}

//*****************************************************************
// Función: interpretar_entrada
//*****************************************************************
int interpretar_entrada(char *entrada, char **args) {
    // Buscar si hay pipes en el comando
    char *pipe_pos = strchr(entrada, '|');
    if (pipe_pos != NULL) {
        char *comandos[MAX_ARGUMENTOS];  // Array para comandos separados
        int num_comandos = 0;
        
        // Dividir la entrada por pipes
        char *token = strtok(entrada, "|");
        while (token != NULL && num_comandos < MAX_ARGUMENTOS) {
            comandos[num_comandos++] = token;
            token = strtok(NULL, "|");
        }
        
        // Procesar cada comando individual
        char *args_comandos[MAX_ARGUMENTOS][MAX_ARGUMENTOS];
        for (int i = 0; i < num_comandos; i++) {
            // Dividir cada comando en argumentos
            char *arg_token = strtok(comandos[i], " ");
            int j = 0;
            while (arg_token != NULL && j < MAX_ARGUMENTOS - 1) {
                args_comandos[i][j++] = arg_token;
                arg_token = strtok(NULL, " ");
            }
            args_comandos[i][j] = NULL;  // Terminar con NULL
        }
        
        // Ejecutar la secuencia de pipes
        ejecutar_con_pipes(args_comandos, num_comandos);
        return 0;
    }
    
    // Comando simple (sin pipes)
    int i = 0;
    char *token = strtok(entrada, " ");
    while (token != NULL && i < MAX_ARGUMENTOS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // Terminar con NULL
    
    return 1;
}

//*****************************************************************
// Función: cambiar_directorio
//*****************************************************************
void cambiar_directorio(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Uso: cd <directorio>\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("Error al cambiar directorio");
        }
    }
}

//*****************************************************************
// Función: manejar_redirecciones
//*****************************************************************
int manejar_redirecciones(char **args) {
    int i = 0;
    int fd;
    int redireccion_detectada = 0;
    
    // Buscar operadores de redirección
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            // Redirección de salida (>)
            if (args[i+1] == NULL) {
                fprintf(stderr, "Error: falta archivo de salida\n");
                return 0;
            }
            
            // Abrir archivo de salida
            fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                return 0;
            }
            
            // Redirigir stdout al archivo
            dup2(fd, STDOUT_FILENO);
            close(fd);
            
            args[i] = NULL;  // Truncar argumentos
            redireccion_detectada = 1;
            break;
        }
        else if (strcmp(args[i], ">>") == 0) {
            // Redirección de salida (append)
            if (args[i+1] == NULL) {
                fprintf(stderr, "Error: falta archivo de salida\n");
                return 0;
            }
            
            // Abrir archivo en modo append
            fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
                perror("open");
                return 0;
            }
            
            dup2(fd, STDOUT_FILENO);
            close(fd);
            
            args[i] = NULL;
            redireccion_detectada = 1;
            break;
        }
        else if (strcmp(args[i], "<") == 0) {
            // Redirección de entrada
            if (args[i+1] == NULL) {
                fprintf(stderr, "Error: falta archivo de entrada\n");
                return 0;
            }
            
            // Abrir archivo de entrada
            fd = open(args[i+1], O_RDONLY);
            if (fd == -1) {
                perror("open");
                return 0;
            }
            
            dup2(fd, STDIN_FILENO);
            close(fd);
            
            args[i] = NULL;
            redireccion_detectada = 1;
            break;
        }
        i++;
    }
    
    return redireccion_detectada;
}

//*****************************************************************
// Función: ejecutar_comando
//*****************************************************************
void ejecutar_comando(char **args) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Proceso hijo
        execvp(args[0], args);
        perror("Error al ejecutar comando");
        exit(1);
    } else if (pid > 0) {
        // Proceso padre espera al hijo
        wait(NULL);
    } else {
        perror("Error al crear proceso");
    }
}

//*****************************************************************
// Función: ejecutar_con_pipes
//*****************************************************************
void ejecutar_con_pipes(char *comandos[][MAX_ARGUMENTOS], int num_comandos) {
    int i;
    int fd[2];
    int fd_entrada = 0;  // Descriptor para entrada del pipe anterior
    pid_t pid;
    
    for (i = 0; i < num_comandos; i++) {
        pipe(fd);  // Crear nuevo pipe
        
        pid = fork();
        if (pid == 0) {
            // Proceso hijo
            if (i < num_comandos - 1) {
                // Redirigir salida al pipe (excepto último comando)
                dup2(fd[1], STDOUT_FILENO);
            }
            
            if (i > 0) {
                // Redirigir entrada desde pipe anterior (excepto primer comando)
                dup2(fd_entrada, STDIN_FILENO);
            }
            
            // Cerrar descriptores no necesarios
            close(fd[0]);
            ejecutar_comando(comandos[i]);
            exit(0);
        } else {
            // Proceso padre
            wait(NULL);  // Esperar al hijo
            close(fd[1]);  // Cerrar extremo de escritura
            fd_entrada = fd[0];  // Guardar extremo de lectura para siguiente comando
        }
    }
}