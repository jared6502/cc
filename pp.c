#include <stdio.h>
#include <errno.h>
#include <errno.h>

//max number of definitions
#define MAX_DEFS 40

//max unsigned chars in line + 1 null unsigned char
#define LINE_LEN 256

//max nesting for #if/#ifdef/#ifndef statements
#define MAX_IFS 8

//max nesting for #include statements
#define MAX_INCLUDES 8

//unsigned char definitions
#define LF 0x0A
#define CR 0x0D
#define DQ 0x22
#define SQ 0x27

//macro
#define NOT_EOF			(feof(infile[inclevel]) == 0)
#define IS_EOF			(feof(infile[inclevel]) != 0)
#define IS_ERROR		(ferror(infile[inclevel]) != 0)

#define READ_CHAR		do { last = buf; buf = fgetc(infile[inclevel]); } while (0)
#define WRITE_CHAR		do { fputc(buf, outfile); } while (0)
#define READ_WRITE_CHAR	do { last = buf; buf = fgetc(infile[inclevel]); fputc(buf, outfile); } while (0)
#define GET_NEXT_LINE	do { do { READ_CHAR; } while (NOT_EOF && !(last != '\\' && buf == LF)); } while (0)
#define GO_TO_END		do { READ_CHAR; } while (NOT_EOF && !(last == '*' && buf == '/'))
#define GO_TO_END_SQ	do { READ_WRITE_CHAR; } while (NOT_EOF && !(last != '\\' && buf == SQ))
#define GO_TO_END_DQ	do { READ_WRITE_CHAR; } while (NOT_EOF && !(last != '\\' && buf == DQ))

unsigned char buf;
unsigned char rbuf;
unsigned char last;
unsigned char token[LINE_LEN];
unsigned char converted[LINE_LEN];
unsigned char expanded[LINE_LEN];
unsigned char defs[MAX_DEFS][LINE_LEN];
unsigned char defnames[MAX_DEFS][LINE_LEN];

unsigned char pp_ignore;

int iflevel;
int inclevel;
	
FILE *infile[MAX_INCLUDES];
FILE *outfile;

void clearstr(unsigned char *s)
{
	//clear all text from the string, replace with zeros
	int i;
	for(i = 0; i < LINE_LEN; i++) s[i] = 0;
	return;
}

void toupper(unsigned char *s)
{
	int i;
	
	for(i = 0; i < LINE_LEN; i++)
	{
		if(s[i] >= 'a' && s[i] <= 'z') s[i] = s[i] - 32;
	}
	return;
}

void append_str(unsigned char *s1, unsigned char *s2)
{
	int i;
	int len;
	
	len = strlen(s1);
	
	for(i = 0; (i + len) < LINE_LEN; i++)
	{
		s1[(i + len)] = s2[i]; 
	}
	
	s1[(LINE_LEN-1)] = 0x00;
		
	return;
}

int strcmp(unsigned char *s1, unsigned char *s2)
{
	int i;
	
	for(i = 0; i < LINE_LEN; i++)
	{
		//check if unsigned chars match
		if (s1[i] != s2[i])
		{
			//didn't match, indicate no match
			return 0;
		}
		else if (s1[i] == 0)
		{
			//unsigned chars matched and at end of string
			//terminate early, indicate match
			return 1;
		}
	}
	
	//had to check entire string, but strings match
	//indicate match
	return 1;
}

void copy_str(unsigned char *s1, unsigned char *s2)
{
	int i;
	clearstr(s2);
	for(i = 0; i < LINE_LEN && s1[i] != 0; i++) s2[i] = s1[i];
	return;
}

int strncmp(unsigned char *s1, unsigned char *s2, int len)
{
	int i;
	
	for(i = 0; i < LINE_LEN && i < len; i++)
	{
		//check if unsigned chars match
		if (s1[i] != s2[i])
		{
			//didn't match, indicate no match
			return 0;
		}
		else if (s1[i] == 0)
		{
			//unsigned chars matched and at end of string
			//terminate early, indicate match
			return 1;
		}
	}
	
	//had to check entire string, but strings match
	//indicate match
	return 1;
}

int strlen(unsigned char *s1)
{
	int i;
	
	for (i = 0; i < LINE_LEN && s1[i] != 0; i++);
	
	return i;
}

int separator(unsigned char c)
{
	//check if separator unsigned char
	if(c < 0x30 || (c > 0x39 && c < 0x41) || (c > 0x5A && c < 0x61 && c != '_') || c > 0x7A) return 1;
	else return 0;
}

