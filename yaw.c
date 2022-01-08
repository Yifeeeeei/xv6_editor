// #include "types.h"
// #include "stat.h"
// #include "user.h"
#include "kernel/fcntl.h"
// #include "fs.h"

#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

/* 基本参数属性定义 */
#define BUF_SIZE 256
#define MAX_LINE_NUMBER 256
#define MAX_LINE_LENGTH 256
#define MAX_ROLLBAKC_STEP 20
#define NULL 0

char *strcat_n(char *dest, char *src, int len); //用于字符串拼接
int get_line_number(char *text[]);
void show_text(char *text[]);
int com_write(char *text[], int n, char *extra, int flag);	 //插入命令
void com_modify(char *text[], int n, char *extra, int flag); //修改命令
void com_delete(char *text[], int n, int flag);				 //删除命令
void com_help(char *text[]);								 //显示帮助
void com_save(char *text[], char *path);					 //保存命令
void com_exit(char *text[], char *path);					 //退出编辑器
void com_create_new_file(char *text[], char *path);			 //
void com_init_file(char *text[], char *path);
void show_text_syntax_highlighting(char *text[]);
void com_rollback(char *text[], int n);
void record_command(char *command);
int stringtonumber(char *src);
void number2string(int num, char array[]);
void com_find(char *text[], char *extra);
void com_changemode(); //改变模式 文字模式-代码模式

/*全局变量区*/

//标记是否更改过
int changed = 0;
int auto_show = 1;
//存储当前最大的行号，从0开始。即若line_number == x，则从text[0]到text[x]可用
int line_number = 0;
// 记录命令，作为回滚的凭借
char *command_set[MAX_ROLLBAKC_STEP] = {};
int upper_bound = -1;
char keyword[100] = {'\0'};
int searching = 0;
int text_mode = 0; //模式：0-正常文字模式；1-代码模式，开启代码高亮
char opening_line[] = "____Welcome to YAW ----Yet Another Word for xv6____\n ____Input 'help' to get started!____";
char logo[] = "        \e[1;32m '%&$'    `|&%'\e    \e[1;34m.|&&|`\e	     \e[1;33m`%&%'    '%&$'    :$&;\e[0m         /-----/\n\
        \e[1;32m `$##!   !##|\e      \e[1;34m:@###&'\e	     \e[1;33m;##&'   ;###@:   !##!\e[0m        /     /\n\
        \e[1;32m  |##% '&#$`\e      \e[1;34m|#@|$#&'\e	     \e[1;33m!##%.  |#@@#&`  |##;\e[0m        /     /\n\
        \e[1;32m  ;##&&#@:\e      \e[1;34m'&#%.`$#@:\e	     \e[1;33m|##! .%#%;&#%..%#&'\e[0m        /     /\n\
        \e[1;32m  '&###!\e       \e[1;34m!##!  `$#@:\e	    \e[1;33m.%##;'&#! :@#|`%#$`\e[0m       /     /\n\
        \e[1;32m  '&#&'\e      \e[1;34m`$#########@:\e	    \e[1;33m`$#&$@#;  ;##$$#|\e[0m        /     /\n\
        \e[1;32m  |##!\e      \e[1;34m;##$`    '&##:\e	    \e[1;33m'&###&'   !####!\e[0m        /     /\n\
        \e[1;32m ;##%.\e    \e[1;34m.%##|      '&##;\e	    \e[1;33m:@##$`    |##@:\e[0m        /     /\n\
     ________________________________________________/     /\n\
   _/_/  O O O O O O O O O O O O O O O O O O    | .|      /\n\
  (_____________________________________________|__|_____/\n";
