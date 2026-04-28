#include <stdio.h>
#include <stdlib.h>
#include "tinysh.h"

void tinysh_char_out(unsigned char c) //dinh nghia ham in ra ky tu cho ham tinysh_char_out duoc giu cho san trong file header cua thu vien tinysh.h
{
    putchar((int)c);
}

static void display_args(int argc, char **argv)
{
  int i;
  for(i=0;i<argc;i++)
    {
      printf("argv[%d]=\"%s\"\n",i,argv[i]);
    }
}

static void foo_fnt(int argc, char **argv)
{
  printf("foo command called\n");
  display_args(argc,argv);
}

tinysh_cmd_t myfoocmd={
    0,
    "foo",
    "foo command",
    "[args]",
    foo_fnt,
    0,
    0,
    0
};

static void math_fnt(int argc, char **argv)
{
  printf("math command called\n");
  display_args(argc,argv);
}

tinysh_cmd_t mymathcmd={
    0,
    "math",
    "math command",
    "[args]",
    math_fnt,
    0,
    0,
    0
};

static void fixmath_fnt(int argc, char **argv)
{
  printf("Fix math command called\n");
  display_args(argc,argv);
}

tinysh_cmd_t myfixmathcmd={
    .parent = &mymathcmd, //Gan myfixmathcmd la child command cua mymathcmd
    "Fixmath",
    "Fix math command",
    "[args]",
    fixmath_fnt,
    0,
    0,
    0
};

static void tinh_1_cong_2(int argc, char **argv)
{
    printf("Tong 1 va 2 la: %d\n", 1 + 2);
}

tinysh_cmd_t cong_1_va_2 = {
    .parent = &myfixmathcmd, 
    .name = "tong_1_va_2", 
    .help = "tinh tong giua 1 va 2",
    .usage = "[args]",
    .function = tinh_1_cong_2,
    .arg = 0, 
    0, 
    0
};

static void tinh_2_cong_3(int argc, char **argv)
{
    printf("Tong 2 va 3 la: %d\n", 2 + 3);
}

tinysh_cmd_t cong_2_va_3 = {
    .parent = &myfixmathcmd, 
    .name = "tong_2_va_3", 
    .help = "tinh tong giua 2 va 3",
    .usage = "[args]",
    .function = tinh_2_cong_3,
    .arg = 0, 
    NULL, 
    NULL
};

static void tinh_tong(int argc, char **argv)
{
  int num1 = atoi(argv[1]);
  int num2 = atoi(argv[2]);
  printf("Tong %d va %d la: %d\n", num1, num2, num1 + num2);
}

tinysh_cmd_t tinh_tong_cmd = {
    .parent = &mymathcmd, 
    .name = "tong_2_so", 
    .help = "tinh tong cua hai so nguyen",
    .usage = "[args]",
    .function = tinh_tong,
    .arg = 0, 
    NULL, 
    NULL
};



int main(void)
{
    printf("Started Virtual tinysh terminal\n");

    /* change the prompt */
    //tinysh_set_prompt(PACKAGE "-" VERSION "$ ");
      
    /* add the top level command */
    tinysh_add_command(&myfoocmd);
    tinysh_add_command(&mymathcmd);
    tinysh_add_command(&myfixmathcmd);

    /* add sub commands */
    tinysh_add_command(&cong_1_va_2);
    tinysh_add_command(&cong_2_va_3);
    tinysh_add_command(&tinh_tong_cmd);

    int c;//Input character from user, used to passing to the terminal, the terminal uses it to detect and process command form user

  
    while(1)
    {
      c = getchar(); //nhan ky tu tu ban phim va luu vao bien c, tuy theo he thong ma ham nhap ky tu can thay doi (vi du doi voi he thong nhung dung HAL qua uart thi la HAL_UART_Receive) o day vi su dung trong moi truong console nen su dung getchar de nhan ky tu tu ban phim
      tinysh_char_in((unsigned char)c); //Goi ham nay lien tuc de chuong trinh lien tuc doc lenh tu nguoi dung va xu ly lenh do, ham tinysh_char_in duoc dinh nghia trong thu vien tinysh.h va duoc thuc hien trong file tinysh.c, ham nay se xu ly ky tu nhap vao va thuc hien cac lenh tuong ung voi ky tu do

    }
  printf("\nBye\n");
}