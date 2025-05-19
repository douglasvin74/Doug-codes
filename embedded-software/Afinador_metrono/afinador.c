
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306_font.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"


#define WS2812_PIN 7 // Pino de dados para os LEDs
#define LED_RED 13 // Pino Led Vermelho
#define LED_GREEN 11 // Pino Led Verde
#define LED_BLUE 12 // Pino Led Azul
#define JOYSTICK_X 26 // Pinos do joystick
#define JOYSTICK_Y 27 // Pinos do joystick
#define JOYSTICK_BOTAO 22 // Pinos do joystick
#define BOTAO_PIN_A 5 // Pino botao
#define BOTAO_PIN_B 6 // Pino botao
#define BUZZER_PIN 10 // Pino do buzzer
#define BUZZER_FREQUENCY 2000 // Frequência do buzzer em Hz
#define SSD1306_HEIGHT              32
#define SSD1306_WIDTH               128

#define CHECK_INTERVAL_MS 10  // Checa o botão a cada 10 ms
#define TONE_DURATION_MS 50

#define SSD1306_I2C_ADDR            _u(0x3C)

#define SSD1306_I2C_CLK             400



// commands (see datasheet)
#define SSD1306_SET_MEM_MODE        _u(0x20)
#define SSD1306_SET_COL_ADDR        _u(0x21)
#define SSD1306_SET_PAGE_ADDR       _u(0x22)
#define SSD1306_SET_HORIZ_SCROLL    _u(0x26)
#define SSD1306_SET_SCROLL          _u(0x2E)

#define SSD1306_SET_DISP_START_LINE _u(0x40)

#define SSD1306_SET_CONTRAST        _u(0x81)
#define SSD1306_SET_CHARGE_PUMP     _u(0x8D)

#define SSD1306_SET_SEG_REMAP       _u(0xA0)
#define SSD1306_SET_ENTIRE_ON       _u(0xA4)
#define SSD1306_SET_ALL_ON          _u(0xA5)
#define SSD1306_SET_NORM_DISP       _u(0xA6)
#define SSD1306_SET_INV_DISP        _u(0xA7)
#define SSD1306_SET_MUX_RATIO       _u(0xA8)
#define SSD1306_SET_DISP            _u(0xAE)
#define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
#define SSD1306_SET_COM_OUT_DIR_FLIP _u(0xC0)

#define SSD1306_SET_DISP_OFFSET     _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
#define SSD1306_SET_PRECHARGE       _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
#define SSD1306_SET_VCOM_DESEL      _u(0xDB)

#define SSD1306_PAGE_HEIGHT         _u(8)
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)

#define SSD1306_WRITE_MODE         _u(0xFE)
#define SSD1306_READ_MODE          _u(0xFF)

// Definição das notas musicais do violão em Hz
#define NOTE_E1  82
#define NOTE_A1  110
#define NOTE_D2  146
#define NOTE_G2  195
#define NOTE_B3  246
#define NOTE_E3  329



struct render_area {
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_page;
    uint8_t end_page;

    int buflen;
};


// Melodia Cordas do Violão(lista de notas)
int melody[] = {NOTE_E1, NOTE_A1, NOTE_D2, NOTE_G2, NOTE_B3, NOTE_E3};

// Função para iniciar o PWM com uma frequência específica
void start_tone(uint buzzer_pin, int frequency) {
    uint slice_num = pwm_gpio_to_slice_num(buzzer_pin);
    float clock_div = 64.0;  // Ajuste para notas graves
    pwm_set_clkdiv(slice_num, clock_div);

    uint32_t wrap = (125000000 / (clock_div * frequency)) - 1;
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzzer_pin), wrap / 2);

    pwm_set_enabled(slice_num, true);  // Ativa o PWM
}

// Função para parar o som do buzzer
void stop_tone(uint buzzer_pin) {
    pwm_set_enabled(pwm_gpio_to_slice_num(buzzer_pin), false);
}