int main(int argc, char *argv[])
{
	int is_new_file = 0;

	printf("\n%s\n\n%s\n\n", opening_line, logo);

	if (argc == 1)
	{
		fprintf(1, "YAW-> \e[1;31mplease input the command as [editor file_name]\n\e[0m");
		exit(0);
	}

	//存放文件内容
	char *text[MAX_LINE_NUMBER] = {};
	text[0] = malloc(MAX_LINE_LENGTH);
	memset(text[0], 0, MAX_LINE_LENGTH);

	//尝试打开文件
	int fd = open(argv[1], O_RDONLY);
	//如果文件存在，则打开并读取里面的内容
	if (fd != -1)
	{
		// fprintf(1, "YAW-> \e[1;33mfile exist\n\e[0m");
		char buf[BUF_SIZE] = {};
		int len = 0;
		while ((len = read(fd, buf, BUF_SIZE)) > 0)
		{
			int i = 0;
			int next = 0;
			int is_full = 0;
			while (i < len)
			{
				//拷贝"\n"之前的内容
				for (i = next; i < len && buf[i] != '\n'; i++)
					;
				strcat_n(text[line_number], buf + next, i - next);
				//必要时新建一行
				if (i < len && buf[i] == '\n')
				{
					if (line_number >= MAX_LINE_NUMBER - 1)
						is_full = 1;
					else
					{
						line_number++;
						text[line_number] = malloc(MAX_LINE_LENGTH);
						memset(text[line_number], 0, MAX_LINE_LENGTH);
					}
				}
				if (is_full == 1 || i >= len - 1)
					break;
				else
					next = i + 1;
			}
			if (is_full == 1)
				break;
		}
		close(fd);
	}
	else
	{
		com_create_new_file(text, argv[1]);
		is_new_file = 1;
	}

	//输出文件内容,不是新建空文件时才输出
	if (!is_new_file && auto_show)
	{
		show_text_syntax_highlighting(text);
	}
	//输出帮助
	// com_help(text);

	int write_mode = 0;

	//处理命令
	char input[MAX_LINE_LENGTH] = {};
	while (1)
	{
		if (write_mode == 0)
		{
			// fprintf(1, "YAW-> \e[1;33mplease input command:\n\e[0m");
			fprintf(1, "YAW-> \e[1;33m\e[0m");
			memset(input, 0, MAX_LINE_LENGTH);
			gets(input, MAX_LINE_LENGTH);
			int len = strlen(input);
			input[len - 1] = '\0';
			len--;
		}

		//寻找命令中第一个空格
		int pos = MAX_LINE_LENGTH - 1;
		int j = 0;
		for (; j < 8; j++)
		{
			if (input[j] == ' ')
			{
				pos = j + 1;
				break;
			}
		}
		// write
		if (strncmp(input, "write", 5) == 0)
		{
			if (input[5] == '-' && stringtonumber(&input[6]) >= 0)
			{
				com_write(text, stringtonumber(&input[6]), &input[pos], 1);
				//插入操作需要更新行号
				line_number = get_line_number(text);
			}
			else if (input[5] == ' ' || input[5] == '\0')
			{
				write_mode = com_write(text, line_number + 1 + 1, &input[pos], 1);
				printf("write_mode: %d\n", write_mode);
				if (write_mode != 0)
					line_number = get_line_number(text);
			}
			else
			{
				// fprintf(1, "YAW-> \033[1m\e[43;31minvalid command.\e[0m\n");
				fprintf(1, "YAW-> \033[1m\e[41;33minvalid command.\e[0m\n");
				// com_help(text);
			}
		}
		// find
		else if (strncmp(input, "find", 4) == 0)
		{
			if (input[5])
				com_find(text, &input[5]);
		}
		// modify
		else if (strncmp(input, "modify", 6) == 0)
		{
			if (input[6] == '-' && stringtonumber(&input[7]) >= 0)
				com_modify(text, atoi(&input[7]), &input[pos], 1);
			else if (input[6] == ' ' || input[6] == '\0')
				com_modify(text, line_number + 1, &input[pos], 1);
			else
			{
				fprintf(1, "YAW-> \033[1m\e[41;33minvalid command.\e[0m\n");
				// com_help(text);
			}
		}
		// delete
		else if (strncmp(input, "delete", 6) == 0)
		{
			if (input[6] == '-' && stringtonumber(&input[7]) >= 0)
			{
				com_delete(text, stringtonumber(&input[7]), 1);
				//删除操作需要更新行号
				line_number = get_line_number(text);
			}
			else if (input[6] == '\0')
			{
				com_delete(text, line_number + 1, 1);
				line_number = get_line_number(text);
			}
			else
			{
				fprintf(1, "YAW-> \033[1m\e[41;33minvalid command.\e[0m\n");
				// com_help(text);
			}
		}
		else if (strcmp(input, "show") == 0)
		{
			auto_show = 1;
			fprintf(1, "YAW-> \e[1;33menable show current contents after text changed.\n\e[0m");
		}
		else if (strcmp(input, "hide") == 0)
		{
			auto_show = 0;
			fprintf(1, "YAW-> \e[1;33mdisable show current contents after text changed.\n\e[0m");
		}
		// rollback
		else if (strcmp(input, "rollback") == 0)
		{
			com_rollback(text, 1);
		}
		else if (strcmp(input, "help") == 0)
			com_help(text);
		// save
		else if (strcmp(input, "save") == 0)
			com_save(text, argv[1]);
		else if (strcmp(input, "exit") == 0)
			com_exit(text, argv[1]);
		else if (strcmp(input, "changemode") == 0)
		{
			com_changemode();
		}
		else if (strcmp(input, "init") == 0)
			com_init_file(text, argv[1]);
		else if (strcmp(input, "display") == 0)
		{
			show_text_syntax_highlighting(text);
		}
		// else if (strcmp(input, "normaldisp") == 0)
		// {
		// 	show_text(text);
		// }
		else
		{
			fprintf(1, "YAW-> \033[1m\e[41;33minvalid command.\e[0m\n");
			// com_help(text);
		}
	}
	// setProgramStatus(SHELL);

	exit(0);
}

