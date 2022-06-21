#include <Arduino.h>
#include "driver/i2s.h"

#define SAMPLING_RATE  96000
#define BUFFER         512
#define CHANNEL_BUFFER 1024

//GPIO's
#define BCK  26
#define LRC  33
#define DIN  35
#define DOU  25
#define MCK  0

int rxbuf[CHANNEL_BUFFER], txbuf[CHANNEL_BUFFER];
float l_in[CHANNEL_BUFFER/2], r_in[CHANNEL_BUFFER/2];
float l_out[CHANNEL_BUFFER/2], r_out[CHANNEL_BUFFER/2];

float val_gain_input = 1.0f;
float val_gain_out = 2.0f;


i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
    .sample_rate = SAMPLING_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S, 
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = BUFFER,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = SAMPLING_RATE*384
 };

i2s_pin_config_t i2s_pin_config = {.bck_io_num = BCK, .ws_io_num = LRC, .data_out_num = DOU, .data_in_num = DIN};


void process_Audio( void *pvParameters );

void set_I2S()
{
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_pin_config);
  i2s_set_sample_rates(I2S_NUM_0, SAMPLING_RATE);
  i2s_set_clk(I2S_NUM_0, SAMPLING_RATE, I2S_BITS_PER_SAMPLE_24BIT, I2S_CHANNEL_STEREO);
  i2s_start(I2S_NUM_0);
}

void mclk_pin_select(const uint8_t pin)
{
    if(pin != 0 && pin != 1 && pin != 3) Serial.printf("Only support GPIO0/GPIO1/GPIO3, gpio_num:%d",pin);      
    switch(pin){
        case 0:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
            break;
        case 1:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
            WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
            break;
        case 3:
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
            WRITE_PERI_REG(PIN_CTRL, 0xFF00);
            Serial.println("ok"); 
            break;
        default:
            break;
    }
}

void setup() 
{
  set_I2S();
  mclk_pin_select(MCK); 
  xTaskCreatePinnedToCore(process_Audio, "process_Audio", 4096 , NULL, 10,  NULL, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
}

void process_Audio(void *pvParameters){
  (void) pvParameters;
  float sample = 0;  
  for (;;) {
    size_t readsize = 0;  
          
    esp_err_t rxfb = i2s_read(I2S_NUM_0, &rxbuf[0], CHANNEL_BUFFER * 2, &readsize, 1000);                   
                                
    if (rxfb == ESP_OK && readsize==CHANNEL_BUFFER * 2) 
    {                   
           /*split L an R input*/ 
          int y=0;
          for (int i=0; i<CHANNEL_BUFFER; i+=2) 
          {
            r_out[y] = (float) rxbuf[i];
            l_out[y] = (float) rxbuf[i+1];
            y++;
          }
          
          /*gain input   */      
          for (int i=0; i<CHANNEL_BUFFER/2; i++)
          {  
            r_out[i] = val_gain_input * r_out[i];         
            l_out[i] = val_gain_input * l_out[i];                        
          }   
          
          /*gain output */
          for (int i=0; i<CHANNEL_BUFFER/2; i++) 
          {  
            r_out[i] = val_gain_out * r_out[i];         
            l_out[i] = val_gain_out * l_out[i];
                         
          }     
          y=0;
          for (int i=0;i<CHANNEL_BUFFER/2;i++) 
          {
          txbuf[y]   = (int) l_out[i];
          txbuf[y+1] = (int) r_out[i];
          y=y+2;
          }   
             
         i2s_write(I2S_NUM_0, &txbuf[0], CHANNEL_BUFFER * 2, &readsize, 1000);
    }                   
  } 
  vTaskDelete(NULL); 
}