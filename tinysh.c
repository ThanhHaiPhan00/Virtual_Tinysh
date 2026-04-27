/*
 * tinysh.c
 *
 * Minimal portable shell
 *
 * Copyright (C) 2001 Michel Gutierrez <mig@nerim.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "tinysh.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 256
#endif
#ifndef HISTORY_DEPTH
#define HISTORY_DEPTH 16
#endif
#ifndef MAX_ARGS
#define MAX_ARGS 16
#endif
#ifndef PROMPT_SIZE
#define PROMPT_SIZE 16
#endif
#ifndef TOPCHAR /*return to the top level*/
#define TOPCHAR '/'
#endif

#ifndef BACKCHAR /*return to the prior level*/
#define BACKCHAR '<'
#endif

typedef unsigned char uchar;
/* redefine some useful and maybe missing utilities to avoid conflicts */
#define strlen tinysh_strlen
#define puts tinysh_puts
#define putchar tinysh_char_out

static void help_fnt(int argc, char **argv);

static tinysh_cmd_t help_cmd={ 
  0,"help","display help","<cr>",help_fnt,0,0,0 };

static uchar input_buffers[HISTORY_DEPTH][BUFFER_SIZE+1]={0};
static uchar trash_buffer[BUFFER_SIZE+1]={0};
static int cur_buf_index=0;
static uchar context_buffer[BUFFER_SIZE+1]={0};
static int cur_context=0;
static int cur_index=0; //Chi so hien tai trong input buffer
static int echo=1;
static char prompt[PROMPT_SIZE+1]="$ ";
static tinysh_cmd_t *root_cmd=&help_cmd;
static tinysh_cmd_t *cur_cmd_ctx=0; //Current context of command
static void *tinysh_arg=0;

/* few useful utilities that may be missing */

static int strlen(uchar *s)
{
  int i;
  for(i=0;*s;s++,i++);
  return i;
}

static void puts(char *s)
{
  while(*s)
    putchar(*s++);
}

/* callback for help function
 */
static void help_fnt(int argc, char **argv)
{
  puts("?            display help on given or available commands\n");
  puts("<TAB>        auto-completion\n");
  puts("<cr>         execute command line\n");
  puts("CTRL-P       recall previous input line\n");
  puts("CTRL-N       recall next input line\n");
  puts("/            back to the top level\n");
  puts("<            back to the prior level\n");
  puts("<any>        treat as input character\n");
}

/*
 */

enum { NULLMATCH,FULLMATCH,PARTMATCH,UNMATCH,MATCH,AMBIG }; //ambiguity: co nhieu hon mot command khop, full match: command khop hoan toan, part match: command khop mot phan voi input va khac vai ky tu cuoi, unmatch: command khong khop voi input, nullmatch = da den cuoi level nhung van chua co command nao khop

/* verify if the non-spaced part of s2 is included at the begining
 * of s1.
 * return FULLMATCH if s2 equal to s1, PARTMATCH if s1 starts with s2
 * but there are remaining chars in s1, UNMATCH if s1 does not start with
 * s2
 */
int strstart(uchar *s1, uchar *s2)
{
  while(*s1 && *s1==*s2) { s1++; s2++; }

  if(*s2==' ' || *s2==0)
    {
      if(*s1==0)
        return FULLMATCH; /* full match */
      else
        return PARTMATCH; /* partial match */ 
    }
  else
    return UNMATCH;     /* no match */
}

/*
 * check commands at given level with input string.
 * _cmd: point to first command at this level, return matched cmd
 * _str: point to current unprocessed input, return next unprocessed
 */
