#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
/*****************************************************/
/* Struct and Operations */

// node_t: value of type str and pointer to next node_t
// each node is a token within a line of command
typedef struct node_t {

    char *value;
    struct node_t *next;

} node_t;

// mult_cmd_t: an array of pointers that point to the
// leading token of each command line, and a count number
// assume number of commands in an input line fewer than 512
typedef struct mult_cmd_t {

    node_t **commands;
    int num_cmd;

} mult_cmd_t;

/*****************************************************/
// Helper functions to check against " \t\n"
// returns 0 if there is at least one non-blank char in str
int is_blank(char *str)
{
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
	if ((str[i] != ' ') && (str[i] != '\t') && (str[i] != '\n'))
	    return 0;
    }
    return 1;
}
// returns the number of occurrences of a char in str
int num_occurrences(char *str, char val)
{
    int count = 0;
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
	if (str[i] == val)
	    count++;
    }
    return count;
}
// adds ' ' as padding to either side of ">" or ">+"
// ex. "ls>output" -> "ls > output"
// ex. "ls > output" -> "ls  >  output"
// ex. "ls >output" -> "ls  > output"
// ex. "ls>+output" -> "ls >+ output" HAS TO HANDLE!!!
char *add_padding(char *str)
{
    int i;
    int len = strlen(str);
    int len_new = len+2;
    // '\0' and two paddings
    char *str_new = malloc(len_new+1);
    assert (str_new != NULL); 
    // before the occurrence of '>'
    for (i = 0; i < len_new && str[i] != '>'; i++) {
	str_new[i] = str[i];
    }
    if (i < len_new) {
	// ">+"
	if (str[i+1] == '+') {
	    str_new[i] = ' ';
	    str_new[i+1] = '>';
	    str_new[i+2] = '+';
	    str_new[i+3] = ' ';
	    for (i = i+4; i < len_new; i++) {
		str_new[i] = str[i-2];
	    }
	}
	// ">"
	else {
	    str_new[i] = ' ';
	    str_new[i+1] = '>';
	    str_new[i+2] = ' ';
	    for (i = i+3; i < len_new; i++) {
		str_new[i] = str[i-2];
	    }
	}
    }
    str_new[len_new] = '\0';
    assert (str_new != NULL);
    return str_new;
}