//拼接src的前n个字符到dest
char *strcat_n(char *dest, char *src, int len)
{
	if (len <= 0)
		return dest;
	int pos = strlen(dest);
	if (len + pos >= MAX_LINE_LENGTH)
		return dest;
	int i = 0;
	for (; i < len; i++)
		dest[i + pos] = src[i];
	dest[len + pos] = '\0';
	return dest;
}

void show_text(char *text[])
{
	fprintf(1, "YAW-> \033[1m\e[45;33mthe contents of the file are:\e[0m\n");
	fprintf(1, "\n");
	int j = 0;
	for (; text[j] != NULL; j++)
		if (strcmp(text[j], "\n") == 0)
		{
			fprintf(1, "\e[1;30m%d%d%d\e[0m\e[0;32m|\e[0m\n", (j + 1) / 100, ((j + 1) % 100) / 10, (j + 1) % 10);
		}
		else
		{
			fprintf(1, "\e[1;30m%d%d%d\e[0m\e[0;32m|\e[0m%s\n", (j + 1) / 100, ((j + 1) % 100) / 10, (j + 1) % 10, text[j]);
		}
	fprintf(1, "\n");
}

//获取当前最大的行号，从0开始，即return x表示text[0]到text[x]可用
int get_line_number(char *text[])
{
	int i = 0;
	for (; i < MAX_LINE_NUMBER; i++)
		if (text[i] == NULL)
			return i - 1;
	return i - 1;
}

int stringtonumber(char *src)
{
	int number = 0;
	int i = 0;
	int pos = strlen(src);
	for (; i < pos; i++)
	{
		if (src[i] == ' ')
			break;
		if (src[i] > 57 || src[i] < 48)
			return -1;
		number = 10 * number + (src[i] - 48);
	}
	return number;
}

void number2string(int num, char array[])
{
	char array_rvs[20] = {};
	int i, sign;
	if ((sign = num) < 0) // record the sign
		num = -num;		  // make num into positive number
	i = 0;
	do
	{
		array_rvs[i++] = num % 10 + '0'; // fatch the next number
	} while ((num /= 10) > 0);			 // delete this number
	if (sign < 0)
		array_rvs[i++] = '-';
	array_rvs[i] = '\0';
	int length = strlen(array_rvs);
	for (int j = 0; j < length; j++)
	{
		array[j] = array_rvs[length - 1 - j];
	}
	array[length] = '\0';
}