static int parse_command(tinysh_cmd_t **_cmd, uchar **_str)
{
  uchar *str=*_str;
  tinysh_cmd_t *cmd;
  //int matched_len=0;
  tinysh_cmd_t *matched_cmd=0;

  /* first eliminate first blanks */
  while(*str==' ') str++;
  if(!*str)
    {
      *_str=str;
      return NULLMATCH; /* end of input */
    }
  
  /* first pass: count matches */
  for(cmd=*_cmd;cmd;cmd=cmd->next) //duyet tat ca command next tinh tu *_cmd trong cung level de tim ra command khop
    {
      int ret=strstart((uchar *)(cmd->name),(uchar *)(str));

      if(ret==FULLMATCH)
        {
          /* found full match */
          while(*str && *str!=' ') str++; //Dua con tro string den cuoi command
          while(*str==' ') str++; //Dua con tro den dau argument
          *_str=str;
          *_cmd=cmd; //Neu full match thi *_cmd se tro den command full match do
          return MATCH;
        }
      else if (ret==PARTMATCH)
        {
          if(matched_cmd) //Kiem tra truoc do da co command nao khop voi input hay chua, neu da co thi se xay ra ambiguity
            {
              *_cmd=matched_cmd; //tra ve command khop dau tien
              return AMBIG; //Tra ve ambiguity de thong bao cho nguoi dung biet rang co nhieu hon mot command khop voi input hien tai, nguoi dung can nhap them ky tu de xac dinh chinh xac command can goi
            }
          else
            {
              matched_cmd=cmd;
            }
        }
      else /* UNMATCH */
        {
        }
    }
  if(matched_cmd) //Neu chi co mot partial match thi ket qua la match va truyen ve command tuong ung
    {
      while(*str && *str!=' ') str++; //Dua con tro string den cuoi command
      while(*str==' ') str++; //Dua con tro den dau argument
      *_cmd=matched_cmd; //Tra ve command partial match
      *_str=str; //tra ve con tro den dau argument
      return MATCH;
    }
  else
    return UNMATCH;
}

/* create a context from current input line
 */
static void do_context(tinysh_cmd_t *cmd, uchar *str)
{
  while(*str) 
    context_buffer[cur_context++]=*str++;
  context_buffer[cur_context]=0;
  cur_cmd_ctx=cmd;
}

/* execute the given command by calling callback with appropriate 
 * arguments
 */
static void exec_command(tinysh_cmd_t *cmd, uchar *str)
{
  char *argv[MAX_ARGS];
  int argc=0;
  int i;

/* copy command line to preserve it for history */
  for(i=0;i<BUFFER_SIZE;i++)
    trash_buffer[i]=str[i];
  str=trash_buffer;
  
/* cut into arguments */
  argv[argc++]=cmd->name; //phan tu thu 0 cua argument vector luon la ten cua command
  while(*str && argc<MAX_ARGS)
    {
      while(*str==' ') str++; //Arguments duoc phan cach bang cac khoang trang ' 
      if(*str==0)
        break;
      argv[argc++]=(char *)(str); //Ghi nhan argument vao mang argv (ghi nhan con tro thay vi copy string vi vay cuoi string se can phai co ky tu NULL)
      while(*str!=' ' && *str) str++; //Quet den cuoi argument vua duoc ghi nhan
      if(!*str) break; //ket thuc neu da den cuoi chuoi
      *str++=0; //ket thuc string bang mot ky tu NULL va tiep tuc duyet string.
    }
/* call command function if present */
  if(cmd->function)
    {
      tinysh_arg=cmd->arg;
      cmd->function(argc,&argv[0]);
    }
}

/* try to execute the current command line
 */
static int exec_command_line(tinysh_cmd_t *cmd, uchar *_str)
{
  uchar *str=_str;

  while(1)
    {
      int ret;
      ret=parse_command(&cmd,&str);
      if(ret==MATCH) /* found unique match */
        {
          if(cmd)
            {
              if(!cmd->child) /* no sub-command, execute */
                {
                  exec_command(cmd,str); //Goi to ham thuc thi command
                  return 0;
                }
              else
                {
                  if(*str==0) /* no more input, this is a context */
                    {
                      do_context(cmd,_str);
                      return 0;
                    }
                  else /* process next command word */
                    {
                      cmd=cmd->child;
                    }
                }
            } 
          else /* cmd == 0 */
            {
              return 0;
            }
        }
      else if(ret==AMBIG)
        {
          puts("ambiguity: ");
          puts((char *)(str));
          putchar('\n');
          return 0;
        }
      else if(ret==UNMATCH) /* UNMATCH */
        {
          puts("no match: ");
          puts((char *)(str));
          putchar('\n');
          return 0;
        }
      else /* NULLMATCH */
        return 0;
    }
}

