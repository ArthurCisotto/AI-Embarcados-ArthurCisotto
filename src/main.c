#include <asf.h>
#include "conf_board.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"


#define CLK_PIO       PIOD
#define CLK_PIO_ID    ID_PIOD
#define CLK_IDX       30
#define CLK_IDX_MASK  (1u << CLK_IDX)

#define DT_PIO       PIOA
#define DT_PIO_ID    ID_PIOA
#define DT_IDX       6
#define DT_IDX_MASK  (1u << DT_IDX)

#define SW_PIO       PIOC
#define SW_PIO_ID    ID_PIOC
#define SW_IDX       19
#define SW_IDX_MASK  (1u << SW_IDX)

#define LED_PIO           PIOC
#define LED_PIO_ID        ID_PIOC
#define LED_PIO_IDX       30
#define LED_PIO_IDX_MASK  (1 << LED_PIO_IDX)

#define OLED_WIDTH 128
#define OLED_HEIGHT 32

/** RTOS  */
#define TASK_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);


void clk_callback(void);
void sw_callback(void);
void io_init(void);

QueueHandle_t xQueueAcrescimo;
QueueHandle_t xQueueContadorCaracteres;
QueueHandle_t xQueueReset;
QueueHandle_t xQueueLED;

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/


void clk_callback(void) {
	int acrescimo = 0;
	printf("clk_callback \n");
	if(pio_get(DT_PIO, PIO_INPUT, DT_IDX_MASK)){
		printf("Sentido horario \n");
		acrescimo = 1;
		xQueueSendFromISR(xQueueAcrescimo, &acrescimo, NULL);
	} else {
		printf("Sentido anti-horario \n");
		acrescimo = -1;
		xQueueSendFromISR(xQueueAcrescimo, &acrescimo, NULL);
	}
}

void sw_callback(void) {
	int contadorCaracteres = 0;
	int reset = 0;
	if(pio_get(SW_PIO, PIO_INPUT, SW_IDX_MASK)){
		printf("Botao solto \n");
		uint32_t delta_t = rtt_read_timer_value(RTT);
		if (delta_t/32768 >= 5){
			printf("Tempo de pressionamento: %d segundos \n", delta_t/32768);
			printf("SEGUROU 5 SEGUNDOS \n");
			reset = 1;
			xQueueSendFromISR(xQueueReset, &reset, NULL);
			xQueueSendFromISR(xQueueLED, &reset, NULL);
		} else {
			printf("Tempo de pressionamento: %d milisegundos \n", delta_t/32);
			contadorCaracteres = 1;
			xQueueSendFromISR(xQueueContadorCaracteres, &contadorCaracteres, NULL);
		}
	} else {
		printf("Botao pressionado \n");
		rtt_init(RTT, 1);
	}
}


/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_oled(void *pvParameters) {
	gfx_mono_ssd1306_init();
	gfx_mono_draw_string("Arthur Cisotto", 0, 0, &sysfont);

	int caracterSelecionado = 1;
	int acrescimo = 0;
	int contadorCaracteres = 0;
	int reset = 0;

    int caracter1 = 0;
	int caracter2 = 0;
	int caracter3 = 0;
	int caracter4 = 0;
	
	char texto[15];

	for (;;)  {
		if (xQueueReceive(xQueueReset, &reset, 0) == pdTRUE) {
			caracter1 = 0;
			caracter2 = 0;
			caracter3 = 0;
			caracter4 = 0;
			caracterSelecionado = 1;
		}
		if (xQueueReceive(xQueueContadorCaracteres, &contadorCaracteres, 0) ==pdTRUE){
			caracterSelecionado += contadorCaracteres;
			if (caracterSelecionado > 4) {
				caracterSelecionado = 1;
			} else if (caracterSelecionado < 1) {
				caracterSelecionado = 4;
			}
			//printf("caracterSelecionado: %d \n", caracterSelecionado);
		}
		if (xQueueReceive(xQueueAcrescimo, &acrescimo, 0) == pdTRUE) {
			if (caracterSelecionado == 1){
				caracter1 += acrescimo;
				if (caracter1 > 15) {
					caracter1 = 0;
				} else if (caracter1 < 0) {
					caracter1 = 15;
				}
				//printf("caracter1: %X \n", caracter1);
			}
			if (caracterSelecionado == 2){
				caracter2 += acrescimo;
				if (caracter2 > 15) {
					caracter2 = 0;
				} else if (caracter2 < 0) {
					caracter2 = 15;
				}
				//printf("caracter2: %X \n", caracter2);
			}
			if (caracterSelecionado == 3){
				caracter3 += acrescimo;
				if (caracter3 > 15) {
					caracter3 = 0;
				} else if (caracter3 < 0) {
					caracter3 = 15;
				}
				//printf("caracter3: %X \n", caracter3);
			}
			if (caracterSelecionado == 4){
				caracter4 += acrescimo;
				if (caracter4 > 15) {
					caracter4 = 0;
				} else if (caracter4 < 0) {
					caracter4 = 15;
				}
				//printf("caracter4: %X \n", caracter4);
			}
			
		}
		sprintf(texto, "0 x %X %X %X %X", caracter1, caracter2, caracter3, caracter4);
		gfx_mono_draw_string(texto, 0, 20, &sysfont);
		vTaskDelay(100);
		if (caracterSelecionado == 1) {
			texto[4] = ' ';
		} else if (caracterSelecionado == 2) {
			texto[6] = ' ';
		} else if (caracterSelecionado == 3) {
			texto[8] = ' ';
		} else if (caracterSelecionado == 4) {
			texto[10] = ' ';
		}
		gfx_mono_draw_string(texto, 0, 20, &sysfont);
		vTaskDelay(100);
		//printf("task oled\r");
    

	}
}

