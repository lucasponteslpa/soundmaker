#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

unsigned char hexdigit[] = {0x3F, 0x06, 0x5B, 0x4F,
                            0x66, 0x6D, 0x7D, 0x07, 
                            0x7F, 0x6F, 0x77, 0x7C,
			                      0x39, 0x5E, 0x79, 0x71};

typedef struct PIO {
  int dado;
  int qsys;
} pio;

int main() {
  int i;
  int value;
  pio input, output;
  int valueQsys;
  int dev = open("/dev/de2i150_altera", O_RDWR);


  for (i=0; i>-1; i++) 
  {
    printf("Digite o periferico que voce deseja testar: \n1 - Switches;       | 4 - Leds Vermelhos\n2 - Botoes;         | 5 - Primeiros displays\n3 - Leds Verdes     | 6 - Ultimos displays\nSelecione 7 para aparecer o valor dos switches nos displays ou 0 para encerrar.\n");
    scanf("%d", &valueQsys);
    if(valueQsys)
    {
        if(valueQsys == 7)
        {
          while (input.dado != 65535)
          {
            input.qsys = 1;
            read(dev, &input, sizeof(pio));
            read(dev, &input, sizeof(pio));
            output.qsys = 5;
            output.dado = (
                              (hexdigit[(input.dado >> 12) & 0xF]) << 8) 
                            | (hexdigit[(input.dado >> 8) & 0xF]);
            output.dado = ~output.dado;
            write(dev, &output, sizeof(pio));

            output.qsys = 6;
            output.dado = (
                              (hexdigit[(input.dado >> 4) & 0xF]) << 8) 
                            | (hexdigit[(input.dado) & 0xF]);
            output.dado = ~output.dado;
            write(dev, &output, sizeof(pio));
          }

        }
        else if (valueQsys != 1 && valueQsys != 2)
        {
          printf("Digite o valor que será escrito na memoria do periferico desejado:\n");
          scanf("%d", &value);
          output.qsys = valueQsys;
          output.dado = value;
          if(valueQsys == 5 || valueQsys == 6)
          {
            output.dado = hexdigit[value];
            output.dado = ~output.dado;
            write(dev, &output, sizeof(pio));
          }
          else 
          {
            write(dev, &output, sizeof(pio)); 
          }
        }
        else 
        {
          input.qsys = valueQsys;
          read(dev, &input, sizeof(pio));
          read(dev, &input, sizeof(pio));
            if (valueQsys == 1)
            {
              while (input.dado != 65535)
              {
                read(dev, &input, sizeof(pio));
                read(dev, &input, sizeof(pio));
                printf("O valor dos switches ligados eh (acione todos para parar): %d\n", input.dado);
              }
            }
            else
            {
              while (input.dado != 0)
              {
                read(dev, &input, sizeof(pio));
                read(dev, &input, sizeof(pio));
                printf("O valor dos botoes ligados eh (acione todos para parar): %d\n", input.dado);
              }
            }
          }
        }
    else 
    {
      i = -10;
    }     
  }

  close(dev);
  return 0;
}