//插入命令，n为用户输入的行号，从1开始
// extra:输入命令时接着的信息，代表待插入的文本
int com_write(char *text[], int n, char *extra, int flag)
{
	if (n <= 0 || n > get_line_number(text) + 1 + 1)
	{
		fprintf(1, "YAW-> \033[1m\e[41;33minvalid line number\e[0m\n");
		return 1;
	}
	char input[MAX_LINE_LENGTH] = {};
	if (*extra == '\0')
	{
		fprintf(1, "... \e[1;35minput content:\e[0m");
		gets(input, MAX_LINE_LENGTH);
		input[strlen(input) - 1] = '\0';
	}
	else
		strcpy(input, extra);

	if (strcmp(input, ":exit") == 0)
	{
		return 0;
	}

	char *part4 = malloc(MAX_LINE_LENGTH);
	if (flag)
	{
		strcpy(part4, text[n - 1]);
	}

	int i = MAX_LINE_NUMBER - 1;
	for (; i > n - 1; i--)
	{
		if (text[i - 1] == NULL)
			continue;
		else if (text[i] == NULL && text[i - 1] != NULL)
		{
			text[i] = malloc(MAX_LINE_LENGTH);
			memset(text[i], 0, MAX_LINE_LENGTH);
			strcpy(text[i], text[i - 1]);
		}
		else if (text[i] != NULL && text[i - 1] != NULL)
		{
			memset(text[i], 0, MAX_LINE_LENGTH);
			strcpy(text[i], text[i - 1]);
		}
	}
	// couldn't understand what this code block means
	// maybe it allocates space for text[n-1] to avoid none space of text[n-1]
	if (text[n - 1] == NULL)
	{
		text[n - 1] = malloc(MAX_LINE_LENGTH);
		if (text[n - 2][0] == '\0')
		{
			memset(text[n - 1], 0, MAX_LINE_LENGTH);
			strcpy(text[n - 2], input);
			changed = 1;
			if (auto_show == 1)
				show_text_syntax_highlighting(text);
			return 1;
		}
	}
	memset(text[n - 1], 0, MAX_LINE_LENGTH);
	strcpy(text[n - 1], input);
	changed = 1;

	if (flag)
	{ // 非rollback的调用时才记录命令
		// record the command into command_set
		char *command = malloc(MAX_LINE_LENGTH);
		char part1[] = "write-";
		char part2[10];
		number2string(n, part2);
		char part3[] = " \0";
		strcat_n(part1, part2, strlen(part2));
		strcat_n(part1, part3, strlen(part3));
		strcat_n(part1, part4, strlen(part4));
		strcpy(command, part1);
		record_command(command);
	}

	if (auto_show == 1)
		show_text_syntax_highlighting(text);

	return 1;
}

void com_find(char *text[], char *extra)
{
	strcpy(keyword, extra);
	searching = 1;
	if (auto_show == 1)
		show_text_syntax_highlighting(text);
}
//改变模式，对text_mode进行操作
void com_changemode()
{
	if (text_mode == 0)
	{
		text_mode = 1;
	}
	else if (text_mode == 1)
	{
		text_mode = 0;
	}
}

//修改命令，n为用户输入的行号，从1开始
// extra:输入命令时接着的信息，代表待修改成的文本
void com_modify(char *text[], int n, char *extra, int flag)
{
	if (n <= 0 || n > get_line_number(text) + 1)
	{
		fprintf(1, "YAW-> \033[1m\e[41;33minvalid line number\e[0m\n");
		return;
	}
	char input[MAX_LINE_LENGTH] = {};
	if (*extra == '\0')
	{
		fprintf(1, "... \e[1;35minput content:\e[0m");
		gets(input, MAX_LINE_LENGTH);
		input[strlen(input) - 1] = '\0';
	}
	else
		strcpy(input, extra);

	char *part4 = malloc(MAX_LINE_LENGTH);
	if (flag)
	{
		strcpy(part4, text[n - 1]);
	}

	memset(text[n - 1], 0, MAX_LINE_LENGTH);
	strcpy(text[n - 1], input);
	changed = 1;

	if (flag)
	{ // 非rollback调用才记录
		// record the command into command_set
		char *command = malloc(MAX_LINE_LENGTH);
		char part1[] = "modify-";
		char part2[10];
		number2string(n, part2);
		char part3[] = " \0";
		strcat_n(part1, part2, strlen(part2));
		strcat_n(part1, part3, strlen(part3));
		strcat_n(part1, part4, strlen(part4));
		strcpy(command, part1);
		record_command(command);
	}

	if (auto_show == 1)
		show_text_syntax_highlighting(text);
}