/*****************************************************/
// Operations for node_t
// initiate singly linked list with a string
node_t *node_new(char *val)
{
    node_t *nd = malloc(sizeof(node_t));
    if(nd != NULL){
	nd->value = strdup(val);
	nd->next = NULL;
    }
    return nd;
}
// returns the length of the linked list before 
// the first occurrence of the given str
// if non-present, return whole length
// ex. "ls" -> 1
// ex. "ls > output" -> 1; argc = 2
// ex. "ls -la /tmp > output" -> 3; argc = 4
int length_until(node_t *nd, char val)
{
    int count = 0;
    while (nd != NULL) {
	if (nd->value[0] == val)
	    return count;
	count++;
	nd = nd->next;
    }
    return count;
}
// free all strings and pointers associated with list
void list_free(node_t *nd)
{
    while (nd != NULL) {
	node_t *temp = nd;
	nd = nd->next;
	free(temp->value);
	free(temp);
    }
}
// write content of list to stdout
void list_show(node_t *nd)
{
    if (nd == NULL)
	return;
    while (nd != NULL) {
	write(STDOUT_FILENO, nd->value, strlen(nd->value));
	if (nd->next != NULL)
	    write(STDOUT_FILENO, "=", sizeof(" "));
	nd = nd->next;
    }
    // int count = length(head);
    // write(STDOUT_FILENO, &count, sizeof(int));
    write(STDOUT_FILENO, "\n", sizeof("\n"));
}
// insert a string at the end of list; returns head
// if list originally empty, create list from val
node_t *insert(node_t *nd, char *val)
{
    node_t *head;
    if (nd != NULL) {
	head = nd;
	while (nd->next != NULL) {
	    nd = nd->next;
	}
	node_t *nd_new = node_new(val);
	nd->next = nd_new;
    }
    else {
	head = node_new(val);
    }
    return head;
}
/*****************************************************/
// Operations for mult_cmd_t
// malloc a new array of commands
mult_cmd_t *mult_cmd_new(void)
{
    mult_cmd_t *mult_cmd = malloc(sizeof(mult_cmd_t));
    if (mult_cmd == NULL)
	return NULL;

    node_t **commands = malloc(sizeof(node_t*) * 512);
    if (commands == NULL)
	return NULL;
    for (int i = 0; i < 512; i++) {
	commands[i] = NULL;
    }

    mult_cmd->commands = commands;
    mult_cmd->num_cmd = 0;

    return mult_cmd;
}
// add a list of tokens, aka, a command to the array
// if array originally empty, create new one
mult_cmd_t *add_to_commands(mult_cmd_t *mult_cmd, node_t *command)
{
    int index;

    if (mult_cmd == NULL)
	mult_cmd = mult_cmd_new();

    // the index at which the new command will be in the array
    if (mult_cmd != NULL) {
	index = mult_cmd->num_cmd;
	mult_cmd->commands[index] = command;
	mult_cmd->num_cmd++;
    }

    return mult_cmd;
}
// write content of commands array to stdout
void mult_cmd_show(mult_cmd_t *mc)
{
    if (mc == NULL)
	return;
    if (mc->commands == NULL || mc->num_cmd == 0)
	return;
    for (int i = 0; i < mc->num_cmd; i++) {
	list_show(mc->commands[i]);
    }
}
// deep free the array of commands
void mult_cmd_free(mult_cmd_t *mc)
{
    if (mc == NULL)
	return;
    if (mc->commands == NULL) {
	free(mc);
	return;
    }
    for (int i = 0; i < mc->num_cmd; i++) {
	list_free(mc->commands[i]);
    }
    free(mc);
}

/*****************************************************/
/* Print Helper Functions */
// prints message to stdout
void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
    fflush(stdout);
}

// prints error message
void printError(void)
{
    char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
    fflush(stdout);
}

/*****************************************************/
/* Process multiple command from input source */
// commands are fragments of an input line separated by ';'
// tokens are fragments within a command separated by ' \t\n'
// reads input line by line; create list of tokens
// then append to an array of commands of type mult_cmd_t
// returns NULL if input line too long
mult_cmd_t *createCmdList(FILE *fp)
{
    // including '\n' and '\0', MAX strlen(cmd_buff) == 513
    char cmd_buff[514];
    // getline(&overflow_buff, &len, fp) for arbitrary len?
    char overflow_buff[1026];
    char *pinput;
    // for strtok_r()
    char *token, *command, *saveptr_tok, *saveptr_cmd;
    token = NULL;

    node_t *token_list;
    mult_cmd_t *cmd_array;

    // fgets always add '\0' to end of buffer
    pinput = fgets(cmd_buff, 514, fp);
    if (!pinput) {
	exit(0);
    }

    // LONG COMMAND
    if (strlen(cmd_buff) == 513 && cmd_buff[512] != '\n') {
	pinput = fgets(overflow_buff, 1026, fp);
	if (!pinput)
	    exit(0);
	myPrint(cmd_buff);
	myPrint(overflow_buff);
	printError();
	return NULL;
    }

    // NOMRAL COMMAND    
    cmd_array = NULL;
    char *save_cmd = strdup(cmd_buff);
    assert(save_cmd != NULL);
    char *command_padded = NULL;
    // cut into commands separated by ';'
    command = strtok_r(cmd_buff, ";\n", &saveptr_cmd);
    while (command != NULL) {
	// clear token list
	token_list = NULL;

	// add padding to command
	int occ = num_occurrences(command, '>');
	// multiple occurences, break
	if (occ > 1) {
	    if (fp != stdin)
		myPrint(save_cmd);
	    printError();
	    return NULL;
	}
	else if (occ == 1) {
	    command_padded = add_padding(command);
	}
	else if (occ == 0) {
	    command_padded = strdup(command);
	}	
	// cut command_padded into tokens separated by ' \t\n'
	token = strtok_r(command_padded, " \t\n", &saveptr_tok);

	while (token != NULL) {
	    token_list = insert(token_list, token);
	    token = strtok_r(NULL, " \t\n", &saveptr_tok);
	} 

	cmd_array = add_to_commands(cmd_array, token_list);
	command = strtok_r(NULL, ";\n", &saveptr_cmd);
	free(command_padded);
    }

    // write command to stdout in BATCH MODE; DEBUGGING
    // ignores " \t\n"
    if (fp != stdin) {
	if (!is_blank(save_cmd)) {
	    myPrint(save_cmd);
	}
    }

    free(save_cmd);
    return cmd_array;
}