/* display help for list of commands 
*/
static void display_child_help(tinysh_cmd_t *cmd)
{
  tinysh_cmd_t *cm;
  int len=0;

  putchar('\n');
  for(cm=cmd;cm;cm=cm->next)
    if(len<strlen((uchar *)(cm->name)))
      len=strlen((uchar *)(cm->name));
  for(cm=cmd;cm;cm=cm->next)
    if(cm->help)
      {
        int i;
        puts(cm->name);
        for(i=strlen((uchar *)(cm->name));i<len+2;i++)
          putchar(' ');
        puts((char *)(cm->help));
        putchar('\n');
      }
}

/* try to display help for current comand line
 */
static int help_command_line(tinysh_cmd_t *cmd, uchar *_str)
{
  uchar *str=_str;

  while(1)
    {
      int ret;
      ret=parse_command(&cmd,&str);
      if(ret==MATCH && *str==0) /* found unique match or empty line */
        {
              
          if(cmd->child) /* display sub-commands help */
            {
              display_child_help(cmd->child);
              return 0;
            }
          else  /* no sub-command, show single help */
            {
              if(*(str-1)!=' ')
                putchar(' ');
              if(cmd->usage)
                puts(cmd->usage);
              puts(": ");
              if(cmd->help)
                puts(cmd->help);
              else
                puts("no help available");
              putchar('\n');
            }
          return 0;
        }
      else if(ret==MATCH && *str)
        { /* continue processing the line */
          cmd=cmd->child;
        }
      else if(ret==AMBIG)
        {
          puts("\nambiguity: ");
          puts((char *)(str));
          putchar('\n');
          return 0;
        }
      else if(ret==UNMATCH)
        {
          puts("\nno match: ");
          puts((char *)(str));
          putchar('\n');
          return 0;
        }
      else /* NULLMATCH */
        {
          if(cur_cmd_ctx)
            display_child_help(cur_cmd_ctx->child);
          else
            display_child_help(root_cmd);
          return 0;
        }
    }
}

/* try to complete current command line
 */