int whitespace(unsigned char c)
{
	//check if whitespace unsigned char
	if(c == 0x09 || c == 0x20 || c == 0x00) return 1;
	else return 0;
}

unsigned char read_byte()
{
	if (IS_EOF || IS_ERROR)
	{
		fclose(infile[inclevel]);
		inclevel--;
		return 0x00;
	}
	else
	{
		return fgetc(infile[inclevel]);
	}
}

void eat_whitespace()
{
	//trim off leading space and tab unsigned chars
	do
	{
		buf = read_byte();
	}
	while (NOT_EOF && (whitespace(buf) != 0));
}

void get_string(unsigned char termchar)
{
	int i;
	int j;
	int k;
	
	unsigned char val;
	
	unsigned char finished;
	
	finished = 0;
	
	//printf("get_string termchar = %c\n", termchar);
	
	//read in string (likely a file name)
	for (i = 0; i < LINE_LEN && NOT_EOF && (finished == 0); i++)
	{
		if (buf == 0x00)
		{
			buf = read_byte();
		}
		
		//printf("get_string buf = %c\n", buf);
		
		if (buf == '\\' && (termchar == DQ || termchar == SQ))
		{
			buf = read_byte();
			//printf("get_string buf = %c\n", buf);
			
			switch(buf)
			{
				/*
				case 'a':
					//alarm (0x07)
					token[i] = 0x07;
					buf = 0x00;
					break;
					
				case 'b':
					//backspace (0x08)
					token[i] = 0x08;
					buf = 0x00;
					break;

				case 'f':
					//form feed (0x0C)
					token[i] = 0x0C;
					buf = 0x00;
					break;
					
				case 'n':
					//new line (0x0A)
					token[i] = 0x0A;
					buf = 0x00;
					break;
					
				case 'r':
					//carriage return (0x0D)
					token[i] = 0x0D;
					buf = 0x00;
					break;
					
				case 't':
					//horizontal tab (0x09)
					token[i] = 0x09;
					buf = 0x00;
					break;
					
				case 'v':
					//vertical tab (0x0B)
					token[i] = 0x0B;
					buf = 0x00;
					break;
					*/
				
				case ' ':
					//space (0x20)
				case '\\':
					//backslash (0x5C)
					/*
				case '?':
					//question mark (0x3F)
					*/
				case SQ:
					//single quote (0x27)
				case DQ:
					//double quote (0x22)
					token[i] = buf;
					buf = 0x00;
					break;
					
					/*
				case 'x':
					//hex number
					val = 0;
					
					buf = read_byte();
					
					while ((buf >= '0' && buf <= '9') || (buf >= 'A' && buf <= 'F') || (buf >= 'a' && buf <= 'f'))
					{
						val = (val << 4) + 0x0F;
						switch(buf)
						{
							case '0':
								val--;
							case '1':
								val--;
							case '2':
								val--;
							case '3':
								val--;
							case '4':
								val--;
							case '5':
								val--;
							case '6':
								val--;
							case '7':
								val--;
							case '8':
								val--;
							case '9':
								val--;
							case 'A':
							case 'a':
								val--;
							case 'B':
							case 'b':
								val--;
							case 'C':
							case 'c':
								val--;
							case 'D':
							case 'd':
								val--;
							case 'E':
							case 'e':
								val--;
							case 'F':
							case 'f':
								break;
							default:
								break;
						}
						buf = read_byte();
					}
					
					token[i] = val;
					break;
					*/
				
				default:
					if (buf == termchar)
					{
						//escaped the string terminating character
						token[i] = buf;
					}
					/*
					else
					{
						//octal number	
					
						val = 0;
					
						while (buf >= '0' && buf <= '7')
						{
							val = (val << 3) + (buf - '0');
							buf = read_byte();
						}
					
						token[i] = val;
					}
					*/
					buf = 0x00;
					break;
			}
		}
		else
		{
			if (buf == termchar)
			{
				//received string terminator
				printf("get_string got termchar\n");
				finished = 1;
			}
			else
			{
				if(buf != LF)
				{
					token[i] = buf;
					//printf("getstring got char '%c'\n", buf);
				}
				else
				{
					token[i] = 0x00;
				}
				buf = 0x00;
			}
		}
		
		
	}
	
	//printf("get_string i = %i\n", i);
	//printf("got string '%s'\n", token);
	
	return;
}