// Função do metrônomo
void metronome(uint8_t *buf, struct render_area *frame_area) {
    int bpm = 120;
    char bpm_str[10];  // Buffer para armazenar a string formatada do BPM

    int interval_ms = 60000 / bpm;  // Calcula o intervalo entre os "cliques" em milissegundos
    printf("Metrônomo ativado a %d BPM.\n", bpm);

    while (true) {
    // Verifica botão antes de começar o beat
    if (gpio_get(BOTAO_PIN_B) == 0) {
        sleep_ms(30);
        if (gpio_get(BOTAO_PIN_B) == 0) {
            break;
        }
    }

    // Leitura do joystick
    adc_select_input(0);
    uint joystick_y_val = adc_read();
    adc_select_input(1);
    uint joystick_x_val = adc_read();

    if (joystick_x_val < 1000) bpm -= 5;
    else if (joystick_x_val > 3000) bpm += 5;

    if (joystick_y_val < 1000) bpm -= 1;
    else if (joystick_y_val > 3000) bpm += 1;

    if (bpm < 1) bpm = 1;

    interval_ms = 60000 / bpm;

    // Atualiza display
    sprintf(bpm_str, "%d BPM", bpm);
    WriteString(buf, 40, 8, bpm_str);
    render(buf, frame_area);

    printf("BPM atualizado para: %d\n", bpm);

    // Som do metrônomo
    start_tone(BUZZER_PIN, 1000);
    sleep_ms(TONE_DURATION_MS);
    stop_tone(BUZZER_PIN);

    // Espera o restante do intervalo, verificando o botão constantemente
    uint32_t restante = interval_ms - TONE_DURATION_MS;
    uint32_t elapsed = 0;

    while (elapsed < restante) {
        if (gpio_get(BOTAO_PIN_B) == 0) {
            sleep_ms(30);
            if (gpio_get(BOTAO_PIN_B) == 0) {
                break;
            }
        }
        sleep_ms(CHECK_INTERVAL_MS);
        elapsed += CHECK_INTERVAL_MS;
    }

    // Se botão foi pressionado dentro da espera, sai do loop
    if (gpio_get(BOTAO_PIN_B) == 0) {
        break;
    }
}

}



// Função para tocar notas indefinidamente até que o botão B seja pressionado
void afinador(uint8_t *buf, struct render_area *frame_area) {
    int current_note = 0;  // Começa na primeira nota (E1)

    char note_str[30];

    printf("Entrando em play_melody()...\n");
    printf("Tocando nota inicial: %d Hz\n", melody[current_note]);

    // Exibe o BPM no OLED
    sprintf(note_str, "Nota: %d Hz\n", melody[current_note]);
    WriteString(buf, 10, 8, note_str);

    // Renderiza o buffer no display
    render(buf, frame_area);


    start_tone(BUZZER_PIN, melody[current_note]);  // Começa tocando E1
    

    while (true) {
        // Se o botão A for pressionado, muda para a próxima nota
        if (gpio_get(BOTAO_PIN_A) == 0) {
            current_note = (current_note + 1) % 6;  // Volta para E1 após E3
            printf("Mudando para nota: %d Hz\n", melody[current_note]);

            // Exibe o BPM no OLED
            sprintf(note_str, "Nota: %d Hz\n", melody[current_note]);
            WriteString(buf, 10, 8, note_str);


            // Renderiza o buffer no display
            render(buf, frame_area);
            
            start_tone(BUZZER_PIN, melody[current_note]);  // Toca a nova nota
            sleep_ms(300);  // Pequeno delay para evitar múltiplas leituras
        }

        // Se o botão B for pressionado, interrompe a execução
        if (gpio_get(BOTAO_PIN_B) == 0) {
            printf("Execução interrompida pelo botão B.\n");
            stop_tone(BUZZER_PIN);  // Para o som
            break;
        }
    }
}

void calc_render_area_buflen(struct render_area *area) {
    // calculate how long the flattened buffer will be for a render area
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

#ifdef i2c_default

void SSD1306_send_cmd(uint8_t cmd) {

    uint8_t buf[2] = {0x80, cmd};
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buf, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i=0;i<num;i++)
        SSD1306_send_cmd(buf[i]);
}

void SSD1306_send_buf(uint8_t buf[], int buflen) {

    uint8_t *temp_buf = malloc(buflen + 1);

    temp_buf[0] = 0x40;
    memcpy(temp_buf+1, buf, buflen);

    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);

    free(temp_buf);
}

