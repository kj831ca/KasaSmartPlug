// KASASmartPlug library v0.1 9/2/2022
// I got Covid19 on 8/22/2022 and had to stayed home
//  with the boredom I wrote this thing..

#include "KasaSmartPlug.hpp"

const char *KASAUtil::get_kasa_info = "{\"system\":{\"get_sysinfo\":null}}";
const char *KASAUtil::relay_on = "{\"system\":{\"set_relay_state\":{\"state\":1}}}";
const char *KASAUtil::relay_off = "{\"system\":{\"set_relay_state\":{\"state\":0}}}";

uint16_t KASAUtil::Encrypt(const char *data, int length, uint8_t addLengthByte, char *encryped_data)
{
    uint8_t key = KASA_ENCRYPTED_KEY;
    uint8_t en_b;
    int index = 0;
    if (addLengthByte)
    {
        encryped_data[index++] = 0;
        encryped_data[index++] = 0;
        encryped_data[index++] = (char)(length >> 8);
        encryped_data[index++] = (char)(length & 0xFF); 
    }

    for (int i = 0; i < length; i++)
    {
        en_b = data[i] ^ key;
        encryped_data[index++] = en_b;
        key = en_b;
    }

    return index;
}

uint16_t KASAUtil::Decrypt(char *data, int length, char *decryped_data, int startIndex)
{
    uint8_t key = KASA_ENCRYPTED_KEY;
    uint8_t dc_b;
    int retLength = 0;
    for (int i = startIndex; i < length; i++)
    {
        dc_b = data[i] ^ key;
        key = data[i];
        decryped_data[retLength++] = dc_b;
    }

    return retLength;
}

void KASAUtil::closeSock(int sock)
{
    if (sock != -1)
    {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}

KASAUtil::KASAUtil()
{
    deviceFound = 0;
}

int KASAUtil::ScanDevices()
{
    struct sockaddr_in dest_addr;
    int ret = 0;
    int boardCaseEnable = 1;
    int retValue = 0;
    int sock;

    int err = 1;
    char sendbuf[128];
    char addrbuf[32] = {0};
    int len;
    const char *string_value;
    const char *model;
    int relay_state;

    StaticJsonDocument<512> doc;

    len = strlen(get_kasa_info);

    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9999);

    //Clean up the previous resource
    if(deviceFound > 0)
    {
        for(int i =0;i<deviceFound;i++)
        {
            delete(ptr_plugs[i]);
        }
    }
    deviceFound = 0; //Reset all device..
    

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        retValue = -1;
        closeSock(sock);
        return retValue;
    }

    ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boardCaseEnable, sizeof(boardCaseEnable));
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Unable to set broadcase option %d", errno);
        retValue = -2;
        closeSock(sock);
        return retValue;
    }

    len = KASAUtil::Encrypt(get_kasa_info, len, 0, sendbuf);
    if (len > sizeof(sendbuf))
    {
        ESP_LOGE(TAG, "Overflowed multicast sendfmt buffer!!");

        retValue = -3;
        closeSock(sock);
        return retValue;
    }

    // Sending the first broadcase message out..
    ESP_LOGI(TAG, "Send Query Message length  %s %d", get_kasa_info, len);

    err = sendto(sock, sendbuf, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        closeSock(sock);
        return -4;
    }
    Serial.println("Query Message sent");
    int send_loop = 0;
    while ((err > 0) && (send_loop < 1))
    {

        struct timeval tv = {
            .tv_sec = 3,
            .tv_usec = 0,
        };
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        Serial.println("Enter select function...");

        int s = select(sock + 1, &rfds, NULL, NULL, &tv);
        Serial.printf("Select value = %d\n", s);

        if (s < 0)
        {
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            err = -1;
            break;
        }
        else if (s > 0)
        {
            if (FD_ISSET(sock, &rfds))
            {
                // Incoming datagram received
                char recvbuf[1024];
                char raddr_name[32] = {0};

                struct sockaddr_storage raddr; // Large enough for both IPv4 or IPv6
                socklen_t socklen = sizeof(raddr);
                Serial.println("Waiting incomming package...");
                int len = recvfrom(sock, recvbuf, sizeof(recvbuf) - 1, 0,
                                   (struct sockaddr *)&raddr, &socklen);
                if (len < 0)
                {
                    ESP_LOGE(TAG, "multicast recvfrom failed: errno %d", errno);
                    err = -1;
                    break;
                }
                else
                {
                    len = KASAUtil::Decrypt(recvbuf, len, recvbuf, 0);
                }

                if (raddr.ss_family == PF_INET)
                {
                    inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr,
                                raddr_name, sizeof(raddr_name) - 1);
                }

                Serial.printf("received %d bytes from %s: \r\n", len, raddr_name);

                recvbuf[len] = 0; // Null-terminate whatever we received and treat like a string...

                // We got the response from the broadcast message
                // I found HS103 plug would response around 500 to 700 bytes of JSON data
                if (len > 500)
                {
                    Serial.println("Parsing info...");
                    DeserializationError error = deserializeJson(doc, recvbuf, len);

                    if (error)
                    {
                        Serial.print("deserializeJson() failed: ");
                        Serial.println(error.c_str());
                    }
                    else
                    {
                        JsonObject get_sysinfo = doc["system"]["get_sysinfo"];
                        string_value = get_sysinfo["alias"];
                        relay_state = get_sysinfo["relay_state"];
                        model = get_sysinfo["model"];

                        if (IsStartWith("HS103", model) || IsStartWith("HS200", model))
                        {
                            // Limit the number of devices and make sure no duplicate device.
                            if (IsContainPlug(string_value) == -1)
                            {
                                // New device has been found
                                if (deviceFound < MAX_PLUG_ALLOW)
                                {
                                    ptr_plugs[deviceFound] = new KASASmartPlug(string_value, raddr_name);
                                    ptr_plugs[deviceFound]->state = relay_state;
                                    strcpy(ptr_plugs[deviceFound]->model, model);
                                    deviceFound++;
                                } else 
                                {
                                    Serial.printf("\r\n Error unable to add more plug");
                                }
                            }else 
                            {
                                //Plug is already in the collection then update IP Address..
                                KASASmartPlug *plug = KASAUtil::GetSmartPlug(string_value);
                                if(plug != NULL)
                                {
                                    plug->UpdateIPAddress(raddr_name);
                                }
                            }
                        }
                    }
                }
            }
            else
            {

                // int len = snprintf(sendbuf, sizeof(sendbuf), sendfmt, send_count++);

                Serial.println("Send Query Message");

                err = sendto(sock, sendbuf, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (err < 0)
                {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    retValue = -5;
                }
                Serial.println("Query Message sent");
            }
        }
        else if (s == 0) // Timeout package
        {
            Serial.println("S Timeout Send Query Message");
            send_loop++;

            err = sendto(sock, sendbuf, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                retValue = -1;
                closeSock(sock);
                return retValue;
            }
            Serial.println("Query Message sent");
        }

        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
    closeSock(sock);
    return deviceFound;
}

