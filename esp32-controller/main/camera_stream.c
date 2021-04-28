#include "camera_stream.h"

void wifi_rx_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
  static int s_retry_num;
  printf("%p", event_data);
  //printf("\n\nSource Port: %d\nDest Port: %d\nData Len: %d\nChecksum: %d\n", source_port, dest_port, data_len, checksum);
  if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        initialize_socket();
        s_retry_num = 0;
  }
  
  if(event_base == WIFI_EVENT){
      switch (event_id){
        case WIFI_EVENT_AP_STACONNECTED:
          #ifdef DEBUG
          printf("Station Connected to AP\n");
          socket_info_t * sock = initialize_socket();
          xTaskCreate(handle_camera_socket, "CAMSOCK", 4096, sock, 5, NULL);
          #endif
          break;
        case WIFI_EVENT_AP_START:
          #ifdef DEBUG
          printf("AP has come online\n");
          #endif
          break;
        case WIFI_EVENT_STA_START:
          #ifdef DEBUG
          printf("Station mode has initialized\n");
          #endif
          break;
        case WIFI_EVENT_WIFI_READY:
          #ifdef DEBUG_I
          printf("Wifi ready?\n");
          #endif
          // payload is zero in length; Do we get the header though?
          break;
        case WIFI_EVENT_STA_DISCONNECTED:
          esp_wifi_connect();
          s_retry_num += 1;
          if(s_retry_num > 3){
            initialize_wifi();
          }
          break;
        default:
          #ifdef DEBUG_I
          printf("UNDEFINED PACKET TYPE!\n\n");
          #endif
          break;
      }
  }else if(event_base == WIFI_PROV_EVENT){
    switch (event_id) {
          case WIFI_PROV_START:
              ESP_LOGI(TAG, "Provisioning started");
              break;
          case WIFI_PROV_CRED_RECV: {
              wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
              ESP_LOGI(TAG, "Received Wi-Fi credentials"
                        "\n\tSSID     : %s\n\tPassword : %s",
                        (const char *) wifi_sta_cfg->ssid,
                        (const char *) wifi_sta_cfg->password);
              break;
          }
          case WIFI_PROV_CRED_FAIL: {
              wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
              ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                        "\n\tPlease reset to factory and retry provisioning",
                        (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                        "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
              break;
          }
          case WIFI_PROV_CRED_SUCCESS:
              ESP_LOGI(TAG, "Provisioning successful");
              break;
          case WIFI_PROV_END:
              /* De-initialize manager once provisioning is finished */
              wifi_prov_mgr_deinit();
              break;
          default:
              break;
      }
    
  }
  
  
}

void handle_camera_socket(void * args){
  socket_info_t socket_id;
  memcpy(&socket_id, (socket_info_t *) args, sizeof(socket_info_t));
  free(args); // throw out the memory we allocated
  char * buff = calloc(21000,sizeof(char));
  int bytes_read;
  
  printf("%s", buff);
  for(;;){
    bytes_read = recvfrom(socket_id.sock_fd, buff, 21000,0, &socket_id.addr, sizeof(socket_id.addr));
    printf("read %d\n",bytes_read);
  }
}

