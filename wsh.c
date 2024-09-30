#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>


#define STDIN 0
#define STDOUT 1

#define PIPE_READ 0
#define PIPE_WRITE 1

struct variablearray {
    int i;
    int limit;
    char **words;
};

void add(struct variablearray *var, char *value) {
    if (var->limit == var->i) {
        int newSize = var->limit * 2;
        char **newArray = malloc(newSize * sizeof(char *));
        
        for (int j = 0; j < var->i; j++) {
            newArray[j] = var->words[j];
        }
        
        free(var->words);
        var->words = newArray;
        var->limit = newSize;
    }
    
    var->words[var->i] = value ? strdup(value) : NULL;
    var->i++; 
}

void freeFunction(struct variablearray *var) {
  for (int i = 0; i < var->i; ++i) {
    free(var->words[i]);
  }
  free(var->words);
}




int parseInput(char *userInput, int previousones[2], int nextones[2], struct variablearray *localvariables, struct variablearray *history) {
    struct variablearray arguments = {.limit = 10, .i = 1, .words = malloc(10 * sizeof(char *))};

    char **inputs = arguments.words; 
    char *word; 
    char *delim = " \t\n";

    do {
        word = strsep(&userInput, " \t\n"); 
        *inputs = word; 
    } while (word && !*word); 

    if (**inputs == '$') {
        word = *inputs; 
        char *envValue = getenv(word + 1); 

        if (envValue) {
            *inputs = envValue; 
        } else {

            int found = 0;
            for (int i = 0; !found && i < localvariables->i; i += 2) {
                if (strcmp(localvariables->words[i], word + 1) == 0) {
                    *inputs = localvariables->words[i + 1];
                    found = 1; 
                }
            }
        }
    }

    *(inputs) = strdup(*(inputs));

    while ((word = strsep(&userInput, delim))) {
        if (strcmp(word, "") == 0) {
            continue;
        } else if (*(word) == '$') {
            char *value = getenv(word + 1);
            if (value != NULL) {
                add(&arguments, value);
            } else {
                for (int i = 0; i < localvariables->i; i += 2) {
                    if (strcmp(localvariables->words[i], word + 1) == 0) {
                        add(&arguments, localvariables->words[i+1]);
                        break;
                    }
                }
            }
        } else {
            add(&arguments, word);
        }
    }

    inputs = arguments.words;

    int valueToReturn = -1;

    if (strcmp(*(inputs), "exit") == 0) {
        if (arguments.i > 1) {
            exit(-1);
        } else {
            freeFunction(&arguments);
            exit(0);
        }
    } else if (strcmp(*(inputs), "cd") == 0) {
        if (arguments.i == 1 || arguments.i > 2) {
            exit(-1);
        }
        char *directory = *(inputs + 1);
        if (chdir(directory) == -1) {
            exit(1);
        }

    } else if (strcmp(*(inputs), "export") == 0)  {
        char *secondWord = strdup(*(inputs + 1));

        char *placeholder = secondWord;

        char *variableName = strsep(&secondWord, "=");

        if (strcmp(*(inputs), "") == 0) {
            unsetenv(variableName);
        } else {
            setenv(variableName, secondWord, 1);
        }
        free(placeholder);
    } else if (strcmp(*(inputs), "local") == 0) {
        char *secondWord = *(inputs + 1);

        char *variableName = strsep(&secondWord, "=");

        int set = 0;

        for (int i = 0; set == 0 && i < localvariables->i; i += 2) {
            if (strcmp(localvariables->words[i], variableName) == 0) {
                set = 1;
                free(localvariables->words[i+1]);
                localvariables->words[i+1] = strdup(secondWord);

                if (strcmp(secondWord, "") == 0) {
                    free(localvariables->words[i+1]);
                    free(localvariables->words[i]);

                    for (int j = i; j < localvariables->i-3; j +=2) {
                        localvariables->words[i] = localvariables->words[i+2];
	                    localvariables->words[i+1] = localvariables->words[i+3];
                    }

                    localvariables->words[localvariables->i-2] = NULL;
                    localvariables->words[localvariables->i-1] = NULL;
	                localvariables->i -= 2;

                }
            }
        }

        if (set == 0) {
            add(localvariables, variableName);
            add(localvariables, secondWord);
        }
    } else if (strcmp(*(inputs), "vars") == 0) {
        for (int i = 0; i * 2 < localvariables->i; i++) {
            printf("%s=%s\n", localvariables->words[i*2], localvariables->words[i*2 + 1]);
        }
    } else {
        add(&arguments, NULL);
        inputs = arguments.words;

        int pid = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit(1);
        }

        if (pid == 0) {
            dup2(nextones[PIPE_WRITE], STDOUT);
            dup2(previousones[PIPE_READ], STDIN);

            // printf("exec: %s\n", *inputs);
            execvp(*(inputs), inputs);

            printf("execvp: No such file or directory\n");

            exit(-1);
        }
        
        valueToReturn = pid;
    }

    freeFunction(&arguments);
    return valueToReturn;
}



