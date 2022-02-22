#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
  a = alive; b = born;
  e = empty; d = dead;
*/

void grow(char* world, int dimR, int dimC) {
  for (int i = 0; i < dimR; i++) {
    for (int j = 0; j < dimC; j++) {
      if (world[i * dimC + j] == 'b') {
        world[i * dimC + j] = 'a';
      }
      if (world[i * dimC + j] == 'd') {
        world[i * dimC + j] = 'e';
      }
    }
  }
}

void printWorld(char* world, int dimR, int dimC) {
  if ((dimC > 100) || (dimR > 100)) {
    printf("La matrice è troppo grande per essere stampata a video\n");
    return ;
  }
  for (int i = 0; i < dimR; i++) {
    for (int j = 0; j < dimC; j++) {
      if (world[i * dimC + j] == 'a') {
        printf("O");
      }
      else {
        printf(" ");
      } 
    } 
    if (dimC > 1) printf("\n");
  }
}

int countAlive(char* cell, int row, int col, int numR, int numC) {
  int alive = 0;
  for(int i = -1; i < 2; i++) {
    if ((row + i > -1) && (row + i < numR)) {
      for(int j = -1; j < 2; j++) {
        if ((col + j > -1) && (col + j < numC)) {
          if ((cell[(row + i) * numC + col + j] == 'a') ||
              (cell[(row + i) * numC + col + j] == 'd')) {
            alive++;
          }
        }
      }
    }
  }
  return alive;
}

int countBorderAlive(int col, char* borderArray, int dim) {
  if (!borderArray) return 0;
  int alive = 0;
  for(int i = col - 1; i < col + 2 ; i++) {
    if ((i > -1) && (i < dim)) {
      if (borderArray[i] == 'a') {
        alive++;
      }
    }
  }
  return alive;
}

bool checkLiveness(char* cell, int row, int col, 
                   int  numR, int numC, char* borderArray) {
  int alive = countAlive(cell, row, col, numR, numC);
  int bAlive = countBorderAlive(col, borderArray, numC);
  alive += bAlive;
  if ((alive < 3) || (alive > 4)) {
    return false;
  }
  return true;
}

bool checkBirth(char* cell, int row, int col, 
                int numR, int numC, char* borderArray) {
  int alive = countAlive(cell, row, col, numR, numC);
  int bAlive = countBorderAlive(col, borderArray, numC);
  alive += bAlive;
  if (alive == 3) {
   return true;
 }
 return false;
}

void gameOfLifeSeq(char *world, int n, int m, int anni) {
  int anno = 0;
  while(anno < anni) {
    for (int r = 0; r < n; r++) {
      for (int c = 0; c < m; c++) {

        if (world[r * m + c] == 'a') {
          if (!checkLiveness(world, r, c, n, m, NULL)) {
            world[r * m + c] = 'd';
          }
        }
        else if (world[r * m + c] =='e') {
          if (checkBirth(world, r, c, n, m, NULL)) {
            world[r * m + c] = 'b';
          }
        }
      }
    }    
    grow(world, n, m);
    anno++;
  }

} 

int main (int argc, char **argv) {
  int n, m, anni;
  srand(0);
  bool confronto = false;

  switch (argc) {
  case 3:
    if (*argv[1] == 'c') {
      printf("Troppi pochi argomenti\n");
      printf("Eseguire <nome programma> [c] <# generazioni> <# righe> [<#colonne>]\n");
      exit(0);
    } 
    else {
      anni = atoi(argv[1]);
      n = atoi(argv[2]);
      m = n;
    }
    break;
  case 4:
    if (*argv[1] == 'c') {
      anni = atoi(argv[2]);
      n = atoi(argv[3]);
      m = n;
      confronto = true;
    } 
    else {
      anni = atoi(argv[1]);
      n = atoi(argv[2]);
      m = atoi(argv[3]);
    }
    break;
  case 5:
      if (*argv[1] == 'c') confronto = true;
      else {
        printf("Argomento \'%s\' non valido\n", argv[1]);
        printf("Eseguire <nome programma> [c] <# generazioni> <# righe> [<#colonne>]\n");
        exit(0);
      }
      anni = atoi(argv[1]);
      n = atoi(argv[2]);
      m = atoi(argv[3]);
  default:
    printf("Numero errato di argomenti\n");
    printf("Eseguire <nome programma> [c] <# generazioni> <# righe> [<# colonne>]\n");
    exit(0);
  }


  int num_p;
  int rank;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &num_p);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if ((n / num_p) < 3) num_p = 1;
  MPI_Bcast(&num_p, 1, MPI_INT, 0, MPI_COMM_WORLD);

//1. Inizializzazione Game of Live
  char* world;
  char* seqWorld;
  if (rank == 0) {
    world = (char*) malloc(sizeof(char) * n * m);
    if (confronto) seqWorld = (char*) malloc(sizeof(char) * n * m);
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < m; j++) {
        if (rand()%2 == 0) {
          world[i * m + j] = 'a';
          if (confronto) seqWorld[i * m + j] = 'a';
        }
        else {
          world[i * m + j] = 'e';
          if (confronto) seqWorld[i * m + j] = 'e';
        }
      } 
    }
    printf("Matrice iniziale:\n");
    printWorld(world, n, m);
    fflush(stdout);
  }
//END 1

