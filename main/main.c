#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include <esp_wifi_types.h>
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "intr_types.h"
#include "driver/spi_master.h"


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void  configure_wifi(void);
static void config_spi(void);
static void SpiTranceiveData(uint8_t data_out);

#define DEFAULT_WIFI_PROTOCOL_ (WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N)
#define DEFAULT_CHANNEL_ 1
#define DEFAULT_MAX_NUM_CONNECTIONS 5
#define DEFAULT_MAX_TX_POWER_ 80
#define DEFAULT_IP_A (uint32_t)192
#define DEFAULT_IP_B (uint32_t)168
#define DEFAULT_IP_C (uint32_t)4
#define DEFAULT_IP_D (uint32_t)1
#define DEFAULT_NETMASK_A (uint32_t)255
#define DEFAULT_NETMASK_B (uint32_t)255
#define DEFAULT_NETMASK_C (uint32_t)255
#define DEFAULT_NETMASK_D (uint32_t)0

/*#define CS_PIN                      17
#define MOSI_PIN                    26
#define MISO_PIN                    47
#define CLK_PIN                     21*/

#define CS_PIN                      15
#define MOSI_PIN                    16
#define MISO_PIN                    17
#define CLK_PIN                     18

static const char *TAG_AP = "WiFi SoftAP";
spi_device_handle_t spi2_handle; 
uint8_t tx_buffer[300] ={0x00,};

//All FRERTOS tasks running on core 0 --> configured in menu config
void app_main(void)
{
    configure_wifi();
    config_spi();
    while(1){//send 300 bytes via spi every 100 ms

        for(uint16_t i = 0; i< sizeof(tx_buffer); i++){
            SpiTranceiveData(tx_buffer[i]);
        }
        vTaskDelay(100/portTICK_PERIOD_MS);

    }

}

static void event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void configure_wifi(void){

      esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); 
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    //NETIF VARS
    esp_netif_t* wifi_server_handle;
    esp_netif_ip_info_t ip_default;
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default()); //cria loop default para eventos do sistema 
    
    wifi_server_handle = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,NULL));


    // Initialize and start WiFi
   wifi_config_t wifi_config = {
        .ap = {
            .ssid_hidden = 0,
            //.ssid = SSID_DEFAULT_,
            //.ssid_len = strlen((const char *)SSID_DEFAULT_),
            .channel = DEFAULT_CHANNEL_,
            //.password = DEFAULT_PWD_,
            .max_connection = DEFAULT_MAX_NUM_CONNECTIONS,
            //.authmode = WIFI_AUTH_WPA_WPA2_PSK,
             .pmf_cfg = {
               .required = false,
            },
        },
    };
    
    //printf(" o Tamanhi e %d \r\n", sizeof(wifi_auth_mode_t));
    strcpy((char*) wifi_config.ap.ssid, "ESP_TEST_1234567");
    wifi_config.ap.ssid_len = 16;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    strcpy((char*) wifi_config.ap.password, "12345678");

    //ESP_LOG_BUFFER_HEXDUMP("TAG", &wifi_config.ap, sizeof(wifi_config.ap), ESP_LOG_INFO);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, DEFAULT_WIFI_PROTOCOL_)); 
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    IP4_ADDR(&ip_default.ip, DEFAULT_IP_A, DEFAULT_IP_B, DEFAULT_IP_C, DEFAULT_IP_D);
	IP4_ADDR(&ip_default.gw, DEFAULT_IP_A, DEFAULT_IP_B, DEFAULT_IP_C, DEFAULT_IP_D);
	IP4_ADDR(&ip_default.netmask, DEFAULT_NETMASK_A, DEFAULT_NETMASK_B, DEFAULT_NETMASK_C, DEFAULT_NETMASK_D);
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(wifi_server_handle));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_server_handle, &ip_default));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(wifi_server_handle));


    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
   
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(DEFAULT_MAX_TX_POWER_));


   printf("Finished configuring Wifi\r\n");
}

static void config_spi(void){
      //SPI Init:
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
        .isr_cpu_id = INTR_CPU_ID_0,
    };
 

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    vTaskDelay(200/portTICK_PERIOD_MS);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,  // 1 MHz
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,                  //SPI mode 0
        .spics_io_num = CS_PIN,     
        .queue_size = 1,
        .flags = 0,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi2_handle);
    ESP_ERROR_CHECK(ret);
    printf("Finished configurin SPI \r\n");

}

static void SpiTranceiveData(uint8_t data_out){
 
    uint8_t data_in;
    bool AckReceived = false;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));  
    t.length = 8;
    t.rxlength = 8;
    t.tx_buffer = &data_out;
    t.rx_buffer = &data_in;

    ESP_ERROR_CHECK(spi_device_polling_transmit(spi2_handle, &t));

}