// Helper for processing the commands
// if token_list == NULL, does nothing and returns to main
// checks for other errors
void processCmd(node_t *token_list)
{
    node_t *nd = token_list;
    // error code checking
    int ret = 0;
    char *filepath;
    char *homepath;
    char currdir[256];

    if (nd == NULL)
	return;

    while (nd != NULL) {
	// BUILT-IN COMMANDS: exit, cd, pwd

	if (strncmp(nd->value, "exit", strlen(nd->value)) == 0) {
	    // there should not be tokens following exit
	    if (nd->next != NULL) {
		printError();
		return;
	    }
	    // normal case
	    exit(0);
	}

	else if (strncmp(nd->value, "cd", strlen(nd->value)) == 0) {
	    // checks for args following cd
	    if (nd->next != NULL) {
		// normal case w/ variable
		nd = nd->next;
		filepath = nd->value;
		// chdir returns 0 on success, -1 otherwise
		ret = chdir(filepath);
		if (ret != 0) {
		    printError();
		    return;
		}
		// advances the nd pointer
		nd = nd->next;
		// there should not be tokens following cd path
		if (nd != NULL) {
		    printError();
		    return;
		}
	    }		
	    // normal case w/o variable, char *getenv(const char *name)
	    else {
		homepath = getenv("HOME");
		ret = chdir(homepath);
		if (homepath == NULL || ret != 0) {
		    printError();
		    return;
		}
	    }	
	}

	else if (strncmp(nd->value, "pwd", strlen(nd->value)) == 0) {
	    // there should not be tokens following pwd
	    if (nd->next != NULL) {
		printError();
		return;
	    }
	    // normal case, char *getcwd(char *buf, size_t size)
	    getcwd(currdir, sizeof(currdir));
	    if (currdir == NULL) {
		printError();
		return;
	    }
	    else {
		// return to main, ignoring trailing tokens
		myPrint(currdir);
		myPrint("\n");
		return;
	    }
	}
	// NON-BUILT-IN COMMANDS: ls, ps, who etc.
	else {
	    pid_t pid;
	    int status = 0;
	    int argc = length_until(nd, '>');
	    char *argv[argc+1];

	    char *outfile = NULL;
	    int outfd = -1;
	    char tok_temp;
	    char outbuf[4096];
	    ssize_t num_bytes = 0;

	    // fill in argv with values in node
	    // advance node; set last to NULL
	    for (int i = 0; i < argc; i++) {
		assert(nd != NULL);
		argv[i] = nd->value;
		nd = nd->next;
	    }
	    argv[argc] = NULL;

	    // if nd == NULL, no redirection, continue to fork
	    // else either ">" or ">+", checks for output path
	    if (nd != NULL && nd->value[0] == '>') {
		tok_temp = nd->value[1];
		// if no output path specified
		if (nd->next == NULL) {
		    printError();
		    return;
		}
		else {
		    // advance the node; nd->next should be NULL
		    nd = nd->next;
		    outfile = nd->value;
		    if (nd->next != NULL) {
			printError();
			return;
		    }
		}
	    }
	    if (outfile != NULL) {
		outfd = open(outfile, O_RDONLY);
		if (outfd != -1 && tok_temp == '+') {
		    num_bytes = read(outfd, outbuf, 4096);
		    if (num_bytes == -1) {
			printError();
			return;
		    }
		    ret = close(outfd);
		}
	    }
	    pid = fork();
	    if (pid == 0) {
		// child
		if (outfile != NULL) {
		    outfd = open(outfile, O_RDONLY);
		    // open returns -1 if file non-existent
		    if (outfd == -1) {
			outfd = creat(outfile, S_IRWXU);
			if (outfd == -1) {
			    printError();
			    exit(1);
			}
			ret = dup2(outfd, STDOUT_FILENO);
			if (ret == -1) {
			    printError();
			    exit(1);
			}
			close(outfd);
		    }

		    // else file already exists
		    // if ">" error, if ">+" append
		    else {
			if (tok_temp == '\0') {
			    printError();
			    exit(status);
			}
			else if (tok_temp == '+') {	
			    num_bytes = read(outfd, outbuf, 4096);
			    if (num_bytes == -1) {
				printError();
				exit(1);
			    }
			    outfd = open(outfile, O_RDONLY | O_WRONLY | O_TRUNC);
			    if (outfd == -1) {
				printError();
				exit(1);
			    }
			    ret = dup2(outfd, STDOUT_FILENO);
			    if (ret == -1) {
				printError();
				exit(1);
			    }
			}
		    }
		}

		status = execvp(argv[0], argv);				
		// if execvp returns and status == -1, failed
		if (status == -1)
		    printError();
		exit(status);
	    }
	    else {
		// parent
		wait(&status);
		// if child execvp, append outbuf to original outfile
		if (status != -1 && tok_temp == '+') {
		    outfd = open(outfile, O_WRONLY, 0744);
		    ret = lseek(outfd, 0, SEEK_END);
		    // perror(NULL);
		    if (outfd != -1) {
		    	ret = write(outfd, outbuf, num_bytes);
		    	// perror(NULL);
		    	close(outfd);
		    }
		}
	    }
	    return;
	}
	// advance the node
	if (nd != NULL) {
	    nd = nd->next;
	}
    }
}