int KASAUtil::IsContainPlug(const char *name)
{
    int i;
    KASASmartPlug *plug;
    if (deviceFound == 0)
        return -1;
    for (i = 0; i < deviceFound; i++)
    {
        if (strcmp(name, ptr_plugs[i]->alias) == 0)
            return i;
    }

    return -1;
}

SemaphoreHandle_t KASASmartPlug::mutex = xSemaphoreCreateMutex();

KASASmartPlug *KASAUtil::GetSmartPlugByIndex(int index)
{
    if (index < -0)
        return NULL;

    if (index < deviceFound)
    {
        return ptr_plugs[index];
    }
    else
        return NULL;
}

KASASmartPlug *KASAUtil::GetSmartPlug(const char *alias_name)
{
    for (int i = 0; i < deviceFound; i++)
    {
        if (strcmp(alias_name, ptr_plugs[i]->alias) == 0)
            return ptr_plugs[i];
    }
    return NULL;
}

void KASASmartPlug::SendCommand(const char *cmd)
{
    int err;
    char sendbuf[128];
    xSemaphoreTake(mutex, portMAX_DELAY);
    OpenSock();
    int len = KASAUtil::Encrypt(cmd, strlen(cmd), 1, sendbuf);
    if (sock > 0)
    {
        // DebugPrint(sendbuf,len);
        err = send(sock, sendbuf, len, 0);
        if (err < 0)
        {
            Serial.printf("\r\n Error while sending data %d", errno);
        }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Make sure the data has been send out before close the socket.
    CloseSock();
    xSemaphoreGive(mutex);
}

int KASASmartPlug::Query(const char *cmd, char *buffer, int bufferLength, long timeout)
{
    int sendLen;
    int recvLen;
    int err;
    xSemaphoreTake(mutex, portMAX_DELAY);
    recvLen = 0;
    err = 0;
    OpenSock();
    sendLen = KASAUtil::Encrypt(cmd, strlen(cmd), 1, buffer);
    if (sock > 0)
    {
        err = send(sock, buffer, sendLen, 0);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    else
    {
        CloseSock();
        xSemaphoreGive(mutex);
        return 0;
    }

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = timeout,
    };
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    int s = select(sock + 1, &rfds, NULL, NULL, &tv);
    // xSemaphoreTake(mutex, portMAX_DELAY);
    if (s < 0)
    {
        Serial.printf("Select failed: errno %d", errno);
        err = -1;
        CloseSock();
        xSemaphoreGive(mutex);
        return 0;
    }
    else if (s > 0)
    {

        if (FD_ISSET(sock, &rfds))
        {
            // We got response here.
            recvLen = recv(sock, buffer, bufferLength, 0);

            if (recvLen > 0)
            {
                recvLen = KASAUtil::Decrypt(buffer, recvLen, buffer, 4);
            }
        }
    }
    else if (s == 0)
    {
        Serial.println("Error TCP Read timeout...");
        CloseSock();
        xSemaphoreGive(mutex);
        return 0;
    }

    CloseSock();
    xSemaphoreGive(mutex);
    return recvLen;
}
int KASASmartPlug::UpdateInfo()
{
    char buffer[1024];
    int recvLen = Query(KASAUtil::get_kasa_info, buffer, 1024, 300000);

    if (recvLen > 500)
    {

        // Serial.println("Parsing info...");
        // Because the StaticJSONDoc uses quite memory block other plug while parsing JSON ...
        xSemaphoreTake(mutex, portMAX_DELAY);
        DeserializationError error = deserializeJson(doc, buffer, recvLen);

        if (error)
        {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            recvLen = -1;
        }
        else
        {
            JsonObject get_sysinfo = doc["system"]["get_sysinfo"];
            state = get_sysinfo["relay_state"];
            err_code = get_sysinfo["err_code"];
            strcpy(alias,get_sysinfo["alias"]);
            Serial.printf("\r\n Relay state: %d Error Code %d", state, err_code);
        }
        xSemaphoreGive(mutex);
    }

    return recvLen;
}

void KASASmartPlug::SetRelayState(uint8_t state)
{
    if (state > 0)
    {
        SendCommand(KASAUtil::relay_on);
    }
    else
        SendCommand(KASAUtil::relay_off);
}
void KASASmartPlug::DebugBufferPrint(char *data, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (i % 8 == 0)
            Serial.print("\r\n");
        else
            Serial.print(" ");

        Serial.printf("%d ", data[i]);
    }
}