void get_string_no_esc(unsigned char termchar)
{
	int i;
	int j;
	int k;
	
	unsigned char val;
	
	unsigned char finished;
	
	finished = 0;
	
	//printf("get_string termchar = %c\n", termchar);
	
	//read in string (likely a file name)
	for (i = 0; i < LINE_LEN && NOT_EOF && (finished == 0); i++)
	{
		if (buf == 0x00)
		{
			buf = read_byte();
		}
		
		//printf("get_string buf = %c\n", buf);
		
		if (buf == termchar)
		{
			//received string terminator
			printf("get_string got termchar\n");
			finished = 1;
		}
		else
		{
			if(buf != LF)
			{
				token[i] = buf;
				//printf("getstring got char '%c'\n", buf);
			}
			else
			{
				token[i] = 0x00;
			}
			buf = 0x00;
		}
	}
	
	//printf("get_string i = %i\n", i);
	//printf("got string '%s'\n", token);
	
	return;
}


void get_token()
{
	int i;
	
	clearstr(token);
	
	eat_whitespace();
	
	//read in next token
	i = 1;
	
	token[0] = buf;
	
	while (i < LINE_LEN && (NOT_EOF && (separator(buf) == 0)))
	{
		buf = read_byte();
		//read until a separator unsigned char is detected
		if (separator(buf) == 0) token[i] = buf;
		i++;
	}
	
	if(i == 1 && token[0] > 0x7F) clearstr(token);
	
	//printf("token=%s\n", token);
	//printf("token len=%i\n", i);
	
	return;
}

int ppdef_readback(int def, int start)
{
	int i;
	int j;
	
	clearstr(token);
	
	i = start;
	j = 0;
	
	//trim off leading space and tab unsigned chars
	do
	{
		rbuf = defs[def][i];
		i++;
	}
	while (rbuf != 0x00 && (whitespace(rbuf) != 0));
	
	token[j] = rbuf;
	j++;
	
	if(separator(rbuf) != 0)
	{
		printf("readback got separator\n");
		printf("readback token=%s\n", token);
		printf("readback token pos=%i\n", i);
		printf("readback token len=%i\n", j);
		return i;
	}
	
	while (i < LINE_LEN && j < LINE_LEN && (rbuf != 0x00 && (separator(rbuf) == 0)))
	{
		rbuf = defs[def][i];
		//read until a separator unsigned char is detected
		if (separator(rbuf) == 0)
		{
			token[j] = rbuf;
			i++;
		}
		j++;
	}
	
	if(i == 1 && token[0] > 0x7F) clearstr(token);
	
	printf("readback token=%s\n", token);
	printf("readback token pos=%i\n", i);
	printf("readback token len=%i\n", j);
	
	return i;
}

void expand_def(unsigned char *s)
{
	//expand pp definition
	int i;
	int found;
	
	found = 0;
	
	for(i = 0; i < MAX_DEFS; i++)
	{
		if(strcmp(s, defnames[i]) != 0)
		{
			//definition found, replace with defined value
			copy_str(defs[i], converted);
			found = 1;
		}
	}
	
	if (found == 0)
	{
		clearstr(converted);
	}
}