/*****************************************************/


/*****************************************************/


/*****************************************************/
int main(int argc, char *argv[]) 
{
    // an array of commands of type mult_cmd_t
    // a list of tokens of type node_t
    mult_cmd_t *cmd_array;
    node_t *token_list;
    // BATCH MODE
    char *filename = NULL;
    FILE *fp = NULL;

    // BATCH MODE
    // checks for [batchFile]
    if (argc == 2) {
	filename = argv[1];
	fp = fopen(filename, "r");
	// error opening the file
	if (fp == NULL) {
	    printError();
	    exit(0);
	}

	while (1) {
	    // create an array of commands
	    // loop through and clean up
	    cmd_array = createCmdList(fp);
	    if (cmd_array != NULL) {

		for (int i = 0; i < cmd_array->num_cmd; i++) {
		    token_list = cmd_array->commands[i];					
		    processCmd(token_list);

		}
		mult_cmd_free(cmd_array);
	    }

	}
    }
    // too many files
    else if (argc > 2) {
	printError();
	exit(0);
    }

    // INTERACTIVE MODE
    else if (argc == 1 && fp == NULL) {
	while (1) {
	    myPrint("myshell> ");
	    // create an array of commands
	    cmd_array = createCmdList(stdin);
	    // mult_cmd_show(cmd_array);

	    if (cmd_array != NULL) {
		for (int i = 0; i < cmd_array->num_cmd; i++) {
		    token_list = cmd_array->commands[i];
		    processCmd(token_list);
		}
		mult_cmd_free(cmd_array);
	    }

	}
    }
}
