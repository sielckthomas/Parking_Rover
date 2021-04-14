/* HTTP Restful API Server
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "camera_stream.h"


void wifi_rx_handler(void *buf, wifi_promiscuous_pkt_type_t type)
{
  wifi_promiscuous_pkt_t * packet = (wifi_promiscuous_pkt_t *)buf;
  if (packet == NULL){
    return;
  }
  static u_int16_t source_port, dest_port, data_len, checksum;
  memcpy(&source_port, packet->payload, 2);
  memcpy(&dest_port, &(packet->payload[2]), 2);
  memcpy(&data_len, &(packet->payload[4]), 2);
  memcpy(&checksum, &(packet->payload[6]), 2);

  //printf("\n\nSource Port: %d\nDest Port: %d\nData Len: %d\nChecksum: %d\n", source_port, dest_port, data_len, checksum);
  switch (type){
    case WIFI_PKT_MGMT:
      #ifdef DEBUG_I
      printf("MGMT!");
      #endif
      break;
    case WIFI_PKT_CTRL:
      #ifdef DEBUG_I
      printf("CNTRL PKT!");
      #endif
      break;
    case WIFI_PKT_DATA:
      #ifdef DEBUG_I
      printf("\nDATA PKT!\n");
      #endif
      break;
    case WIFI_PKT_MISC:
      #ifdef DEBUG_I
      printf("MISC PKT!");
      #endif
      // payload is zero in length; Do we get the header though?
      break;
    default:
      #ifdef DEBUG_I
      printf("UNDEFINED PACKET TYPE!\n\n");
      #endif
      break;
  }
  
}

/*
FUNCTION: initialize_sockets
ARGS:     None
RETURN    None
USAGE:    Used to initialize all sockets necessary for the rover function
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
void initialize_sockets(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data){
  int s_fd;
  struct sockaddr_in main_controller_addr;
  struct hostent * he;


  #ifdef DEBUG
      printf("Trying to open socket\n");
  #endif
  s_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (s_fd == -1) { 
	  #ifdef DEBUG
      printf("Failed to open socket\n");
    #endif
    return;
	}

  if ((he = gethostbyname(MYADDR)) == NULL) {
		perror("gethostbyname");
	}
  
  main_controller_addr.sin_family = AF_INET;
  main_controller_addr.sin_addr = *((struct in_addr *)he->h_addr_list[0]);
  main_controller_addr.sin_port = htons(MYPORT);

	if (bind(s_fd, (struct sockaddr *)&main_controller_addr, sizeof(main_controller_addr)) == -1) {
		perror("bind");
	}

  #ifdef DEBUG
  printf("Success!\n");
  #endif
  return;
}

void initialize_DHCP_client(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data){
  
  return;
}

void schedule_wifi_handlers(void){
    esp_event_handler_register(WIFI_EVENT, IP_EVENT_STA_GOT_IP, initialize_sockets, NULL);
}

/*
FUNCTION: initialize_wifi
ARGS:     None
RETURN    None
USAGE:    Used to initialize the esp32 as a station and access point for local nodes
          Set the station provisioning details at the top for function(wifi names & passwords) !(move to file with time left)
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
static void initialize_wifi(void)
{
  wifi_config_t * ap_config = calloc(1, sizeof(wifi_config_t));
  wifi_config_t * sta_config = calloc(1, sizeof(wifi_config_t));
  bool status;
  
  // Set up the wifi
  const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init((&cfg)));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  // Configure the (soft-AP) Access Point
  strcpy((char *)ap_config->ap.ssid,_ap_ssid_);
  strcpy((char *)ap_config->ap.password, _ap_password_);
  ap_config->ap.authmode = WIFI_AUTH_WPA_PSK; // Move up to WPA2 for AES(slower)
  ap_config->ap.ssid_len = 0;
  ap_config->ap.max_connection = 4;
  ap_config->ap.channel = 0; /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/

  // Configure the (STA) station
  strcpy((char *)sta_config->sta.ssid, _sta_ssid_);
  strcpy((char *)sta_config->sta.password, _sta_password_);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, sta_config));
  
  ESP_ERROR_CHECK(esp_wifi_start());

  // Setup packet sniffer (turn off for production[hurts performance])
  //esp_wifi_set_protocol(ifx, WIFI_PROTOCOL_11B| WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR);
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_rx_handler));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  esp_wifi_get_promiscuous(&status);
  #ifdef DEBUG 
  printf("Status of promiscuous: %d",status);
  #endif
  
  ESP_ERROR_CHECK(esp_wifi_connect());

  // stop DHCP server
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

  // assign a static IP to the network interface
  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  IP4_ADDR(&info.ip, 192, 168, 10, 1);
  IP4_ADDR(&info.gw, 192, 168, 10, 1);
  IP4_ADDR(&info.netmask, 255, 255, 255, 0);
  ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));

  // start the DHCP server
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

  //initialize mDNS service
  esp_err_t err = mdns_init();
  if (err) {
      printf("MDNS Init failed: %d\n", err);
      return;
  }


  //set hostname
  mdns_hostname_set("Tatow_main");
  //set default instance
  mdns_instance_name_set("Tatow_cntrlr");
  mdns_service_add(NULL, "_camStream", "_udp", (uint16_t)MYPORT, NULL, 0);
  mdns_txt_item_t service_properties[3] = {
      {"s","state"},
      {"u","user"},
      {"p","password"}
  };
  mdns_service_txt_set("_camStream", "_udp",service_properties, 3);

  //xTaskCreate(wifi_event_handler, "WIFI", 4096, NULL, 5, NULL);
}

/*
FUNCTION: start_stream
ARGS:     None
RETURN    None
USAGE:    Initializes the wifi and sets up the sockets to begin to receive camera data.
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
static void start_stream(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());

  #ifdef DEBUG 
  printf("Attempting to initialize wifi\n"); 
  #endif
  initialize_wifi();

  schedule_wifi_handlers();
  
  //openssl_server_init();
}
/*
static void openssl_example_task(void *p)
{
    int ret;

    SSL_CTX *ctx;
    SSL *ssl;

    int sockfd, new_sockfd;
    socklen_t addr_len;
    struct sockaddr_in sock_addr;

    char recv_buf[OPENSSL_EXAMPLE_RECV_BUF_LEN];

    const char send_data[] = OPENSSL_EXAMPLE_SERVER_ACK;
    const int send_bytes = sizeof(send_data);

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    const unsigned int cacert_pem_bytes = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    const unsigned int prvtkey_pem_bytes = prvtkey_pem_end - prvtkey_pem_start;

    ESP_LOGI(TAG, "SSL server context create ......");
    */
    /* For security reasons, it is best if you can use
       TLSv1_2_server_method() here instead of TLS_server_method().
       However some old browsers may not support TLS v1.2.
    */
    /*
    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ESP_LOGI(TAG, "failed");
        goto failed1;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set own certification......");
    ret = SSL_CTX_use_certificate_ASN1(ctx, cacert_pem_bytes, cacert_pem_start);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set private key......");
    ret = SSL_CTX_use_PrivateKey_ASN1(0, ctx, prvtkey_pem_start, prvtkey_pem_bytes);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server create socket ......");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket bind ......");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(OPENSSL_EXAMPLE_LOCAL_TCP_PORT);
    ret = bind(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket listen ......");
    ret = listen(sockfd, 32);
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
    ESP_LOGI(TAG, "SSL server create ......");
    ssl = SSL_new(ctx);
    if (!ssl) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket accept client ......");
    new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
    if (new_sockfd < 0) {
        ESP_LOGI(TAG, "failed" );
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    SSL_set_fd(ssl, new_sockfd);

    ESP_LOGI(TAG, "SSL server accept client ......");
    ret = SSL_accept(ssl);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed5;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server read message ......");
    do {
        memset(recv_buf, 0, OPENSSL_EXAMPLE_RECV_BUF_LEN);
        ret = SSL_read(ssl, recv_buf, OPENSSL_EXAMPLE_RECV_BUF_LEN - 1);
        if (ret <= 0) {
            break;
        }
        ESP_LOGI(TAG, "SSL read: %s", recv_buf);
        if (strstr(recv_buf, "GET ") &&
            strstr(recv_buf, " HTTP/1.1")) {
            ESP_LOGI(TAG, "SSL get matched message");
            ESP_LOGI(TAG, "SSL write message");
            ret = SSL_write(ssl, send_data, send_bytes);
            if (ret > 0) {
                ESP_LOGI(TAG, "OK");
            } else {
                ESP_LOGI(TAG, "error");
            }
            break;
        }
    } while (1);

    SSL_shutdown(ssl);
failed5:
    close(new_sockfd);
    new_sockfd = -1;
failed4:
    SSL_free(ssl);
    ssl = NULL;
    goto reconnect;
failed3:
    close(sockfd);
    sockfd = -1;
failed2:
    SSL_CTX_free(ctx);
    ctx = NULL;
failed1:
    vTaskDelete(NULL);
    return ;
}
*//*
static void openssl_server_init(void)
{
    int ret;
    xTaskHandle openssl_handle;

    ret = xTaskCreate(openssl_example_task,
                      OPENSSL_EXAMPLE_TASK_NAME,
                      OPENSSL_EXAMPLE_TASK_STACK_WORDS,
                      NULL,
                      OPENSSL_EXAMPLE_TASK_PRIORITY,
                      &openssl_handle);

    if (ret != pdPASS)  {
        ESP_LOGI(TAG, "create task %s failed", OPENSSL_EXAMPLE_TASK_NAME);
    }
}
*/