//删除命令，n为用户输入的行号，从1开始
void com_delete(char *text[], int n, int flag)
{
	if (n <= 0 || n > get_line_number(text) + 1)
	{
		// fprintf(1, "n: %d\n", n);
		fprintf(1, "YAW-> \033[1m\e[41;33minvalid line number\e[0m\n");
		return;
	}

	char *part4 = malloc(MAX_LINE_LENGTH);
	if (flag)
	{
		strcpy(part4, text[n - 1]);
	}

	memset(text[n - 1], 0, MAX_LINE_LENGTH);
	int i = n - 1;
	for (; text[i + 1] != NULL; i++)
	{
		strcpy(text[i], text[i + 1]);
		memset(text[i + 1], 0, MAX_LINE_LENGTH);
	}
	if (i != 0)
	{
		free(text[i]);
		text[i] = 0;
	}
	changed = 1;

	// 有bug，实在解决不了，所以delete不提供撤回功能

	if (auto_show == 1)
		show_text_syntax_highlighting(text);
}

void com_save(char *text[], char *path)
{
	//删除旧有文件
	unlink(path);
	//新建文件并打开
	int fd = open(path, 1 | O_CREATE);
	if (fd == -1)
	{
		fprintf(1, "YAW-> \033[1m\e[41;33msave failed, file can't open:\e[0m\n");
		// setProgramStatus(SHELL);
		exit(0);
	}
	if (text[0] == NULL)
	{
		close(fd);
		return;
	}
	//写数据
	write(fd, text[0], strlen(text[0]));
	int i = 1;
	for (; text[i] != NULL; i++)
	{
		fprintf(fd, "\n");
		write(fd, text[i], strlen(text[i]));
	}
	close(fd);
	fprintf(1, "YAW-> \e[1;32msaved successfully\e[0m\n");
	changed = 0;
	return;
}

void com_exit(char *text[], char *path)
{
	//询问是否保存
	while (changed == 1)
	{
		fprintf(1, "YAW-> \e[1;33msave the file?\e[0m \033[1m\e[46;33my\e[0m/\033[1m\e[41;33mn\e[0m\n");
		char input[MAX_LINE_LENGTH] = {};
		gets(input, MAX_LINE_LENGTH);
		input[strlen(input) - 1] = '\0';
		if (strcmp(input, "y") == 0)
			com_save(text, path);
		else if (strcmp(input, "n") == 0)
			break;
		else
			fprintf(2, "YAW-> \e[1;31mwrong answer?\e[0m\n");
	}
	//释放内存
	int i = 0;
	for (; text[i] != NULL; i++)
	{
		free(text[i]);
		text[i] = 0;
	}
	//退出
	// setProgramStatus(SHELL);
	exit(0);
}

// create new file
void com_create_new_file(char *text[], char *path)
{
	int fd = open(path, O_WRONLY | O_CREATE);
	if (fd == -1)
	{
		fprintf(1, "YAW-> \e[1;31mcreate file failed\e[0m\n");
		exit(0);
	}
}

void com_help(char *text[])
{
	fprintf(1, "\e[1;32mhelp information:\n\e[0m");
	fprintf(1, "\e[1;33mhelp\e[0m        | help information\n");
	fprintf(1, "\e[1;30mwrite\e[0m       | write any lines after last line,input \":exit\" to exit write mode\n");
	fprintf(1, "\e[1;31mwrite-n\e[0m     | write a line after line n\n");
	fprintf(1, "\e[1;32mmodify\e[0m      | modify the last line\n");
	fprintf(1, "\e[1;33mmodify-n\e[0m    | modify nth line \n");
	fprintf(1, "\e[1;34mdelete\e[0m      | delete the last line\n");
	fprintf(1, "\e[1;35mdelete-n\e[0m    | delete nth line \n");
	fprintf(1, "\e[1;36mfind keyword\e[0m| find keyword\n");
	fprintf(1, "\e[1;37mshow\e[0m        | enable show current contents after executing a command.\n");
	fprintf(1, "\e[1;30mhide\e[0m        | disable show current contents after executing a command.\n");
	fprintf(1, "\e[1;31mrollback\e[0m    | rollback the file\n");
	fprintf(1, "\e[1;32mdisplay\e[0m     | display current file\n");
	fprintf(1, "\e[1;33msave\e[0m        | save the file\n");
	fprintf(1, "\e[1;32mchangemode\e[0m  | show/hide code syntaxing\n");
	fprintf(1, "\e[1;34mexit\e[0m        | exit editor\n");
}