void SSD1306_init() {

    uint8_t cmds[] = {
        SSD1306_SET_DISP,               // set display off
        /* memory mapping */
        SSD1306_SET_MEM_MODE,           // set memory address mode 0 = horizontal, 1 = vertical, 2 = page
        0x00,                           // horizontal addressing mode
        /* resolution and layout */
        SSD1306_SET_DISP_START_LINE,    // set display start line to 0
        SSD1306_SET_SEG_REMAP | 0x01,   // set segment re-map, column address 127 is mapped to SEG0
        SSD1306_SET_MUX_RATIO,          // set multiplex ratio
        SSD1306_HEIGHT - 1,             // Display height - 1
        SSD1306_SET_COM_OUT_DIR | 0x08, // set COM (common) output scan direction. Scan from bottom up, COM[N-1] to COM0
        SSD1306_SET_DISP_OFFSET,        // set display offset
        0x00,                           // no offset
        SSD1306_SET_COM_PIN_CFG,        // set COM (common) pins hardware configuration. Board specific magic number.
                                        // 0x02 Works for 128x32, 0x12 Possibly works for 128x64. Other options 0x22, 0x32
#if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
        0x02,
#elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
        0x12,
#else
        0x02,
#endif
        /* timing and driving scheme */
        SSD1306_SET_DISP_CLK_DIV,       // set display clock divide ratio
        0x80,                           // div ratio of 1, standard freq
        SSD1306_SET_PRECHARGE,          // set pre-charge period
        0xF1,                           // Vcc internally generated on our board
        SSD1306_SET_VCOM_DESEL,         // set VCOMH deselect level
        0x30,                           // 0.83xVcc
        /* display */
        SSD1306_SET_CONTRAST,           // set contrast control
        0xFF,
        SSD1306_SET_ENTIRE_ON,          // set entire display on to follow RAM content
        SSD1306_SET_NORM_DISP,           // set normal (not inverted) display
        SSD1306_SET_CHARGE_PUMP,        // set charge pump
        0x14,                           // Vcc internally generated on our board
        SSD1306_SET_SCROLL | 0x00,      // deactivate horizontal scrolling if set. This is necessary as memory writes will corrupt if scrolling was enabled
        SSD1306_SET_DISP | 0x01, // turn display on
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

void SSD1306_scroll(bool on) {
    // configure horizontal scrolling
    uint8_t cmds[] = {
        SSD1306_SET_HORIZ_SCROLL | 0x00,
        0x00, // dummy byte
        0x00, // start page 0
        0x00, // time interval
        0x03, // end page 3 SSD1306_NUM_PAGES ??
        0x00, // dummy byte
        0xFF, // dummy byte
        SSD1306_SET_SCROLL | (on ? 0x01 : 0) // Start/stop scrolling
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

void render(uint8_t *buf, struct render_area *area) {
    // update a portion of the display with a render area
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
    SSD1306_send_buf(buf, area->buflen);
}

static void SetPixel(uint8_t *buf, int x,int y, bool on) {
    assert(x >= 0 && x < SSD1306_WIDTH && y >=0 && y < SSD1306_HEIGHT);


    const int BytesPerRow = SSD1306_WIDTH ; // x pixels, 1bpp, but each row is 8 pixel high, so (x / 8) * 8

    int byte_idx = (y / 8) * BytesPerRow + x;
    uint8_t byte = buf[byte_idx];

    if (on)
        byte |=  1 << (y % 8);
    else
        byte &= ~(1 << (y % 8));

    buf[byte_idx] = byte;
}
// Basic Bresenhams.
static void DrawLine(uint8_t *buf, int x0, int y0, int x1, int y1, bool on) {

    int dx =  abs(x1-x0);
    int sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0);
    int sy = y0<y1 ? 1 : -1;
    int err = dx+dy;
    int e2;

    while (true) {
        SetPixel(buf, x0, y0, on);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2*err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static inline int GetFontIndex(uint8_t ch) {
    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
    }
    else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
    }
    else return  0; // Not got that char so space.
}

static void WriteChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    // For the moment, only write on Y row boundaries (every 8 vertical pixels)
    y = y/8;

    ch = toupper(ch);
    int idx = GetFontIndex(ch);
    int fb_idx = y * 128 + x;

    for (int i=0;i<8;i++) {
        buf[fb_idx++] = font[idx * 8 + i];
    }
}

void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str) {
    // Cull out any string off the screen
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    while (*str) {
        WriteChar(buf, x, y, *str++);
        x+=8;
    }
}

// Função para apagar o display
void apagar_display(uint8_t *buf, struct render_area *frame_area) {
    // Preenche o buffer com zeros
    memset(buf, 0, SSD1306_BUF_LEN);
    // Renderiza o buffer limpo no display
    render(buf, frame_area);
}



void introducao(uint8_t *buf, struct render_area *frame_area) {
        while(true){
        gpio_put(LED_RED, 1);  // Liga o led Vermelho
        if(gpio_get(BOTAO_PIN_A) == 0){
            gpio_put(LED_RED, 0);  // Desliga o Led Verde // Desliga o led Vermelho
            gpio_put(LED_GREEN, 1);   // Liga o led verde
            break;
        }
    }
       // Escreve cada string manualmente com posições X e Y específicas
    WriteString(buf, 50, 8, "Seja");
    WriteString(buf, 30, 16, "Bem-vindo  ");

    // Renderiza o buffer no display
    render(buf, frame_area);
    

    sleep_ms(4000);
    
    // Apaga o display antes da segunda parte
    apagar_display(buf, frame_area);

        // Escreve cada string manualmente com posições X e Y específicas
    WriteString(buf, 30, 0, "Este e um");
    WriteString(buf, 30, 8, "software");
    WriteString(buf, 20, 16, "desenvolvido");
    WriteString(buf, 20, 24, "para musicos");

    

    // Renderiza o buffer no display
    render(buf, frame_area);   
    
    sleep_ms(4000);

    // Apaga o display antes da segunda parte
    apagar_display(buf, frame_area);

        // Exibe a primeira mensagem
    WriteString(buf, 30, 8, "Escolha a ");
    WriteString(buf, 10, 16, "Funcionalidade");
    

    // Renderiza o buffer no display
    render(buf, frame_area);

    sleep_ms(4000);
    
    // Apaga o display antes da segunda parte
    apagar_display(buf, frame_area);
}



char escolha(uint8_t *buf, struct render_area *frame_area) {

    // Exibe as opções dos botões
    WriteString(buf, 35, 0, "Botao A:");
    WriteString(buf, 10, 8, "Afinar Violao");
    WriteString(buf, 35, 16, "Botao B:");
    WriteString(buf, 30, 24, "Metronomo");

    // Renderiza o buffer atualizado no display
    render(buf, frame_area);

    while (true) {
        if (gpio_get(BOTAO_PIN_A) == 0) {
            sleep_ms(300);  // Pequeno delay para evitar múltiplas leituras
            return 'A';
        } 
        else if (gpio_get(BOTAO_PIN_B) == 0) {
            sleep_ms(300);  // Pequeno delay para evitar múltiplas leituras
            return 'B';
        }
    }
}

char resultado; // Variável para armazenar a escolha

#endif

int main() {
    stdio_init_all();

#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c / SSD1306_i2d example requires a board with I2C pins
    puts("Default I2C pins were not defined");
#else
    // useful information for picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    bi_decl(bi_program_description("SSD1306 OLED driver I2C example for the Raspberry Pi Pico"));

    printf("Hello, SSD1306 OLED display! Look at my raspberries..\n");

    // I2C is "open drain", pull ups to keep signal high when no data is being
    // sent
    i2c_init(i2c_default, SSD1306_I2C_CLK * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    // Configuração dos pinos dos LEDs como saída
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);

    
      // Configuração dos pinos dos botões como entrada
    gpio_init(BOTAO_PIN_A);
    gpio_set_dir(BOTAO_PIN_A, GPIO_IN);
    gpio_pull_up(BOTAO_PIN_A);
    gpio_init(BOTAO_PIN_B);
    gpio_set_dir(BOTAO_PIN_B, GPIO_IN);
    gpio_pull_up(BOTAO_PIN_B);

    // Configuração do Buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);

    // Configuração dos pinos do joystick para leitura analógica
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);

    // Configuração do botão do joystick
    gpio_init(JOYSTICK_BOTAO);
    gpio_set_dir(JOYSTICK_BOTAO, GPIO_IN);
    gpio_pull_up(JOYSTICK_BOTAO);  // Habilita o pull-up interno (pressionando o botão -> 0)

    // run through the complete initialization process
    SSD1306_init();

    // Initialize render area for entire frame (SSD1306_WIDTH pixels by SSD1306_NUM_PAGES pages)
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1306_WIDTH - 1,
        start_page : 0,
        end_page : SSD1306_NUM_PAGES - 1
        };

    calc_render_area_buflen(&frame_area);

    // zero the entire display
    uint8_t buf[SSD1306_BUF_LEN];
    memset(buf, 0, SSD1306_BUF_LEN);
    render(buf, &frame_area);

restart:
    
    // Chama a função de introdução
    introducao(buf, &frame_area); // Mostra as boas-vindas

    // Captura a escolha do usuário
    resultado = escolha(buf, &frame_area);

    // Apaga o display antes de executar a função escolhida
    apagar_display(buf, &frame_area);

    if (resultado == 'A') {
        // Se a escolha for 'A', chama o afinador
        afinador(buf, &frame_area);
    } 
    else if (resultado == 'B') {
        // Se a escolha for 'A', chama o afinador

        // Verifica se o botão B foi pressionado para interromper o metrônomo
        while (gpio_get(BOTAO_PIN_B) == 1) {
            metronome(buf, &frame_area);
            sleep_ms(100); // Pequeno atraso para Debouce
        }
        
    }

    // Apaga o display após a execução do metrônomo ou afinador
    apagar_display(buf, &frame_area);

    // Desliga o LED verde após a execução
    gpio_put(LED_GREEN, 0);


    goto restart; // Retorna ao ponto de restart
    

#endif
    return 0;
}
