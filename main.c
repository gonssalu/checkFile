/**
 * @file main.c
 * @brief Projeto de Programação Avançada
 * @date 2021-11
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

#include "debug.h"
#include "memory.h"

#define N_SUP_TYPES 7

void strToLower(char* str);

int canOpenFile(char* filePath);

char* readAllFileContents(FILE* tmpFile);

char* getStrAfterChar(char* string, char ch);

char* getFileExt(char* filePath);

int isTypeSupported(char* shorterType);

void printResult(char* filePath, char* fileType);

void verifyFile(char* filePath);

int main(int argc, char *argv[]) {
    /* Disable warnings */
    (void)argc;
    //(void)argv;
    
    char *filePath = argv[1]; //temporary

    if(canOpenFile(filePath)){
        verifyFile(filePath);
    }
    
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
        fprintf(stderr, "[ERROR] cannot open file '%s' -- No such file or directory\n", filePath);
        return 0;
    }

    //Check if the user can read the file
    if(access(filePath, R_OK)){
        fprintf(stderr, "[ERROR] cannot open file '%s' -- Permission denied\n", filePath);
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

    char *string = malloc(fsize); 

    fread(string, 1, fsize, tmpFile);

    string[fsize] = 0; //Set the last '\n' byte to \0.

    fclose(tmpFile); //close the temporary file stream
    
    return string;
}

//Get the rest of a string after the last occurance of a char
char* getStrAfterChar(char* string, char ch){
    char* newStr = strrchr(string, (int)ch);
    newStr++; // Exclude the char from the final string
    return newStr;
}

//Extract the file extension from a path
char* getFileExt(char* filePath){
    char* ext = getStrAfterChar(filePath, '.');

    //Convert string to lower case
    strToLower(ext);

    return (ext == NULL ? "" : ext);
}

int isTypeSupported(char* shorterType){
    
    char *supportedTypes[N_SUP_TYPES] = {"gif", "html", "jpeg", "mp4", "pdf", "png", "zip"};
    for(int i = 0; i<N_SUP_TYPES; i++){
        if(strcmp(shorterType, supportedTypes[i]) == 0)
            return 1;
    }
    return 0;
}

//Process file output
void printResult(char* filePath, char* fileType){

    char* fileExt = getFileExt(filePath);
    //Convert the mime-type to an even shorter type form
    char* shorterType = getStrAfterChar(fileType, '/');

    //Check if the file type is of an unsupported type
    if(!isTypeSupported(shorterType)){
        printf("[INFO] '%s': type '%s' is not supported by checkFile\n", filePath, fileType);
        return;
    }
    char* longExt = fileExt;
    //Convert the short-form of jpeg (jpg) to its long-form
    if(strcmp(fileExt, "jpg")==0)
        longExt = "jpeg";

    //Check if the file type matches the file extension
    if(strcmp(shorterType, longExt)==0)
        printf("[OK] '%s': extension '%s' matches file type '%s'\n", filePath, fileExt, shorterType);
    else
        printf("[MISMATCH] '%s': extension is '%s', file type is '%s'\n", filePath, fileExt, shorterType);
}

//Verify file type
void verifyFile(char* filePath){
    FILE* tmpFile = tmpfile(); //Ask the OS to create a new temporary file.
    int fd = fileno(tmpFile);

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

        printResult(filePath, readAllFileContents(tmpFile)); 
        
        break;
    }
}