//2. Distribuzione lavoro
  int *splitN, *display;
  int workM = m;
  int workN = n;

  char* subWorld = NULL;
  MPI_Status status;
  MPI_Request *req = NULL;

  if (num_p != 1) {
    MPI_Bcast(&workM, 1, MPI_INT, 0, MPI_COMM_WORLD);
    req = (MPI_Request*) malloc(sizeof(MPI_Request) * 2);

    if (rank == 0) {
      splitN = (int*) malloc(sizeof(int) * num_p);
      display = (int*) malloc(sizeof(int) * num_p);
      display[0] = 0;
      splitN[0] = (n / num_p) * workM;
      int r = n % num_p;
      for (int i = 1; i < num_p; i++) {
        splitN[i] = (n / num_p) * workM;
        if (r != 0) { 
          splitN[i] += workM;
          r--;
        }
        display[i] = display[i - 1] + splitN[i - 1];
      }
    }

    MPI_Scatter(splitN, 1, MPI_INT, &workN, 1, MPI_INT, 0, MPI_COMM_WORLD);
    workN /= workM;
    subWorld = (char*) malloc(sizeof(char) * workN * workM);
    MPI_Scatterv(world, splitN, display, MPI_CHAR, subWorld, workN * workM, MPI_CHAR, 0, MPI_COMM_WORLD);
  } 
  else if (rank == 0) {
    subWorld = world;
  }
//END 2
  double start, end;
  MPI_Barrier(MPI_COMM_WORLD);
  start = MPI_Wtime();

//3. Lavoro parallelo
  int anno = 0;
  char *confineUp = NULL, *confineDown = NULL;
  if (num_p != 1) {
    if (rank != 0) {
      confineUp = (char*) malloc(workM * sizeof(char));
    }
    if (rank != (num_p - 1)) {
      confineDown = (char*) malloc(sizeof(char) * workM);
    }
  }

  while (anno < anni) {
    if(rank == 0) {
      printf(".");
      fflush(stdout);
    }
  //3.1. Scambio bordi
    if (confineDown) {
      MPI_Isend(&subWorld[(workN - 1) * workM], workM, MPI_CHAR, rank + 1, 2, MPI_COMM_WORLD, &req[0]);
      MPI_Irecv(confineDown, workM, MPI_CHAR, rank + 1, 2, MPI_COMM_WORLD, &req[0]);
    }
    if (confineUp) {
      MPI_Isend(subWorld, workM, MPI_CHAR, rank - 1, 2, MPI_COMM_WORLD, &req[1]);
      MPI_Irecv(confineUp, workM, MPI_CHAR, rank - 1, 2, MPI_COMM_WORLD, &req[1]);
    }
   //END 3.1
    
    if (subWorld) {

  //3.2. Matrice interna
      for (int r = 1; r < workN - 1; r++) {
        for (int c = 0; c < workM; c++) {

          if (subWorld[r * workM + c] == 'a') {
            if (!checkLiveness(subWorld, r, c, workN, workM, NULL)) {
              subWorld[r * workM + c] = 'd';
            }
          }
          else if (subWorld[r * workM + c] =='e') {
            if (checkBirth(subWorld, r, c, workN, workM, NULL)) {
              subWorld[r * workM + c] = 'b';
            }
          }
        }
      }
  //END 3.2

  //3.3. Bordo matrice    
    
    //confine superiore
      if ((rank != 0) && (num_p != 1)) {
        MPI_Wait(&req[1], MPI_STATUS_IGNORE);
      }
      for (int i = 0; i < workM; i++) {
        if (subWorld[i] == 'a') {
          if (!checkLiveness(subWorld, 0, i, workN, workM, confineUp)) {
              subWorld[i] = 'd';
            }
          }
        else if (subWorld[i] =='e') {
          if (checkBirth(subWorld, 0, i, workN, workM, confineUp)) {
            subWorld[i] = 'b';
          }
        }
      }
    
    //confine inferiore
      if ((rank != num_p - 1) && (num_p != 1)) {
        MPI_Wait(&req[0], MPI_STATUS_IGNORE);
      }
      for (int i = 0; i < workM; i++) {
        if (subWorld[(workN - 1) * workM + i] == 'a') {
          if (!checkLiveness(subWorld, workN - 1, i, workN, workM, confineDown)) {
              subWorld[(workN - 1) * workM + i] = 'd';
            }
          }
        else if (subWorld[(workN - 1) * workM + i] =='e') {
          if (checkBirth(subWorld, workN - 1, i, workN, workM, confineDown)) {
            subWorld[(workN - 1) * workM + i] = 'b';
          }
        }
      }
  //END 3.3
      grow(subWorld, workN, workM);
    }
    anno++;
  }
  if (rank == 0) printf("\n");
  
  MPI_Barrier(MPI_COMM_WORLD);
  end = MPI_Wtime();

  if (num_p != 1) {
    MPI_Gatherv(subWorld, workN * workM, MPI_CHAR, world, splitN, display,
                MPI_CHAR, 0, MPI_COMM_WORLD);
  }
  if (rank == 0) {
    printf("\nMatrice finale\n");
    printWorld(world, n, m);
    fflush(stdout);
  } 
//END 3

  if (confineUp) free(confineUp);
  if (confineDown) free(confineDown);
  if (req != NULL) free(req);
  if (num_p != 1) {
    free(subWorld);
    if (rank == 0) {
      free(splitN);
      free(display);
    }
  }
  if (rank == 0) printf("tempo di esecuzione parallela: %f\n", end - start); 

  MPI_Finalize();

  if (confronto && rank == 0) {
    gameOfLifeSeq(seqWorld, n, m, anni);

    bool flag = false;
    for (int i = 0; i < n * m; i++) {
      if (seqWorld[i] != world[i]) {
        flag = true;
        printf("Le matrici non coincidono %c!=%c\n", seqWorld[i], world[i]);
        printf("Errore nella cella (%d,%d)\n", i/m, i%m);
        break;
      }
    }
    if(!flag) printf("Le matrici coincidono\n");
    free(seqWorld);
  }
  free(world);
}