int main(int argc, unsigned char **argv)
{
	int i;
	int j;
	int k;
	
	i = 0;
	j = 0;
	k = 0;
	
	iflevel = 0;
	inclevel = 0;
	
	pp_ignore = 0;
	
	infile[inclevel] = fopen(argv[1], "r");
	if (infile[inclevel] == 0)
	{
		printf("Could not open input file.\n");
		return -1;
	}
	
	outfile = fopen("preproc.c", "a");
	if (outfile == 0)
	{
		printf("Could not open output file.\n");
		fclose(infile[inclevel]);
		return -1;
	}
	
	//initialize strings (clear first byte to 0)
	clearstr(token);
	for(i = 0; i < MAX_DEFS; i++)
	{
		clearstr(defs[i]);
		clearstr(defnames[i]);
	}
	
	do
	{
		if (IS_EOF)
		{
			fclose(infile[inclevel]);
			inclevel--;
		}
		
		//get next token
		get_token();
		
		switch(token[0])
		{
			case 0:
				//printf("null token\n");
				//token string empty
				//likely hit end of file
				
				if (inclevel == 0)
				{
					//printf("Unexpected end of file.\n");
				}
				else
				{
					//close file
					fclose(infile[inclevel]);
					inclevel--;
				}
				break;
				
			case SQ:
				if (iflevel <= pp_ignore)
				{
					printf("sq literal\n");
					//single quote string literal started
					//fputc(token[0], outfile);
					buf = 0x00;
					fputc(SQ, outfile);
					get_string_no_esc(SQ);
					fputs(token, outfile);
					fputc(SQ, outfile);
					//GO_TO_END_SQ;
				}
				break;
				
			case DQ:
				if (iflevel <= pp_ignore)
				{
					printf("dq literal\n");
					//double quote string literal started
					//fputc(token[0], outfile);
					buf = 0x00;
					fputc(DQ, outfile);
					get_string_no_esc(DQ);
					fputs(token, outfile);
					fputc(DQ, outfile);
					//GO_TO_END_DQ;
				}
				break;
				
			case '/':
				if (iflevel <= pp_ignore)
				{
					//check if comment
					get_token();
					switch(token[0])
					{
						case '*':
							printf("ml comment\n");
							//multiline comment
							GO_TO_END;
							break;
						
						case '/':
							clearstr(token);
							get_string(LF);
							printf("sl comment: '%s'\n", token);
							//GET_NEXT_LINE;
							break;
					
						default:
							//printf("not comment: %s\n", token);
							//not a comment
							fputc('/', outfile);
							fputs(token, outfile);
							if(separator(token[0]) != 1)
							{
								if (buf != 0x00 && buf != 0xFF) fputc(buf, outfile);
							}
							break;
					}
				}
				break;
				
			case '#':
				//preprocessor directive
				//printf("pp statement\n");
				
				get_token();
				copy_str(token, converted);
				toupper(converted);
				
				if (strcmp(converted, "IFDEF") != 0)
				{
					//got ifdef, check if token is defined
					if (iflevel <= pp_ignore)
					{
						//search definition list for token
						get_token();
						for(i = 0; i < MAX_DEFS; i++)
						{
							if(strcmp(token, defnames[i]) != 0)
							{
								//printf("ifdef token found: %s\n", token);
								pp_ignore++;
							}
						}
					}
					else
					{
						//told to ignore this pp statement
					}
					iflevel++;
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "IFNDEF") != 0)
				{
					//got ifndef, check if token is defined
					if (iflevel <= pp_ignore)
					{
						//search definition list for token
						get_token();
						
						int found;
						
						found = 0;
						
						for(i = 0; i < MAX_DEFS; i++)
						{
							if(strcmp(token, defnames[i]) != 0) found = 1;
						}
						
						if (found == 0)
						{
							//printf("ifndef token not found: %s\n", token);
							pp_ignore++;
						}
						else
						{
							//printf("ifndef token found: %s\n", token);
						}
					}
					else
					{
						//told to ignore this pp statement
					}
					iflevel++;
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "ELSE") != 0)
				{
					//got else
					if (iflevel <= pp_ignore)
					{
						//not currently ignoring anything above current level
					}
					else
					{
						//still include in iflevel calcs
						//iflevel++;
						pp_ignore++;
					}
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "DEFINE") != 0)
				{
					//add definition for token
					if (iflevel <= pp_ignore)
					{
						int openslot = -1;
						
						get_token();
						
						for (i = 0; i < MAX_DEFS; i++)
						{
							//definition found, erase it
							if(strcmp(token, defnames[i]) != 0)
							{
								clearstr(defnames[i]);
							}
							
							if(strcmp("\0", defnames[i]) != 0)
							{
								openslot = i;
							}
						}
						
						if (openslot > -1)
						{
							//insert definition identifier into list
							copy_str(token, defnames[openslot]);
							
							printf("Added definition for '%s'\n", token);
							
							//insert definition into list
							clearstr(token);
							eat_whitespace();
							get_string(LF);
							clearstr(defs[openslot]);
							copy_str(token, defs[openslot]);
							
							//expand any macros already defined
							j = 0;
							clearstr(expanded);
							
							while(strcmp("\0", token) == 0)
							{
								j = ppdef_readback(openslot, j);
								printf("token = '%s'\n", token);
								
								if(separator(token[0]) != 1)
								{
									append_str(expanded, " ");
									
									expand_def(token);
								
									if(strcmp("\0", converted) == 0)
									{
										//substitution made
										printf("Token '%s' expanded to '%s'\n", token, converted);
										append_str(expanded, converted);
									}
									else
									{
										append_str(expanded, token);
									}
								}
								else
								{
									append_str(expanded, token);
								}
								
								printf("Expanded string = '%s'\n", expanded);
								
								
							}
							
							copy_str(expanded, defs[openslot]);
							
							printf("Defined as: %s\n", defs[openslot]);
						}
						else
						{
							printf("Out of space for new definitions.\n");
							return -1;
						}
					}
					else
					{
						//told to ignore this pp statement
					}
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "ENDIF") != 0)
				{
					//end current ifdef/ifndef block
					//throw error if iflevel = 0
					if(iflevel == 0)
					{
						printf("#endif without matching #if/#ifdef/#ifndef.\n");
					}
					else
					{
						//printf("#endif iflevel = %i, decrementing\n", iflevel);
						if (iflevel == pp_ignore) pp_ignore--;
						iflevel--;
					}
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "INCLUDE") != 0)
				{
					//include file specified
					if (iflevel <= pp_ignore)
					{
						//printf("#include statement\n");
						//not currently ignoring anything above current level
						
						//printf("#include getting file name.\n");
						//printf("#include buffer unsigned char = %c\n", buf);
							
						get_token();
						//printf("#include token=%s\n", token);
							
						if (token[0] != '<' && token[0] != DQ)
						{
							//invalid file name given
							printf("invalid file name in #include.\n");
							return -1;
						}
						else if(token[0] == '<')
						{
							clearstr(token);
							buf = 0x00;
							get_string('>');
						}
						else if (token[0] == DQ)
						{
							clearstr(token);
							buf = 0x00;
							get_string(DQ);
						}
						
						//printf("#include file name: %s\n", token);
						
						inclevel++;
						if(inclevel >= MAX_INCLUDES)
						{
							printf("#include statements nested too deep (max is %i).\n", MAX_INCLUDES);
							return -1;
						}
							
						printf("Opening include file '%s'\n", token);
						
						infile[inclevel] = fopen(token, "r");
						
						if (errno != 0)
						{
							printf("Error opening file '%s': %d (%s)\n", token, errno, strerror(errno));
							return -1;
						}
						
						if (infile == 0)
						{
							printf("Error opening include file '%s'.\n", token);
							return -1;
						}
						
						if (IS_ERROR)
						{
							printf("ferror result not zero.\n");
							return -1;
						}
						
						//printf("Include file opened.\n");
					}
					else
					{
						//told to ignore this pp statement
					}
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "UNDEF") != 0)
				{
					//remove definition for token
					if (iflevel <= pp_ignore)
					{
						//not currently ignoring anything above current level
						get_token();
						for (i = 0; i < MAX_DEFS; i++)
						{
							//if definition found, erase it
							if(strcmp(token, defnames[i]) != 0)
							{
								clearstr(defnames[i]);
								//printf("Removed definition for '%s'\n", token);
							}
						}
					}
					else
					{
						//told to ignore this pp statement
					}
					//GET_NEXT_LINE;
				}
				else if (strcmp(converted, "ERROR") != 0)
				{
					//display error message
					if (iflevel <= pp_ignore)
					{
						//not currently ignoring anything above current level
						eat_whitespace();
						get_string(LF);
						//printf("%s\n", token);
					}
					else
					{
						//told to ignore this pp statement
					}
					//GET_NEXT_LINE;
				}
				break;
				
			default:
				if(iflevel <= pp_ignore)
				{
					//something else encountered, just pass it through
					//printf("pp default - token: %s\n", token);
					
					//expand any macros already defined
					expand_def(token);
					
					printf("Token = '%s'\n", token);
					
					if(strcmp("\0", converted) == 0)
					{
						printf("Token '%s' expanded to '%s'\n", token, converted);
						copy_str(converted, token);
					}
					
					fputs(token, outfile);
					
					printf("Buffer = '%c'\n", buf);
					
					if(separator(token[0]) != 1)
					{
						
					}
					
					if (buf != 0x00 && buf != 0xFF && buf != token[0])
					{
						printf("Wrote buffer byte to file.\n");
						fputc(buf, outfile);
						buf = 0x00;
					}
				}
				else
				{
					//told to ignore this pp statement
				}
				break;
		}
		
		//printf("iflevel = %i\n", iflevel);
		//printf("pp_ignore = %i\n\n", pp_ignore);
		printf("----------------------------\n");
		
	} while (NOT_EOF || inclevel > 0);
	
	for (i = 0; i <= inclevel; i++) fclose(infile[i]);
	
	fclose(outfile);
	
	if (iflevel > 0)
	{
		printf("Missing #endif - need %i more.\n", iflevel);
	}
	
	return 0;
}