static void task_led(void *pvParameters) {
	int reset = 0;
	for (;;) {
		if (xQueueReceive(xQueueLED, &reset, 0) == pdTRUE) {
			for (int i = 0; i < 20; i++) {
				pio_set(LED_PIO, LED_PIO_IDX_MASK);
				vTaskDelay(25);
				pio_clear(LED_PIO, LED_PIO_IDX_MASK);
				vTaskDelay(25);
			}
		}
		
	}
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}


void io_init(void){
	NVIC_EnableIRQ(CLK_PIO_ID);
	NVIC_SetPriority(CLK_PIO_ID, 4);

	pio_configure(CLK_PIO, PIO_INPUT, CLK_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(CLK_PIO, CLK_IDX_MASK, 60);
	pio_enable_interrupt(CLK_PIO, CLK_IDX_MASK);
	pio_handler_set(CLK_PIO, CLK_PIO_ID, CLK_IDX_MASK, PIO_IT_FALL_EDGE , clk_callback);

	NVIC_EnableIRQ(SW_PIO_ID);
	NVIC_SetPriority(SW_PIO_ID, 4);

	pio_configure(DT_PIO, PIO_INPUT, DT_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(DT_PIO, DT_IDX_MASK, 60);

	pio_configure(SW_PIO, PIO_INPUT, SW_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(SW_PIO, SW_IDX_MASK, 60);
	pio_enable_interrupt(SW_PIO, SW_IDX_MASK);
	pio_handler_set(SW_PIO, SW_PIO_ID, SW_IDX_MASK, PIO_IT_EDGE , sw_callback);

	pmc_enable_periph_clk(LED_PIO_ID);
	pio_configure(LED_PIO, PIO_OUTPUT_0, LED_PIO_IDX_MASK, PIO_DEFAULT);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/


int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	io_init();

	/* Initialize the console uart */
	configure_console();

	xQueueAcrescimo = xQueueCreate(1, sizeof(int));
	xQueueContadorCaracteres = xQueueCreate(1, sizeof(int));
	xQueueReset = xQueueCreate(1, sizeof(int));
	xQueueLED = xQueueCreate(1, sizeof(int));

	/* Create task to control oled */
	if (xTaskCreate(task_oled, "oled", TASK_STACK_SIZE, NULL, TASK_STACK_PRIORITY, NULL) != pdPASS) {
	  printf("Failed to create oled task\r\n");
	}

	/* Create task to control led */
	if (xTaskCreate(task_led, "led", TASK_STACK_SIZE, NULL, TASK_STACK_PRIORITY, NULL) != pdPASS) {
	  printf("Failed to create led task\r\n");
	}

	/* Start the scheduler. */
	vTaskStartScheduler();

  /* RTOS n�o deve chegar aqui !! */
	while(1){}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