int historyFunction(char *input, struct variablearray *history) {
    char *second = strdup(input);
    char *third = second;

    while (*(third) == ' ') {
        third ++;
    }

    char *word = strsep(&third, " \t\n");

    if (strcmp(word, "history") != 0) {
        free(second);
        return -1;
    }

    if (strcmp(third, "") == 0) {
        for (int j = 0; j < history->i; j++) {
            printf("%d) %s", j+1, history->words[j]);
        }
        free(input);
        free(second);
        

        return -5;
    }



    char *part2 = NULL;
    do {
        part2 = strsep(&third, " \t\n"); 
    } while (part2 && *part2 == '\0'); 

    if (strcmp(part2, "set") == 0) {
        char *part3;
        do {
            part3 = strsep(&third, " \t\n"); 
    }    while (part3 && *part3 == '\0'); 



    int newValue = atoi(part3);
    if (newValue != history->limit) {

        char **updatedHistory = malloc(newValue * sizeof(char *));

        int entriesToKeep = history->i < newValue ? history->i : newValue;

//////////
        int i;
        for (i = 0; i < entriesToKeep; ++i) {
            updatedHistory[i] = history->words[i];
        }

        while (i < history->i) {
            free(history->words[i++]);
        }

        free(history->words); 
        history->words = updatedHistory;
        history->i = entriesToKeep;
        history->limit = newValue;

        ////////////
    }
    free(second);
    free(input);
    return -5;
} else {
    int indexToReturn = atoi(second) - 1; 
    free(second);
    free(input);
    return indexToReturn;
}
}



int main(int argc, char *argv[]) {
    struct variablearray localvariables = {0, 10, malloc(10 * sizeof(char *))};
    struct variablearray history = {0, 5, malloc(5 * sizeof(char *))};

    int batch = argc > 1; 

    FILE *batchFile = NULL;
    if (batch) {
        if (argc > 2) exit(1); 
        batchFile = fopen(argv[1], "r"); 
    }   


    while (1) {
        if ((!batch && feof(stdin)) || (batch && (feof(batchFile)))) {
            break;
        }
        if (!batch) {
            printf("wsh> ");
        }

        char *line = NULL;
        size_t sz = 0;
        
        if (!batch)  {
            int lineSize = getline(&line, &sz, stdin);
            if (lineSize < 0) {
                free(line);
                break;
            }
        } else {
            int lineSize = getline(&line, &sz, batchFile);

            if (lineSize < 0) {
                free(line);
                break;
            }

            if (line[lineSize-1] == '\n') {
                line[lineSize-1] = '\0';
            }
        }

        int pass = 0;
        int i = 0;
        size_t len = strlen(line); // To avoid recalculating strlen in each iteration

        while (!pass && i < len) {
            if (!isspace(line[i])) {
                pass = 1;
            }   
            i++;
        }   

        if (!pass) {
            free(line);
            continue;
        }

        int historyValue = historyFunction(line, &history);

        if (historyValue == -5) {
            continue;
        }

        if (historyValue >= 0) {
            line = history.words[historyValue];
        }

        char *originalLine = line;
        char *originalLine2 = strdup(line);

        

        int previousones[2] = {-1, -1};
        int nextones[2] = {-1, -1};

        previousones[PIPE_READ] = STDIN;

        int child_pid = -5;

        int children[1024];

        int child = 0;

        char *word;

        ////////////////////
        while ((word = strsep(&line, "|"))) {
            if (line) {
                pipe(nextones);
            }   else {
                nextones[PIPE_WRITE] = STDOUT;
            }

            child_pid = parseInput(word, previousones, nextones, &localvariables, &history);
            children[child++] = child_pid;


            if (previousones[PIPE_READ] != STDIN) {
                close(previousones[PIPE_READ]);
            }
            previousones[PIPE_READ] = nextones[PIPE_READ];
            if (nextones[PIPE_WRITE] != STDOUT) {
                close(nextones[PIPE_WRITE]); 
            }

            
        }

        ///////////////////

        int setting;

        for (int i = 0; i < child; i++) {
            waitpid(children[i], &setting, WUNTRACED);
        }

        free(originalLine);


        ///////////////////////////

        if (historyValue <= 0 && child_pid > 0 && history.limit > 0) {
            if (history.i == history.limit) {
                free(history.words[history.i-1]);
            }
            for (int i = (history.i < history.limit) ? history.i : history.limit-1; i > 0; i--) {
                history.words[i] = history.words[i-1];
            }
            history.words[0] = strdup(originalLine2);
            if (history.i < history.limit) {
                history.i++;
            }
        }
        

        free(originalLine2);
        if (child_pid == -5) {
            break;
        }

    }

    freeFunction(&localvariables);
    freeFunction(&history);

    if (batch != 0) {
        fclose(batchFile);
    }

    return 0;


    

    

}