// 预留数据
void com_init_file(char *text[], char *path)
{
	char *buf[MAX_LINE_NUMBER] = {};
	for (int i = 0; i < MAX_LINE_NUMBER; i++)
	{
		buf[i] = malloc(MAX_LINE_LENGTH);
	}
	strcpy(buf[0], "// Create a NULL-terminated string by reading the provided file");
	strcpy(buf[1], "static char* readShaderSource(const char* shaderFile)");
	strcpy(buf[2], "{");
	strcpy(buf[3], "	int flag = 24;");
	strcpy(buf[4], "	double ways = 100.43;");
	strcpy(buf[5], "	if ( fp == NULL ) {");
	strcpy(buf[6], "		return NULL;");
	strcpy(buf[7], "	}");
	strcpy(buf[8], "	fseek(fp, 0L, SEEK_END);	//search something");
	strcpy(buf[9], "	long size = ftell(fp);");
	strcpy(buf[10], "	fseek(fp, 0L, SEEK_SET);");
	strcpy(buf[11], "	char* buf = new char[size + 1];");
	strcpy(buf[12], "	memset(buf, 0, size + 1);	//Initiate every bit of buf as 0");
	strcpy(buf[13], "	fread(buf, 1, size, fp);");
	strcpy(buf[14], "	buf[size] = '\\0';");
	strcpy(buf[15], "	fclose(fp);			// close 'fp' stream.");
	strcpy(buf[16], "	return buf;");
	strcpy(buf[17], "	while (flag != 0){");
	strcpy(buf[18], "		ways = ways + ways * 12;");
	strcpy(buf[19], "	}");
	strcpy(buf[20], "	for (int a = 10; a >= 0; a--){");
	strcpy(buf[21], "		float tmp_value = 20.5;");
	strcpy(buf[22], "		fprintf(\"the real value of variable tmp_value is:%f\", tmp_value);");
	strcpy(buf[23], "		continue;");
	strcpy(buf[24], "	}");
	strcpy(buf[25], "}");
	strcpy(buf[26], "// demo | made by Shaun Fong");

	// 将数据覆盖进text的空间中
	for (int i = 0; i <= 26; i++)
	{
		text[i] = malloc(MAX_LINE_LENGTH);
		strcpy(text[i], buf[i]);
	}

	line_number = get_line_number(text);

	show_text_syntax_highlighting(text);

	changed = 1;
}

