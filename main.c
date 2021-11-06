/**
 * @file main.c
 * @brief Projeto de Programação Avançada
 * @date 2021-11-05
 * @author João Ferreira & Gonçalo Paulino
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>

#include "debug.h"
#include "args.h"
#include "memory.h"

#define N_SUP_TYPES 7

void strToLower(char* str);

int canOpenFile(char* filePath);

char* readAllFileContents(FILE* tmpFile);

char* getStrAfterChar(char* string, char ch);

char* getFileExt(char* filePath);

int isTypeSupported(char* shorterType);

void printResult(char* filePath, char* fileType, int* stats);

int isRegularFile(const char *path);

void verifyFileStats(char* filePath, int* stats);

void verifyFile(char* filePath);

int* initializeStatistics();

void printStatistics(int* stats);

void checkFileWithStats(char* filePath, int* stats);

void processBatchFile(char* batchFilePath);

int canOpenDir(char* dirPath, DIR** ptrDir);

void normalizeDirPath(char *dirPath);

void checkAllDirFiles(char* dirPath, DIR* dir, int* stats);

void processDirectory(char* dirPath);

void trataQuit(int sig, siginfo_t *info, void *context);

void setupSigQuit();

void trataUsr1(int sig);

void setupSigUsr1();

void saveTime();

//Global vars to share some info with SIGUSR1
struct tm* _startTime;
int _nFile;
char* _filePath;


int main(int argc, char *argv[]) {
    
    struct gengetopt_args_info args;

    setupSigQuit();

	//Initialize gengetopt
	if(cmdline_parser(argc, argv, &args)){
        fprintf(stderr, "[ERROR] couldn't initialize the argument parser\n");
        exit(1);
    }

    if(argc<2){
        printf("Try './checkFile --help' for more information.\n");
        return 0;
    }

    //File option
    if(args.file_given){
        for(unsigned int i = 0; i<args.file_given; i++){
            char* filePath = args.file_arg[i];
            if(canOpenFile(filePath)){
                verifyFile(filePath);
            }
        }
    }

    //Batch option
    if(args.batch_given){
        saveTime();
        setupSigUsr1();
        char* batchPath = args.batch_arg;
        if(canOpenFile(batchPath)){
            processBatchFile(batchPath);
        }
    }

    //Directory option
    if(args.dir_given){
        char* dir = args.dir_arg;
        processDirectory(dir);
    }

	cmdline_parser_free(&args);

    return 0;
}

//Convert a string to lowercase
void strToLower(char* str){
    int sl = strlen(str);
    for(int i=0; i<sl; i++){
        str[i] = tolower(str[i]);
    }
}

//Check if a file can be opened
int canOpenFile(char* filePath){
    //Check if the file exists
    if(access(filePath, F_OK)){
        fprintf(stderr, "[ERROR] cannot open file '%s' -- %s\n", filePath, strerror(errno));
        return 0;
    }

    //Check if the user can read the file
    if(access(filePath, R_OK)){
        fprintf(stderr, "[ERROR] cannot open file '%s' -- %s\n", filePath, strerror(errno));
        return 0;
    }

    return 1;
}

//Read all output file contents
char* readAllFileContents(FILE* tmpFile){
    //Count the nr of bytes on a file
    fseek(tmpFile, 0, SEEK_END);
    long fsize = ftell(tmpFile)-1; //Don't read the EOF byte
    fseek(tmpFile, 0, SEEK_SET); 

    char *string = malloc(fsize*sizeof(char)); 

    fread(string, 1, fsize, tmpFile);

    string[fsize] = 0; //Set the last '\n' byte to \0.

    fclose(tmpFile); //close the temporary file stream
    
    return string;
}

//Get the rest of a string after the last occurance of a char
char* getStrAfterChar(char* string, char ch){
    char* newStr = strrchr(string, (int)ch);
    if(newStr == NULL){
        return "";
    }
    newStr++; // Exclude the char from the final string
    return newStr;
}

//Extract the file extension from a path
char* getFileExt(char* filePath){
    char* ext = getStrAfterChar(filePath, '.');

    //Convert string to lower case
    strToLower(ext);

    return ext;
}

//Is the file type supported by checkFile?
int isTypeSupported(char* shorterType){
    
    char *supportedTypes[N_SUP_TYPES] = {"gif", "html", "jpeg", "mp4", "pdf", "png", "zip"};
    for(int i = 0; i<N_SUP_TYPES; i++){
        if(strcmp(shorterType, supportedTypes[i]) == 0)
            return 1;
    }
    return 0;
}

//Process file output
void printResult(char* filePath, char* fileType, int* stats){

    char* fileExt = getFileExt(filePath);
    //Convert the mime-type to an even shorter type form
    char* shorterType = getStrAfterChar(fileType, '/');

    //Check if it is an empty file
    if(strcmp(shorterType, "x-empty") == 0){
        printf("[INFO] '%s': is an empty file\n", filePath);
        free(fileType);
        if(stats!=NULL) stats[3]++; //Increments the number of errors
        return;
    }

    //Check if the file type is of an unsupported type
    if(!isTypeSupported(shorterType)){
        printf("[INFO] '%s': type '%s' is not supported by checkFile\n", filePath, fileType);
        free(fileType);
        if(stats!=NULL) stats[3]++; //Increments the number of errors
        return;
    }
    char* longExt = fileExt;
    //Convert the short-form of jpeg (jpg) to its long-form
    if(strcmp(fileExt, "jpg")==0)
        longExt = "jpeg";

    //Check if the file type matches the file extension
    if(strcmp(shorterType, longExt)==0){
        printf("[OK] '%s': extension '%s' matches file type '%s'\n", filePath, fileExt, shorterType);
        if(stats!=NULL) stats[1]++; //Increments the number of ok files
    }
    else{
        printf("[MISMATCH] '%s': extension is '%s', file type is '%s'\n", filePath, fileExt, shorterType);
        if(stats!=NULL) stats[2]++; //Increments the number of mismatch files
    }
    
    free(fileType);
}

//Check if a path points to a regular file
int isRegularFile(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

//Verify file type
void verifyFileStats(char* filePath, int* stats){
    FILE* tmpFile = tmpfile(); //Ask the OS to create a new temporary file.
    int fd = fileno(tmpFile);

    if(!isRegularFile(filePath)){
        fprintf(stderr, "[ERROR] '%s': irregular files are not supported by checkFile\n", filePath);
        return;
    }

    pid_t pid;
    switch (pid = fork()) {
    case -1:
        ERROR(1, "Couldn't execute fork()");
        break;
    case 0: //child process
        dup2(fd, 1); //redirect stdout to the temporary file
        execl("/bin/file", "file", filePath, "--mime-type", "-b", NULL);
        ERROR(1, "Couldn't execute execl()");
        break;
    default:
        wait(NULL);

        printResult(filePath, readAllFileContents(tmpFile), stats); 
        
        break;
    }
}

//Short form of verifyFileStats
void verifyFile(char* filePath){
    verifyFileStats(filePath, NULL);
}

//Initialize the Statistics array for batch/dir options
int* initializeStatistics(){
    int* stats = malloc(sizeof(int) * 4);
    for(int i = 0; i<4; i++)
        stats[i] = 0;
    return stats;
}

//Print out a statistics array
void printStatistics(int* stats){
    printf("[SUMMARY] [SUMMARY] files analyzed: %d; files OK: %d; files mismatch: %d; errors: %d\n", stats[0], stats[1], stats[2], stats[3]);
    free(stats);
}

//Verifies a file while saving statistics
void checkFileWithStats(char* filePath, int* stats){
    stats[0]++; //Increments the number of files analyzed.
    if(canOpenFile(filePath)){
            verifyFileStats(filePath, stats);
        }else
            stats[3]++; //Increments the number of errors
}

//Read all contents on batch file and check the filenames specified inside
void processBatchFile(char* batchFilePath){

    int* stats = initializeStatistics();

    printf("[INFO] analyzing files listed in ‘%s’\n", batchFilePath);
    FILE* bf = fopen(batchFilePath, "r" );

    char* line = NULL;
    size_t len = 0;
    _nFile=0;
    while(getline(&line, &len, bf)!=-1){
        line[strcspn(line, "\n")] = 0; // Replace the \n with \0 to close the string
        if(strcmp(line,"") != 0){ //If its an empty line don't continue
            //update global vars for sigusr1
            _nFile++;
            _filePath=line;
            //check file
            checkFileWithStats(line, stats);
        }
    }
    fclose(bf);

    printStatistics(stats);
}

//Validate if the directory can be opened successfully
int canOpenDir(char* dirPath, DIR** ptrDir){
    (*ptrDir) = opendir(dirPath);

    if(*ptrDir == NULL){
        fprintf(stderr, "[ERROR] cannot open dir '%s' -- %s\n", dirPath, strerror(errno));
        return 0;
    }

    return 1;
}

//Check all files inside a directory
void checkAllDirFiles(char* dirPath, DIR* dir, int* stats){
    errno = 0; //set errno to 0 to know if an error occurred or we got to the end of the directory
    struct dirent *df;
    while((df = readdir(dir))!=NULL){

        char* fileName = df->d_name; //file name

        //join dirPath and fileName together in a separate string
        char* filePath = malloc((strlen(dirPath)+strlen(fileName)+1)*sizeof(char));
        strcpy(filePath, dirPath);
        strcat(filePath, fileName);

        checkFileWithStats(filePath, stats);        

        free(filePath);
    }
    //If an error occurred display it
    if(errno!=0)
        printf("[ERROR] cannot read dir '%s' -- %s \n", dirPath, strerror(errno));
}

//Returns a formated string that the dirPath ends with a /
void normalizeDirPath(char *dirPath){
    if(dirPath[strlen(dirPath)-1]!='/')
        strcat(dirPath, "/");
}

//Process the directory argument
void processDirectory(char* dirPath){
    DIR* dir = NULL;

    normalizeDirPath(dirPath);

    //Try to open the directory
    if(!canOpenDir(dirPath, &dir))
        return;

    int* stats = initializeStatistics();

    printf("[INFO] analyzing files of directory '%s'\n", dirPath);

    checkAllDirFiles(dirPath, dir, stats);

    closedir(dir);

    printStatistics(stats);
}

//Handles SIGQUIT
void trataQuit(int sig, siginfo_t *info, void *context) {
    (void)context; //avoid warnings

    /* Backups global variable errno */
    int aux;
    aux = errno;

    if (sig == SIGQUIT) {
        printf("Captured SIGQUIT signal (sent by PID: %d). Use SIGINT to terminate aplication.\n", info->si_pid);
    }

    /* Restores errno */
    errno = aux;
}