/*
FUNCTION: initialize_socket
ARGS:     None
RETURN    None
USAGE:    Used to initialize all sockets necessary for the rover function
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
socket_info_t * initialize_socket(void){
  wifi_sta_list_t wifi_sta_list;
  tcpip_adapter_sta_list_t ip_sta_list;
  int s_fd;
  struct sockaddr_in cam_addr;
  int new_sock;

  memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
  memset(&ip_sta_list, 0, sizeof(ip_sta_list));


  #ifdef DEBUG
    printf("Getting station info\n");
  #endif
  vTaskDelay(500); // Issue with DHCP not being finished
  ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
  tcpip_adapter_get_sta_list(&wifi_sta_list, &ip_sta_list);
  
  // Print the MAC address
  #ifdef DEBUG
    if(wifi_sta_list.num > 0){
      printf("Num Sta: %d\n\n", ip_sta_list.num);
      for(int i=0; i< wifi_sta_list.num; i++){
        printf("Station: %d\nmac: %d", i, wifi_sta_list.sta->mac[0]);
        for(int j = 1;j < 6; j++){
          printf("::%d", wifi_sta_list.sta->mac[j]);
        }
      }
    }
  #endif

  // Print the IP address
  #ifdef DEBUG
    for(int i = 0;i < ip_sta_list.num; i++){
      printf("\nIP: %s\n\n", ip4addr_ntoa((ip4_addr_t*)&(ip_sta_list.sta[i].ip)));
    }
  #endif
  if(ip_sta_list.num != 0){
    cam_addr.sin_family = AF_INET;
    cam_addr.sin_addr.s_addr = (ip_sta_list.sta[0].ip.addr);
    cam_addr.sin_port = htons(MYPORT);
  }


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

	if (bind(s_fd, (struct sockaddr *)&cam_addr, sizeof(cam_addr)) == -1) {
		perror("bind");
	}
  
  if(listen(s_fd, 1) > 0){
    perror("listen");
  }

  #ifdef DEBUG
  printf("\nSuccess!\n");
  #endif
  
  new_sock = accept(s_fd,&cam_addr, sizeof(cam_addr));
  if(new_sock < 0){
    perror("accept");
  }
  socket_info_t * output = calloc(1, sizeof(socket_info_t));
  output->sock_fd = new_sock;
  memcpy(&output->addr, &cam_addr, sizeof(cam_addr));
  return output;
}


/*
FUNCTION: initialize_wifi
ARGS:     None
RETURN    None
USAGE:    Used to initialize the esp32 as a station and access point for local nodes
          Set the station provisioning details at camera_stream.h for function(wifi names & passwords) !(move to file with time left)
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
static void initialize_wifi(void)
{
  wifi_config_t * ap_config = calloc(1, sizeof(wifi_config_t));
  wifi_config_t * sta_config = calloc(1, sizeof(wifi_config_t));
  
  
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
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_rx_handler,
                                                        NULL,
                                                        NULL));
  ESP_ERROR_CHECK(esp_wifi_start());

  #ifdef PROMIS
  // Setup packet sniffer (turn off for production[hurts performance])
  //esp_wifi_set_protocol(ifx, WIFI_PROTOCOL_11B| WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR);
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_rx_handler));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  esp_wifi_get_promiscuous(&status);

  #ifdef DEBUG 
  printf("Status of promiscuous: %d",status);
  #endif
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
  mdns_hostname_set("TATOW_CONTROLLER");
  
  wifi_prov_mgr_config_t config = {
    .scheme =  wifi_prov_scheme_softap,
    .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
  };
  ESP_ERROR_CHECK( wifi_prov_mgr_init(config) );

  wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
  const char * pop = "TaT0WruL35";
  ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, _ap_ssid_, _ap_password_));
  wifi_prov_mgr_wait();
  
  /*
  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_t ap_netif;
  esp_netif_t sta_netif;
  ESP_ERROR_CHECK(esp_netif_attach_wifi_ap(ap_netif));
  ESP_ERROR_CHECK(esp_netif_attach_wifi_station(sta_netif));
  esp_netif_dhcps_start(ap_netif);
  esp_netif_dhcpc_start(sta_netif);
  */

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
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // initialize the tcp stack
  tcpip_adapter_init();
  #ifdef DEBUG 
  printf("Attempting to initialize wifi\n");
  #endif
  initialize_wifi();
  
  //openssl_server_init();
  vTaskDelay(1000);
  find_mdns_service("_camStream", "_udp");
}


void mdns_print_results(mdns_result_t * results){
    mdns_result_t * r = results;
    mdns_ip_addr_t * a = NULL;
    int i = 1, t;
    while(r){
        printf("%d: Interface: %s, Type: %s\n", i++, if_str[r->tcpip_if], ip_protocol_str[r->ip_protocol]);
        if(r->instance_name){
            printf("  PTR : %s\n", r->instance_name);
        }
        if(r->hostname){
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if(r->txt_count){
            printf("  TXT : [%u] ", r->txt_count);
            for(t=0; t<r->txt_count; t++){
                printf("%s=%s; ", r->txt[t].key, r->txt[t].value);
            }
            printf("\n");
        }
        a = r->addr;
        while(a){
            if(a->addr.type == IPADDR_TYPE_V6){
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }

}

void find_mdns_service(const char * service_name, const char * proto)
{
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t * results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if(err){
        ESP_LOGE(TAG, "Query Failed");
        return;
    }
    if(!results){
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
}