// 语法高亮
void show_text_syntax_highlighting(char *text[])
{
	fprintf(1, "YAW-> \033[1m\e[45;33mthe contents of the file are:\e[0m\n");
	if (text_mode == 0)
	{
		fprintf(1, "YAW-> \033[1m\e[45;33mshown in text mode:\e[0m\n");
	}
	else if (text_mode == 1)
	{
		fprintf(1, "YAW-> \033[1m\e[45;33mshown in code mode:\e[0m\n");
	}
	fprintf(1, "\n");
	int j = 0;
	for (; text[j] != NULL; j++)
	{
		fprintf(1, "\e[1;30m%d%d%d\e[0m\e[0;32m|\e[0m", (j + 1) / 100, ((j + 1) % 100) / 10, (j + 1) % 10);

		// 寻找第一个非空字符
		int pos = 0;
		for (int a = 0; a < MAX_LINE_LENGTH; a++)
		{
			if (text[j][a] != ' ')
			{
				pos = a;
				break;
			}
		}

		if (strcmp(text[j], "\n") == 0)
		{
			fprintf(1, "\n");
		}
		else if (text[j][pos] == '/' && text[j][pos + 1] == '/')
		{
			fprintf(1, "\e[1;32m%s\n\e[0m", text[j]);
		}
		else
		{
			int mark = 0;
			int flag_annotation = 0;
			while (mark < MAX_LINE_LENGTH && text[j][mark] != NULL)
			{
				// do something with 'mark' and print all the statements
				// by the way of one letter by one letter
				// judge annotation
				if (flag_annotation)
				{
					fprintf(1, "\e[1;32m%c\e[0m", text[j][mark++]);
					// mark++;
					continue;
				}
				// keyword
				
				if ((mark + strlen(keyword)) < MAX_LINE_LENGTH && searching && (strncmp(text[j] + mark, keyword, strlen(keyword)) == 0))
				{
					for (int t = 0; t < strlen(keyword); t++)
					{
						fprintf(1, "\e[1;36m%c\e[0m", text[j][mark + t]);
					}
					mark = mark + strlen(keyword);
				}
				// numbers

				else if (text[j][mark] >= '0' && text[j][mark] <= '9')
				{
					fprintf(1, "\033[0;33m%c\033[0m", text[j][mark]);
					mark++;
				}
				
				// else if (text_mode == 1)
				// {
				/*在代码模式下需要显示的东西*/
				// fprintf
				//(text_mode==1)&&
				else if ((text_mode == 1) && ((mark + 5) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "printf", 6) == 0)))
				{
					fprintf(1, "\e[1;36m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;36m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;36m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;36m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;36m%c\e[0m", text[j][mark + 4]);
					fprintf(1, "\e[1;36m%c\e[0m", text[j][mark + 5]);
					mark = mark + 6;
				}
				// int
				else if ((text_mode == 1) && ((mark + 2) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "int", 3) == 0)))
				{
					// highlighting 'int' string
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					mark = mark + 3;
				}
				// float
				else if ((text_mode == 1) && ((mark + 4) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "float", 5) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 4]);
					mark = mark + 5;
				}
				// double
				else if ((text_mode == 1) && ((mark + 5) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "double", 6) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 4]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 5]);
					mark = mark + 6;
				}
				// char
				else if ((text_mode == 1) && ((mark + 3) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "char", 4) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					mark = mark + 4;
				}
				// if
				else if ((text_mode == 1) && ((mark + 1) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "if", 2) == 0)))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					mark = mark + 2;
				}
				// else
				else if ((text_mode == 1) && ((mark + 3) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "else", 4) == 0)))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 3]);
					mark = mark + 4;
				}
				// else if
				else if ((text_mode == 1) && ((mark + 5) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "else if", 7) == 0)))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 4]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 5]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 6]);
					mark = mark + 7;
				}
				// for
				else if ((text_mode == 1) && ((mark + 2) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "for", 3) == 0)))
				{
					// highlighting 'int' string
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 2]);
					mark = mark + 3;
				}
				// while
				else if ((text_mode == 1) && ((mark + 4) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "while", 5) == 0)))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 4]);
					mark = mark + 5;
				}
				// long
				else if ((text_mode == 1) && ((mark + 3) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "long", 4) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					mark = mark + 4;
				}
				// {}[]()
				else if ((text_mode == 1) && (text[j][mark] == '{' || text[j][mark] == '}' || text[j][mark] == '[' || text[j][mark] == ']' || text[j][mark] == '(' || text[j][mark] == ')'))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					mark++;
				}
				// static
				else if ((text_mode == 1) && ((mark + 5) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "static", 6) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 4]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 5]);
					mark = mark + 6;
				}
				// const
				else if ((text_mode == 1) && ((mark + 4) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "const", 5) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 4]);
					mark = mark + 5;
				}
				// //
				else if ((text_mode == 1) && ((mark + 1) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "//", 2) == 0)))
				{
					fprintf(1, "\e[1;32m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;32m%c\e[0m", text[j][mark + 1]);
					mark = mark + 2;
					flag_annotation = 1;
				}
				// NULL
				else if ((text_mode == 1) && ((mark + 3) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "NULL", 4) == 0)))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 3]);
					mark = mark + 4;
				}
				// character string
				else if ((text_mode == 1) && text[j][mark] == '"')
				{
					int tmp_pos = mark + 1;
					int end = -1;
					while (tmp_pos < MAX_LINE_LENGTH)
					{
						if (text[j][tmp_pos] == '"')
						{
							end = tmp_pos;
							break;
						}
						else
						{
							tmp_pos++;
						}
					}
					for (int inter = mark; inter <= end; inter++)
					{
						fprintf(1, "\e[1;33m%c\e[0m", text[j][inter]);
					}
					mark = end + 1;
				}
				// single character
				else if ((text_mode == 1) && text[j][mark] == '\'')
				{
					int tmp_pos = mark + 1;
					int end = -1;
					while (tmp_pos < MAX_LINE_LENGTH)
					{
						if (text[j][tmp_pos] == '\'')
						{
							end = tmp_pos;
							break;
						}
						else
						{
							tmp_pos++;
						}
					}
					for (int inter = mark; inter <= end; inter++)
					{
						fprintf(1, "\e[1;33m%c\e[0m", text[j][inter]);
					}
					mark = end + 1;
				}
				// continue
				else if ((text_mode == 1) && ((mark + 5) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "continue", 7) == 0)))
				{
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 4]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 5]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 6]);
					fprintf(1, "\e[1;35m%c\e[0m", text[j][mark + 7]);
					mark = mark + 8;
				}
				// return
				else if ((text_mode == 1) && ((mark + 5) < MAX_LINE_LENGTH && (strncmp(text[j] + mark, "return", 6) == 0)))
				{
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 1]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 2]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 3]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 4]);
					fprintf(1, "\e[1;34m%c\e[0m", text[j][mark + 5]);
					mark = mark + 6;
				}
				// }
				else
				{
					fprintf(1, "%c\e[0m", text[j][mark]);
					mark++;
				}
			}
			fprintf(1, "\n");
		}
	}
	fprintf(1, "\n");
}