static int complete_command_line(tinysh_cmd_t *cmd, uchar *_str)
{
  uchar *str=_str;

  while(1)
    {
      int ret;
      int common_len=BUFFER_SIZE;
      int _str_len; //Chieu dai commandline tu ky dau tien cua string dang quet cho den khaong trang dau tien chinh la ten command can complete
      int i;
      uchar *__str=str; //Copy command line

      ret=parse_command(&cmd,&str); // Tra ve con tro cmd tro den ham match hoac partial match, *trr tro den dau argument
      for(_str_len=0;__str[_str_len]&&__str[_str_len]!=' ';_str_len++);
      if(ret==MATCH && *str)
        {
          cmd=cmd->child;
        }
      else if(ret==AMBIG || ret==MATCH || ret==NULLMATCH)
        {
          tinysh_cmd_t *cm;
          tinysh_cmd_t *matched_cmd=0;
          int nb_match=0; //Number of command match voi input hien tai, duyet het tat ca command trong cung level de dem so command match voi input
              
          for(cm=cmd;cm;cm=cm->next)
            {
              int r=strstart((uchar *)(cm->name),(uchar *)(__str));
              if(r==FULLMATCH)
                {
                  for(i=_str_len;cmd->name[i];i++) //In ra phan con thieu của ten command
                    tinysh_char_in(cmd->name[i]);
                  if(*(str-1)!=' ')
                    tinysh_char_in(' '); // In ra khoang trang sau command
                  if(!cmd->child) //neu command khong co child thi in ra usage neu co va ket thuc, neu co child thi tiep tuc duyet de tim command tiep theo
                    {
                      if(cmd->usage)
                        {
                          puts(cmd->usage);
                          putchar('\n');
                          return 1;
                        }
                      else
                        return 0;
                    }
                  else
                    {
                      cmd=cmd->child;
                      break;
                    }
                }
              else if(r==PARTMATCH)
                {
                  nb_match++;
                  if(!matched_cmd) //Kiem tra xem truoc do da co command nao khop voi input hay chua
                    {
                      matched_cmd=cm;
                      common_len=strlen((uchar *)(cm->name));
                    }
                  else
                    {
                      for(i=_str_len;cm->name[i] && i<common_len &&
                            cm->name[i]==matched_cmd->name[i];i++); //Dua contro ky tu command line den cuoi ten command
                      if(i<common_len)
                        common_len=i; //Lay length tu command ngan nhat khop
                    }
                }
            }
          if(cm) //Neu duyet string dau tien khong tim duoc match command thi duyet tiep string tiep theo trong command line
            continue;
          if(matched_cmd) //neu nhieu hon mot command khop voi input thi se co 
            {
              if(_str_len==common_len)
                {
                  putchar('\n');
                  for(cm=cmd;cm;cm=cm->next)
                    {
                      int r=strstart((uchar *)(cm->name),(uchar *)(__str));
                      if(r==FULLMATCH || r==PARTMATCH)
                        {
                          puts(cm->name);
                          putchar('\n');
                        }
                    }
                  return 1;
                }
              else
                {
                  for(i=_str_len;i<common_len;i++)
                    tinysh_char_in(matched_cmd->name[i]);
                  if(nb_match==1)
                    tinysh_char_in(' ');
                }
            }
          return 0;
        }
      else /* UNMATCH */
        { 
          return 0;
        }
    }
}

/* start a new line 
 */
static void start_of_line()
{
  /* display start of new line */
  puts(prompt);
  if(cur_context)
    {
      puts((char *)(context_buffer));
      puts("> ");
    }
  cur_index=0;
}

/* character input 
 */