//Sets up the SIGQUIT SIGACTION
void setupSigQuit(){
    //Setup Sigquit
    struct sigaction actQuit;
    actQuit.sa_sigaction = trataQuit;
    sigemptyset(&actQuit.sa_mask);

    actQuit.sa_flags = SA_SIGINFO;

    /* Captures signal SIGQUIT */
    if (sigaction(SIGQUIT, &actQuit, NULL) < 0)
        ERROR(1, "couldn't execute sigaction for SIQUIT");
}

//Handles SIGUSR1
void trataUsr1(int sig) {
    /* Backups global variable errno */
    int aux;
    aux = errno;

    if (sig == SIGUSR1) {
        char timeStr[50];
        strftime(timeStr, sizeof(timeStr), "%Y.%m.%d_%Hh%M:%S", _startTime);
        printf("[SIGUSR1] %s -- nº %d / %s\n", timeStr, _nFile, _filePath);
    }

    /* Restores errno */
    errno = aux;
}

//Sets up the SIGUSR1 SIGACTION
void setupSigUsr1(){
    //Setup Sigquit
    struct sigaction actSig;
    actSig.sa_handler = trataUsr1;
    sigemptyset(&actSig.sa_mask);

    actSig.sa_flags = SA_RESTART;

    /* Captures signal SIGQUIT */
    if (sigaction(SIGUSR1, &actSig, NULL) < 0)
        ERROR(1, "couldn't execute sigaction for SIGUSR1");
}

//Save the current time in the global variable
void saveTime(){
    time_t s;
	time(&s);
	_startTime = localtime(&s);
}