void com_rollback(char *text[], int n)
{
	// rollback the command
	if (upper_bound < 0)
	{
		fprintf(1, "YAW-> \033[1m\e[41;33mcouldn't rollback\e[0m\n");
		return;
	}

	char *input = malloc(MAX_LINE_LENGTH);
	strcpy(input, command_set[upper_bound]);
	upper_bound--;
	// searching the first space of command
	int pos = MAX_LINE_LENGTH - 1;
	int j = 0;
	for (; j < 10; j++)
	{
		if (input[j] == ' ')
		{
			pos = j + 1;
			break;
		}
	}
	// deal 'write' command
	if (input[0] == 'i' && input[1] == 'n' && input[2] == 's')
	{
		// the line to be deleted
		int line = stringtonumber(&input[4]);
		com_delete(text, line, 0);
		line_number = get_line_number(text);
	}
	// deal 'modify' command
	else if (input[0] == 'm' && input[1] == 'o' && input[2] == 'd')
	{
		// the line to be modified
		int line = stringtonumber(&input[4]);
		// the content of modify
		char *content = &input[pos];
		com_modify(text, line, content, 0);
		line_number = get_line_number(text);
	}
	// deal 'delete' command
	else if (input[0] == 'd' && input[1] == 'e' && input[2] == 'l')
	{
		// the line to be deleted
		int line = stringtonumber(&input[4]);
		// the content of deletion
		char *content = &input[pos];
		com_write(text, line, content, 0);
		line_number = get_line_number(text);
	}
}

void record_command(char *command)
{
	if ((upper_bound + 1) < MAX_ROLLBAKC_STEP)
	{
		command_set[upper_bound + 1] = malloc(MAX_LINE_LENGTH);
		strcpy(command_set[upper_bound + 1], command);
		upper_bound++;
	}
	else
	{
		for (int i = 1; i < MAX_ROLLBAKC_STEP; i++)
		{
			strcpy(command_set[i - 1], command_set[i]);
		}
		strcpy(command_set[upper_bound], command);
		upper_bound = MAX_ROLLBAKC_STEP - 1;
	}
}