static void _tinysh_char_in(uchar c)
{
  uchar *line=input_buffers[cur_buf_index]; //Con tro line tro den input buffer hien tai

  if(c=='\n' || c=='\r') /* validate command */ //Neu nhan enter hoac return thi validate command
    {
      tinysh_cmd_t *cmd;
      
/* first, echo the newline */
      if(echo) //Mot so terminal khong hien thi ky tu khi nguoi dung nhap vao, do do can den echo de hien thi ky tu ma nguoi dung dang nhap, tuy chon co hoac khong co san trong thu vien
        putchar(c);

      while(*line && *line==' ') line++;
      if(*line) /* not empty line */
        {
          cmd=cur_cmd_ctx?cur_cmd_ctx->child:root_cmd; //Gan command mac dinh la child command dau tien trong command context sau do doi chieu lan luot voi command line, neu khong co context mac dinh trong thu vien la help command
          exec_command_line(cmd,line);
          cur_buf_index=(cur_buf_index+1)%HISTORY_DEPTH; //sang command line moi
          cur_index=0; //Reset chi so hien tai trong input buffer
          input_buffers[cur_buf_index][0]=0;
        }
      start_of_line();
    }
  else if(c==TOPCHAR) /* return to top level */
    {
      if(echo)
        putchar(c);

      cur_context=0;
      cur_cmd_ctx=0;
    }
  else if(c==8 || c==127) /* backspace hoac delete */
    {
      if(cur_index>0)
        {
          puts("\b \b");
          cur_index--; // dua con tro lui ve sau 1 ky tu
          line[cur_index]=0; //khi an backspace hoac delete thi xoa ky tu cuoi cung trong input buffer
        }
    }
  else if(c==16) /* CTRL-P: back in history */
    {
      int prevline=(cur_buf_index+HISTORY_DEPTH-1)%HISTORY_DEPTH;

      if(input_buffers[prevline][0])
        {
          line=input_buffers[prevline];
          /* fill the rest of the line with spaces */
          while(cur_index-->strlen(line))
            puts("\b \b");
          putchar('\r');
          start_of_line();
          puts((char *)(line));
          cur_index=strlen(line);
          cur_buf_index=prevline;
        }
    }
  else if(c==14) /* CTRL-N: next in history */
    {
      int nextline=(cur_buf_index+1)%HISTORY_DEPTH;

      if(input_buffers[nextline][0])
        {
          line=input_buffers[nextline];
          /* fill the rest of the line with spaces */
          while(cur_index-->strlen(line))
            puts("\b \b");
          putchar('\r');
          start_of_line();
          puts((char *)(line));
          cur_index=strlen(line);
          cur_buf_index=nextline;
        }
    }
  else if(c=='?') /* display help */
    {
      tinysh_cmd_t *cmd;
      cmd=cur_cmd_ctx?cur_cmd_ctx->child:root_cmd;
      help_command_line(cmd,line);
      start_of_line();
      puts((char *)(line));
      cur_index=strlen(line);
    }
  else if(c==9 || c=='!') /* TAB: autocompletion */
    {
      tinysh_cmd_t *cmd;
      cmd=cur_cmd_ctx?cur_cmd_ctx->child:root_cmd;
      if(complete_command_line(cmd,line))
        {
          start_of_line();
          puts((char *)(line));
        }
      cur_index=strlen(line);
    }
  else if(c == BACKCHAR) /* <: return to the prior context*/
  {
      if(echo)
        putchar(c);

      cur_context=0;
      cur_cmd_ctx=0;
  }      
  else /* any input character */
    {
      if(cur_index<BUFFER_SIZE)
        {
          if(echo)
            putchar(c);
          line[cur_index++]=c;
          line[cur_index]=0;
        }
    }
}

/* new character input */
void tinysh_char_in(uchar c)
{
  /*
   * filter characters here
   */
  _tinysh_char_in(c);
}

/* add a new command */
void tinysh_add_command(tinysh_cmd_t *cmd)
{
  tinysh_cmd_t *cm;

  if(cmd->parent)
    {
      cm=cmd->parent->child;
      if(!cm)
        {
          cmd->parent->child=cmd;
        }
      else
        {
          while(cm->next) cm=cm->next;
          cm->next=cmd;
        }
    }
  else if(!root_cmd)
    {
      root_cmd=cmd;
    }
  else
    {
      cm=root_cmd;
      while(cm->next) cm=cm->next;
      cm->next=cmd;      
    }
}

/* modify shell prompt
 */
void tinysh_set_prompt(char *str)
{
  int i;
  for(i=0;str[i] && i<PROMPT_SIZE;i++)
    prompt[i]=str[i];
  prompt[i]=0;
  /* force prompt display by generating empty command */
  tinysh_char_in('\r');
}

/* return current command argument
 */
void *tinysh_get_arg()
{
  return tinysh_arg;
}

/* string to decimal/hexadecimal conversion
 */
unsigned long tinysh_atoxi(char *s)
{
  int ishex=0;
  unsigned long res=0;

  if(*s==0) return 0;

  if(*s=='0' && *(s+1)=='x')
    {
      ishex=1;
      s+=2;
    }

  while(*s)
    {
      if(ishex)
	res*=16;
      else
	res*=10;

      if(*s>='0' && *s<='9')
	res+=*s-'0';
      else if(ishex && *s>='a' && *s<='f')
	res+=*s+10-'a';
      else if(ishex && *s>='A' && *s<='F')
	res+=*s+10-'A';
      else
	break;
      
      s++;
    }

  return res;
}